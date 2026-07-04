// userspace_gpio_rcio_test.c
// Enhanced GPIO bit-banged RCIO SPI test for Navio2 on Pi 5
// Implements full RCIO protocol with configurable timing for CS/SCLK/MOSI/MISO

#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
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

#define RCIO_CS_PIN     16
#define RCIO_MISO_PIN   19
#define RCIO_MOSI_PIN   20
#define RCIO_SCLK_PIN   21

// RCIO Protocol Definitions
#define RCIO_PKT_MAX_REGS       32
#define RCIO_PKT_CODE_READ      0x00
#define RCIO_PKT_CODE_WRITE     0x40
#define RCIO_PKT_CODE_SUCCESS   0x00
#define RCIO_PKT_CODE_CORRUPT   0x40
#define RCIO_PKT_CODE_ERROR     0x80
#define RCIO_PKT_CODE_MASK      0xc0
#define RCIO_PKT_COUNT_MASK     0x3f

#define RCIO_PAGE_STATUS        1
#define RCIO_P_STATUS_FLAGS     2
#define RCIO_P_STATUS_BOARD_TYPE 10

#define RCIO_PAGE_SETUP         50
#define RCIO_P_SETUP_CRC        11

#define RCIO_PAGE_GIT_HASH      22

// Packet structure matching RCIO protocol
#pragma pack(push, 1)
struct rcio_packet {
    uint8_t count_code;
    uint8_t crc;
    uint8_t page;
    uint8_t offset;
    uint16_t regs[RCIO_PKT_MAX_REGS];
};
#pragma pack(pop)

// CRC8 table for RCIO protocol
static const uint8_t crc8_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x7A, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
    0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
    0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
    0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
    0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
    0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
    0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
    0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
    0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
    0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

// GPIO state
static volatile uint8_t *gpio_base;
static volatile uint8_t *rio_base;
static volatile uint8_t *pads_base;
static int mem_fd = -1;

// Timing configuration
static struct {
    unsigned cs_setup_us;       // Delay between CS low and first clock edge
    unsigned cs_hold_us;        // Delay between last clock edge and CS high
    unsigned half_period_us;    // Half clock period (determines SPI frequency)
    unsigned inter_byte_us;     // Delay between bytes within a transfer
    unsigned inter_transfer_us; // Delay between consecutive transfers
    unsigned initial_cs_low_us; // Delay after first CS low before any activity
    unsigned spi_mode;          // 0 or 3
} timing = {
    .cs_setup_us = 50,
    .cs_hold_us = 50,
    .half_period_us = 50,       // 10kHz (slower for reliability)
    .inter_byte_us = 10,
    .inter_transfer_us = 100,
    .initial_cs_low_us = 100,
    .spi_mode = 0
};

// Helper functions
static int bank_offset(int gpio) {
    if (gpio < 28) return RP1_GPIO_BANK0_OFFSET;
    if (gpio < 34) return RP1_GPIO_BANK1_OFFSET;
    return RP1_GPIO_BANK2_OFFSET;
}

static int pin_in_bank(int gpio) {
    if (gpio < 28) return gpio;
    if (gpio < 34) return gpio - 28;
    return gpio - 34;
}

static void set_func_gpio(int gpio) {
    int off = bank_offset(gpio);
    volatile uint32_t *ctrl = (volatile uint32_t *)(gpio_base + off + gpio * 8 + 4);
    uint32_t val = *ctrl;
    val &= ~(0x1fU << RP1_GPIO_CTRL_FUNCSEL_LSB);
    val |= (RP1_GPIO_FUNCSEL_GPIO << RP1_GPIO_CTRL_FUNCSEL_LSB);
    val &= ~(0x3U << RP1_GPIO_CTRL_OUTOVER_LSB);
    val &= ~(0x3U << RP1_GPIO_CTRL_OEOVER_LSB);
    *ctrl = val;
}

static void configure_pad(int gpio) {
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

static void set_oe(int gpio, bool enable) {
    int off = bank_offset(gpio);
    int pin = pin_in_bank(gpio);
    uint32_t mask = 1U << pin;
    volatile uint32_t *reg = (volatile uint32_t *)(rio_base + off + RP1_RIO_OE_OFFSET +
                                                    (enable ? RP1_RIO_SET_OFFSET : RP1_RIO_CLR_OFFSET));
    *reg = mask;
}

static void set_out(int gpio, bool high) {
    int off = bank_offset(gpio);
    int pin = pin_in_bank(gpio);
    uint32_t mask = 1U << pin;
    volatile uint32_t *reg = (volatile uint32_t *)(rio_base + off + RP1_RIO_OUT_OFFSET +
                                                    (high ? RP1_RIO_SET_OFFSET : RP1_RIO_CLR_OFFSET));
    *reg = mask;
}

static bool get_in(int gpio) {
    int off = bank_offset(gpio);
    int pin = pin_in_bank(gpio);
    volatile uint32_t *reg = (volatile uint32_t *)(rio_base + off + RP1_RIO_IN_OFFSET);
    return ((*reg >> pin) & 1U) != 0;
}

static int find_rp1_addresses(uint64_t *gpio_phys, uint64_t *rio_phys, uint64_t *pads_phys) {
    FILE *f = fopen("/proc/iomem", "r");
    if (!f) {
        perror("fopen /proc/iomem");
        return -1;
    }
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
    *pads_phys = (found > 2) ? addr[2] : 0;
    return 0;
}

static void gpio_init(void) {
    uint64_t gpio_phys, rio_phys, pads_phys;
    if (find_rp1_addresses(&gpio_phys, &rio_phys, &pads_phys) < 0) {
        fprintf(stderr, "Cannot find RP1 GPIO/RIO/PADS in /proc/iomem\n");
        exit(EXIT_FAILURE);
    }
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("open /dev/mem");
        exit(EXIT_FAILURE);
    }
    gpio_base = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, gpio_phys);
    rio_base   = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, rio_phys);
    pads_base  = pads_phys ? mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, pads_phys) : NULL;
    if (gpio_base == MAP_FAILED || rio_base == MAP_FAILED) {
        perror("mmap RP1 GPIO/RIO");
        exit(EXIT_FAILURE);
    }
    if (pads_base == MAP_FAILED) pads_base = NULL;
}

static inline void delay_us(unsigned us) {
    if (us > 0) usleep(us);
}

// CRC8 calculation
static uint8_t calc_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc = crc8_table[crc ^ data[i]];
    }
    return crc;
}

static uint8_t calc_packet_crc(const struct rcio_packet *pkt) {
    size_t len = 4 + (RCIO_PKT_COUNT_MASK & pkt->count_code) * 2;
    return calc_crc8((const uint8_t *)pkt, len);
}

// Low-level bit-banged SPI transfer
static uint8_t transfer_byte(uint8_t tx) {
    uint8_t rx = 0;
    bool cpol = (timing.spi_mode == 3);
    bool cpha = (timing.spi_mode == 3);
    unsigned hp = timing.half_period_us;

    for (int bit = 7; bit >= 0; bit--) {
        bool out = (tx >> bit) & 1U;
        if (!cpha) {
            set_out(RCIO_MOSI_PIN, out);
            delay_us(hp);
            set_out(RCIO_SCLK_PIN, !cpol);
            delay_us(hp);
            rx = (uint8_t)((rx << 1) | (get_in(RCIO_MISO_PIN) ? 1 : 0));
            set_out(RCIO_SCLK_PIN, cpol);
        } else {
            set_out(RCIO_SCLK_PIN, !cpol);
            set_out(RCIO_MOSI_PIN, out);
            delay_us(hp);
            set_out(RCIO_SCLK_PIN, cpol);
            delay_us(hp);
            rx = (uint8_t)((rx << 1) | (get_in(RCIO_MISO_PIN) ? 1 : 0));
        }
        if (timing.inter_byte_us > 0 && bit > 0) delay_us(timing.inter_byte_us);
    }
    return rx;
}

// Transfer a packet
static void transfer_packet(const uint8_t *tx, uint8_t *rx, size_t len) {
    bool cpol = (timing.spi_mode == 3);
    set_out(RCIO_SCLK_PIN, cpol);
    delay_us(timing.initial_cs_low_us);
    set_out(RCIO_CS_PIN, false);
    delay_us(timing.cs_setup_us);
    for (size_t i = 0; i < len; i++) {
        rx[i] = transfer_byte(tx[i]);
    }
    delay_us(timing.cs_hold_us);
    set_out(RCIO_CS_PIN, true);
    delay_us(timing.initial_cs_low_us);
}

// Pack uint16_t into bytes
static void pack_u16(uint8_t *buf, uint16_t val) {
    buf[0] = val & 0xff;
    buf[1] = (val >> 8) & 0xff;
}

// Unpack bytes into uint16_t
static uint16_t unpack_u16(const uint8_t *buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

// Build RCIO read request packet
static size_t build_read_request(struct rcio_packet *pkt, uint8_t page, uint8_t offset, uint8_t count) {
    memset(pkt, 0, sizeof(*pkt));
    pkt->count_code = count | RCIO_PKT_CODE_READ;
    pkt->page = page;
    pkt->offset = offset;
    pkt->crc = calc_packet_crc(pkt);
    return 4 + count * 2;
}

// Parse RCIO response
static int parse_response(const uint8_t *rx, size_t len, struct rcio_packet *pkt) {
    if (len < 4) return -1;
    memcpy(pkt, rx, len);
    uint8_t code = RCIO_PKT_CODE_MASK & pkt->count_code;
    if (code == RCIO_PKT_CODE_CORRUPT) {
        return -2; // Corrupt packet
    }
    if (code == RCIO_PKT_CODE_ERROR) {
        return -3; // Error reply
    }
    // Verify CRC (skip CRC byte itself)
    uint8_t saved_crc = pkt->crc;
    pkt->crc = 0;
    uint8_t expected_crc = calc_packet_crc(pkt);
    pkt->crc = saved_crc;
    if (saved_crc != expected_crc) {
        return -4; // CRC mismatch
    }
    return 0; // Success
}

// Read RCIO registers
static int rcio_read(uint8_t page, uint8_t offset, uint16_t *values, uint8_t count) {
    struct rcio_packet tx_pkt, rx_pkt;
    size_t tx_len = build_read_request(&tx_pkt, page, offset, count);
    size_t rx_len = 4 + count * 2;
    
    uint8_t tx_bytes[72];
    uint8_t rx_bytes[72];
    memset(tx_bytes, 0, sizeof(tx_bytes));
    memset(rx_bytes, 0, sizeof(rx_bytes));
    memcpy(tx_bytes, &tx_pkt, tx_len);
    
    // Send write request
    transfer_packet(tx_bytes, rx_bytes, tx_len);
    delay_us(timing.inter_transfer_us);
    
    // Read response (send dummy clock)
    memset(tx_bytes, 0xff, rx_len);
    transfer_packet(tx_bytes, rx_bytes, rx_len);
    
    int ret = parse_response(rx_bytes, rx_len, &rx_pkt);
    if (ret < 0) return ret;
    
    // Extract values
    uint8_t resp_count = RCIO_PKT_COUNT_MASK & rx_pkt.count_code;
    if (resp_count != count) return -5;
    for (uint8_t i = 0; i < count; i++) {
        values[i] = rx_pkt.regs[i];
    }
    return 0;
}

// Poll for response (multiple attempts)
static int rcio_poll(uint8_t page, uint8_t offset, uint16_t *values, uint8_t count, int max_attempts) {
    for (int attempt = 0; attempt < max_attempts; attempt++) {
        delay_us(1000); // 1ms between polls
        int ret = rcio_read(page, offset, values, count);
        if (ret == 0) return 0;
    }
    return -1;
}

// Print packet contents
static void print_packet(const char *label, const uint8_t *data, size_t len) {
    printf("%s", label);
    for (size_t i = 0; i < len && i < 20; i++) {
        printf(" %02x", data[i]);
    }
    if (len > 20) printf(" ...");
    printf("\n");
}

// Test timing configuration
static void test_with_timing(const char *name) {
    printf("\n### Testing: %s ###\n", name);
    printf("cs_setup=%u cs_hold=%u half_period=%u mode=%u inter_byte=%u inter_xfer=%u init_cs_low=%u\n",
           timing.cs_setup_us, timing.cs_hold_us, timing.half_period_us,
           timing.spi_mode, timing.inter_byte_us, timing.inter_transfer_us,
           timing.initial_cs_low_us);
    
    uint16_t values[8];
    
    // Test 1: Read status flags (page 1, offset 2, count 2)
    printf("\nTest 1: Read STATUS flags...\n");
    print_bytes("  TX:", (const uint8_t *)&((const struct rcio_packet){ .count_code = 2 | RCIO_PKT_CODE_READ, .page = RCIO_PAGE_STATUS, .offset = RCIO_P_STATUS_FLAGS }), 4);
    int ret = rcio_read(RCIO_PAGE_STATUS, RCIO_P_STATUS_FLAGS, values, 2);
    if (ret == 0) {
        printf("  SUCCESS: flags=0x%04x alarms=0x%04x\n", values[0], values[1]);
    } else {
        printf("  FAILED: ret=%d\n", ret);
    }
    
    // Test 2: Read board type (page 1, offset 10, count 1)
    printf("\nTest 2: Read BOARD TYPE...\n");
    ret = rcio_read(RCIO_PAGE_STATUS, RCIO_P_STATUS_BOARD_TYPE, values, 1);
    if (ret == 0) {
        printf("  SUCCESS: board_type=%u\n", values[0]);
    } else {
        printf("  FAILED: ret=%d\n", ret);
    }
    
    // Test 3: Read CRC (page 50, offset 11, count 2)
    printf("\nTest 3: Read CRC...\n");
    ret = rcio_read(RCIO_PAGE_SETUP, RCIO_P_SETUP_CRC, values, 2);
    if (ret == 0) {
        uint32_t crc = (values[1] << 16) | values[0];
        printf("  SUCCESS: crc=0x%08x\n", crc);
    } else {
        printf("  FAILED: ret=%d\n", ret);
    }
    
    // Test 4: Read git hash (page 22, offset 0, count 5)
    printf("\nTest 4: Read GIT HASH...\n");
    ret = rcio_read(RCIO_PAGE_GIT_HASH, 0, values, 5);
    if (ret == 0) {
        char hash[20] = {0};
        memcpy(hash, values, 10);
        printf("  SUCCESS: git_hash=%s\n", hash);
    } else {
        printf("  FAILED: ret=%d\n", ret);
    }
}

int main(int argc, char *argv[]) {
    printf("=== Userspace GPIO RCIO Test (Pi 5) ===\n");
    printf("Purpose: Implement RCIO protocol with full manual control over SPI timing\n\n");
    
    gpio_init();
    
    // Configure pins
    int pins[] = {RCIO_CS_PIN, RCIO_MISO_PIN, RCIO_MOSI_PIN, RCIO_SCLK_PIN};
    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        set_func_gpio(pins[i]);
        configure_pad(pins[i]);
    }
    
    // Set directions
    set_oe(RCIO_CS_PIN, true);
    set_oe(RCIO_MOSI_PIN, true);
    set_oe(RCIO_SCLK_PIN, true);
    set_oe(RCIO_MISO_PIN, false);
    
    // Initialize idle state
    set_out(RCIO_CS_PIN, true);    // CS high (inactive)
    set_out(RCIO_MOSI_PIN, false); // MOSI low
    set_out(RCIO_SCLK_PIN, false); // SCLK low (mode 0)
    
    printf("GPIO initialized.\n");
    printf("Pins: CS=GPIO%d MISO=GPIO%d MOSI=GPIO%d SCLK=GPIO%d\n\n",
           RCIO_CS_PIN, RCIO_MISO_PIN, RCIO_MOSI_PIN, RCIO_SCLK_PIN);
    
    // Check initial pin states
    printf("Initial states: MISO=%d CS=%d MOSI=%d SCLK=%d\n",
           get_in(RCIO_MISO_PIN), get_in(RCIO_CS_PIN),
           get_in(RCIO_MOSI_PIN), get_in(RCIO_SCLK_PIN));
    
    // Test with various timing configurations
    unsigned test_configs[][6] = {
        // cs_setup, cs_hold, half_period, inter_byte, inter_xfer, init_cs_low
        {100, 100, 100, 20, 500, 200},    // Very slow, generous delays
        {50, 50, 50, 10, 200, 100},       // Slow
        {20, 20, 20, 5, 100, 50},         // Medium
        {10, 10, 10, 2, 50, 25},          // Medium-fast
        {5, 5, 5, 1, 20, 10},             // Faster
        {2, 2, 2, 1, 10, 5},              // Fast
    };
    
    // Test both SPI MODE 0 and MODE 3
    for (int mode = 0; mode <= 3; mode += 3) { // Test modes 0 and 3
        timing.spi_mode = mode;
        printf("\n\n========================================");
        printf("\n=== SPI MODE %d ===", mode);
        printf("\n========================================\n");
        
        for (unsigned cfg = 0; cfg < sizeof(test_configs) / sizeof(test_configs[0]); cfg++) {
            timing.cs_setup_us = test_configs[cfg][0];
            timing.cs_hold_us = test_configs[cfg][1];
            timing.half_period_us = test_configs[cfg][2];
            timing.inter_byte_us = test_configs[cfg][3];
            timing.inter_transfer_us = test_configs[cfg][4];
            timing.initial_cs_low_us = test_configs[cfg][5];
            
            char name[64];
            snprintf(name, sizeof(name), "Mode%d_cfg%u", mode, cfg);
            test_with_timing(name);
        }
    }
    
    printf("\n\n=== Test Complete ===\n");
    printf("Final states: MISO=%d CS=%d MOSI=%d SCLK=%d\n",
           get_in(RCIO_MISO_PIN), get_in(RCIO_CS_PIN),
           get_in(RCIO_MOSI_PIN), get_in(RCIO_SCLK_PIN));
    
    return 0;
}
