// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define LOG_TAG "SIM_SWITCH"

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
#include <linux/leds.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define ESIM_STATE 2
#define PSIM_STATE 1
#define NULL_STATE 0

static unsigned int SWITCH_GPIO;
static unsigned int gpio_state;

struct sim_switch {
	struct device *dev;
	struct led_classdev cdev;
	struct work_struct brightness_work;
	unsigned int gpio_state;
	unsigned int pre_gpio_state;
};

static int sim_switch_get_state_show(struct seq_file *m, void *data)
{
	int status;

	status = gpio_get_value(SWITCH_GPIO);
	if (status) {
		gpio_state = ESIM_STATE;
	} else {
		gpio_state = PSIM_STATE;
	}
	pr_err("%s: simswitch-state:%d-%d\n", __func__, status, gpio_state);

	seq_printf(m, "%d\n", gpio_state);
	return 0;
}

static int sim_switch_open(struct inode *node, struct file *file)
{
	return single_open(file, sim_switch_get_state_show, PDE_DATA(node));
}


static ssize_t sim_switch_set_state_write(struct file *file,
		const char *buffer, size_t count, loff_t *data)
{
	int len = 0, ret = 0;
	char desc[32] = {0};
	unsigned int state = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	ret = kstrtou32(desc, 10, &state);
	if (ret == 0) {
		gpio_state = state;

		pr_err("%s: set gpio status:%\n", __func__, gpio_state);
		return count;
	}

	pr_err("%s: bad argument\n", __func__);
	return count;
}

static const struct proc_ops sim_switch_fops = {
	//.owner = THIS_MODULE,
	.proc_open = sim_switch_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = sim_switch_set_state_write,
};

#ifdef MODULE
static char __sim_switch_cmdline[2048];
static char *sim_switch_cmdline = __sim_switch_cmdline;

const char *sim_switch_get_cmd(void)
{
    struct device_node *of_chosen = NULL;
    char *bootargs = NULL;

    if (__sim_switch_cmdline[0] != 0)
        return sim_switch_cmdline;

    of_chosen = of_find_node_by_path("/chosen");
    if (of_chosen) {
        bootargs = (char *)of_get_property(
            of_chosen, "bootargs", NULL);
        if (!bootargs)
            pr_err("%s: failed to get bootargs\n", __func__);
        else {
            strcpy(__sim_switch_cmdline, bootargs);
            pr_err("%s: bootargs: %s\n", __func__, bootargs);
        }
    } else
        pr_err("%s: failed to get /chosen\n", __func__);

    return sim_switch_cmdline;
}
#else
const char *sim_switch_get_cmd(void)
{
	return saved_command_line;
}
#endif

static int sim_switch_get_cmdline_sim_state(void)
{
    char *ptr = NULL;
    int sim_state = 0;
    ptr = strstr(sim_switch_get_cmd(),"simSwitch=");
    if(ptr == NULL)
    {
        pr_err("%s get null sim_switch_get_cmd = %s\n", __func__, sim_switch_get_cmd());
        return -1;
    }
    pr_err("%s ptr = %s\n", __func__, ptr);
    ptr += strlen("simSwitch=");
    sim_state = simple_strtol(ptr, NULL, 10);
    pr_err("%s simswitch_state = %d\n", __func__, sim_state);

    return sim_state;
}

static ssize_t sim_switch_status_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct sim_switch *sim_switch = container_of(led_cdev, struct sim_switch, cdev);
	unsigned int databuf[1] = { 0 };
	int ret = 0;

	if (sscanf(buf, "%d", &databuf[0]) == 1) {
		pr_err("%s: data:%d\n", __func__, databuf[0]);
		sim_switch->pre_gpio_state = databuf[0];

		if (databuf[0] == 1 || databuf[0] == 2) {
			databuf[0] = databuf[0] - 1;

			ret = gpio_direction_output(SWITCH_GPIO, databuf[0]);
			if (ret < 0) {
				pr_err("[SIM_SWITCH]Unable to set gpio status\n");
			} else {
				pr_err("[SIM_SWITCH]set switch to %s successfully!\n",
						(databuf[0] == 2) ? "high" : "low");
			}
		}

	}

	return count;
}


static ssize_t sim_switch_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct sim_switch *sim_switch = container_of(led_cdev, struct sim_switch, cdev);
	ssize_t len = 0;
	int status;

	status = gpio_get_value(SWITCH_GPIO);
	if (status) {
		gpio_state = ESIM_STATE;
	} else {
		gpio_state = PSIM_STATE;
	}
	sim_switch->gpio_state = gpio_state;
	pr_err("%s: simswitch-state:%d-%d\n", __func__, status, gpio_state);

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", gpio_state);

	return len;
	
}

static DEVICE_ATTR(status, 0664, sim_switch_status_show, sim_switch_status_store);

static struct attribute *sim_switch_attributes[] = {
	&dev_attr_status.attr,
	NULL,
};

static struct attribute_group sim_switch_attribute_group = {
	.attrs = sim_switch_attributes
};

static void sim_switch_brightness_work(struct work_struct *work)
{
	struct sim_switch *sim_switch = container_of(work,
				struct sim_switch,
				brightness_work);

	pr_info("%s: enter\n", __func__);

	if (sim_switch->cdev.brightness) {
		pr_info("%s: brightness:%d\n", __func__, sim_switch->cdev.brightness);
	} else {
		pr_info("%s: brightness:%d\n", __func__, sim_switch->cdev.brightness);
	}

}

static void sim_switch_set_brightness(struct led_classdev *cdev,
				enum led_brightness brightness)
{
	struct sim_switch *sim_switch = container_of(cdev, struct sim_switch, cdev);

	sim_switch->cdev.brightness = brightness;

	schedule_work(&sim_switch->brightness_work);
}


static int setup_sim_switch_proc_files(struct platform_device *pdev)
{
	int ret = 0;
	struct proc_dir_entry *sim_switch_dir = NULL, *entry = NULL;

	sim_switch_dir = proc_mkdir("sim_switch", NULL);
	if (!sim_switch_dir) {
		pr_err("%s: mkdir /proc/sim_switch failed\n", __func__);
		return -ENOMEM;
	}

	entry = proc_create("status", 0666, sim_switch_dir,
					&sim_switch_fops);
	if (!entry) {
		ret = -ENODEV;
		goto fail_procfs;
	}

	return 0;

fail_procfs:
	remove_proc_subtree("sim_switch", NULL);
	return ret;
}

#define DPS_DEV_NAME  "prize,sim-switch"
#ifdef CONFIG_OF
static const struct of_device_id sim_switch_of_match[] = {
	{.compatible = DPS_DEV_NAME},
	{},
};
MODULE_DEVICE_TABLE(of, sim_switch_of_match);
#endif

static int sim_switch_probe(struct platform_device *pdev)
{
	struct device	*dev = &pdev->dev;
	const struct of_device_id *match;
	struct sim_switch *sim_switch;
	int ret = 0;
	bool value = false;
	int sim_state = 0;

	pr_err("%s:%d start\n",__func__,__LINE__);

	sim_switch = devm_kzalloc(&pdev->dev, sizeof(struct sim_switch), GFP_KERNEL);
	if (sim_switch == NULL)
		return -ENOMEM;
	sim_switch->dev = &pdev->dev;

	if (dev->of_node) {
		match = of_match_device(of_match_ptr(sim_switch_of_match), dev);
		if (!match) {
			pr_err("[SIM_SWITCH][ERROR] No device match found\n");
			return -ENODEV;
		}
	}

	ret = of_property_read_string(dev->of_node, "sim-switch,name",
						&sim_switch->cdev.name);
	if (ret) {
		pr_err("[SIM_SWITCH][ERROR] sim-switch,name is not absent\n");
		sim_switch->cdev.name = "sim-switch";
	}

	ret = of_property_read_u32(dev->of_node, "sim-switch,brightness",
						&sim_switch->cdev.brightness);
	if (ret) {
		pr_err("[SIM_SWITCH][ERROR] sim-switch,brightness is not absent\n");
		sim_switch->cdev.brightness = 128;
	}

	ret = of_property_read_u32(dev->of_node, "sim-switch,max_brightness",
						&sim_switch->cdev.max_brightness);
	if (ret) {
		pr_err("[SIM_SWITCH][ERROR] sim-switch,max_brightness is not absent\n");
		sim_switch->cdev.max_brightness = 255;
	}

	SWITCH_GPIO = of_get_named_gpio(dev->of_node, "switch_gpio", 0);
	if (gpio_is_valid(SWITCH_GPIO)) {
		ret = gpio_request(SWITCH_GPIO, "switch_gpio");
		if (ret < 0)
			pr_err("[SIM_SWITCH]Unable to request switch_gpio\n");
	} else {
		dev_warn(dev, "%s: sim switch gpio not match\n", __func__);
	}
	/*-----------------------------SWITCH_GPIO-------------------------*/
	sim_state = sim_switch_get_cmdline_sim_state();
	if(sim_state == ESIM_STATE)
		value = true;
	else
		value = false;
	ret = gpio_direction_output(SWITCH_GPIO, value);
	if (ret < 0) {
		pr_err("[SIM_SWITCH]Unable to set gpio status\n");
	} else {
		pr_err("[SIM_SWITCH]set switch to %s successfully!\n", value ? "high" : "low");
	}

	sim_switch->gpio_state = gpio_get_value(SWITCH_GPIO);
	if (sim_switch->gpio_state == value) {
		pr_err("[SIM_SWITCH]check set sim gpio status OK\n");
	}
	sim_switch->pre_gpio_state = sim_switch->gpio_state;

	INIT_WORK(&sim_switch->brightness_work, sim_switch_brightness_work);

	sim_switch->cdev.brightness_set = sim_switch_set_brightness;
	ret = led_classdev_register(sim_switch->dev, &sim_switch->cdev);
	if (ret){
		pr_err("[SIM_SWITCH]failed to create sim switch sys class\n");
		goto free_class;
	}

	ret = sysfs_create_group(&sim_switch->cdev.dev->kobj,
			       &sim_switch_attribute_group);
	if (ret) {
		pr_err("[SIM_SWITCH]failed to create sim switch sys group\n");
		goto free_class;
	}

	setup_sim_switch_proc_files(pdev);
	return 0;

free_class:
	led_classdev_unregister(&sim_switch->cdev);
	return ret;

}

static int sim_switch_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver sim_switch_platform_driver = {
	.probe = sim_switch_probe,
	.remove = sim_switch_remove,
	.driver = {
		.name = DPS_DEV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = sim_switch_of_match,
#endif
	},
};

static int __init sim_switch_init(void)
{
	int ret;
	ret = platform_driver_register(&sim_switch_platform_driver);
	if (ret) {
		pr_err("Failed to register sim_switch platform driver\n");
		return ret;
	}

	pr_info("%s-%d end\n",__func__,__LINE__);
	return 0;
}

static void __exit sim_switch_exit(void)
{
	platform_driver_unregister(&sim_switch_platform_driver);
}

module_init(sim_switch_init);
module_exit(sim_switch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("danil.liu <danil.liu@cooseagroup.com>");
MODULE_DESCRIPTION("sim_switch Driver");
