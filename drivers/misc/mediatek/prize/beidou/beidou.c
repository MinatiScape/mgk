#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/proc_fs.h>
#include <media/rc-core.h>

#define beidou_pwr_DEBUG
#ifdef beidou_pwr_DEBUG
#define beidou_pwr_log(fmt,arg...) \
	do{\
		printk("<<beidou_pwr-drv>>[%d]"fmt"", __LINE__, ##arg);\
    }while(0)
#else
#define camera_als_dbg(fmt,arg...)
#endif

struct pinctrl *beidou_pwr_pinctrl;
struct pinctrl_state *beidou_en_on, *beidou_en_off;
struct pinctrl_state *beidou_reset_on, *beidou_reset_off;
struct pinctrl_state *beidou_ldo_on, *beidou_ldo_off;
struct pinctrl_state *beidou_vcc3v3_en_on, *beidou_vcc3v3_en_off;
struct class *beidou_pwr_class;
static int beidou_pwr_status=0;

static ssize_t beidou_pwr_show(struct device *dev, struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", beidou_pwr_status);
}

static ssize_t beidou_pwr_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t size)
{
	if(sscanf(buf, "%u", &beidou_pwr_status) != 1)
	{
		beidou_pwr_log("[beidou]: Invalid values\n");
		return -EINVAL;
	}

	beidou_pwr_log("[beidou] %s beidou_pwr_status value = %d [0:OFF ; mode 1~3 ; other:Singular set thresh, Even numbers set data_width ]\n ", __func__, beidou_pwr_status);

	if(beidou_pwr_status == 0)
	{
		printk("beidou_pwr_set_gpio_output off!\n");
		pinctrl_select_state(beidou_pwr_pinctrl, beidou_reset_off);
		pinctrl_select_state(beidou_pwr_pinctrl, beidou_en_off);
		pinctrl_select_state(beidou_pwr_pinctrl, beidou_vcc3v3_en_off);
		pinctrl_select_state(beidou_pwr_pinctrl, beidou_ldo_off);
	}
	else if(beidou_pwr_status > 0)
	{
		printk("beidou_pwr_set_gpio_output on!\n");
		pinctrl_select_state(beidou_pwr_pinctrl, beidou_ldo_on);
		pinctrl_select_state(beidou_pwr_pinctrl, beidou_vcc3v3_en_on);
		pinctrl_select_state(beidou_pwr_pinctrl, beidou_en_on);
		pinctrl_select_state(beidou_pwr_pinctrl, beidou_reset_on);
		
	}
	else 
		return -EINVAL; 

	return size;
}
static DEVICE_ATTR(beidou_pwr, 0664, beidou_pwr_show, beidou_pwr_store);


int beidou_pwr_dts(struct platform_device *pdev)
{
	int ret;
	beidou_pwr_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(beidou_pwr_pinctrl)) {
		ret = PTR_ERR(beidou_pwr_pinctrl);
		printk("fwq Cannot find beidou_pwr_pinctrl!\n");
		return ret;
	}
	
	beidou_en_on = pinctrl_lookup_state(beidou_pwr_pinctrl, "beidou_en_on");
	if (IS_ERR(beidou_en_on)) {
		ret = PTR_ERR(beidou_en_on);
		printk("fwq Cannot find beidou pinctrl beidou_en_on!\n");
		return ret;
	}
	
	beidou_en_off = pinctrl_lookup_state(beidou_pwr_pinctrl, "beidou_en_off");
	if (IS_ERR(beidou_en_off)) {
		ret = PTR_ERR(beidou_en_off);
		printk("fwq Cannot find beidou pinctrl beidou_en_off!\n");
		return ret;
	}

	beidou_reset_on = pinctrl_lookup_state(beidou_pwr_pinctrl, "beidou_reset_on");
	if (IS_ERR(beidou_reset_on)) {
		ret = PTR_ERR(beidou_reset_on);
		printk("fwq Cannot find beidou_pwr_pinctrl beidou_reset_on!\n");
		return ret;
	}
	
	beidou_reset_off = pinctrl_lookup_state(beidou_pwr_pinctrl, "beidou_reset_off");
	if (IS_ERR(beidou_reset_off)) {
		ret = PTR_ERR(beidou_reset_off);
		printk("fwq Cannot find beidou_pwr_pinctrl  beidou_reset_off!\n");
		return ret;
	}

	beidou_ldo_on = pinctrl_lookup_state(beidou_pwr_pinctrl, "beidou_ldo_on");
	if (IS_ERR(beidou_ldo_on)) {
		ret = PTR_ERR(beidou_ldo_on);
		printk("fwq Cannot find beidou_pwr_pinctrl beidou_ldo_on!\n");
		return ret;
	}
	
	beidou_ldo_off = pinctrl_lookup_state(beidou_pwr_pinctrl, "beidou_ldo_off");
	if (IS_ERR(beidou_ldo_off)) {
		ret = PTR_ERR(beidou_ldo_off);
		printk("fwq Cannot find beidou_pwr_pinctrl  beidou_ldo_off!\n");
		return ret;
	}
	
	beidou_vcc3v3_en_on = pinctrl_lookup_state(beidou_pwr_pinctrl, "beidou_vcc3v3_en_on");
	if (IS_ERR(beidou_vcc3v3_en_on)) {
		ret = PTR_ERR(beidou_vcc3v3_en_on);
		printk("fwq Cannot find beidou_pwr_pinctrl beidou_vcc3v3_en_on!\n");
		return ret;
	}
	
	beidou_vcc3v3_en_off = pinctrl_lookup_state(beidou_pwr_pinctrl, "beidou_vcc3v3_en_off");
	if (IS_ERR(beidou_vcc3v3_en_off)) {
		ret = PTR_ERR(beidou_vcc3v3_en_off);
		printk("fwq Cannot find beidou_pwr_pinctrl  beidou_vcc3v3_en_off!\n");
		return ret;
	}

	pinctrl_select_state(beidou_pwr_pinctrl, beidou_en_off);
	pinctrl_select_state(beidou_pwr_pinctrl, beidou_reset_off);
	pinctrl_select_state(beidou_pwr_pinctrl, beidou_ldo_off);
	pinctrl_select_state(beidou_pwr_pinctrl, beidou_vcc3v3_en_off);
		
	return 0;
}

static int beidou_pwr_probe(struct platform_device *pdev)
{
    int ret;
	struct device *beidou_pwr_dev;
    beidou_pwr_log("%s\n", __func__);
    
    ret = beidou_pwr_dts(pdev);
    if (ret != 0) {
        beidou_pwr_log("beidou_pwr_dts failed!\n");
        return -1;
    }
    
    beidou_pwr_class = class_create(THIS_MODULE, "beidou_pwr");
	if (IS_ERR(beidou_pwr_class)) {
		beidou_pwr_log("Failed to create class(beidou_pwr_class)!");
		return PTR_ERR(beidou_pwr_class);
	}
	beidou_pwr_dev = device_create(beidou_pwr_class, NULL, 0, NULL, "beidou_pwr_data");
	if (IS_ERR(beidou_pwr_dev))
		beidou_pwr_log("Failed to create beidou_pwr_dev device");

	if (device_create_file(beidou_pwr_dev, &dev_attr_beidou_pwr) < 0)
		beidou_pwr_log("Failed to create device file(%s)!",dev_attr_beidou_pwr.attr.name);	

	beidou_pwr_log("%s OK\n", __func__);
    return 0;
}

static int beidou_pwr_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id beidou_pwr_of_match[] = {
	{ .compatible = "kingtop,beidou" },
	{ }
};
MODULE_DEVICE_TABLE(of, beidou_pwr_of_match);

static struct platform_driver beidou_pwr_driver = {
	.probe = beidou_pwr_probe,
	.remove = beidou_pwr_remove,
	.driver = {
		.name = "beidou_pwr",
		.of_match_table = of_match_ptr(beidou_pwr_of_match),
	},
};

static int __init beidou_pwr_init(void) {
	int ret;
    
	beidou_pwr_log("%s\n", __func__);
    
	ret = platform_driver_register(&beidou_pwr_driver);
	if (ret) {
		beidou_pwr_log("****[%s] Unable to register driver (%d)\n", __func__, ret);
		return ret;
	}
	
	return 0;
}

static void __exit beidou_pwr_exit(void) {
	beidou_pwr_log("%s\n", __func__);
	platform_driver_unregister(&beidou_pwr_driver);
}

late_initcall(beidou_pwr_init);
module_exit(beidou_pwr_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<yaozhipeng@szprize.com >");
MODULE_DESCRIPTION("beidou power Driver");

