/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

//#include "../../../misc/mediatek/gate_ic/gate_i2c.h"
//#endif

//drv add by yubo for hardware_info 20240416 start
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/prize/hardware_info/hardware_info.h"
extern struct hardware_info current_lcm_info;
#endif
//drv add by yubo for hardware_info 20240416 end

#include <linux/i2c-dev.h>
#include <linux/i2c.h>

/* prize added by KLJ, prize tp gesture function, 20240418-start */
extern g_tp_gesture_flag;
/* prize added by KLJ, prize tp gesture function, 20240418-start */

#define HFP_SUPPORT 0

#if HFP_SUPPORT
static int current_fps = 60;
#endif

#define BYPASSI2C

#ifndef BYPASSI2C
/* i2c control start */
#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
static struct i2c_client *_lcm_i2c_client;
static int _lcm_i2c_probe(struct i2c_client *client,const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);

struct _lcm_i2c_dev {
	struct i2c_client *client;
};

static const struct of_device_id _lcm_i2c_of_match[] = {
	{
	    .compatible = "mediatek,I2C_LCD_BIAS",
	},
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = { { LCM_I2C_ID_NAME, 0 },
						    {} };

static struct i2c_driver _lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	/* .detect		   = _lcm_i2c_detect, */
	.driver = {
		.owner = THIS_MODULE,
		.name = LCM_I2C_ID_NAME,
		.of_match_table = _lcm_i2c_of_match,
	},
};

static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	printk("[LCM][I2C] %s\n", __func__);
	printk("[LCM][I2C] NT: info==>name=%s addr=0x%x\n", client->name,client->addr);
	_lcm_i2c_client = client;
	return 0;
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	printk("[LCM][I2C] %s\n", __func__);
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		printk("ERROR!! _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_info("[LCM][ERROR] _lcm_i2c write data fail !!\n");

	return ret;
}

// static int __init _lcm_i2c_init(void)
// {
	// printk("[LCM][I2C] %s\n", __func__);
	// i2c_add_driver(&_lcm_i2c_driver);
	// printk("[LCM][I2C] %s success\n", __func__);
	// return 0;
// }

// static void __exit _lcm_i2c_exit(void)
// {
	// printk("[LCM][I2C] %s\n", __func__);
	// i2c_del_driver(&_lcm_i2c_driver);
// }

// module_init(_lcm_i2c_init);
// module_exit(_lcm_i2c_exit);

#endif

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *ldo18_gpio;
	struct gpio_desc *bias_pos, *bias_neg;

	bool prepared;
	bool enabled;

	int error;
};


#define lcm_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	//printk("lcm init write len = %d\n", len);

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void lcm_panel_init(struct lcm *ctx)
{

	pr_info("gc7202m %s\n", __func__);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	//gpiod_set_value(ctx->reset_gpio, 0);
	//udelay(15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(5);//ili9882q at least 10ms
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(30);
	
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	
	lcm_dcs_write_seq_static(ctx,0xFF,0x55,0xAA,0x66);
	lcm_dcs_write_seq_static(ctx,0xFE,0x55,0xAA,0x66);
	lcm_dcs_write_seq_static(ctx,0xFF,0xC3);
	lcm_dcs_write_seq_static(ctx,0xFB,0x00);
	lcm_dcs_write_seq_static(ctx,0x13,0x07);
	lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
	lcm_dcs_write_seq_static(ctx,0x2B,0x0C);
	lcm_dcs_write_seq_static(ctx,0x29,0x3F);
	lcm_dcs_write_seq_static(ctx,0x28,0xC0);
	lcm_dcs_write_seq_static(ctx,0x2A,0x03);
	lcm_dcs_write_seq_static(ctx,0x68,0x0F);
	lcm_dcs_write_seq_static(ctx,0xFF,0x20);
	lcm_dcs_write_seq_static(ctx,0xFB,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0x21);
	lcm_dcs_write_seq_static(ctx,0xFB,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0x22);
	lcm_dcs_write_seq_static(ctx,0xFB,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0x23);
	lcm_dcs_write_seq_static(ctx,0xFB,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0x24);
	lcm_dcs_write_seq_static(ctx,0xFB,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0x26);
	lcm_dcs_write_seq_static(ctx,0xFB,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0x27);
	lcm_dcs_write_seq_static(ctx,0xFB,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0x28);
	lcm_dcs_write_seq_static(ctx,0xFB,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
	lcm_dcs_write_seq_static(ctx,0xFB,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0x10);
	lcm_dcs_write_seq_static(ctx,0xFB,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0xC3);
	lcm_dcs_write_seq_static(ctx,0xFB,0x00);
	lcm_dcs_write_seq_static(ctx,0x13,0x07);
	lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
	lcm_dcs_write_seq_static(ctx,0x4A,0x0F);
	lcm_dcs_write_seq_static(ctx,0x7D,0x80);
	lcm_dcs_write_seq_static(ctx,0x5B,0x4B);
	lcm_dcs_write_seq_static(ctx,0x48,0x26);
	lcm_dcs_write_seq_static(ctx,0x7C,0x8C);
	lcm_dcs_write_seq_static(ctx,0x3E,0x03);
	lcm_dcs_write_seq_static(ctx,0x58,0x84);
	lcm_dcs_write_seq_static(ctx,0x53,0x1A);
	lcm_dcs_write_seq_static(ctx,0xFF,0x28);
	lcm_dcs_write_seq_static(ctx,0x53,0x44);
	lcm_dcs_write_seq_static(ctx,0x50,0x4C);
	lcm_dcs_write_seq_static(ctx,0x52,0x52);
	lcm_dcs_write_seq_static(ctx,0x4D,0xA1);
	lcm_dcs_write_seq_static(ctx,0xFF,0x20);
	lcm_dcs_write_seq_static(ctx,0xA5,0x00);
	lcm_dcs_write_seq_static(ctx,0xA6,0xFF);
	lcm_dcs_write_seq_static(ctx,0xA9,0x00);
	lcm_dcs_write_seq_static(ctx,0xAA,0xFF);
	lcm_dcs_write_seq_static(ctx,0xD3,0x06);
	lcm_dcs_write_seq_static(ctx,0x2D,0x1F);
	lcm_dcs_write_seq_static(ctx,0x2E,0x42);
	lcm_dcs_write_seq_static(ctx,0x2F,0x14);
	lcm_dcs_write_seq_static(ctx,0xFF,0x22);
	lcm_dcs_write_seq_static(ctx,0x1F,0x06);
	lcm_dcs_write_seq_static(ctx,0xF4,0x01);
	lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
	lcm_dcs_write_seq_static(ctx,0x82,0x1A);
	lcm_dcs_write_seq_static(ctx,0xFF,0x20);
	lcm_dcs_write_seq_static(ctx,0xA3,0x47);
	lcm_dcs_write_seq_static(ctx,0xA7,0x47);
	lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
	lcm_dcs_write_seq_static(ctx,0x3F,0x37);
	lcm_dcs_write_seq_static(ctx,0x5E,0x10);
	lcm_dcs_write_seq_static(ctx,0xFF,0x22);
	lcm_dcs_write_seq_static(ctx,0xE4,0x00);
	lcm_dcs_write_seq_static(ctx,0x01,0x05);
	lcm_dcs_write_seq_static(ctx,0x02,0xA0);
	lcm_dcs_write_seq_static(ctx,0x25,0x07);
	lcm_dcs_write_seq_static(ctx,0x26,0x00);
	lcm_dcs_write_seq_static(ctx,0x2E,0x96);
	lcm_dcs_write_seq_static(ctx,0x2F,0x00);
	lcm_dcs_write_seq_static(ctx,0x36,0x08);
	lcm_dcs_write_seq_static(ctx,0x37,0x00);
	lcm_dcs_write_seq_static(ctx,0x3F,0x96);
	lcm_dcs_write_seq_static(ctx,0x40,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0x20);
	lcm_dcs_write_seq_static(ctx,0xC3,0x00);
	lcm_dcs_write_seq_static(ctx,0xC4,0x74);
	lcm_dcs_write_seq_static(ctx,0xB3,0x00);
	lcm_dcs_write_seq_static(ctx,0xB4,0x28);
	lcm_dcs_write_seq_static(ctx,0xB5,0x00);
	lcm_dcs_write_seq_static(ctx,0xB6,0xAA);
	lcm_dcs_write_seq_static(ctx,0xFF,0x28);
	lcm_dcs_write_seq_static(ctx,0x01,0x25);
	lcm_dcs_write_seq_static(ctx,0x02,0x1a);
	lcm_dcs_write_seq_static(ctx,0x03,0x19);
	lcm_dcs_write_seq_static(ctx,0x04,0x08);
	lcm_dcs_write_seq_static(ctx,0x05,0x0A);
	lcm_dcs_write_seq_static(ctx,0x06,0x0C);
	lcm_dcs_write_seq_static(ctx,0x07,0x0E);
	lcm_dcs_write_seq_static(ctx,0x08,0x23);
	lcm_dcs_write_seq_static(ctx,0x09,0x1b);
	lcm_dcs_write_seq_static(ctx,0x0A,0x18);
	lcm_dcs_write_seq_static(ctx,0x0B,0x00);
	lcm_dcs_write_seq_static(ctx,0x0C,0x02);
	lcm_dcs_write_seq_static(ctx,0x0D,0x25);
	lcm_dcs_write_seq_static(ctx,0x0E,0x25);
	lcm_dcs_write_seq_static(ctx,0x0F,0x25);
	lcm_dcs_write_seq_static(ctx,0x10,0x25);
	lcm_dcs_write_seq_static(ctx,0x11,0x25);
	lcm_dcs_write_seq_static(ctx,0x12,0x25);
	lcm_dcs_write_seq_static(ctx,0x13,0x25);
	lcm_dcs_write_seq_static(ctx,0x14,0x25);
	lcm_dcs_write_seq_static(ctx,0x15,0x25);
	lcm_dcs_write_seq_static(ctx,0x16,0x25);
	lcm_dcs_write_seq_static(ctx,0x17,0x25);
	lcm_dcs_write_seq_static(ctx,0x18,0x1a);
	lcm_dcs_write_seq_static(ctx,0x19,0x19);
	lcm_dcs_write_seq_static(ctx,0x1A,0x09);
	lcm_dcs_write_seq_static(ctx,0x1B,0x0B);
	lcm_dcs_write_seq_static(ctx,0x1C,0x0D);
	lcm_dcs_write_seq_static(ctx,0x1D,0x0F);
	lcm_dcs_write_seq_static(ctx,0x1E,0x23);
	lcm_dcs_write_seq_static(ctx,0x1F,0x1b);
	lcm_dcs_write_seq_static(ctx,0x20,0x18);
	lcm_dcs_write_seq_static(ctx,0x21,0x01);
	lcm_dcs_write_seq_static(ctx,0x22,0x03);
	lcm_dcs_write_seq_static(ctx,0x23,0x25);
	lcm_dcs_write_seq_static(ctx,0x24,0x25);
	lcm_dcs_write_seq_static(ctx,0x25,0x25);
	lcm_dcs_write_seq_static(ctx,0x26,0x25);
	lcm_dcs_write_seq_static(ctx,0x27,0x25);
	lcm_dcs_write_seq_static(ctx,0x28,0x25);
	lcm_dcs_write_seq_static(ctx,0x29,0x25);
	lcm_dcs_write_seq_static(ctx,0x2A,0x25);
	lcm_dcs_write_seq_static(ctx,0x2B,0x25);
	lcm_dcs_write_seq_static(ctx,0x2D,0x25);
	lcm_dcs_write_seq_static(ctx,0x30,0x00);
	lcm_dcs_write_seq_static(ctx,0x31,0x00);
	lcm_dcs_write_seq_static(ctx,0x34,0x00);
	lcm_dcs_write_seq_static(ctx,0x35,0x00);
	lcm_dcs_write_seq_static(ctx,0x36,0x00);
	lcm_dcs_write_seq_static(ctx,0x37,0x00);
	lcm_dcs_write_seq_static(ctx,0x38,0x00);
	lcm_dcs_write_seq_static(ctx,0x39,0x00);
	lcm_dcs_write_seq_static(ctx,0x2F,0x10);
	lcm_dcs_write_seq_static(ctx,0xFF,0x21);
	lcm_dcs_write_seq_static(ctx,0x7E,0x03);
	lcm_dcs_write_seq_static(ctx,0x7F,0x23);
	lcm_dcs_write_seq_static(ctx,0x8B,0x23);
	lcm_dcs_write_seq_static(ctx,0x80,0x03);
	lcm_dcs_write_seq_static(ctx,0x8C,0x07);
	lcm_dcs_write_seq_static(ctx,0x81,0x07);
	lcm_dcs_write_seq_static(ctx,0x8D,0x03);
	lcm_dcs_write_seq_static(ctx,0xAF,0x40);
	lcm_dcs_write_seq_static(ctx,0xB0,0x40);
	lcm_dcs_write_seq_static(ctx,0x83,0x02);
	lcm_dcs_write_seq_static(ctx,0x8F,0x02);
	lcm_dcs_write_seq_static(ctx,0x84,0x50);
	lcm_dcs_write_seq_static(ctx,0x90,0x50);
	lcm_dcs_write_seq_static(ctx,0x85,0x60);
	lcm_dcs_write_seq_static(ctx,0x91,0x60);
	lcm_dcs_write_seq_static(ctx,0x87,0x04);
	lcm_dcs_write_seq_static(ctx,0x93,0x00);
	lcm_dcs_write_seq_static(ctx,0x82,0x70);
	lcm_dcs_write_seq_static(ctx,0x8E,0x70);
	lcm_dcs_write_seq_static(ctx,0x2B,0x00);
	lcm_dcs_write_seq_static(ctx,0x2E,0x00);
	lcm_dcs_write_seq_static(ctx,0x88,0x80);
	lcm_dcs_write_seq_static(ctx,0x89,0x00);
	lcm_dcs_write_seq_static(ctx,0x8A,0x00);
	lcm_dcs_write_seq_static(ctx,0x94,0x80);
	lcm_dcs_write_seq_static(ctx,0x95,0x00);
	lcm_dcs_write_seq_static(ctx,0x96,0x00);
	lcm_dcs_write_seq_static(ctx,0x45,0x0F);
	lcm_dcs_write_seq_static(ctx,0x46,0x74);
	lcm_dcs_write_seq_static(ctx,0x4C,0x74);
	lcm_dcs_write_seq_static(ctx,0x52,0x74);
	lcm_dcs_write_seq_static(ctx,0x58,0x74);
	lcm_dcs_write_seq_static(ctx,0x47,0x09);
	lcm_dcs_write_seq_static(ctx,0x4D,0x08);
	lcm_dcs_write_seq_static(ctx,0x53,0x07);
	lcm_dcs_write_seq_static(ctx,0x59,0x06);
	lcm_dcs_write_seq_static(ctx,0x48,0x06);
	lcm_dcs_write_seq_static(ctx,0x4E,0x07);
	lcm_dcs_write_seq_static(ctx,0x54,0x08);
	lcm_dcs_write_seq_static(ctx,0x5A,0x09);
	lcm_dcs_write_seq_static(ctx,0x76,0x40);
	lcm_dcs_write_seq_static(ctx,0x77,0x40);
	lcm_dcs_write_seq_static(ctx,0x78,0x40);
	lcm_dcs_write_seq_static(ctx,0x79,0x40);
	lcm_dcs_write_seq_static(ctx,0x49,0x50);
	lcm_dcs_write_seq_static(ctx,0x4A,0x60);
	lcm_dcs_write_seq_static(ctx,0x4F,0x50);
	lcm_dcs_write_seq_static(ctx,0x50,0x60);
	lcm_dcs_write_seq_static(ctx,0x55,0x50);
	lcm_dcs_write_seq_static(ctx,0x56,0x60);
	lcm_dcs_write_seq_static(ctx,0x5B,0x50);
	lcm_dcs_write_seq_static(ctx,0x5C,0x60);
	lcm_dcs_write_seq_static(ctx,0xBE,0x03);
	lcm_dcs_write_seq_static(ctx,0xC0,0x74);
	lcm_dcs_write_seq_static(ctx,0xC1,0x40);
	lcm_dcs_write_seq_static(ctx,0xBF,0x77);
	lcm_dcs_write_seq_static(ctx,0xC2,0x87);
	lcm_dcs_write_seq_static(ctx,0xC6,0x44);
	lcm_dcs_write_seq_static(ctx,0x29,0x00);
	lcm_dcs_write_seq_static(ctx,0xC3,0x87);
	lcm_dcs_write_seq_static(ctx,0xC7,0x70);
	lcm_dcs_write_seq_static(ctx,0xFF,0x22);
	lcm_dcs_write_seq_static(ctx,0x05,0x10);
	lcm_dcs_write_seq_static(ctx,0x08,0x10);
	lcm_dcs_write_seq_static(ctx,0xFF,0x20);
	lcm_dcs_write_seq_static(ctx,0x25,0x12);
	lcm_dcs_write_seq_static(ctx,0xFF,0x28);
	lcm_dcs_write_seq_static(ctx,0x3D,0x31);
	lcm_dcs_write_seq_static(ctx,0x3E,0x31);
	lcm_dcs_write_seq_static(ctx,0x3F,0x5b);
	lcm_dcs_write_seq_static(ctx,0x40,0x5b);
	lcm_dcs_write_seq_static(ctx,0x45,0x3C);
	lcm_dcs_write_seq_static(ctx,0x46,0x3C);
	lcm_dcs_write_seq_static(ctx,0x47,0x55);
	lcm_dcs_write_seq_static(ctx,0x48,0x55);
	lcm_dcs_write_seq_static(ctx,0x5A,0x90);
	lcm_dcs_write_seq_static(ctx,0x5B,0x98);
	//lcm_dcs_write_seq_static(ctx,0x62,0xd7);
	//lcm_dcs_write_seq_static(ctx,0x63,0xd7);
	lcm_dcs_write_seq_static(ctx,0xFF,0x20);
	lcm_dcs_write_seq_static(ctx,0x7E,0x01);
	lcm_dcs_write_seq_static(ctx,0x7F,0x00);
	lcm_dcs_write_seq_static(ctx,0x80,0x04);
	lcm_dcs_write_seq_static(ctx,0x81,0x00);
	lcm_dcs_write_seq_static(ctx,0x82,0x00);
	lcm_dcs_write_seq_static(ctx,0x83,0x64);
	lcm_dcs_write_seq_static(ctx,0x84,0x64);
	lcm_dcs_write_seq_static(ctx,0x85,0x2D);
	lcm_dcs_write_seq_static(ctx,0x86,0xC0);
	lcm_dcs_write_seq_static(ctx,0x87,0x2D);
	lcm_dcs_write_seq_static(ctx,0x88,0xC0);
	lcm_dcs_write_seq_static(ctx,0x8A,0x0A);
	lcm_dcs_write_seq_static(ctx,0x8B,0x0A);
	lcm_dcs_write_seq_static(ctx,0xFF,0x23);
	lcm_dcs_write_seq_static(ctx,0x29,0x03);
	lcm_dcs_write_seq_static(ctx,0x01,0x00,0x2E,0x00,0x32,0x00,0x4E,0x00,0x6C,0x00,0x83,0x00,0x96,0x00,0xAA,0x00,0xB9);
	lcm_dcs_write_seq_static(ctx,0x02,0x00,0xC8,0x00,0xF9,0x01,0x1B,0x01,0x51,0x01,0x7D,0x01,0xC4,0x02,0x06,0x02,0x08);
	lcm_dcs_write_seq_static(ctx,0x03,0x02,0x4C,0x02,0xA0,0x02,0xD4,0x03,0x16,0x03,0x3F,0x03,0x70,0x03,0x7E,0x03,0x8D);
	lcm_dcs_write_seq_static(ctx,0x04,0x03,0x9D,0x03,0xAF,0x03,0xC3,0x03,0xD9,0x03,0xF2,0x03,0xFF);
	lcm_dcs_write_seq_static(ctx,0x0D,0x00,0x2E,0x00,0x32,0x00,0x4E,0x00,0x6C,0x00,0x83,0x00,0x96,0x00,0xAA,0x00,0xB9);
	lcm_dcs_write_seq_static(ctx,0x0E,0x00,0xC8,0x00,0xF9,0x01,0x1B,0x01,0x51,0x01,0x7D,0x01,0xC4,0x02,0x06,0x02,0x08);
	lcm_dcs_write_seq_static(ctx,0x0F,0x02,0x4C,0x02,0xA0,0x02,0xD4,0x03,0x16,0x03,0x3F,0x03,0x70,0x03,0x7E,0x03,0x8D);
	lcm_dcs_write_seq_static(ctx,0x10,0x03,0x9D,0x03,0xAF,0x03,0xC3,0x03,0xD9,0x03,0xF2,0x03,0xFF);
	lcm_dcs_write_seq_static(ctx,0x2D,0x65);
	lcm_dcs_write_seq_static(ctx,0x2E,0x00);
	lcm_dcs_write_seq_static(ctx,0x32,0x02);
	lcm_dcs_write_seq_static(ctx,0x33,0x16);
	lcm_dcs_write_seq_static(ctx,0xFF,0x10);
	lcm_dcs_write_seq_static(ctx,0x71,0x11,0x46);
	lcm_dcs_write_seq_static(ctx,0xFF,0x10);
	lcm_dcs_write_seq_static(ctx,0x51,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0x53,0x2c);
	lcm_dcs_write_seq_static(ctx,0x55,0x00);
	lcm_dcs_write_seq_static(ctx,0x36,0x08);
	lcm_dcs_write_seq_static(ctx,0x69,0x00);
	lcm_dcs_write_seq_static(ctx,0x35,0x00);
	lcm_dcs_write_seq_static(ctx,0xBA,0x03);
	lcm_dcs_write_seq_static(ctx,0xFF,0x22);
	lcm_dcs_write_seq_static(ctx,0xEE,0x00);
	lcm_dcs_write_seq_static(ctx,0xEF,0x01);
	lcm_dcs_write_seq_static(ctx,0xFF,0x24);
	lcm_dcs_write_seq_static(ctx,0x7D,0x55);
	lcm_dcs_write_seq_static(ctx,0xFF,0x66,0x99,0x55);
	lcm_dcs_write_seq_static(ctx,0xFF,0x10);
	lcm_dcs_write_seq_static(ctx,0x11);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx,0x29);
	lcm_dcs_write_seq_static(ctx,0xFE,0x66,0x99,0x55);
	mdelay(120);

}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

#if defined(TPS65132_BIAS_SUPPORT)
extern int tps65132_write_byte(unsigned char cmd, unsigned char writeData);
#endif
static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("gc7202m %s\n", __func__);
	
	if (g_tp_gesture_flag != 1) {
		//不开TP手势唤醒，VSN、VSP掉电
		lcm_dcs_write_seq_static(ctx,0xFF,0x55,0xAA,0x66),
		lcm_dcs_write_seq_static(ctx,0xFE,0x55,0xAA,0x66),
		lcm_dcs_write_seq_static(ctx,0xFF,0x22);
		lcm_dcs_write_seq_static(ctx,0xE4,0x00);
		lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
		lcm_dcs_write_seq_static(ctx,0x68,0x00);
		lcm_dcs_write_seq_static(ctx,0x2A,0x00);
		lcm_dcs_write_seq_static(ctx,0x28,0x00);
		lcm_dcs_write_seq_static(ctx,0x29,0x00);
		lcm_dcs_write_seq_static(ctx,0x2B,0x00);
		lcm_dcs_write_seq_static(ctx,0xFF,0x20);
		lcm_dcs_write_seq_static(ctx,0x4A,0x01);
		lcm_dcs_write_seq_static(ctx,0x48,0x10);
		lcm_dcs_write_seq_static(ctx,0x49,0x00);
		lcm_dcs_write_seq_static(ctx,0xFF,0x10);
		lcm_dcs_write_seq_static(ctx,0x28,0x00);
		mdelay(50);
		lcm_dcs_write_seq_static(ctx,0x10,0x00);
		mdelay(50);
		lcm_dcs_write_seq_static(ctx,0x4F,0x01);
		mdelay(5);
	}else{
		//开TP手势唤醒，VSN、VSP不掉电
		lcm_dcs_write_seq_static(ctx,0xFF,0x55,0xAA,0x66);
		lcm_dcs_write_seq_static(ctx,0xFE,0x55,0xAA,0x66);
		lcm_dcs_write_seq_static(ctx,0xFF,0x22);
		lcm_dcs_write_seq_static(ctx,0xE4,0x00);
		lcm_dcs_write_seq_static(ctx,0xFF,0xB3);
		lcm_dcs_write_seq_static(ctx,0x68,0x00);
		lcm_dcs_write_seq_static(ctx,0x2A,0x00);
		lcm_dcs_write_seq_static(ctx,0x28,0x00);
		lcm_dcs_write_seq_static(ctx,0x29,0x00);
		lcm_dcs_write_seq_static(ctx,0x2B,0x00);
		lcm_dcs_write_seq_static(ctx,0xFF,0x28);
		lcm_dcs_write_seq_static(ctx,0x2F,0x00);
		lcm_dcs_write_seq_static(ctx,0xFF,0x20);
		lcm_dcs_write_seq_static(ctx,0x4A,0x01);
		lcm_dcs_write_seq_static(ctx,0x48,0x10);
		lcm_dcs_write_seq_static(ctx,0x49,0x00);
		lcm_dcs_write_seq_static(ctx,0xFF,0x10);
		lcm_dcs_write_seq_static(ctx,0x28,0x00);
		mdelay(50);
		lcm_dcs_write_seq_static(ctx,0x10,0x00);
		lcm_dcs_write_seq_static(ctx,0xFE,0x66,0x99,0x55);
		mdelay(120); //120ms
	}
	if (!ctx->prepared)
		return 0;
	lcm_dcs_write_seq_static(ctx, 0xac, 0x0a, 0x00);

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(10);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(120);

	//modified by KLJ For bias datasheet power off to 5.2v 20240401 start
#if defined(TPS65132_BIAS_SUPPORT)
	tps65132_write_byte(0x0, 0x0E);
	tps65132_write_byte(0x1, 0x0E);
#endif
	msleep(10);
	//modified by KLJ For bias datasheet power off to 5.2v 20240401 end

	ctx->error = 0;
	ctx->prepared = false;
/* prize added by KLJ, prize tp gesture function, 20240418-start */
	if (g_tp_gesture_flag != 1) {
	/*ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);*/


		pr_info("%s gesture wakeup is disable\n", __func__);
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	mdelay(10);//modified by KLJ For FAE suggestion delay 10ms;

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

    }else{
		pr_info("%s gesture wakeup is enable\n", __func__);
	}
/* prize added by KLJ, prize tp gesture function, 20240418-start */

	//modified by KLJ For FAE suggestion  after suspend lcm rst keep high start
	//gpiod_set_value(ctx->reset_gpio, 1);
	//devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	//modified by KLJ For FAE suggestion  after suspend lcm rst keep high end

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("gc7272m %s\n", __func__);
	if (ctx->prepared)
		return 0;

//prize-add gpio ldo1.8v-pengzhipeng-20220514-start
	ctx->ldo18_gpio = devm_gpiod_get(ctx->dev, "ldo18", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->ldo18_gpio)) {
		dev_info(ctx->dev, "cannot get ldo18-gpios %ld\n",
			PTR_ERR(ctx->ldo18_gpio));
		return PTR_ERR(ctx->ldo18_gpio);
	}
	gpiod_set_value(ctx->ldo18_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->ldo18_gpio);
	udelay(1000);
//prize-add gpio ldo1.8v-pengzhipeng-20220514-end
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	udelay(2000);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	udelay(2000);

#ifndef BYPASSI2C
	_lcm_i2c_write_bytes(0x0, 0x14);
	_lcm_i2c_write_bytes(0x1, 0x14);
#endif

#if defined(TPS65132_BIAS_SUPPORT)
	tps65132_write_byte(0x0, 0x14);
	tps65132_write_byte(0x1, 0x14);
#endif

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;


#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	pr_info("%s-\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s+\n", __func__);
	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

/*vdo time config*/
#define FRAME_WIDTH                 720
#define FRAME_HEIGHT                1440

#define HAC (720)
#define VAC (1440)
/*120HZ D:1000*/
#define HFP (32)
#define HSA (4)
#define HBP (32)
#define VFP_60 (970)
#define VFP_90 (150)
//#define VFP_120 (32)
#define VSA (6)
#define VBP (38)
#define DATA_RATE					752

#define PHYSICAL_WIDTH              64500 //70200
#define PHYSICAL_HEIGHT             128200 //152100

/* DSC RELATED */
#define DSC_ENABLE                  1
#define DSC_VER                     17
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 34
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         8
#define DSC_DSC_LINE_BUF_DEPTH      9
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           128

#define DSC_SLICE_HEIGHT            20
#define DSC_SLICE_WIDTH             540
#define DSC_CHUNK_SIZE              540
#define DSC_XMIT_DELAY              512
#define DSC_DEC_DELAY               549
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      469
#define DSC_DECREMENT_INTERVAL      7
#define DSC_LINE_BPG_OFFSET         13
#define DSC_NFL_BPG_OFFSET          1402
#define DSC_SLICE_BPG_OFFSET        1302
#define DSC_INITIAL_OFFSET          6144
#define DSC_FINAL_OFFSET            4336
#define DSC_FLATNESS_MINQP          3
#define DSC_FLATNESS_MAXQP          12
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    11
#define DSC_RC_QUANT_INCR_LIMIT1    11
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3

#define PCLK_IN_KHZ_60HZ \
    ((HAC+HFP+HSA+HBP)*(VAC+VFP_60+VSA+VBP)*(60)/1000)
#define PCLK_IN_KHZ_90HZ \
    ((HAC+HFP+HSA+HBP)*(VAC+VFP_90+VSA+VBP)*(90)/1000)
//#define PCLK_IN_KHZ_120HZ \
//    ((HAC+HFP+HSA+HBP)*(VAC+VFP_120+VSA+VBP)*(120)/1000)

static const struct drm_display_mode default_mode = {
	.clock = PCLK_IN_KHZ_60HZ,//PCLK_IN_KHZ,  //339264
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,//HFP
	.hsync_end = HAC + HFP + HSA,//HSA
	.htotal = HAC + HFP + HSA + HBP,//HBP
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_60,//VFP
	.vsync_end = VAC + VFP_60 + VSA,//VSA
	.vtotal = VAC + VFP_60 + VSA + VBP,//VBP
};

static const struct drm_display_mode performance_mode_90hz = {
	.clock = PCLK_IN_KHZ_90HZ,//PCLK2_IN_KHZ,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,//HFP
	.hsync_end = HAC + HFP + HSA,//HSA
	.htotal = HAC + HFP + HSA + HBP,//HBP
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_90,//VFP
	.vsync_end = VAC + VFP_90 + VSA,//VSA
	.vtotal = VAC + VFP_90 + VSA + VBP,//VBP
};
#if 0
static const struct drm_display_mode performance_mode_120hz = {
	.clock = PCLK_IN_KHZ_120HZ,//PCLK3_IN_KHZ,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,//HFP
	.hsync_end = HAC + HFP + HSA,//HSA
	.htotal = HAC + HFP + HSA + HBP,//HBP
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_120,//VFP
	.vsync_end = VAC + VFP_120 + VSA,//VSA
	.vtotal = VAC + VFP_120 + VSA + VBP,//VBP
};
#endif
#if defined(CONFIG_MTK_PANEL_EXT)
#if 0
static struct mtk_panel_params ext_params_120 = {
	//.vfp_low_power = 2540,//60hz
	.pll_clk = DATA_RATE/2,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.data_rate = DATA_RATE,
	//.is_cphy = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = DATA_RATE,
		.hfp = HFP,
		.vfp = VFP_120,
	},

	//modified by KLJ for LP rate start
	.phy_timcon = {
		.lpx = 8,
	},
	//modified by KLJ for LP rate end
};
#endif
static struct mtk_panel_params ext_params_90 = {
	//.vfp_low_power = 2540,//60hz
	.pll_clk = DATA_RATE/2,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.data_rate = DATA_RATE,
	.dyn = {
		.switch_en = 1,
		.data_rate = DATA_RATE,
		.hfp = HFP,
		.vfp = VFP_90,
	},
};

static struct mtk_panel_params ext_params_60 = {
	//.vfp_low_power = 2540,//60hz
	.pll_clk = DATA_RATE/2,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = DATA_RATE,
		.hfp = HFP,
		.vfp = VFP_60,
	},
};

struct drm_display_mode *get_mode_by_id_hfp(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}
static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id_hfp(connector, mode);

	if (m == NULL) {
		pr_err("%s:%d invalid display_mode\n", __func__, __LINE__);
		return -1;
	}

	if (drm_mode_vrefresh(m) == 60) {
printk("drm_mode_vrefresh  60hz\n");
		ext->params = &ext_params_60;
#if HFP_SUPPORT
		current_fps = 60;
#endif
	} else if (drm_mode_vrefresh(m)== 90) {
printk("drm_mode_vrefresh  90hz\n");	
		ext->params = &ext_params_90;
#if HFP_SUPPORT
		current_fps = 90;
#endif
	} else
		ret = 1;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x30, 0x00, 0x00};//modified by KLJ for lcd ata test
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0xDA, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
	    data[1] == id[1] &&
	    data[2] == id[2])
		return 1;

	pr_info("ATA expect data is %x %x %x\n", id[0], id[1], id[2]);

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	char bl_tb0[] = {0x51, 0xFF};

	bl_tb0[1] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}


static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;
	//struct drm_display_mode *mode3;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 default_mode.hdisplay, default_mode.vdisplay,
			 drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode2 = drm_mode_duplicate(connector->dev, &performance_mode_90hz);
	if (!mode2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_90hz.hdisplay, performance_mode_90hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_90hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);

#if 0
	mode3 = drm_mode_duplicate(connector->dev, &performance_mode_120hz);
	if (!mode3) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_120hz.hdisplay, performance_mode_120hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_120hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode3);
#endif
	connector->display_info.width_mm = 69;
	connector->display_info.height_mm = 157;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	pr_info("gc7272m %s+\n", __func__);

#ifndef BYPASSI2C
	_lcm_i2c_init();
#endif

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(dev, "%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(dev, "%s: cannot get bias-neg 1 %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);

	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params_60, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_info("%s-\n", __func__);

//drv add by yubo for hardware_info 20240416 start
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
    strcpy(current_lcm_info.chip,"GC7202M");
    strcpy(current_lcm_info.vendor,"longyu");
    sprintf(current_lcm_info.id,"0x%02x",0x01);
    strcpy(current_lcm_info.more,"720*1440");
#endif
//drv add by yubo for hardware_info 20240416 end

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif
	pr_info("%s+\n", __func__);
	//i2c_del_driver(&_lcm_i2c_driver);
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "hdplus,gc7202m,longyu", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-longyu-gc7202m-hdplus-dsi-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("txd td4160 VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
