// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
//drv Added the double-click wake up function-pzp-20240817-start
#if IS_ENABLED(CONFIG_PRIZE_COMMON_NODE)
#include "../../../input/touchscreen/focaltech_ft8725_spi/focaltech_common.h"
#endif
//drv Added the double-click wake up function-pzp-20240817-end
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/prize/hardware_info/hardware_info.h"
extern struct hardware_info current_lcm_info;
#endif





extern int mtk_drm_esd_check_status(void);
extern void mtk_drm_esd_set_status(int status);

/*LCM_DEGREE default value*/
#define PROBE_FROM_DTS 0
struct focaltech_lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *amoled_vddi_en_gpio;
	struct gpio_desc *amoled_vdd_en_gpio;
	struct gpio_desc *amoled_vci_en_gpio;
	bool prepared;
	bool enabled;

	unsigned int lcm_degree;
	/* drv modify hbm function start */
	bool hbm_en;
	bool hbm_wait;
	bool hbm_stat;
	bool doze_en; //drv-Fixed the issue of entering aod and TP having touch-pengzhipeng-20230516
	/* drv modify hbm function end */

	int error;
};
struct focaltech_lcm *g_ctx;

#define focaltech_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		focaltech_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define focaltech_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		focaltech_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct focaltech_lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct focaltech_lcm, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int focaltech_dcs_read(struct focaltech_lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret,
			 cmd);
		ctx->error = ret;
	}

	return ret;
}

static void focaltech_panel_get_data(struct focaltech_lcm *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	ret = focaltech_dcs_read(ctx, 0x0A, buffer, 1);
	pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
	dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
		ret, buffer[0] | (buffer[1] << 8));
}
#endif

static void focaltech_dcs_write(struct focaltech_lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;

	if (len > 1)
		udelay(20);

	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}



static void focaltech_panel_init(struct focaltech_lcm *ctx)
{
	pr_info("%s+\n", __func__);
	
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10000, 10001);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10000, 10001);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(50000, 50001);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	
	focaltech_dcs_write_seq_static(ctx,0x00,0x00);
	focaltech_dcs_write_seq_static(ctx,0xFF,0x87,0x25,0x01);
	focaltech_dcs_write_seq_static(ctx,0x00,0x80);
	focaltech_dcs_write_seq_static(ctx,0xFF,0x87,0x25);
	focaltech_dcs_write_seq_static(ctx,0x00,0xA3);
	focaltech_dcs_write_seq_static(ctx,0xB3,0x09,0x68,0x00,0x18); //1080x2408
	//==============================================
	//TCON		
	focaltech_dcs_write_seq_static(ctx,0x00, 0x80);		
	focaltech_dcs_write_seq_static(ctx,0xC0, 0x00 ,0x4A ,0x00 ,0x2F ,0x00 ,0x14);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0x90);		
	focaltech_dcs_write_seq_static(ctx,0xC0, 0x00 ,0x4A ,0x00 ,0x2F ,0x00 ,0x14);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xA0);		
	focaltech_dcs_write_seq_static(ctx,0xC0, 0x00 ,0x97 ,0x00 ,0x2F ,0x00 ,0x14);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xB0);		
	focaltech_dcs_write_seq_static(ctx,0xC0, 0x00 ,0xCD ,0x00 ,0x2F ,0x14);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xC1);		
	focaltech_dcs_write_seq_static(ctx,0xC0, 0x00 ,0x94 ,0x00 ,0x81 ,0x00 ,0x63 ,0x00 ,0xC1);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0x70);		
	focaltech_dcs_write_seq_static(ctx,0xC0, 0x00 ,0xB1 ,0x00 ,0x2F ,0x00 ,0x14);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xA3);		
	focaltech_dcs_write_seq_static(ctx,0xC1, 0x00, 0x67, 0x00 ,0x2F ,0x00 ,0x02);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xB7);		
	focaltech_dcs_write_seq_static(ctx,0xC1, 0x00 ,0x48);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0x7B);		
	focaltech_dcs_write_seq_static(ctx,0xCE, 0xFF ,0xFF);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0x80);		
	focaltech_dcs_write_seq_static(ctx,0xCE, 0x01, 0x81 ,0xFF ,0xFF ,0x00 ,0xC0 ,0x00 ,0xC8 ,0x00 ,0xBC ,0x00 ,0xC8 ,0x00 ,0xD0 ,0x00,0xD0);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0x90);		
	focaltech_dcs_write_seq_static(ctx,0xCE, 0x00 ,0xB7 ,0x0F ,0x7D ,0x00 ,0xB7 ,0x80 ,0xFF ,0xFF ,0x00 ,0x0B ,0xB8 ,0x0F ,0x0F ,0x11);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xA0);		
	focaltech_dcs_write_seq_static(ctx,0xCE, 0x00 ,0x00 ,0x00);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xB0);		
	focaltech_dcs_write_seq_static(ctx,0xCE, 0x22 ,0x00 ,0x00);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xD1);		
	focaltech_dcs_write_seq_static(ctx,0xCE, 0x00 ,0x00 ,0x01 ,0x00 ,0x00 ,0x00 ,0x00);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xE1);		
	focaltech_dcs_write_seq_static(ctx,0xCE, 0x05 ,0x03 ,0xEF ,0x02 ,0xFB ,0x02 ,0xFB ,0x00 ,0x00 ,0x00 ,0x00);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xF1);		
	focaltech_dcs_write_seq_static(ctx,0xCE, 0x24 ,0x13 ,0x12 ,0x01 ,0x0C ,0x00 ,0xEC ,0x01 ,0x0C);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xB0);		
	focaltech_dcs_write_seq_static(ctx,0xCF, 0x00 ,0x00 ,0xB6 ,0xBA);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xB5);		
	focaltech_dcs_write_seq_static(ctx,0xCF, 0x05 ,0x05 ,0x66 ,0x6A);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xC0);		
	focaltech_dcs_write_seq_static(ctx,0xCF, 0x09 ,0x09 ,0x63 ,0x67);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xC5);		
	focaltech_dcs_write_seq_static(ctx,0xCF, 0x09 ,0x09 ,0x69 ,0x6D);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0x60);		
	focaltech_dcs_write_seq_static(ctx,0xCF, 0x00 ,0x00 ,0xCB ,0xCF ,0x05 ,0x05 ,0x43 ,0x47);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0x70);		
	focaltech_dcs_write_seq_static(ctx,0xCF, 0x00 ,0x00 ,0xB7 ,0xBB ,0x05 ,0x05 ,0x67 ,0x6B);		
			
	//Qsync Detect		
	focaltech_dcs_write_seq_static(ctx,0x00, 0xD1);		
	focaltech_dcs_write_seq_static(ctx,0xC1, 0x0B ,0x60 ,0x0F ,0xDC ,0x1B ,0x15 ,0x05 ,0xAF ,0x07 ,0xE5 ,0x0D ,0x81);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xE1);		
	focaltech_dcs_write_seq_static(ctx,0xC1, 0x0F ,0xDC);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0xE4);		
	focaltech_dcs_write_seq_static(ctx,0xCF, 0x09 ,0xFB ,0x09 ,0xFA ,0x09 ,0xFA ,0x09 ,0xFA ,0x09 ,0xFA ,0x09 ,0xFA);		
			
	//OSC		
	focaltech_dcs_write_seq_static(ctx,0x00, 0x80);		
	focaltech_dcs_write_seq_static(ctx,0xC1, 0x44 ,0x44);		
			
	focaltech_dcs_write_seq_static(ctx,0x00, 0x90);		
	focaltech_dcs_write_seq_static(ctx,0xC1, 0x03);		
			
	//Line rate for TP		
	focaltech_dcs_write_seq_static(ctx,0x00, 0xF5);		
	focaltech_dcs_write_seq_static(ctx,0xCF, 0x00);		
			
	//TP Frame rate		
	focaltech_dcs_write_seq_static(ctx,0x00, 0xF6);		
	focaltech_dcs_write_seq_static(ctx,0xCF, 0x78);		
			
	//TCON Frame rate		
	focaltech_dcs_write_seq_static(ctx,0x00, 0xF1);		
	focaltech_dcs_write_seq_static(ctx,0xCF, 0x78);		
			
	//Gram Vesa Line		
	focaltech_dcs_write_seq_static(ctx,0x00, 0x85);		
	focaltech_dcs_write_seq_static(ctx,0xB4, 0x7F);		
							
	// decrese vddi current
	focaltech_dcs_write_seq_static(ctx,0x00, 0x93);
	focaltech_dcs_write_seq_static(ctx,0xC1, 0x82);

	//VDEC ICG
	focaltech_dcs_write_seq_static(ctx,0x00, 0xCC);
	focaltech_dcs_write_seq_static(ctx,0xC1, 0x18);

	//Source Clk Select
	focaltech_dcs_write_seq_static(ctx,0x00, 0x91);
	focaltech_dcs_write_seq_static(ctx,0xC4, 0x08);

	//VDD=1.275V LVDSVDD=1.25V VDD_TP=1.2V
	focaltech_dcs_write_seq_static(ctx,0x00,0x80);
	focaltech_dcs_write_seq_static(ctx,0xC5,0x87,0x59);

	//4 power VDD=1.275V LVDSVDD=1.25V
	focaltech_dcs_write_seq_static(ctx,0x00,0x9E);
	focaltech_dcs_write_seq_static(ctx,0xC5,0x87);

	focaltech_dcs_write_seq_static(ctx,0x00,0x88);
	focaltech_dcs_write_seq_static(ctx,0xC4,0x08);
			
	//========================================
	//STV1 & STV2 Setting
	focaltech_dcs_write_seq_static(ctx,0x00,0x80);
	focaltech_dcs_write_seq_static(ctx,0xC2,0x83,0x01,0x05,0x8c,0x82,0x01,0x05,0x8c);

	//CKV1-3 setting
	focaltech_dcs_write_seq_static(ctx,0x00,0xA0);
	focaltech_dcs_write_seq_static(ctx,0xC2,0x8A,0x07,0x00,0x05,0x8c,0x89,0x08,0x00,0x05,0x8c,0x88,0x09,0x00,0x05,0x8c);

	//CKV4 setting
	focaltech_dcs_write_seq_static(ctx,0x00,0xB0);
	focaltech_dcs_write_seq_static(ctx,0xC2,0x87,0x0A,0x00,0x05,0x8c);

	//CKV width setting
	focaltech_dcs_write_seq_static(ctx,0x00,0xE0);
	focaltech_dcs_write_seq_static(ctx,0xC2,0x33,0x33,0x00,0x00);

	//Rst1 Setting
	focaltech_dcs_write_seq_static(ctx,0x00,0xE8);
	focaltech_dcs_write_seq_static(ctx,0xC2,0x12,0x00,0x0A,0x0A,0x03,0x88,0x00,0x00);

	//GOFF setting
	focaltech_dcs_write_seq_static(ctx,0x00,0xD0);
	focaltech_dcs_write_seq_static(ctx,0xC3,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00);


	focaltech_dcs_write_seq_static(ctx,0x00,0xE0);
	focaltech_dcs_write_seq_static(ctx,0xC3,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00,0x35,0x0A,0x00,0x00);

	//power off enmode setting
	focaltech_dcs_write_seq_static(ctx,0x00,0x80);
	focaltech_dcs_write_seq_static(ctx,0xCB,0xCD,0xCD,0xCC,0x00,0xCD,0xCC,0x00,0xCD,0xCE,0xFE,0xCD,0x00,0xCC,0xCC,0x00,0x00);

	//power on enmode setting
	focaltech_dcs_write_seq_static(ctx,0x00,0x90);
	focaltech_dcs_write_seq_static(ctx,0xCB,0x0C,0x00,0x00,0x00,0x0C,0x0C,0x00,0x00,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

	//skip & powr on1 enmode setting
	focaltech_dcs_write_seq_static(ctx,0x00,0xA0);
	focaltech_dcs_write_seq_static(ctx,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

	//power off blank enmode setting
	focaltech_dcs_write_seq_static(ctx,0x00,0xB0);
	focaltech_dcs_write_seq_static(ctx,0xCB,0x50,0x41,0xA4,0x00);

	//power on blank enmode setting
	focaltech_dcs_write_seq_static(ctx,0x00,0xC0);
	focaltech_dcs_write_seq_static(ctx,0xCB,0x50,0x41,0xA4,0x00);

	//power on blank enmode setting
	focaltech_dcs_write_seq_static(ctx,0x00,0xD5);
	focaltech_dcs_write_seq_static(ctx,0xCB,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00);

	focaltech_dcs_write_seq_static(ctx,0x00,0xE0);
	focaltech_dcs_write_seq_static(ctx,0xCB,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83);

	//panel mapping setting
	//u2d_L
	focaltech_dcs_write_seq_static(ctx,0x00,0x80);
	focaltech_dcs_write_seq_static(ctx,0xCC,0x2C,0x29,0x26,0x26,0x23,0x23,0x18,0x17,0x16,0x1b,0x1a,0x19,0x24,0x07,0x06,0x09);

	focaltech_dcs_write_seq_static(ctx,0x00,0x90);
	focaltech_dcs_write_seq_static(ctx,0xCC,0x08,0x01,0x25,0x25,0x26,0x22,0x03,0x02);
	//u2d_R
	focaltech_dcs_write_seq_static(ctx,0x00,0x80);
	focaltech_dcs_write_seq_static(ctx,0xCD,0x2C,0x29,0x26,0x26,0x23,0x23,0x18,0x17,0x16,0x1b,0x1a,0x19,0x24,0x07,0x06,0x09);

	focaltech_dcs_write_seq_static(ctx,0x00,0x90);
	focaltech_dcs_write_seq_static(ctx,0xCD,0x08,0x01,0x25,0x25,0x26,0x22,0x03,0x02);
	//d2u_L
	focaltech_dcs_write_seq_static(ctx,0x00,0xA0);
	focaltech_dcs_write_seq_static(ctx,0xCC,0x2C,0x29,0x26,0x26,0x23,0x23,0x18,0x17,0x16,0x1b,0x1a,0x19,0x24,0x08,0x09,0x06);

	focaltech_dcs_write_seq_static(ctx,0x00,0xB0);
	focaltech_dcs_write_seq_static(ctx,0xCC,0x07,0x01,0x25,0x25,0x22,0x26,0x02,0x03);

	//d2u_R
	focaltech_dcs_write_seq_static(ctx,0x00,0xA0);
	focaltech_dcs_write_seq_static(ctx,0xCD,0x2C,0x29,0x26,0x26,0x23,0x23,0x18,0x17,0x16,0x1b,0x1a,0x19,0x24,0x08,0x09,0x06);

	focaltech_dcs_write_seq_static(ctx,0x00,0xB0);
	focaltech_dcs_write_seq_static(ctx,0xCD,0x07,0x01,0x25,0x25,0x22,0x26,0x02,0x03);
	//==============================================

	//ckh
	focaltech_dcs_write_seq_static(ctx,0x00,0x86);//Normal
	focaltech_dcs_write_seq_static(ctx,0xC0,0x01,0x02,0x01,0x00,0x10,0x10,0x10,0x03);

	focaltech_dcs_write_seq_static(ctx,0x00,0x96);//IDLE
	focaltech_dcs_write_seq_static(ctx,0xC0,0x01,0x02,0x01,0x01,0x13,0x13,0x13,0x04);

	focaltech_dcs_write_seq_static(ctx,0x00,0xA6);//LPF
	focaltech_dcs_write_seq_static(ctx,0xC0,0x01,0x02,0x01,0x01,0x2A,0x2A,0x2A,0x04);

	focaltech_dcs_write_seq_static(ctx,0x00,0xA3);//PER_DMY
	focaltech_dcs_write_seq_static(ctx,0xCE,0x01,0x02,0x00,0x00,0x10,0x03);

	focaltech_dcs_write_seq_static(ctx,0x00,0xB3);//POS_DMY
	focaltech_dcs_write_seq_static(ctx,0xCE,0x01,0x02,0x00,0x00,0x10,0x03);

	focaltech_dcs_write_seq_static(ctx,0x00,0x76);//FIFO
	focaltech_dcs_write_seq_static(ctx,0xC0,0x01,0x02,0x01,0x01,0x2b,0x2b,0x2b,0x05);

	//CKH_dummy			
	focaltech_dcs_write_seq_static(ctx,0x00,0x82);			
	focaltech_dcs_write_seq_static(ctx,0xa7,0x20,0x00);			
				
	focaltech_dcs_write_seq_static(ctx,0x00,0x8d);			
	focaltech_dcs_write_seq_static(ctx,0xa7,0x02);			
				
	focaltech_dcs_write_seq_static(ctx,0x00,0x8f);			
	focaltech_dcs_write_seq_static(ctx,0xa7,0x01);


	//analog setting
	//vgh=9V
	focaltech_dcs_write_seq_static(ctx,0x00,0x93);
	focaltech_dcs_write_seq_static(ctx,0xC5,0x23);

	focaltech_dcs_write_seq_static(ctx,0x00,0x97);
	focaltech_dcs_write_seq_static(ctx,0xC5,0x23);

	//vgl=-9V
	focaltech_dcs_write_seq_static(ctx,0x00,0x9A);
	focaltech_dcs_write_seq_static(ctx,0xC5,0x23);

	focaltech_dcs_write_seq_static(ctx,0x00,0x9C);
	focaltech_dcs_write_seq_static(ctx,0xC5,0x23);

	//vgho1=8V, vglo1=-8V 
	focaltech_dcs_write_seq_static(ctx,0x00,0xB6);
	focaltech_dcs_write_seq_static(ctx,0xC5,0x19,0x19,0x19,0x19);

	//GVDDP=5.2V, GVDDN=-4.8V
	focaltech_dcs_write_seq_static(ctx,0x00,0x00);
	focaltech_dcs_write_seq_static(ctx,0xD8,0x2F,0x27);

	//VCOM=-0.2V
	//focaltech_dcs_write_seq_static(ctx,0x00,0x00);
	//focaltech_dcs_write_seq_static(ctx,0xD9,0x23,0x23,0x23,0x23);
	//focaltech_dcs_write_seq_static(ctx,0x00,0x06);
	//focaltech_dcs_write_seq_static(ctx,0xD9,0x23,0x23,0x23);

	focaltech_dcs_write_seq_static(ctx,0x00,0x87);
	focaltech_dcs_write_seq_static(ctx,0xC4,0x08,0x08);

	//CKH Rotate	
	//0x03：RGBBGR  0x10：RGBRGB
	focaltech_dcs_write_seq_static(ctx,0x00,0x80);
	focaltech_dcs_write_seq_static(ctx,0xA7,0x03);		

				
	focaltech_dcs_write_seq_static(ctx,0x00,0xA0);
	focaltech_dcs_write_seq_static(ctx,0xC3,0x00,0x01,0x23,0x45,0x21,0x03,0x45,0x00,0x00,0x00,0x21,0x03,0x45,0x01,0x23,0x45);

	focaltech_dcs_write_seq_static(ctx,0x00,0xB1);			
	focaltech_dcs_write_seq_static(ctx,0xF5,0x1F);
	//C0CB[7:4]=PONBLANK=1= 2 FRAME
	//C0CB[3:0]=POFBLANK=1= 2 FRAME
	focaltech_dcs_write_seq_static(ctx,0x00,0xCB);
	focaltech_dcs_write_seq_static(ctx,0xC0,0x01);

	focaltech_dcs_write_seq_static(ctx,0x00,0x88);
	focaltech_dcs_write_seq_static(ctx,0xC4,0x08);

	focaltech_dcs_write_seq_static(ctx,0x00,0x94);
	focaltech_dcs_write_seq_static(ctx,0xE9,0x00);

	focaltech_dcs_write_seq_static(ctx,0x00,0x9A);
	focaltech_dcs_write_seq_static(ctx,0xC4,0x11);

	focaltech_dcs_write_seq_static(ctx,0x00,0x95);
	focaltech_dcs_write_seq_static(ctx,0xE9,0x10);
	focaltech_dcs_write_seq_static(ctx,0x00,0x82);
	focaltech_dcs_write_seq_static(ctx,0xF5,0x00);
	focaltech_dcs_write_seq_static(ctx,0x00,0x93);
	focaltech_dcs_write_seq_static(ctx,0xF5,0x00);

	//AC MODE GIP toggle
	focaltech_dcs_write_seq_static(ctx,0x00,0x99);			
	focaltech_dcs_write_seq_static(ctx,0xCF,0x50);

	focaltech_dcs_write_seq_static(ctx,0x00,0x9C);			
	focaltech_dcs_write_seq_static(ctx,0xF5,0x00);

	focaltech_dcs_write_seq_static(ctx,0x00,0x9E);			
	focaltech_dcs_write_seq_static(ctx,0xF5,0x00);

	focaltech_dcs_write_seq_static(ctx,0x00,0xB0);
	focaltech_dcs_write_seq_static(ctx,0xC5,0x10,0x4A,0x01,0x1F,0x4A,0x00);//fw q全驱 8725 其他客戶

	focaltech_dcs_write_seq_static(ctx,0x00,0xA0);
	focaltech_dcs_write_seq_static(ctx,0xB0,0x00,0x00,0x00,0x00,0x00,0x1D,0x01); 

	focaltech_dcs_write_seq_static(ctx,0x00,0x93);
	focaltech_dcs_write_seq_static(ctx,0xE9,0xff,0xff,0xb0);

	focaltech_dcs_write_seq_static(ctx,0x00,0x9B);
	focaltech_dcs_write_seq_static(ctx,0xC4,0x08);

	focaltech_dcs_write_seq_static(ctx,0x00,0x87);
	focaltech_dcs_write_seq_static(ctx,0xF5,0x00);


	focaltech_dcs_write_seq_static(ctx,0x00,0x80);
	focaltech_dcs_write_seq_static(ctx,0xA4,0xC9);//20230628 SAP

	focaltech_dcs_write_seq_static(ctx,0x00,0x87);
	focaltech_dcs_write_seq_static(ctx,0xC5,0x08,0x08);//GAP

	focaltech_dcs_write_seq_static(ctx,0x00,0xBE);
	focaltech_dcs_write_seq_static(ctx,0xC5,0xC0,0xC0);

	focaltech_dcs_write_seq_static(ctx,0x00,0xC0);
	focaltech_dcs_write_seq_static(ctx,0xC5,0x0F);//mux2 timing 20230705

	//mirror_x2=1
	focaltech_dcs_write_seq_static(ctx,0x00,0xE8);
	focaltech_dcs_write_seq_static(ctx,0xC0,0x40);

	focaltech_dcs_write_seq_static(ctx,0x00,0x00);
	focaltech_dcs_write_seq_static(ctx,0xE1,0x00,0x04,0x08,0x10,0x09,0x1B,0x23,0x2A,0x34,0xA2,0x3C,0x43,0x49,0x4E,0xE6,0x53,0x5B,0x62,0x69,0x6E,0x70,0x77,0x7F,0x87,0xA0,0x90,0x96,0x9C,0xA3,0x80,0xAB,0xB5,0xC2,0xCB,0x50,0xD6,0xE8,0xF6,0xFF,0xA8);
	focaltech_dcs_write_seq_static(ctx,0x00,0x30);
	focaltech_dcs_write_seq_static(ctx,0xE1,0x00,0x04,0x08,0x10,0x09,0x1B,0x23,0x2A,0x34,0xA2,0x3C,0x43,0x49,0x4E,0xE6,0x53,0x5B,0x62,0x69,0x6E,0x70,0x77,0x7F,0x87,0xA0,0x90,0x96,0x9C,0xA3,0x80,0xAB,0xB5,0xC2,0xCB,0x50,0xD6,0xE8,0xF6,0xFF,0xA8);
	focaltech_dcs_write_seq_static(ctx,0x00,0x60);
	focaltech_dcs_write_seq_static(ctx,0xE1,0x00,0x04,0x08,0x10,0x09,0x1B,0x23,0x2A,0x34,0xA2,0x3C,0x43,0x49,0x4E,0xE6,0x53,0x5B,0x62,0x69,0x6E,0x70,0x77,0x7F,0x87,0xA0,0x90,0x96,0x9C,0xA3,0x80,0xAB,0xB5,0xC2,0xCB,0x50,0xD6,0xE8,0xF6,0xFF,0xA8);
	focaltech_dcs_write_seq_static(ctx,0x00,0x90);
	focaltech_dcs_write_seq_static(ctx,0xE1,0x00,0x04,0x08,0x10,0x09,0x1B,0x23,0x2A,0x34,0xA2,0x3C,0x43,0x49,0x4E,0xE6,0x53,0x5B,0x62,0x69,0x6E,0x70,0x77,0x7F,0x87,0xA0,0x90,0x96,0x9C,0xA3,0x80,0xAB,0xB5,0xC2,0xCB,0x50,0xD6,0xE8,0xF6,0xFF,0xA8);
	focaltech_dcs_write_seq_static(ctx,0x00,0xC0);
	focaltech_dcs_write_seq_static(ctx,0xE1,0x00,0x04,0x08,0x10,0x09,0x1B,0x23,0x2A,0x34,0xA2,0x3C,0x43,0x49,0x4E,0xE6,0x53,0x5B,0x62,0x69,0x6E,0x70,0x77,0x7F,0x87,0xA0,0x90,0x96,0x9C,0xA3,0x80,0xAB,0xB5,0xC2,0xCB,0x50,0xD6,0xE8,0xF6,0xFF,0xA8);
	focaltech_dcs_write_seq_static(ctx,0x00,0xF0);
	focaltech_dcs_write_seq_static(ctx,0xE1,0x00,0x04,0x08,0x10,0x09,0x1B,0x23,0x2A,0x34,0xA2,0x3C,0x43,0x49,0x4E,0xE6,0x53);
	focaltech_dcs_write_seq_static(ctx,0x00,0x00);
	focaltech_dcs_write_seq_static(ctx,0xE2,0x5B,0x62,0x69,0x6E,0x70,0x77,0x7F,0x87,0xA0,0x90,0x96,0x9C,0xA3,0x80,0xAB,0xB5,0xC2,0xCB,0x50,0xD6,0xE8,0xF6,0xFF,0xA8);

	focaltech_dcs_write_seq_static(ctx,0x00,0xE0);
	focaltech_dcs_write_seq_static(ctx,0xCF,0x34);
	//TP noise
	focaltech_dcs_write_seq_static(ctx,0x00,0x92);//VGH clamp off      
	focaltech_dcs_write_seq_static(ctx,0xC5,0x00);

	focaltech_dcs_write_seq_static(ctx,0x00,0xA0);//VGH pump stop with charge phase @TP term
	focaltech_dcs_write_seq_static(ctx,0xC5,0x40);

	focaltech_dcs_write_seq_static(ctx,0x00,0x98);//VGH pump stop with charge phase @TP term
	focaltech_dcs_write_seq_static(ctx,0xC5,0x24);

	focaltech_dcs_write_seq_static(ctx,0x00,0xA1);//VGL pump stop with pull phase @TP term
	focaltech_dcs_write_seq_static(ctx,0xC5,0x40);

	focaltech_dcs_write_seq_static(ctx,0x00,0x9D);//VGL pump line rate 1/4H
	focaltech_dcs_write_seq_static(ctx,0xC5,0x24);

	focaltech_dcs_write_seq_static(ctx,0x00,0x94);//VGH pump line rate 1/2H
	focaltech_dcs_write_seq_static(ctx,0xC5,0x04);
	//TP noise end

	//SPI慢半拍
	focaltech_dcs_write_seq_static(ctx,0x00,0x0E);
	focaltech_dcs_write_seq_static(ctx,0xF3,0x80,0xFF);
	//关RTN补偿
	focaltech_dcs_write_seq_static(ctx,0x00,0xE0);
	focaltech_dcs_write_seq_static(ctx,0xCE,0x00);
	//CKH补偿off
	focaltech_dcs_write_seq_static(ctx,0x00,0xFC);
	focaltech_dcs_write_seq_static(ctx,0xC0,0x00,0x17);

	//focaltech_dcs_write_seq_static(ctx,0x00,0x85);
	//focaltech_dcs_write_seq_static(ctx,0xA7,0x00);

	//ESD 
	focaltech_dcs_write_seq_static(ctx,0x00,0x80); //disable 21h
	focaltech_dcs_write_seq_static(ctx,0xB3,0x22);

	focaltech_dcs_write_seq_static(ctx,0x00,0xB0); //HS lock CMD1   
	focaltech_dcs_write_seq_static(ctx,0xB3,0x00);

	focaltech_dcs_write_seq_static(ctx,0x00,0x83); //packet loss cover 
	focaltech_dcs_write_seq_static(ctx,0xB0,0x63);

	//EQ
	//focaltech_dcs_write_seq_static(ctx,0x00,0x80);
	//focaltech_dcs_write_seq_static(ctx,0xC3,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	//CHOP
	focaltech_dcs_write_seq_static(ctx,0x00,0x81);
	focaltech_dcs_write_seq_static(ctx,0xA4,0x23,0x23);
	focaltech_dcs_write_seq_static(ctx,0x00,0x90);
	focaltech_dcs_write_seq_static(ctx,0xE9,0x50);
	//TP TREM LIMIT 24
	focaltech_dcs_write_seq_static(ctx,0x00,0x82);
	focaltech_dcs_write_seq_static(ctx,0xCE,0x17,0x17);

	//OSC trim
	//focaltech_dcs_write_seq_static(ctx,0x00,0x84);
	//focaltech_dcs_write_seq_static(ctx,0xF4,0x42);
	//LCD BUSY
	//focaltech_dcs_write_seq_static(ctx,0x00,0x80);
	//focaltech_dcs_write_seq_static(ctx,0xF6,0x69,0x10);

	//focaltech_dcs_write_seq_static(ctx,0x00,0x80);
	//focaltech_dcs_write_seq_static(ctx,0xF6,0x69,0x0B,0x80,0x80);
	//focaltech_dcs_write_seq_static(ctx,0x00,0x82);
	//focaltech_dcs_write_seq_static(ctx,0xCB,0x05);
	//focaltech_dcs_write_seq_static(ctx,0x00,0x88);
	//focaltech_dcs_write_seq_static(ctx,0xC2,0xC2,0x90,0x00,0x02,0x8B);

	focaltech_dcs_write_seq_static(ctx,0x35,0x00);
	//drv modify Slice Height 8 start
	focaltech_dcs_write_seq_static(ctx,0x00,0xB0);
	focaltech_dcs_write_seq_static(ctx,0xB4,0x00,0x08,0x02,0x00,0x00,0xbb,0x00,0x07,0x0d,0xb7,0x0c,0xb7,0x10,0xf0);
	//drv modify Slice Height 8 end
	//focaltech_dcs_write_seq_static(ctx,0x1C,0x02);

	//CMD2 disable
	//focaltech_dcs_write_seq_static(ctx,0x00,0x00); 
	//focaltech_dcs_write_seq_static(ctx,0xFF,0xFF,0xFF,0xFF);
	//----------------------LCD initial code End----------------------//			
	focaltech_dcs_write_seq_static(ctx, 0x11,0x00);
	msleep(120);
	focaltech_dcs_write_seq_static(ctx, 0x29,0x00);

	pr_info("%s-\n", __func__);
}

static int focaltech_disable(struct drm_panel *panel)
{
	struct focaltech_lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}
//drv-Entering deep sleep to solve the problem of electricity leakage-pzp-20240701-start
/*void focaltech_deep_slepp(struct focaltech_lcm *ctx)
{
	focaltech_dcs_write_seq_static(ctx, 0x00,0x00);
	focaltech_dcs_write_seq_static(ctx, 0xFF,0x87,0x25,0x01); 
	focaltech_dcs_write_seq_static(ctx, 0x00,0x80);
	focaltech_dcs_write_seq_static(ctx, 0xFF,0x87,0x25);
	focaltech_dcs_write_seq_static(ctx, 0x00,0x00);
	focaltech_dcs_write_seq_static(ctx, 0xF7,0x5A,0xA5,0x95,0x27);
}*/
//drv-Entering deep sleep to solve the problem of electricity leakage-pzp-20240701-end
static int focaltech_unprepare(struct drm_panel *panel)
{
	struct focaltech_lcm *ctx = panel_to_lcm(panel);

	pr_info("%s+\n", __func__);
	
	if (!ctx->prepared)
		return 0;


	focaltech_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(50);
	focaltech_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(150);
//drv Added the double-click wake up function-pzp-20240817-start
#if IS_ENABLED(CONFIG_PRIZE_COMMON_NODE)
	if(!fts_gesture_status())	
	{
#endif
		//drv-Entering deep sleep to solve the problem of electricity leakage-pzp-20240701-start
		//focaltech_deep_slepp(ctx);
	//drv-Entering deep sleep to solve the problem of electricity leakage-pzp-20240701-end
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->amoled_vddi_en_gpio = devm_gpiod_get(ctx->dev, "enp-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->amoled_vddi_en_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->amoled_vddi_en_gpio);

		ctx->amoled_vdd_en_gpio = devm_gpiod_get(ctx->dev, "enn-en", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->amoled_vdd_en_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->amoled_vdd_en_gpio);
#if IS_ENABLED(CONFIG_PRIZE_COMMON_NODE)
	}
#endif
//drv Added the double-click wake up function-pzp-20240817-end
	ctx->hbm_en = false;
	ctx->doze_en = false;//drv-Fixed the issue of entering aod and TP having touch-pengzhipeng-20230516
	ctx->error = 0;
	ctx->prepared = false;
	
	pr_info("%s-\n", __func__);
	return 0;
}

static int focaltech_prepare(struct drm_panel *panel)
{
	struct focaltech_lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	// lcd reset H -> L -> L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
    usleep_range(5000, 5001);
	// end

	ctx->amoled_vddi_en_gpio = devm_gpiod_get(ctx->dev, "enp-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->amoled_vddi_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->amoled_vddi_en_gpio);
	usleep_range(5000, 5001);
	
	ctx->amoled_vdd_en_gpio = devm_gpiod_get(ctx->dev, "enn-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->amoled_vdd_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->amoled_vdd_en_gpio);
	usleep_range(5000, 5001);
	
	
	
	focaltech_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		focaltech_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	focaltech_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int focaltech_enable(struct drm_panel *panel)
{
	struct focaltech_lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define FRAME_WIDTH                 1080
#define FRAME_HEIGHT                2408
//drv-Added the physical size of the screen-pzp-20240829-start
#define PHYSICAL_WIDTH              68430   
#define PHYSICAL_HEIGHT             152570 
//drv-Added the physical size of the screen-pzp-20240829-end

#define HFP (92)//drv Modify proch and clk to solve the underflow problem-pzp-20240701
#define HSA (20)
#define HBP (56)
#define HACT (1080)
#define VFP (2528)
#define VSA (4)
#define VBP (12)
#define VACT (2408)
#define VFP_90 (877)
#define VFP_120 (51)

#define PCLK_IN_KHZ_60HZ \
    ((HACT+HFP+HSA+HBP)*(VACT+VFP+VSA+VBP)*(60)/1000)
#define PCLK_IN_KHZ_90HZ \
    ((HACT+HFP+HSA+HBP)*(VACT+VFP_90+VSA+VBP)*(90)/1000)
	
#define PCLK_IN_KHZ_120HZ \
    ((HACT+HFP+HSA+HBP)*(VACT+VFP_120+VSA+VBP)*(120)/1000)
static const struct drm_display_mode switch_mode_120hz = {
	.clock		= PCLK_IN_KHZ_120HZ,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP_120,
	.vsync_end	= VACT + VFP_120 + VSA,
	.vtotal		= VACT + VFP_120 + VSA + VBP,
};

static const struct drm_display_mode switch_mode_90hz = {
	.clock		= PCLK_IN_KHZ_90HZ,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP_90,
	.vsync_end	= VACT + VFP_90 + VSA,
	.vtotal		= VACT + VFP_90 + VSA + VBP,
};

static const struct drm_display_mode switch_mode_60hz = {
	.clock		= PCLK_IN_KHZ_60HZ,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP,
};

static struct mtk_panel_params ext_params_120hz = {
	//drv Modify proch and clk to solve the underflow problem-pzp-20240701-start
	.pll_clk = 545,
    .data_rate = 1090,
//drv Modify proch and clk to solve the underflow problem-pzp-20240701-end
	.physical_width_um = PHYSICAL_WIDTH,
    .physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	//drv Solve the problem of blurred screens and black screens when playing esd -20240830-start
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0xAC,
		.count = 1,
		.para_list[0] = 0x00,
	},
	
	.lcm_esd_check_table[2] = {
		.cmd = 0x05,
		.count = 1,	
		.para_list[0] = 0x00,
	},
	//drv Solve the problem of blurred screens and black screens when playing esd -20240830-end
	.ssc_enable = 0,
    .output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
        .bdg_dsc_enable = 0,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 2088,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2408,
		.pic_width = 1080,
		.slice_height = 8,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 616,
		.scale_value = 32,
		.increment_interval = 187,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
};

static struct mtk_panel_params ext_params_90hz = {
	//drv Modify proch and clk to solve the underflow problem-pzp-20240701-start
	.pll_clk = 545,
    .data_rate = 1090,
//drv Modify proch and clk to solve the underflow problem-pzp-20240701-end
	.physical_width_um = PHYSICAL_WIDTH,
    .physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	//drv Solve the problem of blurred screens and black screens when playing esd -20240830-start
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0xAC,
		.count = 1,
		.para_list[0] = 0x00,
	},
	
	.lcm_esd_check_table[2] = {
		.cmd = 0x05,
		.count = 1,	
		.para_list[0] = 0x00,
	},
	//drv Solve the problem of blurred screens and black screens when playing esd -20240830-end
	.ssc_enable = 0,
    .output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
        .bdg_dsc_enable = 0,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 2088,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2408,
		.pic_width = 1080,
		.slice_height = 8,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 616,
		.scale_value = 32,
		.increment_interval = 187,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
};

static struct mtk_panel_params ext_params_60hz = {
//drv Modify proch and clk to solve the underflow problem-pzp-20240701-start
	.pll_clk = 545,
    .data_rate = 1090,
//drv Modify proch and clk to solve the underflow problem-pzp-20240701-end
	.physical_width_um = PHYSICAL_WIDTH,
    .physical_height_um = PHYSICAL_HEIGHT,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	//drv Solve the problem of blurred screens and black screens when playing esd -20240830-start
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0xAC,
		.count = 1,
		.para_list[0] = 0x00,
	},
	
	.lcm_esd_check_table[2] = {
		.cmd = 0x05,
		.count = 1,	
		.para_list[0] = 0x00,
	},
	//drv Solve the problem of blurred screens and black screens when playing esd -20240830-end
	.ssc_enable = 0,
    .output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
        .bdg_dsc_enable = 0,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 2088,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2408,
		.pic_width = 1080,
		.slice_height = 8,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 616,
		.scale_value = 32,
		.increment_interval = 187,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 3511,
		.slice_bpg_offset = 3255,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}




/* drv modify hbm function end */				 
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

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct focaltech_lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	
	return 0;
}



 static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	int dst_fps = 0;
	struct drm_display_mode *m = get_mode_by_id_hfp(connector, mode);

	dst_fps = m ? drm_mode_vrefresh(m) : -EINVAL;
	pr_info("%s, dst_fps %d\n", __func__, dst_fps);
	if (dst_fps == 60) {
		//ext_params_60hz.skip_vblank = 0;
		ext->params = &ext_params_60hz;
	} else if (dst_fps == 90) {
		ext->params = &ext_params_90hz;
	} else if (dst_fps == 120) {
		//ext_params_120hz.skip_vblank = 0;
		ext->params = &ext_params_120hz;
	} else {
		pr_info("%s, dst_fps %d\n", __func__, dst_fps);
		ret = 1;
	}

	return ret;
}



static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
};
//#endif

static int focaltech_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;
	
    mode = drm_mode_duplicate(connector->dev, &switch_mode_60hz);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_60hz.hdisplay, switch_mode_60hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_60hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode_1 = drm_mode_duplicate(connector->dev, &switch_mode_90hz);
	if (!mode_1) {
		dev_info(connector->dev->dev, "failed to add mode_1 %ux%ux@%u\n",
			 switch_mode_90hz.hdisplay, switch_mode_90hz.vdisplay,
			 drm_mode_vrefresh(&switch_mode_90hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);

    mode_2 = drm_mode_duplicate(connector->dev, &switch_mode_120hz);
	if (!mode_2) {
		dev_info(connector->dev->dev, "failed to add mode_2 %ux%ux@%u\n",
			 switch_mode_120hz.hdisplay, switch_mode_120hz.vdisplay,
			 drm_mode_vrefresh(&switch_mode_120hz));
		return -ENOMEM;
	}

	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_2);

	connector->display_info.width_mm = 68;
	connector->display_info.height_mm = 153;

	return 1;
}

static const struct drm_panel_funcs focaltech_drm_funcs = {
	.disable = focaltech_disable,
	.unprepare = focaltech_unprepare,
	.prepare = focaltech_prepare,
	.enable = focaltech_enable,
	.get_modes = focaltech_get_modes,
};

static int focaltech_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct focaltech_lcm *ctx;
	struct device_node *backlight;
	unsigned int lcm_degree;
	int ret;
	int probe_ret;

	pr_info("%s+ focaltech,rm692e0,cmd,120hz\n", __func__);

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

	ctx = devm_kzalloc(dev, sizeof(struct focaltech_lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	//dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_NO_EOT_PACKET;
    dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_NO_EOT_PACKET | MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
	

	ctx->amoled_vddi_en_gpio = devm_gpiod_get(dev, "enp-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->amoled_vddi_en_gpio)) {
		dev_info(dev, "cannot get amoled-vddi-en-gpios %ld\n",
			 PTR_ERR(ctx->amoled_vddi_en_gpio));
		return PTR_ERR(ctx->amoled_vddi_en_gpio);
	}
	devm_gpiod_put(dev, ctx->amoled_vddi_en_gpio);

	ctx->amoled_vdd_en_gpio = devm_gpiod_get(dev, "enn-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->amoled_vdd_en_gpio)) {
		dev_info(dev, "cannot get amoled-vdd-en-gpios %ld\n",
			 PTR_ERR(ctx->amoled_vdd_en_gpio));
		return PTR_ERR(ctx->amoled_vdd_en_gpio);
	}
	devm_gpiod_put(dev, ctx->amoled_vdd_en_gpio);
	

	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &focaltech_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params_60hz, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
	probe_ret = of_property_read_u32(dev->of_node, "lcm-degree", &lcm_degree);
	if (probe_ret < 0)
		lcm_degree = 0;
	else
		ext_params_60hz.lcm_degree = lcm_degree;
	pr_info("lcm_degree: %d\n", ext_params_60hz.lcm_degree);
#endif
	g_ctx = ctx;
	g_ctx->doze_en = false;//drv-Fixed the issue of entering aod and TP having touch-pengzhipeng-20230516
	pr_info("%s- focaltech,rm692e0,cmd,120hz\n", __func__);
	/* drv modify hbm function start */
	ctx->hbm_en = false;
	ctx->hbm_stat = false;
	/* drv modify hbm function end */

//drv add by shenwenbin for lcd hardware info 20231214 start
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
    strcpy(current_lcm_info.chip,"Focaltech-ft8725");
    strcpy(current_lcm_info.vendor,"Focaltech");
    sprintf(current_lcm_info.id,"0x%02x",0x01);
    strcpy(current_lcm_info.more,"2408*1080");
#endif
//drv add by shenwenbin for lcd hardware info 20231214 end
	return ret;
}

static int focaltech_remove(struct mipi_dsi_device *dsi)
{
	struct focaltech_lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	if (ext_ctx == NULL)
		return 0;

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

	return 0;
}

static const struct of_device_id focaltech_of_match[] = {
	{
	    .compatible = "boe,ft8725,vdo,120Hz",
	},
	{}
};

MODULE_DEVICE_TABLE(of, focaltech_of_match);

static struct mipi_dsi_driver focaltech_driver = {
	.probe = focaltech_probe,
	.remove = focaltech_remove,
	.driver = {
		.name = "panel-boe-ft8725-dphy-vdo-120hz",
		.owner = THIS_MODULE,
		.of_match_table = focaltech_of_match,
	},
};

module_mipi_dsi_driver(focaltech_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("FT8725 AMOLED VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
