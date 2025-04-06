/*
 * Copyright (c) 2025 Subbotin N.Y. <neymarus@yandex.ru>
 * GitHub: https://github.com/nktsb
 * SPDX-License-Identifier: Apache-2.0
 */


#define DT_DRV_COMPAT solomon_ssd1351

#include "ssd1351.h"

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>

#define LOG_LEVEL CONFIG_DISPLAY_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_ssd1351);

struct ssd1351_config {
	struct spi_dt_spec spi;
	struct gpio_dt_spec data_cmd;
	struct gpio_dt_spec reset;
	uint16_t width;
	uint16_t height;
	uint16_t rotation;
};

struct ssd1351_data {
	uint16_t xres;
	uint16_t yres;

	enum display_orientation orientation;
};

static int ssd1351_write_spi(const struct device *dev, uint8_t *buf, 
		size_t len, bool command)
{
	const struct ssd1351_config *config = dev->config;
	int ret;

	gpio_pin_set_dt(&config->data_cmd, command ? 0 : 1);
	struct spi_buf tx_buf = {.buf = buf, .len = len};

	struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};

	ret = spi_write_dt(&config->spi, &tx_bufs);

	return ret;
}

static int ssd1351_init_device(const struct device *dev)
{
	
}

static int ssd1351_init(const struct device *dev)
{
	const struct ssd1351_config *config = dev->config;
	struct ssd1351_data *data = dev->data;
	int ret;

	if (!gpio_is_ready_dt(&config->data_cmd))
	{
		LOG_ERR("D/C GPIO device not ready");
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&config->data_cmd, GPIO_OUTPUT_INACTIVE);
	if (ret < 0)
	{
		LOG_ERR("Couldn't configure D/C pin");
		return ret;
	}

	if (!spi_is_ready_dt(&config->spi))
	{
		LOG_ERR("SPI device not ready");
		return -ENODEV;
	}

	if (config->reset.port)
	{
		if (!gpio_is_ready_dt(&config->reset))
		{
			LOG_ERR("Reset GPIO device not ready");
			return -ENODEV;
		}
		ret = gpio_pin_configure_dt(&config->reset, GPIO_OUTPUT_INACTIVE);
		if (ret < 0)
		{
			LOG_ERR("Couldn't configure reset pin");
			return ret;
		}
	}

	if (config->rotation == 0)
	{
		data->xres = config->width;
		data->yres = config->height;
		data->orientation = DISPLAY_ORIENTATION_NORMAL;
	}
	else
	if (config->rotation == 90)
	{
		data->xres = config->height;
		data->yres = config->width;
		data->orientation = DISPLAY_ORIENTATION_ROTATED_90;
	}
	else
	if (config->rotation == 180)
	{
		data->xres = config->width;
		data->yres = config->height;
		data->orientation = DISPLAY_ORIENTATION_ROTATED_180;
	}
	else
	if (config->rotation == 270)
	{
		data->xres = config->height;
		data->yres = config->width;
		data->orientation = DISPLAY_ORIENTATION_ROTATED_270;
	}

	ssd1351_init_device(dev);
}

static int ssd1351_blanking_on(const struct device *dev)
{
	// TODO blanking on command
}

static int ssd1351_blanking_off(const struct device *dev)
{
	// TODO blanking off command
}

static int ssd1351_write(const struct device *dev, const uint16_t x, const uint16_t y,
		const struct display_buffer_descriptor *desc, const void *buf)
{
	const struct ssd1351_config *config = dev->config;
}

static int ssd1351_set_brightness(const struct device *dev, const uint8_t brightness)
{
	// TODO set brightness
}

static int ssd1351_set_contrast(const struct device *dev, const uint8_t contrast)
{
	// TODO set contrast
}

static void ssd1351_get_capabilities(const struct device *dev, 
		struct display_capabilities *capabilities)
{
	const struct ssd1351_config *config = dev->config;
	struct ssd1351_data *data = dev->data;

	memset(capabilities, 0, sizeof(struct display_capabilities));
	capabilities->x_resolution = config->width;
	capabilities->y_resolution = config->height;

	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
	capabilities->current_pixel_format = PIXEL_FORMAT_RGB_565;

	capabilities->current_orientation = data->orientation;
}

static int ssd1351_set_orientation(const struct device *dev,
			    const enum display_orientation orientation)
{
	const struct ssd1351_config *config = dev->config;
	struct ssd1351_data *data = dev->data;

	data->orientation = orientation;

	if (orientation == DISPLAY_ORIENTATION_NORMAL)
	{
		data->xres = config->width;
		data->yres = config->height;
	} 
	else
	if (orientation == DISPLAY_ORIENTATION_ROTATED_90)
	{
		data->xres = config->height;
		data->yres = config->width;
	}
	else
	if (orientation == DISPLAY_ORIENTATION_ROTATED_180)
	{
		data->xres = config->width;
		data->yres = config->height;
	}
	else
	if (orientation == DISPLAY_ORIENTATION_ROTATED_270)
	{
		data->xres = config->height;
		data->yres = config->width;
	}


	// TODO write orientation command
}

static const struct display_driver_api ssd1351_api = {
	.blanking_on = ssd1351_blanking_on,
	.blanking_off = ssd1351_blanking_off,
	.write = ssd1351_write,
	.set_brightness = ssd1351_set_brightness,
	.set_contrast = ssd1351_set_contrast,
	.get_capabilities = ssd1351_get_capabilities,
	.set_orientation = ssd1351_set_orientation,
};


#define SSD1351_INIT(inst)																\
	static struct ssd1351_data ssd1351_data_##inst;										\
	static const struct ssd1351_config ssd1351_config_##inst = {						\
		.spi = SPI_DT_SPEC_INST_GET(													\
				inst, SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8), 0),		\
		.data_cmd = GPIO_DT_SPEC_INST_GET_OR(inst, dc_gpios, {0}),						\
		.reset = GPIO_DT_SPEC_INST_GET_OR(inst, resets, {0}),							\
		.height = DT_INST_PROP(inst, height),											\
		.width = DT_INST_PROP(inst, width),												\
		.rotation = DT_INST_PROP(inst, rotation)										\
	};																					\
	DEVICE_DT_INST_DEFINE(inst, ssd1351_init, NULL, &ssd1351_data_##inst,				\
			&ssd1351_config_##inst, POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY,			\
			&ssd1351_api);

DT_INST_FOREACH_STATUS_OKAY(SSD1351_INIT)