/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/ctype.h>

#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/input.h>
#include <linux/time.h>

#include <linux/sysfs.h>




#define TTM_DEVNAME "lepton_dev"


static int lepton_remove(struct platform_device *dev);
static int lepton_probe(struct platform_device *pdev);
static void lepton_shutdown(struct platform_device *dev);
//void lepton_rfid_pwr(u8 enable);

static const struct of_device_id lepton_of_match[] = {
	{.compatible = "prize,lepton_power"},
	{},
};
MODULE_DEVICE_TABLE(of, lepton_of_match);

//prize add by lipengpeng 20210330 start 
static int air_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	printk("lpp lepton enter suspend\n");
	//lepton_enter_suspend(0);
        return 0;
}

static int air_resume(struct platform_device *pdev)
{
	printk("lpp lepton enter resume\n");
	//lepton_enter_resume(0);
        return 0;
}
//prize add by lipengpeng 20210330 end

static struct platform_driver lepton_platform_driver = {
	.probe = lepton_probe,
	.remove = lepton_remove,
	.shutdown = lepton_shutdown,
//prize add by lipengpeng 20210330 start 
	.suspend = air_suspend,
    .resume = air_resume,
//prize add by lipengpeng 20210330 end 
	.driver = {
		   .name = TTM_DEVNAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = lepton_of_match,
#endif
	},
};

/*----------------------------------------------------------------------------*/

#ifdef CONFIG_PINCTRL
static struct pinctrl *lepton_gpio;
static struct pinctrl_state *lepton_reset_low; 
static struct pinctrl_state *lepton_reset_high;
static struct pinctrl_state *lepton_pdn_low;
static struct pinctrl_state *lepton_pdn_high;
static struct pinctrl_state *lepton_vdd_disable;
static struct pinctrl_state *lepton_vdd_en;
static struct pinctrl_state *lepton_vddio_disable;
static struct pinctrl_state *lepton_vddio_en;
static struct pinctrl_state *lepton_vddc_disable;
static struct pinctrl_state *lepton_vddc_en;

static unsigned int gpio_reset_status;
static unsigned int gpio_pdn_status;
static unsigned int gpio_vdd_status;
static unsigned int gpio_vddio_status;
static unsigned int gpio_vddc_status;

#endif

static int lepton_gpio_init(struct device	*dev)
{    
	int ret=0;
//	unsigned int mode;
	//const struct of_device_id *match;

	pr_debug("[lepton][GPIO] enter %s, %d\n", __func__, __LINE__);

	lepton_gpio = devm_pinctrl_get(dev);
	if (IS_ERR(lepton_gpio)) {
		ret = PTR_ERR(lepton_gpio);
		pr_info("[lepton][ERROR] Cannot find lepton_gpio!\n");
		return ret;
	}
//enable1
	lepton_reset_low = pinctrl_lookup_state(lepton_gpio, "lepton_reset_low");
	if (IS_ERR(lepton_reset_low)) {
		ret = PTR_ERR(lepton_reset_low);
		pr_info("[lepton][ERROR] Cannot find lepton_reset_low %d!\n",
			ret);
	}
	lepton_reset_high= pinctrl_lookup_state(lepton_gpio, "lepton_reset_high");
	if (IS_ERR(lepton_reset_high)) {
		ret = PTR_ERR(lepton_reset_high);
		pr_info("[lepton][ERROR] Cannot find lepton_reset_high %d!\n",
			ret);
	}
//enable2	
	lepton_pdn_low = pinctrl_lookup_state(lepton_gpio, "lepton_pdn_low");
	if (IS_ERR(lepton_pdn_low)) {
		ret = PTR_ERR(lepton_pdn_low);
		pr_info("[lepton][ERROR] Cannot find lepton_pdn_low %d!\n",
			ret);
	}
	lepton_pdn_high= pinctrl_lookup_state(lepton_gpio, "lepton_pdn_high");
	if (IS_ERR(lepton_pdn_high)) {
		ret = PTR_ERR(lepton_pdn_high);
		pr_info("[lepton][ERROR] Cannot find lepton_pdn_high %d!\n",
			ret);
	}
//enable3	
 	lepton_vdd_disable = pinctrl_lookup_state(lepton_gpio, "lepton_vdd_disable");
	if (IS_ERR(lepton_vdd_disable)) {
		ret = PTR_ERR(lepton_vdd_disable);
		pr_info("[lepton][ERROR] Cannot find lepton_vdd_disable %d!\n",
			ret);
	}
	lepton_vdd_en= pinctrl_lookup_state(lepton_gpio, "lepton_vdd_en");
	if (IS_ERR(lepton_vdd_en)) {
		ret = PTR_ERR(lepton_vdd_en);
		pr_info("[lepton][ERROR] Cannot find lepton_vdd_en %d!\n",
			ret);
	}
//enable4	
	lepton_vddio_disable = pinctrl_lookup_state(lepton_gpio, "lepton_vddio_disable");
	if (IS_ERR(lepton_vddio_disable)) {
		ret = PTR_ERR(lepton_vddio_disable);
		pr_info("[lepton][ERROR] Cannot find lepton_vddio_disable %d!\n",
			ret);
	}
	lepton_vddio_en= pinctrl_lookup_state(lepton_gpio, "lepton_vddio_en");
	if (IS_ERR(lepton_vddio_en)) {
		ret = PTR_ERR(lepton_vddio_en);
		pr_info("[lepton][ERROR] Cannot find lepton_vddio_en %d!\n",
			ret);
	}
//enable5
	lepton_vddc_disable = pinctrl_lookup_state(lepton_gpio, "lepton_vddc_disable");
	if (IS_ERR(lepton_vddc_disable)) {
		ret = PTR_ERR(lepton_vddc_disable);
		pr_info("[lepton][ERROR] Cannot find lepton_vddc_disable %d!\n",
			ret);
	}
	lepton_vddc_en= pinctrl_lookup_state(lepton_gpio, "lepton_vddc_en");
	if (IS_ERR(lepton_vddc_en)) {
		ret = PTR_ERR(lepton_vddc_en);
		pr_info("[lepton][ERROR] Cannot find lepton_vddc_en %d!\n",
			ret);
	}
	
	gpio_reset_status =of_get_named_gpio(dev->of_node, "gpio_reset_status", 0);
	gpio_pdn_status =of_get_named_gpio(dev->of_node, "gpio_pdn_status", 0);
	gpio_vdd_status =of_get_named_gpio(dev->of_node, "gpio_vdd_status", 0);
	gpio_vddio_status =of_get_named_gpio(dev->of_node, "gpio_vddio_status", 0);
	gpio_vddc_status =of_get_named_gpio(dev->of_node, "gpio_vddc_status", 0);

	printk("[lepton][GPIO] lepton_gpio_get_info end!\n");

    return ret;

}

static struct pinctrl *lepton_gpio;
static struct pinctrl_state *lepton_reset_low; 
static struct pinctrl_state *lepton_reset_high;
static struct pinctrl_state *lepton_pdn_low;
static struct pinctrl_state *lepton_pdn_high;
static struct pinctrl_state *lepton_vdd_disable;
static struct pinctrl_state *lepton_vdd_en;
static struct pinctrl_state *lepton_vddio_disable;
static struct pinctrl_state *lepton_vddio_en;
static struct pinctrl_state *lepton_vddc_disable;
static struct pinctrl_state *lepton_vddc_en;

static int power_status=0;
void lepton_power_on_off_filr(u8 enable)
{
    pr_debug("%s enable =%d\n", __func__);

    if(enable)
	{
	
	  power_status = 1;
	  pinctrl_select_state(lepton_gpio, lepton_vdd_en);  //enable vdd3.0v
	  mdelay(5);
	  pinctrl_select_state(lepton_gpio, lepton_vddio_en);  //enable vddio2.8v
	  mdelay(5);
	  pinctrl_select_state(lepton_gpio, lepton_vddc_en);  //enable vddc1.2v
	  mdelay(5);
	  pinctrl_select_state(lepton_gpio, lepton_pdn_high);  //pdn set high
	  mdelay(5);
	  pinctrl_select_state(lepton_gpio, lepton_reset_low);  //reset low
	  mdelay(5);
	  pinctrl_select_state(lepton_gpio, lepton_reset_high);  //reset high
	  mdelay(5);
	}
    else{
	  power_status = 0;	  
	  pinctrl_select_state(lepton_gpio, lepton_vdd_en);  //disable vdd3.0v
	  mdelay(5);
	  pinctrl_select_state(lepton_gpio, lepton_vddio_en);  //disable vddio2.8v
	  mdelay(5);
	  pinctrl_select_state(lepton_gpio, lepton_vddc_en);  //disable vddc1.2v
	  mdelay(5);
	  pinctrl_select_state(lepton_gpio, lepton_pdn_high);  //pdn set low
	  mdelay(5);
      pinctrl_select_state(lepton_gpio, lepton_reset_low);
	}

}

static ssize_t lepton_poweron_status_show(struct device_driver *ddri, char *buf)
{
	//struct lepton *lep = dev_get_drvdata(device);
	return sprintf(buf, "%d\n", power_status);
}

static ssize_t lepton_poweron_status_store(struct device_driver *ddri,
                    const char *buf, size_t tCount)
{

  int lepton_poweron_status_flag;
  int ret = 0;

  if (strlen(buf) < 1) {
      pr_notice("%s() Invalid input!!\n", __func__);
      return -EINVAL;
  }

  ret = sscanf(buf, "%d", &lepton_poweron_status_flag);

   printk("lepton_poweron_status_flag=%d\n",lepton_poweron_status_flag);
   
  if (lepton_poweron_status_flag == 1){
      lepton_power_on_off_filr(1);

   }else{
      lepton_power_on_off_filr(0);
  }
 
  return tCount;
} 

/*----------------------------------------------------------------------------*/

static DRIVER_ATTR_RW(lepton_poweron_status);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *lepton_attr_list[] = {    
	&driver_attr_lepton_poweron_status,
};


/*----------------------------------------------------------------------------*/
static int lepton_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(lepton_attr_list);

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, lepton_attr_list[idx]);
		if (err) {
			pr_err("driver_create_file (%s) = %d\n",
				   lepton_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int lepton_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(lepton_attr_list);

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, lepton_attr_list[idx]);

	return err;
}

static int lepton_probe(struct platform_device *pdev)
{

    const struct of_device_id *id;
	struct device	*dev = &pdev->dev;
    int err =0 ;
  	printk("lepton_probe start\n");
        
	id = of_match_node(lepton_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;
    lepton_gpio_init(dev);

    /* Register sysfs attribute */
	err = lepton_create_attr(&lepton_platform_driver.driver);
	if (err) {
		pr_err("create attribute err = %d\n", err);
		goto exit_sysfs_create_group_failed;
	}
    
    //leptonprt_enable(1);

	printk("lepton_probe done\n");

	return 0;
exit_sysfs_create_group_failed:
    return -1;

}

static int lepton_remove(struct platform_device *dev)
{
    int err = 0;
	err = lepton_delete_attr(&lepton_platform_driver.driver);
	if (err)
		pr_err("lepton_delete_attr fail: %d\n", err);

	return err;
}

static void lepton_shutdown(struct platform_device *dev)
{

}


static int __init lepton_init(void)
{
	int ret;

	pr_debug("Init start\n");    

	ret = platform_driver_register(&lepton_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	pr_debug("Init done\n");

	return 0;
}

static void __exit lepton_exit(void)
{
	pr_debug("Exit start\n");

	platform_driver_unregister(&lepton_platform_driver);

	pr_debug("Exit done\n");
}

module_init(lepton_init);
module_exit(lepton_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("pengpeng li <lipengpeng@szprize.com>");
MODULE_DESCRIPTION("MTK lepton Core Driver");

