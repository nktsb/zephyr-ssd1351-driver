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

static int ssd1351_spi_write_data(const struct device *dev, uint8_t *buf, 
		const size_t len, bool command)
{
	const struct ssd1351_config *config = dev->config;
	int ret;

	gpio_pin_set_dt(&config->data_cmd, command ? 0 : 1);
	struct spi_buf tx_buf = {.buf = buf, .len = len};

	struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};

	ret = spi_write_dt(&config->spi, &tx_bufs);

	return ret;
}

static int ssd1351_spi_write_byte(const struct device *dev, uint8_t byte, bool command)
{
	const struct ssd1351_config *config = dev->config;
	int ret;

	gpio_pin_set_dt(&config->data_cmd, command ? 0 : 1);
	struct spi_buf tx_buf = {.buf = &byte, .len = 1};

	struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};

	ret = spi_write_dt(&config->spi, &tx_bufs);

	return ret;
}

static int ssd1351_init_device(const struct device *dev)
{
	ssd1351_spi_write_byte(dev, SSD1351_CMD_COMMANDLOCK, true);
	ssd1351_spi_write_byte(dev, 0x12, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_COMMANDLOCK, true);
	ssd1351_spi_write_byte(dev, 0xB1, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_DISPLAYOFF, true);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_CLOCKDIV, true);
	ssd1351_spi_write_byte(dev, 0xF1, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_MUXRATIO, true);
	ssd1351_spi_write_byte(dev, 0x7F, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_DISPLAYOFFSET, true);
	ssd1351_spi_write_byte(dev, 0x00, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_SETGPIO, true);
	ssd1351_spi_write_byte(dev, 0x00, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_FUNCTIONSELECT, true);
	ssd1351_spi_write_byte(dev, 0x01, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_PRECHARGE, true);
	ssd1351_spi_write_byte(dev, 0x32, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_VCOMH, true);
	ssd1351_spi_write_byte(dev, 0x05, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_NORMALDISPLAY, true);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_CONTRASTABC, true);
	ssd1351_spi_write_byte(dev, 0xC8, false);
	ssd1351_spi_write_byte(dev, 0x80, false);
	ssd1351_spi_write_byte(dev, 0xC8, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_CONTRASTMASTER, true);
	ssd1351_spi_write_byte(dev, 0x0F, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_SETVSL, true);
	ssd1351_spi_write_byte(dev, 0xA0, false);
	ssd1351_spi_write_byte(dev, 0xB5, false);
	ssd1351_spi_write_byte(dev, 0x55, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_PRECHARGE2, true);
	ssd1351_spi_write_byte(dev, 0x01, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_DISPLAYON, true);

	return 0;	
}

static int ssd1351_blanking_on(const struct device *dev)
{
	ssd1351_spi_write_byte(dev, SSD1351_CMD_DISPLAYOFF, true);
}

static int ssd1351_blanking_off(const struct device *dev)
{
	ssd1351_spi_write_byte(dev, SSD1351_CMD_DISPLAYON, true);
}

static int ssd1351_write(const struct device *dev, const uint16_t x, const uint16_t y,
		const struct display_buffer_descriptor *desc, const void *buf)
{
	ssd1351_spi_write_byte(dev, SSD1351_CMD_SETCOLUMN, true);
	ssd1351_spi_write_byte(dev, x, false);
	ssd1351_spi_write_byte(dev, x + desc->width - 1, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_SETROW, true);
	ssd1351_spi_write_byte(dev, y, false);
	ssd1351_spi_write_byte(dev, y + desc->height - 1, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_WRITERAM, true);

	size_t buff_size = desc->buf_size;

	return ssd1351_spi_write_data(dev, buf, buff_size, false);
}


static int ssd1351_set_brightness(const struct device *dev, const uint8_t brightness)
{
	uint8_t scaled = (brightness * 0x0F) / 100;

	ssd1351_spi_write_byte(dev, SSD1351_CMD_CONTRASTMASTER, true);
	ssd1351_spi_write_byte(dev, scaled, false);

	return 0;
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

	uint8_t madctl = 0b01100100; // 64K, enable split, CBA

	if (orientation == DISPLAY_ORIENTATION_NORMAL)
	{
		madctl |= 0b00010000;
		data->xres = config->width;
		data->yres = config->height;
	} 
	else
	if (orientation == DISPLAY_ORIENTATION_ROTATED_90)
	{
		madctl |= 0b00010011;
		data->xres = config->height;
		data->yres = config->width;
	}
	else
	if (orientation == DISPLAY_ORIENTATION_ROTATED_180)
	{
		madctl |= 0b00000010;
		data->xres = config->width;
		data->yres = config->height;
	}
	else
	if (orientation == DISPLAY_ORIENTATION_ROTATED_270)
	{
		madctl |= 0b00000001;
		data->xres = config->height;
		data->yres = config->width;
	}

	uint8_t startline = (orientation == DISPLAY_ORIENTATION_NORMAL ||
		orientation == DISPLAY_ORIENTATION_ROTATED_90) ?
		config->height : 0;

	ssd1351_spi_write_byte(dev, SSD1351_CMD_SETREMAP, true);
	ssd1351_spi_write_byte(dev, startline, false);

	data->orientation = orientation;

	return 0;
}


static int ssd1351_init(const struct device *dev)
{
	const struct ssd1351_config *config = dev->config;
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
		k_msleep(1);
		gpio_pin_set_dt(&config->reset, 1);
		k_msleep(1);
		gpio_pin_set_dt(&config->reset, 0);
	}
	k_msleep(200);

	ssd1351_init_device(dev);

	if (config->rotation == 0)
	{
		ssd1351_set_orientation(dev, DISPLAY_ORIENTATION_NORMAL);
	}
	else
	if (config->rotation == 90)
	{
		ssd1351_set_orientation(dev, DISPLAY_ORIENTATION_ROTATED_90);
	}
	else
	if (config->rotation == 180)
	{
		ssd1351_set_orientation(dev, DISPLAY_ORIENTATION_ROTATED_180);
	}
	else
	if (config->rotation == 270)
	{
		ssd1351_set_orientation(dev, DISPLAY_ORIENTATION_ROTATED_270);
	}
	return 0;
}

static const struct display_driver_api ssd1351_api = {
	.blanking_on = ssd1351_blanking_on,
	.blanking_off = ssd1351_blanking_off,
	.write = ssd1351_write,
	.set_brightness = ssd1351_set_brightness,
	.get_capabilities = ssd1351_get_capabilities,
	.set_orientation = ssd1351_set_orientation,
};


#define SSD1351_INIT(inst)																\
	static struct ssd1351_data ssd1351_data_##inst;										\
	static const struct ssd1351_config ssd1351_config_##inst = {						\
		.spi = SPI_DT_SPEC_INST_GET(													\
				inst, SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8), 0),		\
		.data_cmd = GPIO_DT_SPEC_INST_GET_OR(inst, dc_gpios, {0}),						\
		.reset = GPIO_DT_SPEC_INST_GET_OR(inst, reset_gpios, {0}),							\
		.height = DT_INST_PROP(inst, height),											\
		.width = DT_INST_PROP(inst, width),												\
		.rotation = DT_INST_PROP(inst, rotation)										\
	};																					\
	DEVICE_DT_INST_DEFINE(inst, ssd1351_init, NULL, &ssd1351_data_##inst,				\
			&ssd1351_config_##inst, POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY,			\
			&ssd1351_api);

DT_INST_FOREACH_STATUS_OKAY(SSD1351_INIT)