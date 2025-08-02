// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Willsemi Co. Ltd.
 *
 * Author: Ray Deng <ray.deng@corp.ovt.com>
 */

#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#define WL28681_DEVICE_D1		0x00
#define WL28681_DEVICE_D2		0x01
#define WL28681_ILIM			0x02
#define WL28681_LDO_EN			0x03

#define WL28681_LDO1_VOUT		0x04
#define WL28681_LDO2_VOUT		0x05
#define WL28681_LDO3_VOUT		0x06
#define WL28681_LDO4_VOUT		0x07
#define WL28681_LDO5_VOUT		0x08
#define WL28681_LDO6_VOUT		0x09
#define WL28681_LDO7_VOUT		0x0a

#define WL28681_LDO1_LDO2_SEQ		0x0b
#define WL28681_LDO3_LDO4_SEQ		0x0c
#define WL28681_LDO5_LDO6_SEQ		0x0d
#define WL28681_LDO7_SEQ			0x0e
#define WL28681_SEQ_STATUS			0x0f

#define WL28681_DISCHARGE_RESISTORS		0x10
#define WL28681_RESET					0x11
#define WL28681_REPROGRAMMABLE_I2C_ADDR	0x12

#define WL28681_LDO1234_COMP	0x13
#define WL28681_LDO567_COMP		0x14

#define WL28681_UVP_INT			0x15
#define WL28681_OCP_INT			0x16
#define WL28681_TSD_UVLO_INT	0x17
#define WL28681_UVP_INT_STATUS			0x18
#define WL28681_OCP_INT_STATUS			0x19
#define WL28681_TSD_UVLO_INT_STATUS		0x1a
#define WL28681_SUSD_STATUS				0x1b
#define WL28681_UVP_INT_MASK			0x1c
#define WL28681_OCP_INT_MASK			0x1d
#define WL28681_TSD_UVLO_INT_MASK		0x1e

#define WL28681_VSEL_MASK		0xff

enum wl28681_regulators {
	WL28681_REGULATOR_LDO1 = 0,
	WL28681_REGULATOR_LDO2,
	WL28681_REGULATOR_LDO3,
	WL28681_REGULATOR_LDO4,
	WL28681_REGULATOR_LDO5,
	WL28681_REGULATOR_LDO6,
	WL28681_REGULATOR_LDO7,
	WL28681_MAX_REGULATORS,
};

//static const unsigned int wl28681_curr_table[] = {0, 1};
/* ldo1~7  0-current,1-current*/	
/*			740000, 950000
			50000, 700000
			740000, 950000
			500000, 700000
			500000, 700000
			1460000, 2000000
*/

struct wl28681 {
	struct device *dev;
	struct regmap *regmap;
	struct regulator_dev *rdev;
	struct gpio_desc *reset_gpio;
	int min_dropout_uv;
	int ldo_vout[7];
	int ldo_en;
};

static const struct regulator_ops wl28681_reg_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= regulator_set_voltage_sel_regmap,
	//.set_current_limit  = regulator_set_current_limit_regmap,
	.enable			= regulator_enable_regmap,
	.disable		= regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
};

#define WL28681_DESC(_id, _match, _supply, _min, _max, _step, _vreg,	\
	_vmask, _ereg, _emask, _enval, _disval)		\
	{								\
		.id		= (_id),				\
		.name		= (_match),				\
		.of_match	= of_match_ptr(_match),			\
		.supply_name	= (_supply),				\
		.min_uV		= (_min) * 1000,			\
		.uV_step	= (_step) * 1000,			\
		.n_voltages	= (((_max) - (_min)) / (_step) + 1),	\
		.regulators_node = of_match_ptr("regulators"),		\
		.type		= REGULATOR_VOLTAGE,			\
		.vsel_reg	= (_vreg),				\
		.vsel_mask	= (_vmask),				\
		.enable_reg	= (_ereg),				\
		.enable_mask	= (_emask),				\
		.enable_val     = (_enval),				\
		.disable_val     = (_disval),				\
		.ops		= &wl28681_reg_ops,			\
		.owner		= THIS_MODULE,				\
	}

static const struct regulator_desc wl28681_reg[] = {
	WL28681_DESC(WL28681_REGULATOR_LDO1, "WL_LDO1", "ldo1", 496, 2048, 8,
		     WL28681_LDO1_VOUT, WL28681_VSEL_MASK, WL28681_LDO_EN, BIT(0), 
			 BIT(0), 0),
	WL28681_DESC(WL28681_REGULATOR_LDO2, "WL_LDO2", "ldo2", 496, 2048, 8,
		     WL28681_LDO2_VOUT, WL28681_VSEL_MASK, WL28681_LDO_EN, BIT(1), 
			 BIT(1), 0),
	WL28681_DESC(WL28681_REGULATOR_LDO3, "WL_LDO3", "ldo3", 1372, 3412, 8,
		     WL28681_LDO3_VOUT, WL28681_VSEL_MASK, WL28681_LDO_EN, BIT(2), 
			 BIT(2), 0),
	WL28681_DESC(WL28681_REGULATOR_LDO4, "WL_LDO4", "ldo4", 1372, 3412, 8,
		     WL28681_LDO4_VOUT, WL28681_VSEL_MASK, WL28681_LDO_EN, BIT(3), 
			 BIT(3), 0),
	WL28681_DESC(WL28681_REGULATOR_LDO5, "WL_LDO5", "ldo5", 1372, 3412, 8,
		     WL28681_LDO5_VOUT, WL28681_VSEL_MASK, WL28681_LDO_EN, BIT(4), 
			 BIT(4), 0),
	WL28681_DESC(WL28681_REGULATOR_LDO6, "WL_LDO6", "ldo6", 1372, 3412, 8,
		     WL28681_LDO6_VOUT, WL28681_VSEL_MASK, WL28681_LDO_EN, BIT(5), 
			 BIT(5), 0),
	WL28681_DESC(WL28681_REGULATOR_LDO7, "WL_LDO7", "ldo7", 1372, 3412, 8,
		     WL28681_LDO7_VOUT, WL28681_VSEL_MASK, WL28681_LDO_EN, BIT(6),
			 BIT(6), 0),
};
#if 0
static const struct regmap_range wl28681_writeable_ranges[] = {
	regmap_reg_range(WL28681_DISCHARGE_RESISTORS, WL28681_SEQ_STATUS),
	regmap_reg_range(WL28681_ILIM, WL28681_LDO7_SEQ),
	regmap_reg_range(WL28681_DISCHARGE_RESISTORS, WL28681_TSD_UVLO_INT),
	regmap_reg_range(WL28681_UVP_INT_MASK, WL28681_TSD_UVLO_INT_MASK),
};

static const struct regmap_range wl28681_readable_ranges[] = {
	regmap_reg_range(WL28681_DEVICE_D1, WL28681_DEVICE_D2),
};

static const struct regmap_range wl28681_volatile_ranges[] = {
	regmap_reg_range(WL28681_DISCHARGE_RESISTORS, WL28681_SEQ_STATUS),
	regmap_reg_range(WL28681_ILIM, WL28681_LDO7_SEQ),
	regmap_reg_range(WL28681_DISCHARGE_RESISTORS, WL28681_TSD_UVLO_INT),
	regmap_reg_range(WL28681_UVP_INT_MASK, WL28681_TSD_UVLO_INT_MASK),
};
#else
static const struct regmap_range wl28681_writeable_ranges[] = {
	regmap_reg_range(WL28681_ILIM, WL28681_LDO567_COMP),
};

static const struct regmap_range wl28681_readable_ranges[] = {
	regmap_reg_range(WL28681_DEVICE_D1, WL28681_TSD_UVLO_INT_MASK),
};

static const struct regmap_range wl28681_volatile_ranges[] = {
	regmap_reg_range(WL28681_DEVICE_D1, WL28681_TSD_UVLO_INT_MASK),
};	
#endif
static const struct regmap_access_table wl28681_writeable_table = {
	.yes_ranges   = wl28681_writeable_ranges,
	.n_yes_ranges = ARRAY_SIZE(wl28681_writeable_ranges),
};

static const struct regmap_access_table wl28681_readable_table = {
	.yes_ranges   = wl28681_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(wl28681_readable_ranges),
};

static const struct regmap_access_table wl28681_volatile_table = {
	.yes_ranges   = wl28681_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(wl28681_volatile_ranges),
};

static const struct regmap_config wl28681_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x1F, //RESERVED_2,
	.wr_table = &wl28681_writeable_table,
	.rd_table = &wl28681_readable_table,
	.cache_type = REGCACHE_RBTREE,
	.volatile_table = &wl28681_volatile_table,
};

static void wl28681_reset(struct wl28681 *wl28681)
{
	gpiod_set_value_cansleep(wl28681->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(wl28681->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(wl28681->reset_gpio, 1);
	usleep_range(10000, 11000);
}

static int wl28681_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	const struct regulator_desc *regulators;
	struct wl28681 *wl28681;
	int ret, i;
	
	printk("%s enter\n", __func__);
	wl28681 = devm_kzalloc(dev, sizeof(struct wl28681), GFP_KERNEL);
	if (!wl28681)
		return -ENOMEM;

	wl28681->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(wl28681->reset_gpio)) {
		ret = PTR_ERR(wl28681->reset_gpio);
		dev_err(dev, "failed to request reset GPIO: %d\n", ret);
		return ret;
	}

	wl28681_reset(wl28681);

	i2c_set_clientdata(client, wl28681);
	wl28681->dev = dev;
	wl28681->regmap = devm_regmap_init_i2c(client, &wl28681_regmap_config);
	if (IS_ERR(wl28681->regmap)) {
		ret = PTR_ERR(wl28681->regmap);
		dev_err(dev, "Failed to allocate register map: %d\n", ret);
		return ret;
	}

	config.dev = &client->dev;
	config.regmap = wl28681->regmap;
	regulators = wl28681_reg;
	
	/* Instantiate the regulators */
	for (i = 0; i < WL28681_MAX_REGULATORS; i++) {
		rdev = devm_regulator_register(&client->dev,
					       &regulators[i], &config);
		if (IS_ERR(rdev)) {
			dev_err(&client->dev,
				"failed to register %d regulator\n", i);
			return PTR_ERR(rdev);
		}
	}

	/*Inital related mask for interrupt here*/
	regmap_write(wl28681->regmap, WL28681_UVP_INT_MASK, 0);
	regmap_write(wl28681->regmap, WL28681_OCP_INT_MASK, 0);
	regmap_write(wl28681->regmap, WL28681_TSD_UVLO_INT_MASK, 0);

	printk("%s end\n", __func__);
	
	return 0;
}

static void wl28681_regulator_shutdown(struct i2c_client *client)
{
	struct wl28681 *wl28681 = i2c_get_clientdata(client);
	if (system_state == SYSTEM_POWER_OFF)
		regmap_write(wl28681->regmap, WL28681_LDO_EN, 0x80);
}

static int __maybe_unused wl28681_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct wl28681 *wl28681 = i2c_get_clientdata(client);
	int i;
	
	regmap_read(wl28681->regmap, WL28681_LDO_EN, &wl28681->ldo_en);
	for (i = 0; i < ARRAY_SIZE(wl28681->ldo_vout); i++)
		regmap_read(wl28681->regmap, WL28681_LDO1_VOUT + i,
			    &wl28681->ldo_vout[i]);
					
	gpiod_set_value_cansleep(wl28681->reset_gpio, 0);
	
	return 0;
}

static int __maybe_unused wl28681_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct wl28681 *wl28681 = i2c_get_clientdata(client);
	int i;

	wl28681_reset(wl28681);
	for (i = 0; i < ARRAY_SIZE(wl28681->ldo_vout); i++)
		regmap_write(wl28681->regmap, WL28681_LDO1_VOUT + i,
			     wl28681->ldo_vout[i]);
	regmap_write(wl28681->regmap, WL28681_LDO_EN, wl28681->ldo_en);

	return 0;
}

static SIMPLE_DEV_PM_OPS(wl28681_pm_ops, wl28681_suspend, wl28681_resume);

static const struct i2c_device_id wl28681_i2c_id[] = {
	{ "wl28681", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, wl28681_i2c_id);

static const struct of_device_id wl28681_of_match[] = {
	{ .compatible = "willsemi,wl28681" },
	{}
};
MODULE_DEVICE_TABLE(of, wl28681_of_match);

static struct i2c_driver wl28681_i2c_driver = {
	.driver = {
		.name = "wl28681",
		.of_match_table = of_match_ptr(wl28681_of_match),
		.pm = &wl28681_pm_ops,
	},
	.id_table = wl28681_i2c_id,
	.probe	= wl28681_i2c_probe,
	.shutdown = wl28681_regulator_shutdown,
};

module_i2c_driver(wl28681_i2c_driver);

MODULE_DESCRIPTION("WL28681 regulator driver");
MODULE_AUTHOR("willsemi");
MODULE_LICENSE("GPL");
