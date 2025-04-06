/*
 * Copyright (c) 2025 Subbotin N.Y. <neymarus@yandex.ru>
 * GitHub: https://github.com/nktsb
 * SPDX-License-Identifier: Apache-2.0
 */


#define DT_DRV_COMPAT sitronix_st7789v

#include "ssd1351.h"

#include <zephyr/device.h>
#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/display.h>

#define LOG_LEVEL CONFIG_DISPLAY_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_st7789v);

struct ssd1351_config {
	struct spi_dt_spec spi;
	struct gpio_dt_spec data_cmd_gpio;
	struct gpio_dt_spec reset_gpio;
	uint16_t width;
	uint16_t height;
};


static DEVICE_API(display, ssd1351_api) = {
	.blanking_on = ssd1351_blanking_on,
	.blanking_off = ssd1351_blanking_off,
	.write = ssd1351_write,
	.get_capabilities = ssd1351_get_capabilities,
	.set_pixel_format = ssd1351_set_pixel_format,
	.set_orientation = ssd1351_set_orientation,
};
