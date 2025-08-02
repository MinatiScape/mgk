/*
 * GalaxyCore touchscreen driver
 *
 * Copyright (C) 2021 GalaxyCore Incorporated
 *
 * Copyright (C) 2021 Neo Chen <neo_chen@gcoreinc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "gcore_drv_common.h"
#ifdef CONFIG_FB
#include <linux/fb.h>
#endif

void gcore_suspend(void)
{
	GTP_DEBUG("enter gcore suspend");

#ifdef	CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
		if(gcore_tpd_proximity_flag && gcore_tpd_proximity_flag_off){
		fn_data.gdev->ts_stat = TS_SUSPEND;
		GTP_DEBUG("Proximity TP Now.");
		return ;
	}
	
#endif


#if GCORE_WDT_RECOVERY_ENABLE
	cancel_delayed_work_sync(&fn_data.gdev->wdt_work);
#endif
	
	cancel_delayed_work_sync(&fn_data.gdev->fwu_work);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	enable_irq_wake(fn_data.gdev->touch_irq);
#endif
#ifdef CONFIG_SAVE_CB_CHECK_VALUE
		fn_data.gdev->CB_ckstat = false;
#endif
	
	fn_data.gdev->ts_stat = TS_SUSPEND;

	gcore_touch_release_all_point(fn_data.gdev->input_device);
	
	GTP_DEBUG("gcore suspend end");
}

void gcore_resume(void)
{
	struct gcore_dev *gdev = fn_data.gdev;

	GTP_DEBUG("enter gcore resume");
	
#ifdef	CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
		if(fn_data.gdev->PS_Enale == true){
			tpd_enable_ps(1);
		}
	
#endif

	/* drv added by kuangliangjun, touch data reporting contrl, start */
	underwater_report_status = true;
	/* drv added by kuangliangjun, touch data reporting contrl, start */

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	disable_irq_wake(fn_data.gdev->touch_irq);
#endif

#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD
	gcore_request_firmware_update_work(NULL);
#else
#if CONFIG_GCORE_RESUME_EVENT_NOTIFY
	queue_delayed_work(fn_data.gdev->gtp_workqueue, &fn_data.gdev->resume_notify_work, msecs_to_jiffies(200));
#endif
	gcore_touch_release_all_point(fn_data.gdev->input_device);
#endif

	gdev->ts_stat = TS_NORMAL;

	GTP_DEBUG("gcore resume end");
}

#ifdef TP_RESUME_BY_FB_NOTIFIER
int gcore_ts_fb_notifier_callback(struct notifier_block *nb, \
											unsigned long value, void *v)
{
	struct gcore_dev *gdev = container_of(nb, struct gcore_dev, disp_notifier);
	int *data = (int *)v;
	
	if (gdev && v) {
		GTP_DEBUG("%s IN", __func__);
		if (value == MTK_DISP_EARLY_EVENT_BLANK) {
			if (*data == MTK_DISP_BLANK_POWERDOWN) {
				gcore_suspend();
			}
		} else if (value == MTK_DISP_EVENT_BLANK) {
			if (*data == MTK_DISP_BLANK_UNBLANK) {
				gcore_resume();
			}
		}
		GTP_DEBUG("%s OUT", __func__);
	} else {
		GTP_DEBUG("gcore IC can not suspend or resume");
		return -1;
	}

	return 0;


}

#else
static ssize_t ts_suspend_show(struct device *dev, struct device_attribute *attr, char *buf)
{
/* return sprintf(buf, "%s\n", */
/* cts_data->suspend ? "true" : "false"); */
	return 0;
}

static ssize_t ts_suspend_store(struct device *dev, struct device_attribute *attr, const char *buf,
				size_t count)
{
	if ((buf[0] == '1'))
		gcore_suspend();
	else if ((buf[0] == '0'))
		gcore_resume();

	return count;
}
static DEVICE_ATTR_RW(ts_suspend);

static struct attribute *gcore_dev_suspend_atts[] = {
	&dev_attr_ts_suspend.attr,
	NULL,
};

static const struct attribute_group gcore_dev_suspend_atts_group = {
	.attrs = gcore_dev_suspend_atts,
};

int gcore_sysfs_add_device(struct device *dev)
{
	int ret = 0;

	ret = sysfs_create_group(&dev->kobj, &gcore_dev_suspend_atts_group);
	if (ret) {
		GTP_ERROR("Add device attr failed!");
	}

	ret = sysfs_create_link(NULL, &dev->kobj, "touchscreen");
	if (ret < 0) {
		GTP_ERROR("Failed to create link!");
	}
	return 0;
}
#endif


static int __init touch_driver_init(void)
{
	GTP_DEBUG("touch driver init.");

	if (gcore_touch_bus_init()) {
		GTP_ERROR("bus init fail!");
		return -EPERM;
	}

	return 0;
}

/* should never be called */
static void __exit touch_driver_exit(void)
{
	gcore_touch_bus_exit();
}

module_init(touch_driver_init);
module_exit(touch_driver_exit);

MODULE_AUTHOR("GalaxyCore, Inc.");
MODULE_DESCRIPTION("GalaxyCore Touch Main Mudule");
MODULE_LICENSE("GPL");
