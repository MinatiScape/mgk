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
#include <linux/fb.h>

#include <linux/of_gpio.h>
#include <linux/gpio.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

//drv-mod by shenwenbin for hardware info 20240508 start
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/prize/hardware_info/hardware_info.h"
extern struct hardware_info current_lcm_info;
#endif
//drv-mod by shenwenbin for hardware info 20240508 end

/*drv-add gesture function-shenwenbin-20240509-start */
//#include "../../../misc/mediatek/prize/prize_common_node/prize_common_node.h"
extern unsigned int tp_gesture_flag;
/*drv-add gesture function-shenwenbin-20240509-end */

//drv-mod by shenwenbin for bias bringup 20240410 start
#include "mtk_drm_gateic.h"
extern int mtk_panel_i2c_write_bytes(unsigned char addr, unsigned char value);
//drv-mod by shenwenbin for bias bringup 20240410 end

extern unsigned int ilitek_tp_rst;

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;

	bool prepared;
	bool enabled;

	int error;
};

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
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
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void lcm_panel_init(struct lcm *ctx)
{

	pr_info("ILI9883C %s\n", __func__);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}

	if (!tp_gesture_flag){
    	gpio_set_value(ilitek_tp_rst, 0);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(1);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(10);
	gpiod_set_value(ctx->reset_gpio, 1);
    gpio_set_value(ilitek_tp_rst, 1);
	msleep(20);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

    lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x83,0x01);
    //STV Setting
    lcm_dcs_write_seq_static(ctx,0x00,0x4E);
    lcm_dcs_write_seq_static(ctx,0x01,0x35);
    lcm_dcs_write_seq_static(ctx,0x02,0x00);
    lcm_dcs_write_seq_static(ctx,0x03,0x00);
    lcm_dcs_write_seq_static(ctx,0x04,0xCA);
    lcm_dcs_write_seq_static(ctx,0x05,0x15);
    lcm_dcs_write_seq_static(ctx,0x06,0x00);
    lcm_dcs_write_seq_static(ctx,0x07,0x00);
    //CLK Setting
    lcm_dcs_write_seq_static(ctx,0x08,0x89);
    lcm_dcs_write_seq_static(ctx,0x09,0x0A);
    lcm_dcs_write_seq_static(ctx,0x0A,0xB4);
    lcm_dcs_write_seq_static(ctx,0x0C,0x00);
    lcm_dcs_write_seq_static(ctx,0x0D,0x00);
    lcm_dcs_write_seq_static(ctx,0x0E,0x00);
    lcm_dcs_write_seq_static(ctx,0x0F,0x00);
    lcm_dcs_write_seq_static(ctx,0x0B,0x00);
    lcm_dcs_write_seq_static(ctx,0x16,0x89);
    lcm_dcs_write_seq_static(ctx,0x17,0x0A);
    lcm_dcs_write_seq_static(ctx,0x18,0x34);
    lcm_dcs_write_seq_static(ctx,0x1A,0x00);
    lcm_dcs_write_seq_static(ctx,0x1B,0x00);
    lcm_dcs_write_seq_static(ctx,0x1C,0x00);
    lcm_dcs_write_seq_static(ctx,0x1D,0x00);
    lcm_dcs_write_seq_static(ctx,0x19,0x00);
    //STCH Setting
    lcm_dcs_write_seq_static(ctx,0x28,0x4E);
    lcm_dcs_write_seq_static(ctx,0x2A,0x4E);
    lcm_dcs_write_seq_static(ctx,0x29,0x95);
    lcm_dcs_write_seq_static(ctx,0x2B,0x95);
    lcm_dcs_write_seq_static(ctx,0xEE,0x10);
    lcm_dcs_write_seq_static(ctx,0xEF,0x00);
    lcm_dcs_write_seq_static(ctx,0xF0,0x00);

    //FW_R
    lcm_dcs_write_seq_static(ctx,0x31,0x02);     // R[01]_VGL (VGLO)
    lcm_dcs_write_seq_static(ctx,0x32,0x02);     // R[02]_VGL (VGLO)
    lcm_dcs_write_seq_static(ctx,0x33,0x02);     // R[03]_VDS (VGLO)  22
    lcm_dcs_write_seq_static(ctx,0x34,0x02);     // R[04]_VDS (VGLO)  22
    lcm_dcs_write_seq_static(ctx,0x35,0x22);     // R[05]_VSD (STCH_1)   02
    lcm_dcs_write_seq_static(ctx,0x36,0x22);     // R[06]_VSD (STCH_1)   02
    lcm_dcs_write_seq_static(ctx,0x37,0x23);     // R[07]_TPE (STCH_2)
    lcm_dcs_write_seq_static(ctx,0x38,0x02);     // R[08]_VGL (VGLO)
    lcm_dcs_write_seq_static(ctx,0x39,0x02);     // R[09]_VGL (VGLO)
    lcm_dcs_write_seq_static(ctx,0x3A,0x02);     // R[10]_VGL (VGLO)
    lcm_dcs_write_seq_static(ctx,0x3B,0x23);     // R[11]_RESET (STCH_2)
    lcm_dcs_write_seq_static(ctx,0x3C,0x10);     // R[12]_CLK1L (CLK_A_1)
    lcm_dcs_write_seq_static(ctx,0x3D,0x12);     // R[13]_CLK2L (CLK_A_3)
    lcm_dcs_write_seq_static(ctx,0x3E,0x14);     // R[14]_CLK3L (CLK_A_5)
    lcm_dcs_write_seq_static(ctx,0x3F,0x16);     // R[15]_CLK4L (CLK_A_7)
    lcm_dcs_write_seq_static(ctx,0x40,0x18);     // R[16]_CLK5L (CLK_B_1)
    lcm_dcs_write_seq_static(ctx,0x41,0x1A);     // R[17]_CLK6L (CLK_B_3)
    lcm_dcs_write_seq_static(ctx,0x42,0x0C);     // R[18]_STV3L (STV_B_1)
    lcm_dcs_write_seq_static(ctx,0x43,0x0A);     // R[19]_STV2L (STV_A_3)
    lcm_dcs_write_seq_static(ctx,0x44,0x08);     // R[20]_STV1L (STV_A_1)
    lcm_dcs_write_seq_static(ctx,0x45,0x07);     // R[21]_NC (Hi-Z)
    lcm_dcs_write_seq_static(ctx,0x46,0x07);     // R[22]_NC (Hi-Z)
    //FW_L
    lcm_dcs_write_seq_static(ctx,0x47,0x02);     // L[01]_VGL (VGLO)
    lcm_dcs_write_seq_static(ctx,0x48,0x02);     // L[02]_VGL (VGLO)
    lcm_dcs_write_seq_static(ctx,0x49,0x02);     // L[03]_VDS (VGLO)   22
    lcm_dcs_write_seq_static(ctx,0x4A,0x02);     // L[04]_VDS (VGLO)  22
    lcm_dcs_write_seq_static(ctx,0x4B,0x22);     // L[05]_VSD (STCH_1)     02
    lcm_dcs_write_seq_static(ctx,0x4C,0x22);     // L[06]_VSD (STCH_1)    02
    lcm_dcs_write_seq_static(ctx,0x4D,0x23);     // L[07]_TPE (STCH_2)
    lcm_dcs_write_seq_static(ctx,0x4E,0x02);     // L[08]_VGL (VGLO)
    lcm_dcs_write_seq_static(ctx,0x4F,0x02);     // L[09]_VGL (VGLO)
    lcm_dcs_write_seq_static(ctx,0x50,0x02);     // L[10]_VGL (VGLO)
    lcm_dcs_write_seq_static(ctx,0x51,0x23);     // L[11]_RESET (STCH_2)
    lcm_dcs_write_seq_static(ctx,0x52,0x11);     // L[12]_CLK1R (CLK_A_2)
    lcm_dcs_write_seq_static(ctx,0x53,0x13);     // L[13]_CLK2R (CLK_A_4)
    lcm_dcs_write_seq_static(ctx,0x54,0x15);     // L[14]_CLK3R (CLK_A_6)
    lcm_dcs_write_seq_static(ctx,0x55,0x17);     // L[15]_CLK4R (CLK_A_8)
    lcm_dcs_write_seq_static(ctx,0x56,0x19);     // L[16]_CLK5R (CLK_B_2)
    lcm_dcs_write_seq_static(ctx,0x57,0x1B);     // L[17]_CLK6R (CLK_B_4)
    lcm_dcs_write_seq_static(ctx,0x58,0x0D);     // L[18]_STV3R (STV_B_2)
    lcm_dcs_write_seq_static(ctx,0x59,0x0B);     // L[19]_STV2R (STV_A_4)
    lcm_dcs_write_seq_static(ctx,0x5A,0x09);     // L[20]_STV1R (STV_A_2)
    lcm_dcs_write_seq_static(ctx,0x5B,0x07);     // L[21]_NC (Hi-Z)
    lcm_dcs_write_seq_static(ctx,0x5C,0x07);     // L[22]_NC (Hi-Z)

    lcm_dcs_write_seq_static(ctx,0xC3,0x00);   

    lcm_dcs_write_seq_static(ctx,0xD1,0x00);    
    lcm_dcs_write_seq_static(ctx,0xE6,0x12);
    lcm_dcs_write_seq_static(ctx,0xE7,0x54);
    lcm_dcs_write_seq_static(ctx,0xD5,0x04);
    lcm_dcs_write_seq_static(ctx,0xD3,0x00);      //20240305

    lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x83,0x02);
    lcm_dcs_write_seq_static(ctx,0x06,0x57);
    lcm_dcs_write_seq_static(ctx,0x0A,0x5B);
    lcm_dcs_write_seq_static(ctx,0x0C,0x00);
    lcm_dcs_write_seq_static(ctx,0x0D,0x22);
    lcm_dcs_write_seq_static(ctx,0x0E,0x6E);
    lcm_dcs_write_seq_static(ctx,0x39,0x05);
    lcm_dcs_write_seq_static(ctx,0x3A,0x22);
    lcm_dcs_write_seq_static(ctx,0x3B,0x6E);
    lcm_dcs_write_seq_static(ctx,0x3C,0xA8);
    lcm_dcs_write_seq_static(ctx,0xF0,0x01);
    lcm_dcs_write_seq_static(ctx,0xF1,0x6F);
    // lcm_dcs_write_seq_static(ctx,0x29,0x38);     
    // lcm_dcs_write_seq_static(ctx,0x2A,0x38);    
    lcm_dcs_write_seq_static(ctx,0x48,0x01);
    lcm_dcs_write_seq_static(ctx,0x44,0x68);

    lcm_dcs_write_seq_static(ctx,0x40,0x50);  //SDT  2us  


    lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x83,0x03);
    lcm_dcs_write_seq_static(ctx,0x20,0x01); 
    lcm_dcs_write_seq_static(ctx,0x22,0xFA);    

    lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x83,0x05);
    lcm_dcs_write_seq_static(ctx,0x03,0x00);        //VCOM1 01
    lcm_dcs_write_seq_static(ctx,0x04,0xDC);       //VCOM1  13
    lcm_dcs_write_seq_static(ctx,0x69,0x9C);      
    lcm_dcs_write_seq_static(ctx,0x6A,0x9C);
    lcm_dcs_write_seq_static(ctx,0x6D,0xA1);        //VGHO  15V
    lcm_dcs_write_seq_static(ctx,0x73,0xA7);        //VGH      16V
    lcm_dcs_write_seq_static(ctx,0x79,0x8D);       //VGLO    11V
    lcm_dcs_write_seq_static(ctx,0x7F,0x7F);        //VGL      12V
    lcm_dcs_write_seq_static(ctx,0x61,0x11);        //12
    //lcm_dcs_write_seq_static(ctx,0x5D,0x5D);
    lcm_dcs_write_seq_static(ctx,0x68,0x3E);
    lcm_dcs_write_seq_static(ctx,0x66,0x33);
    // lcm_dcs_write_seq_static(ctx,0x65,0x07);

    //lcm_dcs_write_seq_static(ctx,0xA5,0x7B);      // TS Sequence Enable
    //lcm_dcs_write_seq_static(ctx,0x4F,0x0A);      // TS Option Select

    //lcm_dcs_write_seq_static(ctx,0x6E,0xA1);      // VGHO_C = 15V
    //lcm_dcs_write_seq_static(ctx,0x6F,0x8D);     // VGHO_L = 14V
    //lcm_dcs_write_seq_static(ctx,0x70,0x8D);     // VGHO_M = 14V
    //lcm_dcs_write_seq_static(ctx,0x71,0x8D);     // VGHO_H = 14V
    //lcm_dcs_write_seq_static(ctx,0x74,0xA7);     // VGH_C = 16V
    //lcm_dcs_write_seq_static(ctx,0x75,0x93);     // VGH_L = 15V
    //lcm_dcs_write_seq_static(ctx,0x76,0x93);     // VGH_M = 15V
    //lcm_dcs_write_seq_static(ctx,0x77,0x93);     // VGH_H = 15V

    lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x83,0x06);
    lcm_dcs_write_seq_static(ctx,0xD9,0x1F);
    lcm_dcs_write_seq_static(ctx,0xC0,0x4C);
    lcm_dcs_write_seq_static(ctx,0xC1,0x16);
    lcm_dcs_write_seq_static(ctx,0x93,0x50);
    lcm_dcs_write_seq_static(ctx,0x0A,0x50);    

    //lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x83,0x07);
    //lcm_dcs_write_seq_static(ctx,0xC0,0x01);     // TS Enable
    //lcm_dcs_write_seq_static(ctx,0xCB,0xB5);     // TS_TH0  (0 C)
    //lcm_dcs_write_seq_static(ctx,0xCC,0xAC);     // TS_TH1  (9 C)
    //lcm_dcs_write_seq_static(ctx,0xCD,0x9D);     // TS_TH2 (25 C)
    //lcm_dcs_write_seq_static(ctx,0xCE,0x7E);     // TS_TH3  (56 C)
    //lcm_dcs_write_seq_static(ctx,0xD1,0x20);    // TS_Ready for TP 

    lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x83,0x08);
    lcm_dcs_write_seq_static(ctx,0xE0,0x00,0x24,0x33,0x59,0x83,0x50,0xBC,0xEE,0x16,0x48,0x95,0x6F,0xAF,0xE3,0x12,0xAA,0x3F,0x6E,0xA4,0xC6,0xFE,0xF0,0x13,0x3F,0x74,0x3F,0x9E,0xD7,0xEC);
    lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x24,0x33,0x59,0x83,0x50,0xBC,0xEE,0x16,0x48,0x95,0x6F,0xAF,0xE3,0x12,0xAA,0x3F,0x6E,0xA4,0xC6,0xFE,0xF0,0x13,0x3F,0x74,0x3F,0x9E,0xD7,0xEC);


    lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x83,0x0A);
    lcm_dcs_write_seq_static(ctx,0xE0,0x01);
    lcm_dcs_write_seq_static(ctx,0xE1,0x0B);
    lcm_dcs_write_seq_static(ctx,0xE2,0x01);

    lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x83,0x0B);
    lcm_dcs_write_seq_static(ctx,0x9A,0x85);
    lcm_dcs_write_seq_static(ctx,0x9B,0x85);
    lcm_dcs_write_seq_static(ctx,0x9C,0x04);
    lcm_dcs_write_seq_static(ctx,0x9D,0x04);
    lcm_dcs_write_seq_static(ctx,0x9E,0x8A);
    lcm_dcs_write_seq_static(ctx,0x9F,0x8A);
    lcm_dcs_write_seq_static(ctx,0xAA,0x22);
    lcm_dcs_write_seq_static(ctx,0xAB,0xE0);

    lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x83,0x0E);
    lcm_dcs_write_seq_static(ctx,0x11,0x4B);       //50
    lcm_dcs_write_seq_static(ctx,0x12,0x02);    
    lcm_dcs_write_seq_static(ctx,0x13,0x14);
    lcm_dcs_write_seq_static(ctx,0x00,0xA0);
    lcm_dcs_write_seq_static(ctx,0xFF,0x98,0x83,0x00);
    lcm_dcs_write_seq_static(ctx,0x35,0x00);
	
	lcm_dcs_write_seq_static(ctx,0x11,0x00);
	msleep(80);
	lcm_dcs_write_seq_static(ctx,0x29,0x00);
	msleep(20);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;
	pr_info("%s\n", __func__);
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}
	pr_info("%s_end\n", __func__);
	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	if (!ctx->prepared)
		return 0;
    //-------> 1:28 10
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);

	ctx->error = 0;
	ctx->prepared = false;

#if 1
    if (tp_gesture_flag){
       	return 0;
    }

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

    gpio_set_value(ilitek_tp_rst, 1);
	msleep(2);

    //drv-mod by shenwenbin for bias powerdown 20240530 start
    mtk_panel_i2c_write_bytes(0x0, 0x0C);
    mtk_panel_i2c_write_bytes(0x1, 0x0C);
    //drv-mod by shenwenbin for bias powerdown 20240530 end
    msleep(20);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	msleep(2);

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
    msleep(2);
#endif

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;
    
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
    //drv-mod by shenwenbin for bias bringup 20240410 start
    mtk_panel_i2c_write_bytes(0x0, 0x12);
    //drv-mod by shenwenbin for bias bringup 20240410 end
	msleep(2);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
    //drv-mod by shenwenbin for bias bringup 20240410 start
    mtk_panel_i2c_write_bytes(0x1, 0x12);
    //drv-mod by shenwenbin for bias bringup 20240410 end
    msleep(2);
	lcm_panel_init(ctx);  //3: LCD reset--->4:init lcd

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	pr_info("%s-\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}
#define HFP (148)
#define HSA (12)
#define HBP (48)
#define HACT (720)
#define VFP (1372)
#define VSA (4)
#define VBP (30)
#define VACT (1612)
#define VFP_90 (366)

static struct drm_display_mode performance_mode_90 = {
	.clock		= 168210,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP_90,
	.vsync_end	= VACT + VFP_90 + VSA,
	.vtotal		= VACT + VFP_90 + VSA + VBP,
};

static struct drm_display_mode default_mode = {
	.clock		= 168043,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params_90 = {
	//.vfp_low_power = 2540,//60hz
	.pll_clk = 550,
	.physical_width_um = 67930,
	.physical_height_um = 152090,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.data_rate = 1100,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
	//.lfr_enable = 1,
	//.lfr_minimum_fps = 60,
    .lcm_index = 0, //drv-mod by shenwenbin for compatible with two panel PQ 20240902
};
static struct mtk_panel_params ext_params_60 = {
	//.vfp_low_power = 2540,//60hz
	.pll_clk = 550,
	.physical_width_um = 67930,
	.physical_height_um = 152090,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.data_rate = 1100,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
	//.lfr_enable = 1,
	//.lfr_minimum_fps = 60,
    .lcm_index = 0, //drv-mod by shenwenbin for compatible with two panel PQ 20240902
};

static struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
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
static int current_fps = 60;

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);


	if (drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params_60;
	else if (drm_mode_vrefresh(m) == 90)
		ext->params = &ext_params_90;
	else
		ret = 1;
	if (!ret)
		current_fps = drm_mode_vrefresh(m);
	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	pr_info("%s success\n", __func__);
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0xFF};

	bl_tb0[1] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
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

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode_60;
	struct drm_display_mode *mode_90;

	mode_60 = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode_60) {
		dev_info(connector->dev->dev, "failed to add mode_60 %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode_60);
	mode_60->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_60);

	mode_90 = drm_mode_duplicate(connector->dev, &performance_mode_90);
	if (!mode_90) {
		dev_info(connector->dev->dev, "failed to add mode_90 %ux%ux@%u\n",
			performance_mode_90.hdisplay, performance_mode_90.vdisplay,
			drm_mode_vrefresh(&performance_mode_90));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_90);
	mode_90->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_90);
	
	connector->display_info.width_mm = 68;
	connector->display_info.height_mm = 152;

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

	pr_info("%s+\n", __func__);
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
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

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

//drv-mod by shenwenbin for hardware info 20240508 start	
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
    strcpy(current_lcm_info.chip,"ili9883c");
    strcpy(current_lcm_info.vendor,"HKC");
    sprintf(current_lcm_info.id,"0x%02x",0x11);
    strcpy(current_lcm_info.more,"720x1612");
#endif
//drv-mod by shenwenbin for hardware info 20240508 end

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
  
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "hkc,ili9883c,vdo,90hz", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-hkc-ili9883c-vdo-dphy-90hz",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("hkc ili9883c VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
