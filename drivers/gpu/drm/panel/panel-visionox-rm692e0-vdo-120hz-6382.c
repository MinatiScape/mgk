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

//drv add by shenwenbin for lcd hardware info 20231214 start
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/prize/hardware_info/hardware_info.h"
extern struct hardware_info current_lcm_info;
#endif
//drv add by shenwenbin for lcd hardware info 20231214 end

static char init_head_tb[] = {0xFE, 0x00};	//modify by shenwenbin for tuning backlight 20231108
static char bl_tb[] = {0x51, 0x0D, 0xBB};
static char hbm_tb[] = {0x51, 0x0F, 0xFE};  // drv modify hbm function
static char aod_bl_tb[] = {0x51, 0x00, 0x00}; //DRV-modify by shenwenbin for add AOD mode 20240201  

//modify by shenwenbin for ESD recovery not send backlight 20240106 start
extern int mtk_drm_esd_check_status(void);
extern void mtk_drm_esd_set_status(int status);
//modify by shenwenbin for ESD recovery not send backlight 20240106 end

/*LCM_DEGREE default value*/
#define PROBE_FROM_DTS 0

/*****************************************************************************
 * Function
 *****************************************************************************/

#ifdef VENDOR_EDIT
// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
extern void lcd_queue_load_tp_fw(void);
#endif /*VENDOR_EDIT*/

struct visionox_lcm {
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
	bool hbm_stat;           //0?′?úHBM  1?úHBM
	/* drv modify hbm function end */

	int error;
};
static unsigned int last_level;  // drv modify hbm function
struct visionox_lcm *g_ctx;
static u32 bdg_support;
#define visionox_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		visionox_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define visionox_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		visionox_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct visionox_lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct visionox_lcm, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int visionox_dcs_read(struct visionox_lcm *ctx, u8 cmd, void *data, size_t len)
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

static void visionox_panel_get_data(struct visionox_lcm *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	ret = visionox_dcs_read(ctx, 0x0A, buffer, 1);
	pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
	dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
		ret, buffer[0] | (buffer[1] << 8));
}
#endif

static void visionox_dcs_write(struct visionox_lcm *ctx, const void *data, size_t len)
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

//modify by shenwenbin for ESD recovery not send backlight 20240106 start
static void lcm_pannel_reconfig_blk(struct visionox_lcm *ctx)
{
	if(mtk_drm_esd_check_status()){
        visionox_dcs_write(ctx,init_head_tb,ARRAY_SIZE(init_head_tb));
		visionox_dcs_write(ctx,bl_tb,ARRAY_SIZE(bl_tb));
		mtk_drm_esd_set_status(0);
	}
}
//modify by shenwenbin for ESD recovery not send backlight 20240106 end

static void visionox_panel_init(struct visionox_lcm *ctx)
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
	
    visionox_dcs_write_seq_static(ctx, 0xFE,0xD4);
    visionox_dcs_write_seq_static(ctx, 0x40,0x03);
    visionox_dcs_write_seq_static(ctx, 0xFE,0xFD);
    visionox_dcs_write_seq_static(ctx, 0x80,0x06);
    visionox_dcs_write_seq_static(ctx, 0x83,0x00);
    visionox_dcs_write_seq_static(ctx, 0xEE,0x00);
    visionox_dcs_write_seq_static(ctx, 0xEF,0x00);
    visionox_dcs_write_seq_static(ctx, 0xFE,0xD0);
    visionox_dcs_write_seq_static(ctx, 0x7E,0x80);
    visionox_dcs_write_seq_static(ctx, 0xFE,0xD2);
    visionox_dcs_write_seq_static(ctx, 0x23,0x56);

    visionox_dcs_write_seq_static(ctx, 0xFE,0xA0);
    visionox_dcs_write_seq_static(ctx, 0x06,0x32);
    visionox_dcs_write_seq_static(ctx, 0xFE,0xA1);
    visionox_dcs_write_seq_static(ctx, 0xB3,0x3F);
    visionox_dcs_write_seq_static(ctx, 0xFE,0xA1);
    visionox_dcs_write_seq_static(ctx, 0xC3,0xC3);
    visionox_dcs_write_seq_static(ctx, 0xC4,0x9C);
    visionox_dcs_write_seq_static(ctx, 0xC5,0x1E);
    visionox_dcs_write_seq_static(ctx, 0xC6,0x23);
    
    visionox_dcs_write_seq_static(ctx, 0xFE,0x40);
    visionox_dcs_write_seq_static(ctx, 0x0A,0xB5);
    visionox_dcs_write_seq_static(ctx, 0xBE,0xB5);
    visionox_dcs_write_seq_static(ctx, 0xC0,0xB5);
    visionox_dcs_write_seq_static(ctx, 0xC1,0xB5);
    visionox_dcs_write_seq_static(ctx, 0x08,0x13);
    visionox_dcs_write_seq_static(ctx, 0x09,0x0D);
    visionox_dcs_write_seq_static(ctx, 0xC7,0x3B);

    visionox_dcs_write_seq_static(ctx, 0xFE,0x74);
    visionox_dcs_write_seq_static(ctx, 0x45,0xA8);
    visionox_dcs_write_seq_static(ctx, 0x46,0xA8);
    visionox_dcs_write_seq_static(ctx, 0x3b,0x60);
    visionox_dcs_write_seq_static(ctx, 0x3d,0x60);

	visionox_dcs_write_seq_static(ctx, 0xFE,0x40);
	visionox_dcs_write_seq_static(ctx, 0xBD,0x05);
	visionox_dcs_write_seq_static(ctx, 0xFE,0xB0);
	visionox_dcs_write_seq_static(ctx, 0x91,0x70);
	visionox_dcs_write_seq_static(ctx, 0x27,0x7E);
//	visionox_dcs_write_seq_static(ctx, 0xFE,0xFD);
//	visionox_dcs_write_seq_static(ctx, 0x80,0xF6);
	visionox_dcs_write_seq_static(ctx, 0xFE,0xA1);
	visionox_dcs_write_seq_static(ctx, 0xCD,0x6B);
	visionox_dcs_write_seq_static(ctx, 0xCE,0xBB);
	visionox_dcs_write_seq_static(ctx, 0xFE,0xD1);
	visionox_dcs_write_seq_static(ctx, 0xB4,0x01);
	visionox_dcs_write_seq_static(ctx, 0xFE,0x38);
	visionox_dcs_write_seq_static(ctx, 0x17,0x0F);
	visionox_dcs_write_seq_static(ctx, 0x18,0x0F);
	visionox_dcs_write_seq_static(ctx, 0xFE,0x00);
	visionox_dcs_write_seq_static(ctx, 0xFA,0x01);
	visionox_dcs_write_seq_static(ctx, 0xC2,0x03);
	visionox_dcs_write_seq_static(ctx, 0x35,0x00);
	visionox_dcs_write_seq_static(ctx, 0x51,0x00,0x00);
	usleep_range(100, 101);
	visionox_dcs_write_seq_static(ctx, 0x51,0x00,0x00);
	usleep_range(100, 101);
    
    lcm_pannel_reconfig_blk(ctx);   //modify by shenwenbin for ESD recovery not send backlight 20240106
    
	visionox_dcs_write_seq_static(ctx, 0x53,0x28);  //modify by shenwenbin for low backlight flick 20240109
	
	visionox_dcs_write_seq_static(ctx, 0x11,0x00);
	msleep(120);
	visionox_dcs_write_seq_static(ctx, 0x29,0x00);
	msleep(50);

	//bist mode
/* 	visionox_dcs_write_seq_static(ctx, 0xFE,0x00);
	visionox_dcs_write_seq_static(ctx, 0xFA,0x07);
	visionox_dcs_write_seq_static(ctx, 0xFE,0xD0);
	visionox_dcs_write_seq_static(ctx, 0x43,0x7F);
	visionox_dcs_write_seq_static(ctx, 0x44,0x01);
	visionox_dcs_write_seq_static(ctx, 0x45,0x21);
	visionox_dcs_write_seq_static(ctx, 0x45,0x21);
	visionox_dcs_write_seq_static(ctx, 0x3A,0xC8);
	visionox_dcs_write_seq_static(ctx, 0x3A,0xC8);
	visionox_dcs_write_seq_static(ctx, 0xFE,0xD4);
	visionox_dcs_write_seq_static(ctx, 0x52,0x90);
	visionox_dcs_write_seq_static(ctx, 0x53,0x40); */
	pr_info("%s-\n", __func__);
}

static int visionox_disable(struct drm_panel *panel)
{
	struct visionox_lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int visionox_unprepare(struct drm_panel *panel)
{
	struct visionox_lcm *ctx = panel_to_lcm(panel);

	pr_info("%s+\n", __func__);
	
	if (!ctx->prepared)
		return 0;

	visionox_dcs_write_seq_static(ctx, 0xFE,0x00);	//modify by shenwenbin for suspend timing 20231118
	visionox_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(50);
	visionox_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(150);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->amoled_vddi_en_gpio = devm_gpiod_get(ctx->dev, "amoled-vddi-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->amoled_vddi_en_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->amoled_vddi_en_gpio);

	ctx->amoled_vdd_en_gpio = devm_gpiod_get(ctx->dev, "amoled-vdd-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->amoled_vdd_en_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->amoled_vdd_en_gpio);
	
	ctx->amoled_vci_en_gpio = devm_gpiod_get(ctx->dev, "amoled-vci-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->amoled_vci_en_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->amoled_vci_en_gpio);

	ctx->error = 0;
	ctx->prepared = false;
	
	pr_info("%s-\n", __func__);
	return 0;
}

static int visionox_prepare(struct drm_panel *panel)
{
	struct visionox_lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	// lcd reset H -> L -> L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(5000, 5001);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	// end

	ctx->amoled_vddi_en_gpio = devm_gpiod_get(ctx->dev, "amoled-vddi-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->amoled_vddi_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->amoled_vddi_en_gpio);
	usleep_range(5000, 5001);
	
	ctx->amoled_vdd_en_gpio = devm_gpiod_get(ctx->dev, "amoled-vdd-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->amoled_vdd_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->amoled_vdd_en_gpio);
	usleep_range(5000, 5001);
	
	ctx->amoled_vci_en_gpio = devm_gpiod_get(ctx->dev, "amoled-vci-en", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->amoled_vci_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->amoled_vci_en_gpio);
	usleep_range(5000, 5001);
	
/* 	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10000, 10001);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10000, 10001);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(50000, 50001);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio); */
	
	visionox_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		visionox_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	//visionox_panel_get_data(ctx);
#endif

#ifdef VENDOR_EDIT
	// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
	lcd_queue_load_tp_fw();
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int visionox_enable(struct drm_panel *panel)
{
	struct visionox_lcm *ctx = panel_to_lcm(panel);

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
#define FRAME_WIDTH                 1080
#define FRAME_HEIGHT                2400

#define HAC (1080)
#define VAC (2400)
/*120HZ D:1100*/
#define HFP (140)
#define HSA (4)
#define HBP (36)
#define VFP (2449)
#define VSA (4)
#define VBP (12)
#define DATA_RATE					946

#define PHYSICAL_WIDTH              69552
#define PHYSICAL_HEIGHT             154560

/*Parameter setting for mode 0 Start*/
#define MODE_0_FPS                  60
#define MODE_0_VFP                  2449
#define MODE_0_HFP                  140
//#define MODE_0_DATA_RATE            1100
/*Parameter setting for mode 0 End*/

/*Parameter setting for mode 1 Start*/
#define MODE_1_FPS                  90
#define MODE_1_VFP                  825
#define MODE_1_HFP                  140
//#define MODE_1_DATA_RATE            1100
/*Parameter setting for mode 1 End*/

/*Parameter setting for mode 2 Start*/
#define MODE_2_FPS                  120
#define MODE_2_VFP                  17
#define MODE_2_HFP                  140
//#define MODE_2_DATA_RATE            1100
/*Parameter setting for mode 2 End*/

//static u32 fake_heigh = 2400;
//static u32 fake_width = 1080;
//static bool need_fake_resolution;

static const struct drm_display_mode default_mode = {
	.clock = 367795,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_0_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,
//	.vrefresh = MODE_0_FPS,
};

static const struct drm_display_mode performance_mode_90hz = {
	.clock = 367530,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_1_HFP,
	.hsync_end = FRAME_WIDTH + MODE_1_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_1_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_1_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_1_VFP + VSA + VBP,
//	.vrefresh = MODE_1_FPS,
};

static const struct drm_display_mode performance_mode_120hz = {
	.clock = 367870,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_2_HFP,
	.hsync_end = FRAME_WIDTH + MODE_2_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_2_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_2_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_2_VFP + VSA + VBP,
//	.vrefresh = MODE_2_FPS,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 473,
	.data_rate = 946,
//	.vfp_low_power = 2454, //45hz
    .physical_width_um = PHYSICAL_WIDTH,
    .physical_height_um = PHYSICAL_HEIGHT,
//drv-add esd-pengzhipeng-20240102-start
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_degree = PROBE_FROM_DTS,
/* 	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0B, .count = 1, .para_list[0] = 0x00,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0xFA, .count = 1, .para_list[0] = 0x01,
	}, */
//drv-add esd-pengzhipeng-20240102-end
	.ssc_enable = 0,
	.bdg_ssc_enable = 0,
	.lane_swap_en = 0,
	.bdg_lane_swap_en = 0,
	//.lp_perline_en = 0,
	.ap_tx_keep_hs_during_vact = 1,
	//.vdo_per_frame_lp_enable = 1,
    .hbm_en_time = 0,
	.hbm_dis_time = 1,
    //modify by shenwenbin for MT6382+VFP+switch cmd to change framerate 20240127 start
    .change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0xFE, 0x40} },
		.dfps_cmd_table[1] = {0, 2, {0xBD, 0x05} },
		.dfps_cmd_table[2] = {0, 2, {0xFE, 0x00} },
	},
    //modify by shenwenbin for MT6382+VFP+switch cmd to change framerate 20240127 end
	.dsc_params = {
		.enable = 0,
		.bdg_dsc_enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 20,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 488,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1294,
		.slice_bpg_offset = 1302,
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
	.pll_clk = 473,
	.data_rate = 946,
//	.vfp_low_power = 2454, //60hz
    .physical_width_um = PHYSICAL_WIDTH,
    .physical_height_um = PHYSICAL_HEIGHT,
	//drv-add esd-pengzhipeng-20240102-start
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_degree = PROBE_FROM_DTS,
/* 	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0B, .count = 1, .para_list[0] = 0x00,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0xFA, .count = 1, .para_list[0] = 0x01,
	}, */
//drv-add esd-pengzhipeng-20240102-end
	.ssc_enable = 0,
	.bdg_ssc_enable = 0,
	.lane_swap_en = 0,
	.bdg_lane_swap_en = 0,
	.ap_tx_keep_hs_during_vact = 1,
    .hbm_en_time = 0,
	.hbm_dis_time = 1,
    //modify by shenwenbin for MT6382+VFP+switch cmd to change framerate 20240127 start
    .change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0xFE, 0x40} },
		.dfps_cmd_table[1] = {0, 2, {0xBD, 0x06} },
		.dfps_cmd_table[2] = {0, 2, {0xFE, 0x00} },
	},
    //modify by shenwenbin for MT6382+VFP+switch cmd to change framerate 20240127 end
	.dsc_params = {
		.enable = 0,
		.bdg_dsc_enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 20,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 488,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1294,
		.slice_bpg_offset = 1302,
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

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = 473,
	.data_rate = 946,
    .physical_width_um = PHYSICAL_WIDTH,
    .physical_height_um = PHYSICAL_HEIGHT,
//drv-add esd-pengzhipeng-20240102-start
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_degree = PROBE_FROM_DTS,
/* 	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0B, .count = 1, .para_list[0] = 0x00,
	},
	.lcm_esd_check_table[2] = {
		.cmd = 0xFA, .count = 1, .para_list[0] = 0x01,
	}, */
//drv-add esd-pengzhipeng-20240102-end
	.ssc_enable = 0,
	.bdg_ssc_enable = 0,
	.lane_swap_en = 0,
	.bdg_lane_swap_en = 0,
	.ap_tx_keep_hs_during_vact = 1,
    .hbm_en_time = 0,
	.hbm_dis_time = 1,
    //modify by shenwenbin for MT6382+VFP+switch cmd to change framerate 20240127 start
    .change_fps_by_vfp_send_cmd = 1,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0xFE, 0x40} },
		.dfps_cmd_table[1] = {0, 2, {0xBD, 0x00} },
		.dfps_cmd_table[2] = {0, 2, {0xFE, 0x00} },
	},
    //modify by shenwenbin for MT6382+VFP+switch cmd to change framerate 20240127 end
	.dsc_params = {
		.enable = 0,
		.bdg_dsc_enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 20,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 488,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1294,
		.slice_bpg_offset = 1302,
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

static int visionox_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	unsigned int rawlevel = 0xDBB;
	
	/*if (level > 259) {
		if (level == 260) {
			if (!cb)
				return -1;
			pr_err("%s panel into HBM", __func__);
			cb(dsi, handle, hbm_tb, ARRAY_SIZE(hbm_tb));
		} else if (level == 270) {
			if (rawlevel <= 0xDBB) {
				bl_tb[1] = (rawlevel>>8)&0xf;
				bl_tb[2] = (rawlevel)&0xff;
			} else {
				pr_err("%s err, rawlevel=%d\n", __func__, rawlevel);
			}
			if (!cb)
				return -1;
			pr_err("%s panel out HBM, rawlevel=%d, bl_tb[1]=0x%x, bl_tb[2]=0x%x\n",
				__func__, rawlevel, bl_tb[1], bl_tb[2]);
			cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
		}
	} else {*/
		if (level) {
			rawlevel = level*3515/2047;
			last_level = level;  // drv modify finger hbm function
		} else {
			rawlevel = 0;
		}
		if (rawlevel <= 0xDBB) {
			bl_tb[1] = (rawlevel>>8)&0xf;
			bl_tb[2] = (rawlevel)&0xff;
			if (!cb)
				return -1;
			
			if(bl_tb[2] == 0x35 || bl_tb[2] == 0x44){
				bl_tb[2] += 1;
			}
			
			if(g_ctx->hbm_stat == false || rawlevel == 0) {
				cb(dsi, handle, init_head_tb, ARRAY_SIZE(init_head_tb));	//modify by shenwenbin for tuning backlight 20231108
				cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
			}
			usleep_range(100, 101);
			pr_err("%s level=%d, rawlevel=%d, bl_tb[1]=0x%x, bl_tb[2]=0x%x\n",
				__func__, level, rawlevel, bl_tb[1], bl_tb[2]);
		} else {
			pr_err("%s err, rawlevel=%d\n", __func__, rawlevel);
		}
	//}
	
	return 0;
}

/* drv modify hbm function start */
static int panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
			      dcs_write_gce cb, void *handle, bool en)
{
	struct visionox_lcm *ctx = panel_to_lcm(panel);

	if (!cb)
		return -1;

	if (ctx->hbm_en == en)
		goto done;

	if (en)
	{
		pr_info("[panel] %s : enter HBM, last_level = %d\n",__func__,last_level);
		ctx->hbm_stat = true;
		cb(dsi, handle, init_head_tb, ARRAY_SIZE(init_head_tb));	//modify by shenwenbin for tuning backlight 20231108
		cb(dsi, handle, hbm_tb, ARRAY_SIZE(hbm_tb));
		usleep_range(100, 101);
	}
	else
	{
		pr_info("[panel] %s : exit HBM\n",__func__);
		ctx->hbm_stat = false;
		visionox_setbacklight_cmdq(dsi,cb,handle,last_level);
	}

	ctx->hbm_en = en;
	ctx->hbm_wait = true;

done:
	return 0;
}

static void panel_hbm_get_state(struct drm_panel *panel, bool *state)
{

	struct visionox_lcm *ctx = panel_to_lcm(panel);
	//pr_info("[panel] %s : hbm_en = %d\n",__func__,ctx->hbm_en);

	*state = ctx->hbm_en;
}


static void panel_hbm_get_wait_state(struct drm_panel *panel, bool *wait)
{
	struct visionox_lcm *ctx = panel_to_lcm(panel);

	*wait = ctx->hbm_wait;
}

static bool panel_hbm_set_wait_state(struct drm_panel *panel, bool wait)
{
	struct visionox_lcm *ctx = panel_to_lcm(panel);
	bool old = ctx->hbm_wait;

	ctx->hbm_wait = wait;
	return old;
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

 static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	int dst_fps = 0;
	struct drm_display_mode *m = get_mode_by_id_hfp(connector, mode);

	dst_fps = m ? drm_mode_vrefresh(m) : -EINVAL;

    pr_info("[panel] %s : dst_fps = %d\n",__func__,dst_fps);

	if (dst_fps == 60) {
		ext_params.skip_vblank = 0;
		ext->params = &ext_params;
	} else if (dst_fps == 90) {
		ext->params = &ext_params_90hz;
	} else if (dst_fps == 120) {
		ext_params_120hz.skip_vblank = 0;
		ext->params = &ext_params_120hz;
	} else {
		pr_info("%s, dst_fps %d\n", __func__, dst_fps);
		ret = 1;
	}

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct visionox_lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10000, 10001);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10000, 10001);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(50000, 50001);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	
	return 0;
}

//DRV-modify by shenwenbin for add AOD mode 20240201 start
static int panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct visionox_lcm *ctx = panel_to_lcm(panel);

	pr_info("panel %s\n", __func__);
	/*PageA0*/
	visionox_dcs_write_seq_static(ctx, 0xFE, 0xA0);
	/*AOD推力减小*/
	visionox_dcs_write_seq_static(ctx, 0x25, 0xC0);
	visionox_dcs_write_seq_static(ctx, 0x3A, 0x60);
	visionox_dcs_write_seq_static(ctx, 0x22, 0x04);
	visionox_dcs_write_seq_static(ctx, 0x37, 0x8F);
	/*Page40*/
	visionox_dcs_write_seq_static(ctx, 0xFE, 0x40);
	/*Inter power on*/
	visionox_dcs_write_seq_static(ctx, 0xAC, 0x84);
	/*Page26*/
	visionox_dcs_write_seq_static(ctx, 0xFE, 0x26);
	/*Fast discharge off*/
	visionox_dcs_write_seq_static(ctx, 0x22, 0x1A);
	/*Page00*/
	visionox_dcs_write_seq_static(ctx, 0xFE, 0x00);
	/*Idle mode on*/
	visionox_dcs_write_seq_static(ctx, 0x39);
	/*Enter AOD 15nit*/
	visionox_dcs_write_seq_static(ctx, 0x51, 0x04, 0x00);
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct visionox_lcm *ctx = panel_to_lcm(panel);

	pr_info("panel %s\n", __func__);

	/*Page00*/
	visionox_dcs_write_seq_static(ctx, 0xFE, 0x00);
	/*Idle mode off*/
	visionox_dcs_write_seq_static(ctx, 0x38);
	/*Page26*/
	visionox_dcs_write_seq_static(ctx, 0xFE, 0x26);
	/*Fast discharge ON*/
	visionox_dcs_write_seq_static(ctx, 0x22, 0x19);
	/*PageA0*/
	visionox_dcs_write_seq_static(ctx, 0xFE, 0xA0);
	/*返回Normal推力*/
	visionox_dcs_write_seq_static(ctx, 0x25, 0x40);
	visionox_dcs_write_seq_static(ctx, 0x3A, 0x40);
	visionox_dcs_write_seq_static(ctx, 0x22, 0x44);
	visionox_dcs_write_seq_static(ctx, 0x37, 0x88);
	/*Page00*/
	visionox_dcs_write_seq_static(ctx, 0xFE, 0x00);

	return 0;
}

static int panel_set_aod_light_mode(void *dsi,
	dcs_write_gce cb, void *handle, unsigned int mode)
{
	pr_info("panel %s\n", __func__);

	if (mode >= 2) {
		/*Enter AOD 60nit*/
		pr_info("panel %s Enter AOD 60nit\n", __func__);
        aod_bl_tb[1] = 0x0F;
        aod_bl_tb[2] = 0xFE;
	}else if(mode == 1) {
       	/*Enter AOD 15nit*/
        pr_info("panel %s Enter AOD 15nit\n", __func__);
        aod_bl_tb[1] = 0x04;
        aod_bl_tb[2] = 0x00;
    }else {
		/*Enter AOD 5nit*/
        pr_info("panel %s Enter AOD 5nit\n", __func__);
        aod_bl_tb[1] = 0x01;
        aod_bl_tb[2] = 0x55;
	}
    
    cb(dsi, handle, init_head_tb, ARRAY_SIZE(init_head_tb));
    cb(dsi, handle, aod_bl_tb, ARRAY_SIZE(aod_bl_tb));
	pr_info("%s : %d !\n", __func__, mode);

	return 0;
}
//DRV-modify by shenwenbin for add AOD mode 20240201 end

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = visionox_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	/* drv modify hbm function start */
	.hbm_set_cmdq = panel_hbm_set_cmdq,
	.hbm_get_state = panel_hbm_get_state,
    .hbm_get_wait_state = panel_hbm_get_wait_state,
	.hbm_set_wait_state = panel_hbm_set_wait_state,
	/* drv modify hbm function end */
    //DRV-modify by shenwenbin for add AOD mode 20240201 start
    /*aod mode*/
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
	.set_aod_light_mode = panel_set_aod_light_mode,
    //DRV-modify by shenwenbin for add AOD mode 20240201 end
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

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *	   become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *	  display the first valid frame after starting to receive
	 *	  video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *	   turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *		 to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int visionox_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;
	struct drm_display_mode *mode3;

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

	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 155;

	return 1;
}

static const struct drm_panel_funcs visionox_drm_funcs = {
	.disable = visionox_disable,
	.unprepare = visionox_unprepare,
	.prepare = visionox_prepare,
	.enable = visionox_enable,
	.get_modes = visionox_get_modes,
};

static void check_is_bdg_support(struct device *dev)
{
	unsigned int ret = 0;

	ret = of_property_read_u32(dev->of_node, "bdg-support", &bdg_support);
	if (!ret && bdg_support == 1) {
		pr_info("%s, bdg support 1", __func__);
	} else {
		pr_info("%s, bdg support 0", __func__);
		bdg_support = 0;
	}
}

static int visionox_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct visionox_lcm *ctx;
	struct device_node *backlight;
	unsigned int lcm_degree;
	int ret;
	int probe_ret;

	pr_info("%s+ visionox,rm692e0,vdo,120hz,6382\n", __func__);

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

	ctx = devm_kzalloc(dev, sizeof(struct visionox_lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_NO_EOT_PACKET;	

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
	

	ctx->amoled_vddi_en_gpio = devm_gpiod_get(dev, "amoled-vddi-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->amoled_vddi_en_gpio)) {
		dev_info(dev, "cannot get amoled-vddi-en-gpios %ld\n",
			 PTR_ERR(ctx->amoled_vddi_en_gpio));
		return PTR_ERR(ctx->amoled_vddi_en_gpio);
	}
	devm_gpiod_put(dev, ctx->amoled_vddi_en_gpio);

	ctx->amoled_vdd_en_gpio = devm_gpiod_get(dev, "amoled-vdd-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->amoled_vdd_en_gpio)) {
		dev_info(dev, "cannot get amoled-vdd-en-gpios %ld\n",
			 PTR_ERR(ctx->amoled_vdd_en_gpio));
		return PTR_ERR(ctx->amoled_vdd_en_gpio);
	}
	devm_gpiod_put(dev, ctx->amoled_vdd_en_gpio);
	
	ctx->amoled_vci_en_gpio = devm_gpiod_get(dev, "amoled-vci-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->amoled_vci_en_gpio)) {
		dev_info(dev, "cannot get amoled-vci-en-gpios %ld\n",
			 PTR_ERR(ctx->amoled_vci_en_gpio));
		return PTR_ERR(ctx->amoled_vci_en_gpio);
	}
	devm_gpiod_put(dev, ctx->amoled_vci_en_gpio);

	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &visionox_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	check_is_bdg_support(dev);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
	probe_ret = of_property_read_u32(dev->of_node, "lcm-degree", &lcm_degree);
	if (probe_ret < 0)
		lcm_degree = 0;
	else
		ext_params.lcm_degree = lcm_degree;
	pr_info("lcm_degree: %d\n", ext_params.lcm_degree);
#endif
	g_ctx = ctx;
	pr_info("%s- visionox,rm692e0,vdo,120hz,6382\n", __func__);
	/* drv modify hbm function start */
	ctx->hbm_en = false;
	ctx->hbm_stat = false;
	/* drv modify hbm function end */

//drv add by shenwenbin for lcd hardware info 20231214 start
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
    strcpy(current_lcm_info.chip,"raydiumic-rm692e0");
    strcpy(current_lcm_info.vendor,"visionox");
    sprintf(current_lcm_info.id,"0x%02x",0x01);
    strcpy(current_lcm_info.more,"2400*1080");
#endif
//drv add by shenwenbin for lcd hardware info 20231214 end
	return ret;
}

static int visionox_remove(struct mipi_dsi_device *dsi)
{
	struct visionox_lcm *ctx = mipi_dsi_get_drvdata(dsi);
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

static const struct of_device_id visionox_of_match[] = {
	{
	    .compatible = "visionox,rm692e0,vdo,120hz,6382",
	},
	{}
};

MODULE_DEVICE_TABLE(of, visionox_of_match);

static struct mipi_dsi_driver visionox_driver = {
	.probe = visionox_probe,
	.remove = visionox_remove,
	.driver = {
		.name = "panel-visionox-rm692e0-vdo-120hz-6382",
		.owner = THIS_MODULE,
		.of_match_table = visionox_of_match,
	},
};

module_mipi_dsi_driver(visionox_driver);

MODULE_AUTHOR("CooseaGroup");
MODULE_DESCRIPTION("VISIONOX RM692E0 VDO 120HZ AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
