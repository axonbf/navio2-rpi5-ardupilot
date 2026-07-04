#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <err.h>
#include <errno.h>

#define MAP_SIZE            (64 * 1024)

#define RP1_GPIO_BANK0_OFFSET   0x0000
#define RP1_GPIO_BANK1_OFFSET   0x4000
#define RP1_GPIO_BANK2_OFFSET   0x8000

#define RP1_GPIO_CTRL_FUNCSEL_LSB   0
#define RP1_GPIO_FUNCSEL_GPIO       5
#define RP1_GPIO_CTRL_OEOVER_LSB    14
#define RP1_GPIO_CTRL_OUTOVER_LSB   12

#define RP1_RIO_OUT_OFFSET      0x0000
#define RP1_RIO_OE_OFFSET       0x0004
#define RP1_RIO_IN_OFFSET       0x0008
#define RP1_RIO_SET_OFFSET      0x2000
#define RP1_RIO_CLR_OFFSET      0x3000

#define RP1_PADS_BANK0_OFFSET  0x0004
#define RP1_PADS_BANK1_OFFSET  0x4004
#define RP1_PADS_BANK2_OFFSET   0x8004

#define RP1_PADS_PULLDOWN_LSB   2
#define RP1_PADS_PULLUP_LSB     3
#define RP1_PADS_SCHMITT_LSB    1

#define GPIO_SWDCLK  12
#define GPIO_SWDIO   13

static volatile uint8_t *gpio_base = NULL;
static volatile uint8_t *rio_base = NULL;
static volatile uint8_t *pads_base = NULL;
static int mem_fd = -1;

static int rp1_gpio_bank_offset(int gpio) {
    if (gpio < 28) return RP1_GPIO_BANK0_OFFSET;
    if (gpio < 34) return RP1_GPIO_BANK1_OFFSET;
    return RP1_GPIO_BANK2_OFFSET;
}

static int rp1_pin_in_bank(int gpio) {
    if (gpio < 28) return gpio;
    if (gpio < 34) return gpio - 28;
    return gpio - 34;
}

static void rp1_gpio_set_func(int gpio, uint32_t funcsel) {
    int bank_off = rp1_gpio_bank_offset(gpio);
    volatile uint32_t *ctrl = (volatile uint32_t *)(gpio_base + bank_off + gpio * 8 + 4);
    uint32_t val = *ctrl;
    val &= ~(0x1FU << RP1_GPIO_CTRL_FUNCSEL_LSB);
    val |= (funcsel << RP1_GPIO_CTRL_FUNCSEL_LSB);
    val &= ~(0x3U << RP1_GPIO_CTRL_OUTOVER_LSB);
    val &= ~(0x3U << RP1_GPIO_CTRL_OEOVER_LSB);
    *ctrl = val;
}

static void rp1_set_oe(int gpio, bool enable) {
    int bank_off = rp1_gpio_bank_offset(gpio);
    int pin_num = rp1_pin_in_bank(gpio);
    uint32_t mask = 1U << pin_num;
    volatile uint32_t *oe_reg = (volatile uint32_t *)(rio_base + bank_off + RP1_RIO_OE_OFFSET +
                                   (enable ? RP1_RIO_SET_OFFSET : RP1_RIO_CLR_OFFSET));
    *oe_reg = mask;
}

static void rp1_set_out(int gpio, bool high) {
    int bank_off = rp1_gpio_bank_offset(gpio);
    int pin_num = rp1_pin_in_bank(gpio);
    uint32_t mask = 1U << pin_num;
    volatile uint32_t *out_reg = (volatile uint32_t *)(rio_base + bank_off + RP1_RIO_OUT_OFFSET +
                                    (high ? RP1_RIO_SET_OFFSET : RP1_RIO_CLR_OFFSET));
    *out_reg = mask;
}

static bool rp1_get_in(int gpio) {
    int bank_off = rp1_gpio_bank_offset(gpio);
    int pin_num = rp1_pin_in_bank(gpio);
    volatile uint32_t *in_reg = (volatile uint32_t *)(rio_base + bank_off + RP1_RIO_IN_OFFSET);
    uint32_t val = *in_reg;
    return (val >> pin_num) & 1 ? true : false;
}

static void rp1_configure_pad(int gpio) {
    int bank_off;
    if (gpio < 28) bank_off = RP1_PADS_BANK0_OFFSET;
    else if (gpio < 34) bank_off = RP1_PADS_BANK1_OFFSET;
    else bank_off = RP1_PADS_BANK2_OFFSET;

    volatile uint32_t *pad = (volatile uint32_t *)(pads_base + bank_off + gpio * 4);
    uint32_t val = *pad;
    val &= ~(1U << RP1_PADS_PULLDOWN_LSB);
    val |= (1U << RP1_PADS_PULLUP_LSB);
    val |= (1U << RP1_PADS_SCHMITT_LSB);
    *pad = val;
}

static int find_rp1_addresses(uint64_t *gpio_phys, uint64_t *rio_phys, uint64_t *pads_phys) {
    FILE *f = fopen("/proc/iomem", "r");
    if (!f) err(EXIT_FAILURE, "Cannot open /proc/iomem");

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
        fprintf(stderr, "Cannot find RP1 GPIO in /proc/iomem\n");
        return -1;
    }

    *gpio_phys = addresses[0];
    *rio_phys = addresses[1];
    *pads_phys = addresses[2];
    return 0;
}

static int gpio_init(void) {
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) err(EXIT_FAILURE, "/dev/mem (are you root?)");

    uint64_t gpio_phys, rio_phys, pads_phys;
    if (find_rp1_addresses(&gpio_phys, &rio_phys, &pads_phys) < 0) {
        close(mem_fd);
        return -1;
    }

    printf("RP1 GPIO phys: 0x%llx\n", (unsigned long long)gpio_phys);
    printf("RP1 RIO  phys: 0x%llx\n", (unsigned long long)rio_phys);
    printf("RP1 PADS phys: 0x%llx\n", (unsigned long long)pads_phys);

    gpio_base = (volatile uint8_t *)mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE,
                                          MAP_SHARED, mem_fd, gpio_phys);
    if (gpio_base == MAP_FAILED) err(EXIT_FAILURE, "mmap gpio");

    rio_base = (volatile uint8_t *)mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE,
                                          MAP_SHARED, mem_fd, rio_phys);
    if (rio_base == MAP_FAILED) err(EXIT_FAILURE, "mmap rio");

    if (pads_phys) {
        pads_base = (volatile uint8_t *)mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE,
                                              MAP_SHARED, mem_fd, pads_phys);
        if (pads_base == MAP_FAILED) {
            fprintf(stderr, "Warning: could not mmap PADS\n");
            pads_base = NULL;
        }
    }

    printf("RP1 GPIO/RIO/PADS mapped OK\n");
    return 0;
}

/* SWD bit-level operations with delays for slow clock */

static void swd_write_bit(uint8_t bit) {
    rp1_set_oe(GPIO_SWDIO, true);
    rp1_set_out(GPIO_SWDIO, bit ? true : false);
    usleep(10);
    rp1_set_out(GPIO_SWDCLK, true);
    usleep(10);
    rp1_set_out(GPIO_SWDCLK, false);
    usleep(10);
}

static uint8_t swd_read_bit(void) {
    rp1_set_oe(GPIO_SWDIO, false);
    usleep(10);
    bool val = rp1_get_in(GPIO_SWDIO);
    rp1_set_out(GPIO_SWDCLK, true);
    usleep(10);
    rp1_set_out(GPIO_SWDCLK, false);
    usleep(10);
    return val ? 1 : 0;
}

static void swd_turnaround(bool to_target) {
    static bool old_to_target = false;
    if (to_target == old_to_target) return;
    old_to_target = to_target;

    if (to_target) {
        rp1_set_oe(GPIO_SWDIO, false);
    }
    rp1_set_out(GPIO_SWDCLK, true);
    usleep(10);
    rp1_set_out(GPIO_SWDCLK, false);
    usleep(10);
    if (!to_target) {
        rp1_set_oe(GPIO_SWDIO, true);
    }
}

static void swd_line_reset(void) {
    swd_turnaround(false);
    for (int i = 0; i < 50; i++) {
        swd_write_bit(1);
    }
}

static void swd_idle_cycles(int count) {
    for (int i = 0; i < count; i++) {
        swd_write_bit(0);
    }
}

static uint8_t swd_read_ack(void) {
    uint8_t ack = 0;
    swd_turnaround(true);
    ack |= swd_read_bit();
    ack |= swd_read_bit() << 1;
    ack |= swd_read_bit() << 2;
    return ack;
}

static uint32_t swd_read_data(uint8_t *parity) {
    uint32_t data = 0;
    *parity = 0;
    for (int i = 0; i < 32; i++) {
        uint8_t bit = swd_read_bit();
        data |= (uint32_t)(bit & 1) << i;
        *parity ^= bit;
    }
    *parity ^= swd_read_bit();
    return data;
}

static void swd_write_data(uint32_t data) {
    uint8_t parity = 0;
    for (int i = 0; i < 32; i++) {
        uint8_t bit = (data >> i) & 1;
        swd_write_bit(bit);
        parity ^= bit;
    }
    swd_write_bit(parity & 1);
}

/* Read IDCODE: APnDP=0, RnW=1, ADDR=0 */
static uint32_t swd_read_idcode(uint8_t *out_ack) {
    swd_turnaround(false);

    /* Start bit */
    swd_write_bit(1);
    /* APnDP=0 */
    swd_write_bit(0);
    /* RnW=1 (read) */
    swd_write_bit(1);
    /* ADDR[2:3]=0 */
    swd_write_bit(0);
    /* Parity: start(1)+AP(0)+RnW(1)+addr(0)=0, even parity -> 0 */
    swd_write_bit(0);
    /* Stop bit */
    swd_write_bit(0);
    /* Park bit */
    swd_write_bit(1);

    uint8_t ack = swd_read_ack();
    if (out_ack) *out_ack = ack;

    uint8_t data_parity;
    uint32_t data = swd_read_data(&data_parity);

    swd_turnaround(false);
    swd_idle_cycles(8);
    return data;
}

/* Read DP register: APnDP=0, RnW=1 */
static uint32_t swd_read_dp(uint8_t addr, uint8_t *out_ack) {
    swd_turnaround(false);

    swd_write_bit(1);
    swd_write_bit(0);
    swd_write_bit(1);
    swd_write_bit(addr & 1);
    uint8_t parity = (0 + 0 + 1 + (addr & 1)) & 1;
    swd_write_bit(parity);
    swd_write_bit(0);
    swd_write_bit(1);

    uint8_t ack = swd_read_ack();
    if (out_ack) *out_ack = ack;

    uint8_t data_parity;
    uint32_t data = swd_read_data(&data_parity);

    swd_turnaround(false);
    swd_idle_cycles(8);
    return data;
}

/* Write DP register: APnDP=0, RnW=0 */
static uint8_t swd_write_dp(uint8_t addr, uint32_t data) {
    swd_turnaround(false);

    swd_write_bit(1);
    swd_write_bit(0);
    swd_write_bit(0);
    swd_write_bit(addr & 1);
    uint8_t parity = (0 + 0 + 0 + (addr & 1)) & 1;
    swd_write_bit(parity);
    swd_write_bit(0);
    swd_write_bit(1);

    uint8_t ack = swd_read_ack();

    swd_turnaround(false);
    swd_write_data(data);

    swd_idle_cycles(8);
    return ack;
}

int main(void) {
    printf("=== Navio2 STM32F103 SWD Test v2 (with delays) ===\n");

    if (gpio_init() < 0) {
        fprintf(stderr, "Failed to init GPIO\n");
        return 1;
    }

    printf("Configuring GPIO %d (SWDIO) and GPIO %d (SWDCLK)\n", GPIO_SWDIO, GPIO_SWDCLK);

    rp1_gpio_set_func(GPIO_SWDIO, RP1_GPIO_FUNCSEL_GPIO);
    rp1_gpio_set_func(GPIO_SWDCLK, RP1_GPIO_FUNCSEL_GPIO);

    if (pads_base) {
        rp1_configure_pad(GPIO_SWDIO);
        rp1_configure_pad(GPIO_SWDCLK);
    }

    /* Start with both as input to read initial state */
    rp1_set_oe(GPIO_SWDCLK, false);
    rp1_set_oe(GPIO_SWDIO, false);

    printf("Initial pin states (inputs): SWDIO=%d, SWDCLK=%d\n",
           rp1_get_in(GPIO_SWDIO), rp1_get_in(GPIO_SWDCLK));

    /* Test: toggle SWDCLK and read SWDIO to verify GPIO works */
    printf("\n--- GPIO Toggle Test ---\n");
    rp1_set_oe(GPIO_SWDCLK, true);
    rp1_set_out(GPIO_SWDCLK, false);
    usleep(1000);
    printf("SWDCLK=LOW, SWDIO (input)=%d\n", rp1_get_in(GPIO_SWDIO));
    rp1_set_out(GPIO_SWDCLK, true);
    usleep(1000);
    printf("SWDCLK=HIGH, SWDIO (input)=%d\n", rp1_get_in(GPIO_SWDIO));
    rp1_set_out(GPIO_SWDCLK, false);
    rp1_set_oe(GPIO_SWDCLK, false);

    /* Also test driving SWDIO and reading it back */
    rp1_set_oe(GPIO_SWDIO, true);
    rp1_set_out(GPIO_SWDIO, false);
    usleep(1000);
    printf("SWDIO driven LOW, readback=%d\n", rp1_get_in(GPIO_SWDIO));
    rp1_set_out(GPIO_SWDIO, true);
    usleep(1000);
    printf("SWDIO driven HIGH, readback=%d\n", rp1_get_in(GPIO_SWDIO));

    /* Release SWDIO back to input */
    rp1_set_oe(GPIO_SWDIO, false);
    rp1_set_out(GPIO_SWDIO, false);
    rp1_set_oe(GPIO_SWDCLK, false);
    rp1_set_out(GPIO_SWDCLK, false);

    printf("\n--- SWD Protocol Test (slow clock, 10us delays) ---\n");

    printf("Step 1: Line reset (50x SWDIO=1, SWDCLK toggle)...\n");
    swd_line_reset();

    printf("Step 2: JTAG-to-SWD sequence (0xE79E)...\n");
    swd_turnaround(false);
    uint16_t jtag2swd = 0xE79E;
    for (int i = 0; i < 16; i++) {
        swd_write_bit((jtag2swd >> i) & 1);
    }

    printf("Step 3: Line reset again...\n");
    swd_line_reset();

    printf("Step 4: Idle cycles (16 zeros)...\n");
    swd_idle_cycles(16);

    printf("\nStep 5: Read IDCODE (DP, addr=0, RnW=1)...\n");
    uint8_t ack;
    uint32_t idcode = swd_read_idcode(&ack);
    printf("IDCODE: 0x%08X  ACK: %d\n", idcode, ack);

    if (idcode == 0xFFFFFFFF) {
        printf("RESULT: No target (all 1s) - check wiring\n");
    } else if (idcode == 0x00000000) {
        printf("RESULT: No target (all 0s) - SWDIO stuck low\n");
    } else if ((idcode & 0xFFF) == 0x147) {
        printf("RESULT: STM32F1 Cortex-M3 detected!\n");
    } else {
        printf("RESULT: Unknown target\n");
    }

    printf("\nStep 6: Read DP CTRL/STAT (addr=0x01)...\n");
    uint32_t ctrl = swd_read_dp(0x01, &ack);
    printf("CTRL/STAT: 0x%08X  ACK: %d\n", ctrl, ack);

    printf("\nStep 7: Write DP ABORT (0x1E)...\n");
    ack = swd_write_dp(0x00, 0x0000001E);
    printf("ABORT write ACK: %d\n", ack);

    printf("\nStep 8: Re-read IDCODE...\n");
    uint32_t idcode2 = swd_read_idcode(&ack);
    printf("IDCODE (2nd): 0x%08X  ACK: %d\n", idcode2, ack);

    printf("\nFinal pin states: SWDIO=%d, SWDCLK=%d\n",
           rp1_get_in(GPIO_SWDIO), rp1_get_in(GPIO_SWDCLK));

    printf("\n=== Test complete ===\n");
    return 0;
}
