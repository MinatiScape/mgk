// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define LOG_TAG "BOARD_ID"

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
//#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static unsigned int GPIO_SEL1;
static unsigned int GPIO_SEL2;
static unsigned int GPIO_SEL3;
static unsigned int board_id_version = 0;

static const char * const hwid_type_text[] = {
	"0.0", /* EVB */
	"0.1", /* EVT */
	"0.2", /* DVT */
	"1.0", /* PVT */
	"1.0", /* PVT */
	"1.0", /* PVT */
	"1.0", /* PVT */
	"1.0", /* PVT */
};

static int prize_pcb_version_show(struct seq_file *m, void *data)
{
	seq_printf(m, "%s\n", hwid_type_text[board_id_version]);
	return 0;
}

static int prize_pcb_version_open(struct inode *node, struct file *file)
{
	return single_open(file, prize_pcb_version_show, PDE_DATA(node));
}

static const struct proc_ops prize_pcb_version_fops = {
	//.owner = THIS_MODULE,
	.proc_open = prize_pcb_version_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = NULL,
};

static int setup_board_id_proc_files(struct platform_device *pdev)
{
	int ret = 0;
	struct proc_dir_entry *pcb_config_dir = NULL, *entry = NULL;

	pcb_config_dir = proc_mkdir("pcb_config", NULL);
	if (!pcb_config_dir) {
		pr_err("%s: mkdir /proc/pcb_config failed\n", __func__);
		return -ENOMEM;
	}

	entry = proc_create("prize_pcb_version", 0644, pcb_config_dir,
					&prize_pcb_version_fops);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}

	return 0;

fail_procfs:
	remove_proc_subtree("pcb_config", NULL);
	return ret;
}

#define DPS_DEV_NAME  "prize,board-id"
#ifdef CONFIG_OF
static const struct of_device_id board_id_of_match[] = {
	{.compatible = DPS_DEV_NAME},
	{},
};
MODULE_DEVICE_TABLE(of, board_id_of_match);
#endif

static int board_id_probe(struct platform_device *pdev)
{
	struct device	*dev = &pdev->dev;
	const struct of_device_id *match;
	int ret = 0;
	unsigned int sel1, sel2, sel3;

	pr_err("%s:%d start\n",__func__,__LINE__);

	if (dev->of_node) {
		match = of_match_device(of_match_ptr(board_id_of_match), dev);
		if (!match) {
			pr_err("[board_id_probe][ERROR] No device match found\n");
			return -ENODEV;
		}
	}

	GPIO_SEL1 = of_get_named_gpio(dev->of_node, "board_id_sel1", 0);
	if (gpio_is_valid(GPIO_SEL1)) {
		ret = gpio_request(GPIO_SEL1, "board_id_sel1");
		if (ret < 0)
			pr_err("[BOARD_ID]Unable to request board_id_sel1\n");

		ret = gpio_direction_input(GPIO_SEL1);
		if (ret < 0) {
			pr_err("Failed to set GPIO%d as input pin(ret = %d)\n",GPIO_SEL1, ret);
		}
		sel1 = gpio_get_value(GPIO_SEL1);
	}
	else {
		sel1 = 0;
		dev_warn(dev, "%s: board id sel1 is not configured\n", __func__);
	}
	/*-----------------------------SEL1 END-------------------------*/

	GPIO_SEL2 = of_get_named_gpio(dev->of_node, "board_id_sel2", 0);
	if (gpio_is_valid(GPIO_SEL2)) {
		ret = gpio_request(GPIO_SEL2, "board_id_sel2");
		if (ret < 0)
			pr_err("[BOARD_ID]Unable to request board_id_sel2\n");
		else
			pr_err("[BOARD_ID]success to request board_id_sel2\n");

		ret = gpio_direction_input(GPIO_SEL2);
		if (ret < 0) {
			pr_err("Failed to set GPIO%d as input pin(ret = %d)\n",GPIO_SEL2, ret);
		}
		sel2 = gpio_get_value(GPIO_SEL2);
	}
	else {
		sel2 = 0;
		dev_warn(dev, "%s: board id sel2 is not configured\n", __func__);
	}
	/*-----------------------------SEL2 END-------------------------*/

	GPIO_SEL3 = of_get_named_gpio(dev->of_node, "board_id_sel3", 0);
	if (gpio_is_valid(GPIO_SEL3)) {
		ret = gpio_request(GPIO_SEL3, "board_id_sel3");
		if (ret < 0)
			pr_err("[BOARD_ID]Unable to request board_id_sel3\n");
		else
			pr_err("[BOARD_ID]success to request board_id_sel3\n");

		ret = gpio_direction_input(GPIO_SEL3);
		if (ret < 0) {
			pr_err("Failed to set GPIO%d as input pin(ret = %d)\n",GPIO_SEL3, ret);
		}
		sel3 = gpio_get_value(GPIO_SEL3);
	}
	else {
		sel3 = 0;
		dev_err(dev, "%s: board id sel3 is not configured\n", __func__);
	}
	/*-----------------------------SEL3 END-------------------------*/

	board_id_version = (sel1 << 2) | (sel2 << 1) | (sel3 << 0);
	pr_info("[BOARD_ID]sel1:%d, sel2:%d, sel3:%d,board_id_version:%d\n",
						sel1, sel2, sel3, board_id_version);

	setup_board_id_proc_files(pdev);
	return 0;
}

static int board_id_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver board_id_platform_driver = {
	.probe = board_id_probe,
	.remove = board_id_remove,
	.driver = {
		.name = DPS_DEV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = board_id_of_match,
#endif
	},
};

static int __init board_id_init(void)
{
	int ret;
	ret = platform_driver_register(&board_id_platform_driver);
	if (ret) {
		pr_err("Failed to register board_id platform driver\n");
		return ret;
	}

	pr_info("%s-%d end\n",__func__,__LINE__);
	return 0;
}

static void __exit board_id_exit(void)
{
	platform_driver_unregister(&board_id_platform_driver);
}

module_init(board_id_init);
module_exit(board_id_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("danil.liu <danil.liu@cooseagroup.com>");
MODULE_DESCRIPTION("board_id Driver");
