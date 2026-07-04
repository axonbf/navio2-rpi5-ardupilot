#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <err.h>
#include <errno.h>

#include "platform.h"
#include "gpio.h"

#define LOW                 0
#define HIGH                1

#define MAP_SIZE           (64*1024)

#define RP1_GPIO_CTRL_FUNCSEL_LSB      0
#define RP1_GPIO_CTRL_OUTOVER_LSB      12
#define RP1_GPIO_CTRL_OEOVER_LSB      14
#define RP1_GPIO_CTRL_INOVER_LSB      16

#define RP1_GPIO_FUNCSEL_GPIO         5

#define RP1_GPIO_BANK0_OFFSET         0x0000
#define RP1_GPIO_BANK1_OFFSET          0x4000
#define RP1_GPIO_BANK2_OFFSET          0x8000

#define RP1_RIO_OUT_OFFSET            0x0000
#define RP1_RIO_OE_OFFSET             0x0004
#define RP1_RIO_IN_OFFSET             0x0008

#define RP1_RIO_SET_OFFSET            0x2000
#define RP1_RIO_CLR_OFFSET            0x3000

#define RP1_PADS_BANK0_OFFSET          0x0004
#define RP1_PADS_BANK1_OFFSET          0x4004
#define RP1_PADS_BANK2_OFFSET          0x8004

#define RP1_PADS_SLEWFAST_LSB         0
#define RP1_PADS_SCHMITT_LSB          1
#define RP1_PADS_PULLDOWN_LSB         2
#define RP1_PADS_PULLUP_LSB           3
#define RP1_PADS_DRIVE_LSB             4

static volatile uint8_t *gpio_base = NULL;
static volatile uint8_t *rio_base = NULL;
static volatile uint8_t *pads_base = NULL;
static int mem_fd = -1;

static int rp1_gpio_bank_offset(int gpio)
{
    if (gpio < 28) return RP1_GPIO_BANK0_OFFSET;
    if (gpio < 34) return RP1_GPIO_BANK1_OFFSET;
    return RP1_GPIO_BANK2_OFFSET;
}

static int rp1_rio_bank_offset(int gpio)
{
    if (gpio < 28) return RP1_GPIO_BANK0_OFFSET;
    if (gpio < 34) return RP1_GPIO_BANK1_OFFSET;
    return RP1_GPIO_BANK2_OFFSET;
}

static int rp1_pads_bank_offset(int gpio)
{
    if (gpio < 28) return RP1_PADS_BANK0_OFFSET;
    if (gpio < 34) return RP1_PADS_BANK1_OFFSET;
    return RP1_PADS_BANK2_OFFSET;
}

static int rp1_pin_in_bank(int gpio)
{
    if (gpio < 28) return gpio;
    if (gpio < 34) return gpio - 28;
    return gpio - 34;
}

static void rp1_gpio_write_ctrl(int gpio, uint32_t funcsel)
{
    int bank_off = rp1_gpio_bank_offset(gpio);
    volatile uint32_t *ctrl = (volatile uint32_t *)(gpio_base + bank_off + gpio * 8 + 4);

    uint32_t val = *ctrl;
    val &= ~(0x1FU << RP1_GPIO_CTRL_FUNCSEL_LSB);
    val |= (funcsel << RP1_GPIO_CTRL_FUNCSEL_LSB);
    val &= ~(0x3U << RP1_GPIO_CTRL_OEOVER_LSB);
    val &= ~(0x3U << RP1_GPIO_CTRL_OUTOVER_LSB);
    *ctrl = val;
}

static void rp1_pads_configure(int gpio)
{
    int bank_off = rp1_pads_bank_offset(gpio);
    volatile uint32_t *pad = (volatile uint32_t *)(pads_base + bank_off + gpio * 4);

    uint32_t val = *pad;
    val &= ~(1U << RP1_PADS_PULLDOWN_LSB);
    val |= (1U << RP1_PADS_PULLUP_LSB);
    val |= (1U << RP1_PADS_SCHMITT_LSB);
    *pad = val;
}

static void rp1_rio_set_oe(int gpio, bool enable)
{
    int bank_off = rp1_rio_bank_offset(gpio);
    int pin_num = rp1_pin_in_bank(gpio);
    uint32_t mask = 1U << pin_num;

    volatile uint32_t *oe_reg = (volatile uint32_t *)(rio_base + bank_off + RP1_RIO_OE_OFFSET +
                                   (enable ? RP1_RIO_SET_OFFSET : RP1_RIO_CLR_OFFSET));
    *oe_reg = mask;
}

static void rp1_rio_set_out(int gpio, bool high)
{
    int bank_off = rp1_rio_bank_offset(gpio);
    int pin_num = rp1_pin_in_bank(gpio);
    uint32_t mask = 1U << pin_num;

    volatile uint32_t *out_reg = (volatile uint32_t *)(rio_base + bank_off + RP1_RIO_OUT_OFFSET +
                                    (high ? RP1_RIO_SET_OFFSET : RP1_RIO_CLR_OFFSET));
    *out_reg = mask;
}

static bool rp1_rio_get_in(int gpio)
{
    int bank_off = rp1_rio_bank_offset(gpio);
    int pin_num = rp1_pin_in_bank(gpio);

    volatile uint32_t *in_reg = (volatile uint32_t *)(rio_base + bank_off + RP1_RIO_IN_OFFSET);
    uint32_t val = *in_reg;

    return (val >> pin_num) & 1 ? true : false;
}

static int find_rp1_gpio_phys(uint64_t *gpio_phys, uint64_t *rio_phys, uint64_t *pads_phys)
{
    FILE *f = fopen("/proc/iomem", "r");
    if (!f) {
        err(EXIT_FAILURE, "Cannot open /proc/iomem");
    }

    char line[256];
    int found = 0;
    uint64_t addresses[3] = {0, 0, 0};

    while (fgets(line, sizeof(line), f)) {
        unsigned long long start, end;
        char rest[200] = {0};

        if (sscanf(line, "%llx-%llx : %199[^\n]", &start, &end, rest) >= 2) {
            if (strstr(rest, "gpio@d0000") || strstr(rest, "1f000d0000.gpio")) {
                if (found < 3) {
                    addresses[found] = start;
                    found++;
                }
            }
        }
    }
    fclose(f);

    if (found < 2) {
        fprintf(stderr, "ERROR: Cannot find RP1 GPIO registers in /proc/iomem\n");
        fprintf(stderr, "This tool only works on Raspberry Pi 5 with RP1.\n");
        return -1;
    }

    *gpio_phys = addresses[0];
    *rio_phys = addresses[1];
    *pads_phys = addresses[2];

    return 0;
}

int gpio_enable(uint8_t pin)
{
    if (gpio_base != NULL) {
        return 0;
    }

    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        err(EXIT_FAILURE, "/dev/mem (are you root?)");
    }

    uint64_t gpio_phys, rio_phys, pads_phys;
    if (find_rp1_gpio_phys(&gpio_phys, &rio_phys, &pads_phys) < 0) {
        close(mem_fd);
        mem_fd = -1;
        return -1;
    }

    fprintf(stderr, "RP1 GPIO at phys 0x%llx\n", (unsigned long long)gpio_phys);
    fprintf(stderr, "RP1 RIO  at phys 0x%llx\n", (unsigned long long)rio_phys);
    if (pads_phys)
        fprintf(stderr, "RP1 PADS at phys 0x%llx\n", (unsigned long long)pads_phys);

    gpio_base = (volatile uint8_t *)mmap(NULL, MAP_SIZE,
                                         PROT_READ | PROT_WRITE,
                                         MAP_SHARED,
                                         mem_fd,
                                         gpio_phys);
    if (gpio_base == MAP_FAILED) {
        err(EXIT_FAILURE, "mmap gpio");
    }

    rio_base = (volatile uint8_t *)mmap(NULL, MAP_SIZE,
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED,
                                        mem_fd,
                                        rio_phys);
    if (rio_base == MAP_FAILED) {
        err(EXIT_FAILURE, "mmap rio");
    }

    if (pads_phys) {
        pads_base = (volatile uint8_t *)mmap(NULL, MAP_SIZE,
                                            PROT_READ | PROT_WRITE,
                                            MAP_SHARED,
                                            mem_fd,
                                            pads_phys);
        if (pads_base == MAP_FAILED) {
            fprintf(stderr, "Warning: could not mmap PADS, continuing without pad configuration\n");
            pads_base = NULL;
        }
    }

    fprintf(stderr, "RP1 GPIO/RIO/PADS mapped successfully\n");

    return 0;
}

int gpio_direction(uint8_t pin, bool output)
{
    if (gpio_base == NULL || rio_base == NULL)
        return -1;

    rp1_gpio_write_ctrl(pin, RP1_GPIO_FUNCSEL_GPIO);

    if (pads_base)
        rp1_pads_configure(pin);

    rp1_rio_set_oe(pin, output);

    return 0;
}

int gpio_set_value(uint8_t pin, bool value)
{
    if (rio_base == NULL)
        return -1;

    rp1_rio_set_out(pin, value);
    return 0;
}

int gpio_set(uint8_t pin)
{
    return gpio_set_value(pin, true);
}

int gpio_clear(uint8_t pin)
{
    return gpio_set_value(pin, false);
}

bool gpio_get(uint8_t pin)
{
    usleep(1);

    if (rio_base == NULL)
        return false;

    return rp1_rio_get_in(pin);
}