#include <unistd.h>
#include <errno.h>
#include <err.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <gpiod.h>

#include "gpio.h"

#define LOW                 0
#define HIGH                1

#define GPIO_CHIP_PATH      "/dev/gpiochip0"

using namespace Navio;

Pin::Pin(uint8_t pin):
    _pin(pin),
    _chip(NULL),
    _line(NULL),
    _mode(GpioModeInput),
    _initialized(false)
{
}

Pin::~Pin()
{
    _release_line();
    if (_chip) {
        gpiod_chip_close(_chip);
    }
}

void Pin::_release_line()
{
    if (_line) {
        gpiod_line_release(_line);
    }
}

bool Pin::_request_line(GpioMode mode)
{
    int ret;

    if (mode == GpioModeInput) {
        ret = gpiod_line_request_input(_line, "navio");
    } else {
        ret = gpiod_line_request_output(_line, "navio", 0);
    }

    if (ret < 0) {
        warnx("failed to request GPIO %u: %s", _pin, strerror(errno));
        return false;
    }

    _mode = mode;
    return true;
}

bool Pin::init()
{
    _chip = gpiod_chip_open(GPIO_CHIP_PATH);
    if (!_chip) {
        warnx("cannot open %s: %s", GPIO_CHIP_PATH, strerror(errno));
        return false;
    }

    _line = gpiod_chip_get_line(_chip, _pin);
    if (!_line) {
        warnx("cannot get GPIO line %u: %s", _pin, strerror(errno));
        gpiod_chip_close(_chip);
        _chip = NULL;
        return false;
    }

    if (!_request_line(_mode)) {
        gpiod_chip_close(_chip);
        _chip = NULL;
        _line = NULL;
        return false;
    }

    _initialized = true;
    return true;
}

void Pin::setMode(GpioMode mode)
{
    _release_line();
    _request_line(mode);
}

uint8_t Pin::read() const
{
    if (!_line) {
        warnx("GPIO line not initialized");
        return 0;
    }

    int value = gpiod_line_get_value(_line);
    if (value < 0) {
        warnx("failed to read GPIO %u: %s", _pin, strerror(errno));
        return 0;
    }

    return value ? 1 : 0;
}

void Pin::write(uint8_t value)
{
    if (!_line) {
        warnx("GPIO line not initialized");
        return;
    }

    if (_mode != GpioModeOutput) {
        warnx("GPIO %u: write has no effect because mode is not output", _pin);
        return;
    }

    int ret = gpiod_line_set_value(_line, value ? 1 : 0);
    if (ret < 0) {
        warnx("failed to write GPIO %u: %s", _pin, strerror(errno));
    }
}

void Pin::toggle()
{
    write(!read());
}
