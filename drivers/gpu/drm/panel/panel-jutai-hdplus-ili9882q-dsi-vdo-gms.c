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

//prize added by KLJ, prize tp gesture function 20241016 -start
extern bool ilitek_is_gesture_wakeup_enabled(void);
//prize added by KLJ, prize tp gesture function 20241016 -end

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

	pr_info("ili9882q %s\n", __func__);

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
	mdelay(20);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(20);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(150);//ili9882q at least 10ms
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	
	//GIP timing
	lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x01);
	lcm_dcs_write_seq_static(ctx,0x12,0x00); //CLKA falling EQ off
	lcm_dcs_write_seq_static(ctx,0x20,0x00); //CLKB falling EQ off
	
	//GIP timing
	lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x01);
	lcm_dcs_write_seq_static(ctx,0x00,0x48);		//STVA RISE 
	lcm_dcs_write_seq_static(ctx,0x01,0x33);		//STVA Duty 4H
	lcm_dcs_write_seq_static(ctx,0x02,0x97);		//STVA Rise width				
	lcm_dcs_write_seq_static(ctx,0x03,0x05);		//STVA Fall width
	lcm_dcs_write_seq_static(ctx,0x08,0x86);		//CLKA  RISE
	lcm_dcs_write_seq_static(ctx,0x09,0x01);		//CLKA  FALL	
	lcm_dcs_write_seq_static(ctx,0x0a,0x73);		//CLKA  DUTY 8H 
	lcm_dcs_write_seq_static(ctx,0x0b,0x00);		
	lcm_dcs_write_seq_static(ctx,0x0c,0xff);		//CLKA1 Rise width
	lcm_dcs_write_seq_static(ctx,0x0d,0xff);		//CLKA2 Rise width					
	lcm_dcs_write_seq_static(ctx,0x0e,0x05);		//CLKA1 Fall width 
	lcm_dcs_write_seq_static(ctx,0x0f,0x05);		//CLKA2 Fall width 
	lcm_dcs_write_seq_static(ctx,0x12,0x06);		
	lcm_dcs_write_seq_static(ctx,0x28,0x48);		//VDS/GCH (STCH1 rise)
	lcm_dcs_write_seq_static(ctx,0x29,0x86);		//VDS/GCH (STCH1 fall)
	lcm_dcs_write_seq_static(ctx,0x12,0x00);                 //Gate falling EQ off
	lcm_dcs_write_seq_static(ctx,0x20,0x00);                 //Gate falling EQ off
		
	lcm_dcs_write_seq_static(ctx,0x31,0x07);		//dummy 	
	lcm_dcs_write_seq_static(ctx,0x32,0x02);		//GCL_R
	lcm_dcs_write_seq_static(ctx,0x33,0x00);		//VSD_R
	lcm_dcs_write_seq_static(ctx,0x34,0x15);		//CLK2   
	lcm_dcs_write_seq_static(ctx,0x35,0x17);		//CLK4
	lcm_dcs_write_seq_static(ctx,0x36,0x11);		//CLK6
	lcm_dcs_write_seq_static(ctx,0x37,0x13);		//CLK8
	lcm_dcs_write_seq_static(ctx,0x38,0x2a);		//VGL_R
	lcm_dcs_write_seq_static(ctx,0x39,0x06);		//GCH_R
	lcm_dcs_write_seq_static(ctx,0x3a,0x01);		//VDS_R
	lcm_dcs_write_seq_static(ctx,0x3b,0x09);		//STV2
	lcm_dcs_write_seq_static(ctx,0x3c,0x0b);		//STV4
	lcm_dcs_write_seq_static(ctx,0x3d,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x3e,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x3f,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x40,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x41,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x42,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x43,0x07); 		//dummy
	lcm_dcs_write_seq_static(ctx,0x44,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x45,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x46,0x07);		//dummy
	
	lcm_dcs_write_seq_static(ctx,0x47,0x07);		//dummy		
	lcm_dcs_write_seq_static(ctx,0x48,0x02);		//GCL_L
	lcm_dcs_write_seq_static(ctx,0x49,0x00);		//VSD_L
	lcm_dcs_write_seq_static(ctx,0x4a,0x14);		//CLK1
	lcm_dcs_write_seq_static(ctx,0x4b,0x16);		//CLK3
	lcm_dcs_write_seq_static(ctx,0x4c,0x10);		//CLK5
	lcm_dcs_write_seq_static(ctx,0x4d,0x12);		//CLK7
	lcm_dcs_write_seq_static(ctx,0x4e,0x2a);		//VGL_L
	lcm_dcs_write_seq_static(ctx,0x4f,0x06);		//GCH_L
	lcm_dcs_write_seq_static(ctx,0x50,0x01);		//VDS_L
	lcm_dcs_write_seq_static(ctx,0x51,0x08);		//STV1
	lcm_dcs_write_seq_static(ctx,0x52,0x0a);		//STV3
	lcm_dcs_write_seq_static(ctx,0x53,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x54,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x55,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x56,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x57,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x58,0x07);      	//dummy
	lcm_dcs_write_seq_static(ctx,0x59,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x5a,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x5b,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x5c,0x07);		//dummy
					
	lcm_dcs_write_seq_static(ctx,0x61,0x07);		//GOUTR_01_MUX_BW		dummy
	lcm_dcs_write_seq_static(ctx,0x62,0x02);
	lcm_dcs_write_seq_static(ctx,0x63,0x00);
	lcm_dcs_write_seq_static(ctx,0x64,0x12);      
	lcm_dcs_write_seq_static(ctx,0x65,0x10);      
	lcm_dcs_write_seq_static(ctx,0x66,0x16);
	lcm_dcs_write_seq_static(ctx,0x67,0x14);
	lcm_dcs_write_seq_static(ctx,0x68,0x2a);      
	lcm_dcs_write_seq_static(ctx,0x69,0x06);            
	lcm_dcs_write_seq_static(ctx,0x6a,0x01);
	lcm_dcs_write_seq_static(ctx,0x6b,0x0a);    
	lcm_dcs_write_seq_static(ctx,0x6c,0x08);      
	lcm_dcs_write_seq_static(ctx,0x6d,0x07);      //dummy
	lcm_dcs_write_seq_static(ctx,0x6e,0x07);      //dummy
	lcm_dcs_write_seq_static(ctx,0x6f,0x07);      //dummy
	lcm_dcs_write_seq_static(ctx,0x70,0x07);      //dummy
	lcm_dcs_write_seq_static(ctx,0x71,0x07);      //dummy
	lcm_dcs_write_seq_static(ctx,0x72,0x07);      //dummy
	lcm_dcs_write_seq_static(ctx,0x73,0x07);      //dummy
	lcm_dcs_write_seq_static(ctx,0x74,0x07);      //dummy
	lcm_dcs_write_seq_static(ctx,0x75,0x07);      //dummy
	lcm_dcs_write_seq_static(ctx,0x76,0x07);      //dummy
	
	lcm_dcs_write_seq_static(ctx,0x77,0x07);		//GOUTL_01_MUX_BW		dummy
	lcm_dcs_write_seq_static(ctx,0x78,0x02);      
	lcm_dcs_write_seq_static(ctx,0x79,0x00);      
	lcm_dcs_write_seq_static(ctx,0x7a,0x13);
	lcm_dcs_write_seq_static(ctx,0x7b,0x11);
	lcm_dcs_write_seq_static(ctx,0x7c,0x17);
	lcm_dcs_write_seq_static(ctx,0x7d,0x15); 
	lcm_dcs_write_seq_static(ctx,0x7e,0x2a);
	lcm_dcs_write_seq_static(ctx,0x7f,0x06);       
	lcm_dcs_write_seq_static(ctx,0x80,0x01); 
	lcm_dcs_write_seq_static(ctx,0x81,0x0b);      
	lcm_dcs_write_seq_static(ctx,0x82,0x09);
	lcm_dcs_write_seq_static(ctx,0x83,0x07);		//dummy
	lcm_dcs_write_seq_static(ctx,0x84,0x07);       //dummy
	lcm_dcs_write_seq_static(ctx,0x85,0x07);       //dummy
	lcm_dcs_write_seq_static(ctx,0x86,0x07);       //dummy
	lcm_dcs_write_seq_static(ctx,0x87,0x07);       //dummy
	lcm_dcs_write_seq_static(ctx,0x88,0x07);       //dummy
	lcm_dcs_write_seq_static(ctx,0x89,0x07);       //dummy
	lcm_dcs_write_seq_static(ctx,0x8a,0x07);       //dummy
	lcm_dcs_write_seq_static(ctx,0x8b,0x07);       //dummy
	lcm_dcs_write_seq_static(ctx,0x8c,0x07);       //dummy
	
	lcm_dcs_write_seq_static(ctx,0xa0,0x01);
	lcm_dcs_write_seq_static(ctx,0xa1,0x82);
	lcm_dcs_write_seq_static(ctx,0xa2,0x25);
	lcm_dcs_write_seq_static(ctx,0xa7,0x82);		//00
	lcm_dcs_write_seq_static(ctx,0xa8,0x82);		//00
	lcm_dcs_write_seq_static(ctx,0xa9,0x25);
	lcm_dcs_write_seq_static(ctx,0xaa,0x25);
	lcm_dcs_write_seq_static(ctx,0xb0,0x34);
	lcm_dcs_write_seq_static(ctx,0xb2,0x04);
	lcm_dcs_write_seq_static(ctx,0xb9,0x00);		
	lcm_dcs_write_seq_static(ctx,0xba,0x01);
	lcm_dcs_write_seq_static(ctx,0xc1,0x10);
	lcm_dcs_write_seq_static(ctx,0xd0,0x01);		
	lcm_dcs_write_seq_static(ctx,0xd1,0x00);		
	lcm_dcs_write_seq_static(ctx,0xe2,0x00);		
	lcm_dcs_write_seq_static(ctx,0xe6,0x22);		
	lcm_dcs_write_seq_static(ctx,0xea,0x00);
	lcm_dcs_write_seq_static(ctx,0xFC,0x09);		
	
	// RTN. Internal VBP, Internal VFP
	lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x02);
	lcm_dcs_write_seq_static(ctx,0xF1,0x1C);    // Tcon ESD option
	lcm_dcs_write_seq_static(ctx,0x4B,0x5A);    // line_chopper
	lcm_dcs_write_seq_static(ctx,0x50,0xCA);    // line_chopper
	lcm_dcs_write_seq_static(ctx,0x51,0x00);    // line_chopper
	lcm_dcs_write_seq_static(ctx,0x06,0x60);     // Internal Line Time (RTN)
	lcm_dcs_write_seq_static(ctx,0x0B,0xA0);     // Internal VFP[9]
	lcm_dcs_write_seq_static(ctx,0x0C,0x80);     // Internal VFP[8]
	lcm_dcs_write_seq_static(ctx,0x0D,0x14);     // Internal VBP ( <= 255 )
	lcm_dcs_write_seq_static(ctx,0x0E,0x68);     // Internal VFP  ( <= 1023 )
	lcm_dcs_write_seq_static(ctx,0x4E,0x11);     // SRC BIAS
	lcm_dcs_write_seq_static(ctx,0xF2,0x4A);     // Reset Option
	lcm_dcs_write_seq_static(ctx,0x40,0x45);
	lcm_dcs_write_seq_static(ctx,0x4D,0xCF);
	lcm_dcs_write_seq_static(ctx,0x49,0x02);
	lcm_dcs_write_seq_static(ctx,0x47,0x12);
	lcm_dcs_write_seq_static(ctx,0x43,0x02); 
	
	
	// Power Setting
	lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x05);
	lcm_dcs_write_seq_static(ctx,0x03,0x00);		// Vcom
	lcm_dcs_write_seq_static(ctx,0x04,0xF0);		// Vcom		//D7		
	lcm_dcs_write_seq_static(ctx,0x63,0x97);		// GVDDN = -5.5V
	lcm_dcs_write_seq_static(ctx,0x64,0x97);		// GVDDP = 5.5V
	lcm_dcs_write_seq_static(ctx,0x68,0x65);		// VGHO = 12V
	lcm_dcs_write_seq_static(ctx,0x69,0x6B);		// VGH = 13V
	lcm_dcs_write_seq_static(ctx,0x6A,0xC9);		// VGLO = -14V
	lcm_dcs_write_seq_static(ctx,0x6B,0xBB);		// VGL = -15V
	lcm_dcs_write_seq_static(ctx,0x85,0x37);   
	
	// Resolution
	lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x06);
	lcm_dcs_write_seq_static(ctx,0xD9,0x1F);		// 4Lane
	lcm_dcs_write_seq_static(ctx,0xC0,0xA0);		// NL = 1440
	lcm_dcs_write_seq_static(ctx,0xC1,0x15);		// NL = 1440
	lcm_dcs_write_seq_static(ctx,0x80,0x09);		
	
	// Gamma Register
	lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x08);
	lcm_dcs_write_seq_static(ctx,0xE0,0x00,0x24,0x51,0x77,0xAC,0x54,0xDC,0x03,0x34,0x5C,0xA5,0x9E,0xD5,0x07,0x37,0xAA,0x68,0xA3,0xC7,0xF4,0xFF,0x19,0x49,0x84,0xB6,0x03,0xEC);
	lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x24,0x51,0x77,0xAC,0x54,0xDC,0x03,0x34,0x5C,0xA5,0x9E,0xD5,0x07,0x37,0xAA,0x68,0xA3,0xC7,0xF4,0xFF,0x19,0x49,0x84,0xB6,0x03,0xEC);
	
	// OSC Auto Trim Setting
	lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x0B);
	lcm_dcs_write_seq_static(ctx,0x9A,0x86);
	lcm_dcs_write_seq_static(ctx,0x9B,0x13);
	lcm_dcs_write_seq_static(ctx,0x9C,0x04);
	lcm_dcs_write_seq_static(ctx,0x9D,0x04);
	lcm_dcs_write_seq_static(ctx,0x9E,0x98);
	lcm_dcs_write_seq_static(ctx,0x9F,0x98);
	lcm_dcs_write_seq_static(ctx,0xAA,0x22);
	lcm_dcs_write_seq_static(ctx,0xAB,0xE0);     // AutoTrimType
	lcm_dcs_write_seq_static(ctx,0xAC,0x7F);     // trim_osc_max
	lcm_dcs_write_seq_static(ctx,0xAD,0x3F);     // trim_osc_min
	
	// TP Setting
	lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x0E);
	lcm_dcs_write_seq_static(ctx,0x11,0x10);     //TSVD Rise Poisition
	lcm_dcs_write_seq_static(ctx,0x12,0x08);     // LV mode TSHD Rise position
	lcm_dcs_write_seq_static(ctx,0x13,0x10);     // LV mode TSHD Rise position
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);     // LV mode


	lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x82,0x00);
	lcm_dcs_write_seq_static(ctx,0x35,0x00);

	lcm_dcs_write_seq_static(ctx,0x11);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx,0x29);
	mdelay(20);

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

	pr_info("ili9882q %s\n", __func__);

	if (!ctx->prepared)
		return 0;
	//lcm_dcs_write_seq_static(ctx, 0xac, 0x0a, 0x00);

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
	if (ilitek_is_gesture_wakeup_enabled() == 0) {
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

	pr_info("ili9882q %s\n", __func__);
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
//prize added by KLJ, prize tp gesture function 20241016 -start 
	if (ilitek_is_gesture_wakeup_enabled() == 0) {
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
	}
//prize added by KLJ, prize tp gesture function 20241016 -end 
#ifndef BYPASSI2C
	_lcm_i2c_write_bytes(0x0, 0x12);
	_lcm_i2c_write_bytes(0x1, 0x12);
#endif

#if defined(TPS65132_BIAS_SUPPORT)
	tps65132_write_byte(0x0, 0x12);
	tps65132_write_byte(0x1, 0x12);
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
#define HFP (30)
#define HSA (16)
#define HBP (30)
#define VFP_60 (1280)
#define VFP_90 (360)
//#define VFP_120 (32)
#define VSA (2)
#define VBP (18)
#define DATA_RATE					850

#define PHYSICAL_WIDTH              64500 
#define PHYSICAL_HEIGHT             128200  

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
	.cust_esd_check = 0,
	.esd_check_enable = 0,
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
	.cust_esd_check = 1,
	.esd_check_enable = 1,
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

	return 2;
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

	pr_info("ili9882q %s+\n", __func__);

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
    strcpy(current_lcm_info.chip,"ILI9882Q-GMS");
    strcpy(current_lcm_info.vendor,"jutai");
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
	{ .compatible = "hdplus,ili9882q,jutai,gms", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-jutai-hdplus-ili9882q-dsi-vdo-gms",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("jutai ili9882q VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
