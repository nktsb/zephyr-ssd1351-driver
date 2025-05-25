/*
 * Copyright (c) 2025 Subbotin N.Y. <ny.subbotin@yandex.ru>
 *
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
	uint8_t pixel_format;
};

struct ssd1351_data {
	uint16_t xres;
	uint16_t yres;

	enum display_orientation orientation;
	enum display_pixel_format pixel_format;
};

static int ssd1351_spi_write_data(const struct device *dev, uint8_t *buf, 
				  size_t len)
{
	const struct ssd1351_config *config = dev->config;
	int ret;

	gpio_pin_set_dt(&config->data_cmd, 1);

	struct spi_buf tx_buf = {.buf = buf, .len = len};

	struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};

	ret = spi_write_dt(&config->spi, &tx_bufs);

	return ret;
}

static int ssd1351_spi_write_byte(const struct device *dev, uint8_t byte, 
		bool command)
{
	const struct ssd1351_config *config = dev->config;
	int ret;

	gpio_pin_set_dt(&config->data_cmd, command ? 0 : 1);
	struct spi_buf tx_buf = {.buf = &byte, .len = 1};

	struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};

	ret = spi_write_dt(&config->spi, &tx_bufs);

	return ret;
}

static int ssd1351_set_remap(const struct device *dev, 
			     enum display_orientation orientation,
			     enum display_pixel_format pixel_format)
{
	const struct ssd1351_config *config = dev->config;
	struct ssd1351_data *data = dev->data;

	uint8_t remap_mask = 0b00100000;

	switch (pixel_format) {
		case PIXEL_FORMAT_RGB_888:
		remap_mask |= 0b10000100;
		break;
	case PIXEL_FORMAT_BGR_565:
		remap_mask |= 0b01000000;
		break;
	case PIXEL_FORMAT_RGB_565:
	default:
		remap_mask |= 0b01000100;
		break;
	}

	switch (orientation) {
		case DISPLAY_ORIENTATION_ROTATED_90:
			remap_mask |= 0b00010011;

			data->xres = config->height;
			data->yres = config->width;
			break;
		case DISPLAY_ORIENTATION_ROTATED_180:
			remap_mask |= 0b00000010;

			data->xres = config->width;
			data->yres = config->height;
			break;
		case DISPLAY_ORIENTATION_ROTATED_270:
			remap_mask |= 0b00000001;

			data->xres = config->height;
			data->yres = config->width;
			break;
		case DISPLAY_ORIENTATION_NORMAL:
		default:
			remap_mask |= 0b00010000;

			data->xres = config->width;
			data->yres = config->height;
			break;
	}

	uint8_t startline = (orientation == DISPLAY_ORIENTATION_NORMAL || 
			orientation == DISPLAY_ORIENTATION_ROTATED_90) ?
			config->height : 0;

	ssd1351_spi_write_byte(dev, SSD1351_CMD_SETREMAP, true);
	ssd1351_spi_write_byte(dev, remap_mask, false);
	ssd1351_spi_write_byte(dev, SSD1351_CMD_STARTLINE, true);
	ssd1351_spi_write_byte(dev, startline, false);

	data->orientation = orientation;
	data->pixel_format = pixel_format;

	return 0;
}

static const uint8_t ssd1351_grayscale[] = {
	0x02, 0x03, 0x04, 0x05,
	0x06, 0x07, 0x08, 0x09,
	0x0A, 0x0B, 0x0C, 0x0D,
	0x0E, 0x0F, 0x10, 0x11,
	0x12, 0x13, 0x15, 0x17,
	0x19, 0x1B, 0x1D, 0x1F,
	0x21, 0x23, 0x25, 0x27,
	0x2A, 0x2D, 0x30, 0x33,
	0x36, 0x39, 0x3C, 0x3F,
	0x42, 0x45, 0x48, 0x4C,
	0x50, 0x54, 0x58, 0x5C,
	0x60, 0x64, 0x68, 0x6C,
	0x70, 0x74, 0x78, 0x7D,
	0x82, 0x87, 0x8C, 0x91,
	0x96, 0x9B, 0xA0, 0xA5,
	0xAA, 0xAF, 0xB4
};

static int ssd1351_init_device(const struct device *dev,
			       enum display_orientation orientation, 
			       enum display_pixel_format pixel_format)
{
	ssd1351_spi_write_byte(dev, SSD1351_CMD_COMMANDLOCK, true);
	ssd1351_spi_write_byte(dev, 0x12, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_COMMANDLOCK, true);
	ssd1351_spi_write_byte(dev, 0xB1, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_DISPLAYOFF, true);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_CLOCKDIV, true);
	ssd1351_spi_write_byte(dev, 0xF0, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_MUXRATIO, true);
	ssd1351_spi_write_byte(dev, 0x7F, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_DISPLAYOFFSET, true);
	ssd1351_spi_write_byte(dev, 0x00, false);

	ssd1351_set_remap(dev, orientation, pixel_format);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_SETGPIO, true);
	ssd1351_spi_write_byte(dev, 0x00, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_FUNCTIONSELECT, true);
	ssd1351_spi_write_byte(dev, 0x01, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_SETVSL, true);
	ssd1351_spi_write_byte(dev, 0xA0, false);
	ssd1351_spi_write_byte(dev, 0xB5, false);
	ssd1351_spi_write_byte(dev, 0x55, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_CONTRASTABC, true);
	ssd1351_spi_write_byte(dev, 0xC8, false);
	ssd1351_spi_write_byte(dev, 0x80, false);
	ssd1351_spi_write_byte(dev, 0xC8, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_CONTRASTMASTER, true);
	ssd1351_spi_write_byte(dev, 0x0F, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_SETGRAY, true);

	ssd1351_spi_write_data(dev, (uint8_t*)ssd1351_grayscale, 
			sizeof(ssd1351_grayscale));

	ssd1351_spi_write_byte(dev, SSD1351_CMD_PRECHARGE, true);
	ssd1351_spi_write_byte(dev, 0x32, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_DISPLAYENHANCE, true);
	ssd1351_spi_write_byte(dev, 0xA4, false);
	ssd1351_spi_write_byte(dev, 0x00, false);
	ssd1351_spi_write_byte(dev, 0x00, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_PRECHARGELEVEL, true);
	ssd1351_spi_write_byte(dev, 0x17, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_PRECHARGE2, true);
	ssd1351_spi_write_byte(dev, 0x01, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_VCOMH, true);
	ssd1351_spi_write_byte(dev, 0x05, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_NORMALDISPLAY, true);

	// ssd1351_spi_write_byte(dev, SSD1351_CMD_DISPLAYON, true);

	return 0;
}

static int ssd1351_blanking_on(const struct device *dev)
{
	return ssd1351_spi_write_byte(dev, SSD1351_CMD_DISPLAYOFF, true);
}

static int ssd1351_blanking_off(const struct device *dev)
{
	return ssd1351_spi_write_byte(dev, SSD1351_CMD_DISPLAYON, true);
}

static int ssd1351_write(const struct device *dev, const uint16_t x, 
			 const uint16_t y,
			 const struct display_buffer_descriptor *desc, 
			 const void *buf)
{
	struct ssd1351_data *data = dev->data;

	uint8_t bytes_per_pixel = 
			(data->pixel_format == PIXEL_FORMAT_RGB_888)? 3 : 2;

	__ASSERT(desc->width <= desc->pitch, "Pitch is smaller than width");
	__ASSERT((desc->pitch * bytes_per_pixel * desc->height) <= desc->buf_size,
	"Input buffer too small");

	LOG_DBG("Writing %dx%d (w,h) @ %dx%d (x,y)", desc->width, desc->height, x, y);

	bool coord_swap = data->orientation == DISPLAY_ORIENTATION_ROTATED_90 ||
	data->orientation == DISPLAY_ORIENTATION_ROTATED_270 ? true : false;

	ssd1351_spi_write_byte(dev, coord_swap? 
			SSD1351_CMD_SETROW : SSD1351_CMD_SETCOLUMN, true);
	ssd1351_spi_write_byte(dev, x, false);
	ssd1351_spi_write_byte(dev, x + desc->width - 1, false);

	ssd1351_spi_write_byte(dev, coord_swap? 
			SSD1351_CMD_SETCOLUMN : SSD1351_CMD_SETROW, true);
	ssd1351_spi_write_byte(dev, y, false);
	ssd1351_spi_write_byte(dev, y + desc->height - 1, false);

	ssd1351_spi_write_byte(dev, SSD1351_CMD_WRITERAM, true);

	size_t buff_size = desc->buf_size;

	if (data->pixel_format == PIXEL_FORMAT_RGB_888) { /* conversion RGB_888 -> RGB_666 */
		int ret = 0;

		uint8_t *buf_888 = (uint8_t*)buf;
		size_t row_size = desc->width * bytes_per_pixel;
		uint8_t row_666[row_size];

		for (size_t i = 0; i < desc->height; i++) {
			uint8_t *row_888 = buf_888 + (i * row_size);

			for (size_t k = 0; k < sizeof(row_666); k++)
			row_666[k] = row_888[k] >> 2;

			ret = ssd1351_spi_write_data(dev, row_666, 
					sizeof(row_666));

			if (ret) {
				return ret;
			}
		}
		return 0;
	} else {
		return ssd1351_spi_write_data(dev, (uint8_t *)buf, buff_size);
	}
}


static int ssd1351_set_brightness(const struct device *dev, 
				  const uint8_t brightness)
{
	uint8_t scaled = brightness >> 4;

	ssd1351_spi_write_byte(dev, SSD1351_CMD_CONTRASTMASTER, true);
	ssd1351_spi_write_byte(dev, scaled, false);

	return 0;
}

static void ssd1351_get_capabilities(const struct device *dev, 
				     struct display_capabilities *capabilities)
{
	struct ssd1351_data *data = dev->data;

	memset(capabilities, 0, sizeof(struct display_capabilities));

	capabilities->x_resolution = data->xres;
	capabilities->y_resolution = data->yres;

	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_888 | 
			PIXEL_FORMAT_RGB_565 | PIXEL_FORMAT_BGR_565;

	capabilities->current_pixel_format = data->pixel_format;

	capabilities->current_orientation = data->orientation;
}

static int ssd1351_set_orientation(const struct device *dev,
				   const enum display_orientation orientation)
{
	struct ssd1351_data *data = dev->data;

	ssd1351_set_remap(dev, orientation, data->pixel_format);

	return 0;
}

static int ssd1351_set_pixel_format(const struct device *dev,
				    const enum display_pixel_format pixel_format)
{
	struct ssd1351_data *data = dev->data;

	if (pixel_format != PIXEL_FORMAT_RGB_565 && pixel_format != 
			PIXEL_FORMAT_RGB_888 && 
			pixel_format != PIXEL_FORMAT_BGR_565) {

		LOG_ERR("Unsupported pixel format");
		return -ENOTSUP;
	}

	ssd1351_set_remap(dev, data->orientation, pixel_format);

	return 0;
}

static int ssd1351_init(const struct device *dev)
{
	const struct ssd1351_config *config = dev->config;
	int ret;

	if (!gpio_is_ready_dt(&config->data_cmd)) {
		LOG_ERR("D/C GPIO device not ready");
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&config->data_cmd, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Couldn't configure D/C pin");
		return ret;
	}

	if (!spi_is_ready_dt(&config->spi)) {
		LOG_ERR("SPI device not ready");
		return -ENODEV;
	}

	if (config->reset.port) {
		if (!gpio_is_ready_dt(&config->reset)) {
			LOG_ERR("Reset GPIO device not ready");
			return -ENODEV;
		}
		ret = gpio_pin_configure_dt(&config->reset, GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			LOG_ERR("Couldn't configure reset pin");
			return ret;
		}
		gpio_pin_set_dt(&config->reset, 1);
		k_msleep(1);
		gpio_pin_set_dt(&config->reset, 0);
	}
	k_msleep(200);

	enum display_orientation orientation;

	switch(config->rotation) {
		case 90:
			orientation = DISPLAY_ORIENTATION_ROTATED_90;
			break;
		case 180:
			orientation = DISPLAY_ORIENTATION_ROTATED_180;
			break;
		case 270:
			orientation = DISPLAY_ORIENTATION_ROTATED_270;
			break;
		case 0:
		default:
			orientation = DISPLAY_ORIENTATION_NORMAL;
			break;
	}

	ssd1351_init_device(dev, orientation, config->pixel_format);

	return 0;
}

static const struct display_driver_api ssd1351_api = {
	.blanking_on = ssd1351_blanking_on,
	.blanking_off = ssd1351_blanking_off,
	.write = ssd1351_write,
	.set_brightness = ssd1351_set_brightness,
	.get_capabilities = ssd1351_get_capabilities,
	.set_pixel_format = ssd1351_set_pixel_format,
	.set_orientation = ssd1351_set_orientation,
};


#define SSD1351_INIT(inst)								\
	static struct ssd1351_data ssd1351_data_##inst;					\
	static const struct ssd1351_config ssd1351_config_##inst = {			\
		.spi = SPI_DT_SPEC_INST_GET(inst, SPI_OP_MODE_MASTER |			\
					    SPI_TRANSFER_MSB | SPI_WORD_SET(8), 0),	\
		.data_cmd = GPIO_DT_SPEC_INST_GET_OR(inst, dc_gpios, {0}),		\
		.reset = GPIO_DT_SPEC_INST_GET_OR(inst, reset_gpios, {0}),		\
		.height = DT_INST_PROP(inst, height),					\
		.width = DT_INST_PROP(inst, width),					\
		.rotation = DT_INST_PROP(inst, rotation),				\
		.pixel_format = DT_INST_PROP(inst, pixel_format)			\
	};										\
	DEVICE_DT_INST_DEFINE(inst, ssd1351_init, NULL, &ssd1351_data_##inst,		\
			      &ssd1351_config_##inst, POST_KERNEL,			\
			      CONFIG_DISPLAY_INIT_PRIORITY,				\
			      &ssd1351_api);

DT_INST_FOREACH_STATUS_OKAY(SSD1351_INIT)