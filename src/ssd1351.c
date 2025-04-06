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

	enum display_pixel_format pixel_format;
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
		data->xres = cfg->width;
		data->yres = cfg->height;
		data->orientation = DISPLAY_ORIENTATION_NORMAL;
	}
	else
	if (config->rotation == 90)
	{
		data->xres = cfg->height;
		data->yres = cfg->width;
		data->orientation = DISPLAY_ORIENTATION_ROTATED_90;
	}
	else
	if (config->rotation == 180)
	{
		data->xres = cfg->width;
		data->yres = cfg->height;
		data->orientation = DISPLAY_ORIENTATION_ROTATED_180;
	}
	else
	if (config->rotation == 270)
	{
		data->xres = cfg->height;
		data->yres = cfg->width;
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
	// switch(config->pixel_format)
	// {} TODO handle two rgb formats
}

static int ssd1351_set_brightness(const struct device *dev, const uint8_t brightness)
{
	// TODO set brightness
}

static int ssd1351_set_contrast(const struct device *dev, const uint8_t contrast)
{
	// TODO set contrast
}

static void ssd1351_get_capabilities(const struct device *dev, struct display_capabilities *caps)
{
	const struct ssd1351_config *config = dev->config;
	struct ssd1351_data *data = dev->data;

	memset(capabilities, 0, sizeof(struct display_capabilities));
	capabilities->x_resolution = config->width;
	capabilities->y_resolution = config->height;

	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565 | PIXEL_FORMAT_RGB_888;

	capabilities->current_pixel_format = data->pixel_format;
	capabilities->current_orientation = data->orientation;
}

static int ssd1351_set_pixel_format(const struct device *dev,
			     const enum display_pixel_format pixel_format)
{
	const struct ssd1351_config *config = dev->config;
	struct ssd1351_data *data = dev->data;
	config->pixel_format = pixel_format;

	return 0;
}

static int ssd1351_set_orientation(const struct device *dev,
			    const enum display_orientation orientation)
{
	struct ssd1351_data *data = dev->data;

	data->orientation = orientation;

	if (orientation == DISPLAY_ORIENTATION_NORMAL)
	{
		data->xres = cfg->width;
		data->yres = cfg->height;
	} 
	else
	if (orientation == DISPLAY_ORIENTATION_ROTATED_90)
	{
		data->xres = cfg->height;
		data->yres = cfg->width;
	}
	else
	if (orientation == DISPLAY_ORIENTATION_ROTATED_180)
	{
		data->xres = cfg->width;
		data->yres = cfg->height;
	}
	else
	if (orientation == DISPLAY_ORIENTATION_ROTATED_270)
	{
		data->xres = cfg->height;
		data->yres = cfg->width;
	}


	// TODO write orientation command
}

static DEVICE_API(display, ssd1351_api) = {
	.blanking_on = ssd1351_blanking_on,
	.blanking_off = ssd1351_blanking_off,
	.write = ssd1351_write,
	.set_brightness = ssd1351_set_brightness,
	.set_contrast = ssd1351_set_contrast,
	.get_capabilities = ssd1351_get_capabilities,
	.set_pixel_format = ssd1351_set_pixel_format,
	.set_orientation = ssd1351_set_orientation,
};


#define SSD1351_DEFINE(node_id)															\
	static struct ssd1351_data ssd1351_data_##inst = {									\
		.pixel_format = DT_INST_PROP(inst, pixel_format),								\
	};																					\
	static const struct ssd1351_config ssd1351_config_##node_id = {						\
		.spi = SPI_DT_SPEC_GET(															\
				node_id, SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8), 0)	\
		.data_cmd = GPIO_DT_SPEC_GET_OR(node_id, dc_gpios, {0}),						\
		.reset = GPIO_DT_SPEC_GET_OR(node_id, resets, {0}),								\
		.height = DT_PROP(node_id, height),												\
		.width = DT_PROP(node_id, width),												\
		.rotation = DT_PROP(node_id, rotation)											\
	};																					\											\
	DEVICE_DT_DEFINE(node_id, ssd1351_init, NULL, &ssd1351_data_##inst,					\
			&ssd1351_config_##node_id, POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY,		\
			&ssd1351_api);

DT_INST_FOREACH_STATUS_OKAY(SSD1351_DEFINE)