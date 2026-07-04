#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAP_SIZE (64 * 1024)

#define RP1_GPIO_BANK0_OFFSET 0x0000
#define RP1_GPIO_BANK1_OFFSET 0x4000
#define RP1_GPIO_BANK2_OFFSET 0x8000

#define RP1_GPIO_CTRL_FUNCSEL_LSB 0
#define RP1_GPIO_CTRL_OUTOVER_LSB 12
#define RP1_GPIO_CTRL_OEOVER_LSB 14
#define RP1_GPIO_FUNCSEL_GPIO 5

#define RP1_RIO_OUT_OFFSET 0x0000
#define RP1_RIO_OE_OFFSET 0x0004
#define RP1_RIO_IN_OFFSET 0x0008
#define RP1_RIO_SET_OFFSET 0x2000
#define RP1_RIO_CLR_OFFSET 0x3000

#define RP1_PADS_BANK0_OFFSET 0x0004
#define RP1_PADS_BANK1_OFFSET 0x4004
#define RP1_PADS_BANK2_OFFSET 0x8004
#define RP1_PADS_PULLDOWN_LSB 2
#define RP1_PADS_PULLUP_LSB 3
#define RP1_PADS_SCHMITT_LSB 1

#define RCIO_CS 16
#define RCIO_MISO 19
#define RCIO_MOSI 20
#define RCIO_SCLK 21

static volatile uint8_t *gpio_base;
static volatile uint8_t *rio_base;
static volatile uint8_t *pads_base;
static int mem_fd = -1;

static int bank_offset(int gpio)
{
    if (gpio < 28) return RP1_GPIO_BANK0_OFFSET;
    if (gpio < 34) return RP1_GPIO_BANK1_OFFSET;
    return RP1_GPIO_BANK2_OFFSET;
}

static int pin_in_bank(int gpio)
{
    if (gpio < 28) return gpio;
    if (gpio < 34) return gpio - 28;
    return gpio - 34;
}

static void set_func_gpio(int gpio)
{
    int off = bank_offset(gpio);
    volatile uint32_t *ctrl = (volatile uint32_t *)(gpio_base + off + gpio * 8 + 4);
    uint32_t val = *ctrl;
    val &= ~(0x1fU << RP1_GPIO_CTRL_FUNCSEL_LSB);
    val |= (RP1_GPIO_FUNCSEL_GPIO << RP1_GPIO_CTRL_FUNCSEL_LSB);
    val &= ~(0x3U << RP1_GPIO_CTRL_OUTOVER_LSB);
    val &= ~(0x3U << RP1_GPIO_CTRL_OEOVER_LSB);
    *ctrl = val;
}

static void configure_pad_pullup(int gpio)
{
    if (!pads_base) return;

    int off;
    if (gpio < 28) off = RP1_PADS_BANK0_OFFSET;
    else if (gpio < 34) off = RP1_PADS_BANK1_OFFSET;
    else off = RP1_PADS_BANK2_OFFSET;

    volatile uint32_t *pad = (volatile uint32_t *)(pads_base + off + gpio * 4);
    uint32_t val = *pad;
    val &= ~(1U << RP1_PADS_PULLDOWN_LSB);
    val |= (1U << RP1_PADS_PULLUP_LSB);
    val |= (1U << RP1_PADS_SCHMITT_LSB);
    *pad = val;
}

static void set_oe(int gpio, bool enable)
{
    int off = bank_offset(gpio);
    int pin = pin_in_bank(gpio);
    uint32_t mask = 1U << pin;
    volatile uint32_t *reg = (volatile uint32_t *)(rio_base + off + RP1_RIO_OE_OFFSET +
                                                   (enable ? RP1_RIO_SET_OFFSET : RP1_RIO_CLR_OFFSET));
    *reg = mask;
}

static void set_out(int gpio, bool high)
{
    int off = bank_offset(gpio);
    int pin = pin_in_bank(gpio);
    uint32_t mask = 1U << pin;
    volatile uint32_t *reg = (volatile uint32_t *)(rio_base + off + RP1_RIO_OUT_OFFSET +
                                                   (high ? RP1_RIO_SET_OFFSET : RP1_RIO_CLR_OFFSET));
    *reg = mask;
}

static bool get_in(int gpio)
{
    int off = bank_offset(gpio);
    int pin = pin_in_bank(gpio);
    volatile uint32_t *reg = (volatile uint32_t *)(rio_base + off + RP1_RIO_IN_OFFSET);
    return ((*reg >> pin) & 1U) != 0;
}

static int find_rp1_addresses(uint64_t *gpio_phys, uint64_t *rio_phys, uint64_t *pads_phys)
{
    FILE *f = fopen("/proc/iomem", "r");
    if (!f) err(EXIT_FAILURE, "open /proc/iomem");

    char line[256];
    uint64_t addr[3] = {0, 0, 0};
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        unsigned long long start, end;
        char rest[200] = {0};
        if (sscanf(line, "%llx-%llx : %199[^\n]", &start, &end, rest) >= 2) {
            if (strstr(rest, "gpio@d0000") || strstr(rest, "1f000d0000.gpio")) {
                if (found < 3) addr[found++] = start;
            }
        }
    }
    fclose(f);

    if (found < 2) return -1;
    *gpio_phys = addr[0];
    *rio_phys = addr[1];
    *pads_phys = addr[2];
    return 0;
}

static void gpio_init(void)
{
    uint64_t gpio_phys, rio_phys, pads_phys;
    if (find_rp1_addresses(&gpio_phys, &rio_phys, &pads_phys) < 0)
        errx(EXIT_FAILURE, "cannot find RP1 GPIO/RIO/PADS in /proc/iomem");

    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) err(EXIT_FAILURE, "open /dev/mem");

    gpio_base = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, gpio_phys);
    rio_base = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, rio_phys);
    pads_base = pads_phys ? mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, pads_phys) : NULL;
    if (gpio_base == MAP_FAILED || rio_base == MAP_FAILED) err(EXIT_FAILURE, "mmap RP1 GPIO/RIO");
    if (pads_base == MAP_FAILED) pads_base = NULL;
}

static void delay_us(unsigned us)
{
    if (us) usleep(us);
}

static uint8_t transfer_byte(uint8_t tx, int mode, unsigned half_period_us)
{
    uint8_t rx = 0;
    bool cpol = (mode == 3);
    bool cpha = (mode == 3);

    for (int bit = 7; bit >= 0; bit--) {
        bool out = (tx >> bit) & 1U;

        if (!cpha) {
            set_out(RCIO_MOSI, out);
            delay_us(half_period_us);
            set_out(RCIO_SCLK, !cpol);
            delay_us(half_period_us);
            rx = (uint8_t)((rx << 1) | (get_in(RCIO_MISO) ? 1 : 0));
            set_out(RCIO_SCLK, cpol);
        } else {
            set_out(RCIO_SCLK, !cpol);
            set_out(RCIO_MOSI, out);
            delay_us(half_period_us);
            set_out(RCIO_SCLK, cpol);
            delay_us(half_period_us);
            rx = (uint8_t)((rx << 1) | (get_in(RCIO_MISO) ? 1 : 0));
        }
        delay_us(half_period_us);
    }
    return rx;
}

static void spi_xfer(const uint8_t *tx, uint8_t *rx, size_t len, int mode, unsigned half_period_us, unsigned cs_setup_us)
{
    bool cpol = (mode == 3);
    set_out(RCIO_SCLK, cpol);
    set_out(RCIO_CS, false);
    delay_us(cs_setup_us);
    for (size_t i = 0; i < len; i++) rx[i] = transfer_byte(tx[i], mode, half_period_us);
    delay_us(cs_setup_us);
    set_out(RCIO_CS, true);
    delay_us(cs_setup_us);
}

static void print_bytes(const char *label, const uint8_t *buf, size_t len)
{
    printf("%s", label);
    for (size_t i = 0; i < len; i++) printf(" %02x", buf[i]);
    printf("\n");
}

int main(void)
{
    gpio_init();

    int pins[] = {RCIO_CS, RCIO_MISO, RCIO_MOSI, RCIO_SCLK};
    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        set_func_gpio(pins[i]);
        configure_pad_pullup(pins[i]);
    }

    set_oe(RCIO_CS, true);
    set_oe(RCIO_MOSI, true);
    set_oe(RCIO_SCLK, true);
    set_oe(RCIO_MISO, false);
    set_out(RCIO_CS, true);
    set_out(RCIO_MOSI, false);
    set_out(RCIO_SCLK, false);
    usleep(1000);

    printf("=== GPIO Bit-Banged RCIO SPI Test ===\n");
    printf("pins: CS=GPIO%d MISO=GPIO%d MOSI=GPIO%d SCLK=GPIO%d\n", RCIO_CS, RCIO_MISO, RCIO_MOSI, RCIO_SCLK);
    printf("initial MISO=%d CS=%d MOSI=%d SCLK=%d\n", get_in(RCIO_MISO), get_in(RCIO_CS), get_in(RCIO_MOSI), get_in(RCIO_SCLK));

    uint8_t zeros[68] = {0};
    uint8_t ffs[68];
    uint8_t alt[68];
    uint8_t read_pkt[68] = {0x01, 0x81, 0x14, 0x00};
    uint8_t rx[68];
    memset(ffs, 0xff, sizeof(ffs));
    for (size_t i = 0; i < sizeof(alt); i++) alt[i] = (i & 1) ? 0x55 : 0xaa;

    unsigned half_periods[] = {500, 100, 50, 10, 5, 1};
    unsigned setups[] = {1000, 100, 10};
    for (int mode_i = 0; mode_i < 2; mode_i++) {
        int mode = mode_i ? 3 : 0;
        for (unsigned h = 0; h < sizeof(half_periods) / sizeof(half_periods[0]); h++) {
            for (unsigned s = 0; s < sizeof(setups) / sizeof(setups[0]); s++) {
                printf("\nmode=%d half_period_us=%u cs_setup_us=%u\n", mode, half_periods[h], setups[s]);
                spi_xfer(zeros, rx, 32, mode, half_periods[h], setups[s]);
                print_bytes("zeros32:", rx, 16);
                spi_xfer(ffs, rx, 32, mode, half_periods[h], setups[s]);
                print_bytes("ff32:   ", rx, 16);
                spi_xfer(alt, rx, 32, mode, half_periods[h], setups[s]);
                print_bytes("alt32:  ", rx, 16);
                spi_xfer(read_pkt, rx, 68, mode, half_periods[h], setups[s]);
                print_bytes("readreq:", rx, 16);
                usleep(1000);
                spi_xfer(ffs, rx, 68, mode, half_periods[h], setups[s]);
                print_bytes("clock:  ", rx, 16);
            }
        }
    }

    printf("\nfinal MISO=%d CS=%d MOSI=%d SCLK=%d\n", get_in(RCIO_MISO), get_in(RCIO_CS), get_in(RCIO_MOSI), get_in(RCIO_SCLK));
    return 0;
}
