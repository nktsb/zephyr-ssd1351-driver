/*
 * Copyright (c) 2025 Subbotin N.Y. <ny.subbotin@yandex.ru>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT solomon_ssd1351

#include "ssd1351.h"

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#define LOG_LEVEL CONFIG_DISPLAY_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_ssd1351);

static int ssd1351_mipi_transmit_byte(const struct device *dev, uint8_t cmd, uint8_t byte)
{
	const struct ssd1351_config *config = dev->config;

	return mipi_dbi_command_write(config->mipi_dbi, &config->dbi_config, cmd,
			       &byte, sizeof(byte));
}

static int ssd1351_mipi_transmit(const struct device *dev, uint8_t cmd,
			     const uint8_t* tx_data, size_t tx_len)
{
	const struct ssd1351_config *config = dev->config;

	return mipi_dbi_command_write(config->mipi_dbi, &config->dbi_config, cmd,
			       tx_data, tx_len);
}

static int ssd1351_set_remap(const struct device* dev,
			     enum display_orientation orientation,
			     enum display_pixel_format pixel_format)
{
	const struct ssd1351_config* config = dev->config;
	struct ssd1351_data* data = dev->data;

	uint8_t remap_mask = 0b00100000;

	switch (pixel_format)
	{
		case PIXEL_FORMAT_BGR_565:
			remap_mask |= 0b01000000;
			data->bytes_per_pixel = 2;
			break;
		case PIXEL_FORMAT_RGB_565:
		default:
			remap_mask |= 0b01000100;
			data->bytes_per_pixel = 2;
			break;
	}

	switch (orientation)
	{
		case DISPLAY_ORIENTATION_ROTATED_90:
			remap_mask |= 0b00010011;

			data->xres = config->height;
			data->yres = config->width;
			data->x_cmd = SSD1351_CMD_SETROW;
			data->y_cmd = SSD1351_CMD_SETCOLUMN;
			break;
		case DISPLAY_ORIENTATION_ROTATED_180:
			remap_mask |= 0b00000010;

			data->xres = config->width;
			data->yres = config->height;
			data->x_cmd = SSD1351_CMD_SETCOLUMN;
			data->y_cmd = SSD1351_CMD_SETROW;
			break;
		case DISPLAY_ORIENTATION_ROTATED_270:
			remap_mask |= 0b00000001;

			data->xres = config->height;
			data->yres = config->width;
			data->x_cmd = SSD1351_CMD_SETROW;
			data->y_cmd = SSD1351_CMD_SETCOLUMN;
			break;
		case DISPLAY_ORIENTATION_NORMAL:
		default:
			remap_mask |= 0b00010000;

			data->xres = config->width;
			data->yres = config->height;
			data->x_cmd = SSD1351_CMD_SETCOLUMN;
			data->y_cmd = SSD1351_CMD_SETROW;
			break;
	}

	uint8_t startline = (orientation == DISPLAY_ORIENTATION_NORMAL ||
			     orientation == DISPLAY_ORIENTATION_ROTATED_90) ?
				    config->height :
				    0;

	ssd1351_mipi_transmit(dev, SSD1351_CMD_SETREMAP, &remap_mask, sizeof(remap_mask));
	ssd1351_mipi_transmit(dev, SSD1351_CMD_STARTLINE, &startline, sizeof(startline));

	data->orientation = orientation;
	data->pixel_format = pixel_format;

	return 0;
}

static const uint8_t ssd1351_grayscale[] = {
	0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
	0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x15, 0x17, 0x19, 0x1B,
	0x1D, 0x1F, 0x21, 0x23, 0x25, 0x27, 0x2A, 0x2D, 0x30, 0x33, 0x36,
	0x39, 0x3C, 0x3F, 0x42, 0x45, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C,
	0x60, 0x64, 0x68, 0x6C, 0x70, 0x74, 0x78, 0x7D, 0x82, 0x87, 0x8C,
	0x91, 0x96, 0x9B, 0xA0, 0xA5, 0xAA, 0xAF, 0xB4};

static int ssd1351_init_device(const struct device* dev,
			       enum display_orientation orientation,
			       enum display_pixel_format pixel_format)
{
	ssd1351_mipi_transmit_byte(dev, SSD1351_CMD_COMMANDLOCK, 0x12);

	ssd1351_mipi_transmit_byte(dev, SSD1351_CMD_COMMANDLOCK, 0xB1);

	ssd1351_mipi_transmit(dev, SSD1351_CMD_DISPLAYOFF, NULL, 0);

	ssd1351_mipi_transmit_byte(dev, SSD1351_CMD_MUXRATIO, 0x7F);

	ssd1351_mipi_transmit_byte(dev, SSD1351_CMD_DISPLAYOFFSET, 0x00);

	ssd1351_set_remap(dev, orientation, pixel_format);

	ssd1351_mipi_transmit_byte(dev, SSD1351_CMD_SETGPIO, 0x00);

	ssd1351_mipi_transmit_byte(dev, SSD1351_CMD_FUNCTIONSELECT, 0x01);

	uint8_t cmd_buf[3] = {0xA0, 0xB5, 0x55};
	ssd1351_mipi_transmit(dev, SSD1351_CMD_SETVSL, cmd_buf, sizeof(cmd_buf));

	ssd1351_mipi_transmit_byte(dev, SSD1351_CMD_CONTRASTMASTER, 0x0F);

	// ssd1351_mipi_transmit(dev, SSD1351_CMD_SETGRAY, ssd1351_grayscale, sizeof(ssd1351_grayscale));
	ssd1351_mipi_transmit(dev, SSD1351_CMD_USELUT, NULL, 0);

	ssd1351_mipi_transmit_byte(dev, SSD1351_CMD_CLOCKDIV, 0xF0);
	ssd1351_mipi_transmit_byte(dev, SSD1351_CMD_PRECHARGE, 0xFF);
	ssd1351_mipi_transmit_byte(dev, SSD1351_CMD_PRECHARGE2, 0x0F);
	ssd1351_mipi_transmit_byte(dev, SSD1351_CMD_PRECHARGELEVEL, 0x00);
	ssd1351_mipi_transmit_byte(dev, SSD1351_CMD_VCOMH, 0x06);

	cmd_buf[0] = 0x8A;
	cmd_buf[1] = 0x51;
	cmd_buf[2] = 0x8A;
	ssd1351_mipi_transmit(dev, SSD1351_CMD_CONTRASTABC, cmd_buf, sizeof(cmd_buf));

	ssd1351_mipi_transmit(dev, SSD1351_CMD_NORMALDISPLAY, NULL, 0);

	return 0;
}

static int ssd1351_blanking_on(const struct device* dev)
{
	return ssd1351_mipi_transmit(dev, SSD1351_CMD_DISPLAYOFF, NULL, 0);
}

static int ssd1351_blanking_off(const struct device* dev)
{
	return ssd1351_mipi_transmit(dev, SSD1351_CMD_DISPLAYON, NULL, 0);
}

static int ssd1351_write(const struct device* dev, const uint16_t x,
			 const uint16_t y,
			 const struct display_buffer_descriptor* desc,
			 const void* buf)
{
	const struct ssd1351_config *config = dev->config;
	struct ssd1351_data* data = dev->data;

	__ASSERT(desc->width <= desc->pitch, "Pitch is smaller than width");
	__ASSERT((desc->pitch * data->bytes_per_pixel * desc->height) <=
			 desc->buf_size,
		 "Input buffer too small");

	LOG_DBG("Writing %dx%d (w,h) @ %dx%d (x,y)", desc->width, desc->height,
		x, y);

	struct display_buffer_descriptor mipi_desc = *desc;

	uint8_t x_buff[] = {x, x + desc->width - 1};
	uint8_t y_buff[] = {y, y + desc->height - 1};

	ssd1351_mipi_transmit(dev, data->x_cmd, x_buff, sizeof(x_buff));
	ssd1351_mipi_transmit(dev, data->y_cmd, y_buff, sizeof(y_buff));
	ssd1351_mipi_transmit(dev, SSD1351_CMD_WRITERAM, NULL, 0);

	return mipi_dbi_write_display(config->mipi_dbi, &config->dbi_config, buf, &mipi_desc,
				     data->pixel_format);
}

static int ssd1351_set_brightness(const struct device* dev,
				  const uint8_t brightness)
{
	uint8_t scaled = brightness * 0x0F / 100;

	ssd1351_mipi_transmit(dev, SSD1351_CMD_CONTRASTMASTER, &scaled, sizeof(scaled));

	return 0;
}

static void ssd1351_get_capabilities(const struct device* dev,
				     struct display_capabilities* capabilities)
{
	struct ssd1351_data* data = dev->data;

	memset(capabilities, 0, sizeof(struct display_capabilities));

	capabilities->x_resolution = data->xres;
	capabilities->y_resolution = data->yres;

	capabilities->supported_pixel_formats = PIXEL_FORMAT_RGB_565 |
						PIXEL_FORMAT_BGR_565;

	capabilities->current_pixel_format = data->pixel_format;

	capabilities->current_orientation = data->orientation;
}

static int ssd1351_set_orientation(const struct device* dev,
				   const enum display_orientation orientation)
{
	struct ssd1351_data* data = dev->data;

	ssd1351_set_remap(dev, orientation, data->pixel_format);

	return 0;
}

static int
ssd1351_set_pixel_format(const struct device* dev,
			 const enum display_pixel_format pixel_format)
{
	struct ssd1351_data* data = dev->data;

	if (pixel_format != PIXEL_FORMAT_RGB_565 &&
	    pixel_format != PIXEL_FORMAT_BGR_565)
	{

		LOG_ERR("Unsupported pixel format");
		return -ENOTSUP;
	}

	ssd1351_set_remap(dev, data->orientation, pixel_format);

	return 0;
}

static void ssd1351_reset_display(const struct device *dev)
{
	const struct ssd1351_config *config = dev->config;
	int ret;

	LOG_DBG("Resetting display");

	ret = mipi_dbi_reset(config->mipi_dbi, 6);
	if (ret < 0) {
		LOG_ERR("Can't reset displa [%d]", ret);
		return;
	}
	k_msleep(200);
}

static int ssd1351_init(const struct device* dev)
{
	const struct ssd1351_config* config = dev->config;

	if (!device_is_ready(config->mipi_dbi))
	{
		LOG_ERR("MIPI device not ready");
		return -ENODEV;
	}

	ssd1351_reset_display(dev);

	ssd1351_init_device(dev, config->orientation, config->pixel_format);

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

#define SSD1351_INIT(inst)							\
	static struct ssd1351_data ssd1351_data_##inst;				\
	static const struct ssd1351_config ssd1351_config_##inst = {		\
		.mipi_dbi = DEVICE_DT_GET(DT_INST_PARENT(inst)),		\
		.dbi_config = MIPI_DBI_CONFIG_DT_INST(inst,			\
						      SPI_WORD_SET(8) |		\
						      SPI_OP_MODE_MASTER, 0),	\
		.height = DT_INST_PROP(inst, height),				\
		.width = DT_INST_PROP(inst, width),				\
		.orientation = DT_INST_ENUM_IDX(inst, rotation),		\
		.pixel_format = DT_INST_PROP(inst, pixel_format)};		\
	DEVICE_DT_INST_DEFINE(inst, ssd1351_init, NULL,				\
			      &ssd1351_data_##inst, &ssd1351_config_##inst,	\
			      POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY,	\
			      &ssd1351_api);

DT_INST_FOREACH_STATUS_OKAY(SSD1351_INIT)