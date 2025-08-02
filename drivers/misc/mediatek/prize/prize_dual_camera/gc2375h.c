/**************************************************************************
*  double_camera.c
*
*  Create Date :
*
*  Modify Date :
*
*  Create by   : AWINIC Technology CO., LTD
*
*  Version     : 0.9, 2016/02/15
**************************************************************************/

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>


#include "prize_dual_cam.h"


#define GC2375H_SENSOR_ID 0x2375


static char i2c_write_reg(struct i2c_client *client, char addr, char reg_data)
{
	char ret;
	u8 wdbuf[512] = {0};

	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= wdbuf,
		},
	};

	wdbuf[0] = addr;
	wdbuf[1] = reg_data;

	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);

    return ret;
}

static char i2c_read_reg(struct i2c_client *client, char addr)
{
	char ret;
	u8 rdbuf[2] = {0};

	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rdbuf,
		},
		{
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= rdbuf,
		},
	};

	rdbuf[0] = addr;

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);

    return rdbuf[0];
}

static inline char gc2375h_write_cmos_sensor(struct i2c_client *client, char addr, char para)
{
    return i2c_write_reg(client, addr, para);
}
static char gc2375h_read_cmos_sensor(struct i2c_client *client, char addr)
{
	char get_byte=0;

	get_byte = i2c_read_reg(client, addr);
    return get_byte;
}

static void gc2375h_sensor_init(struct i2c_client *client)
{
	gc2375h_write_cmos_sensor(client,0xfe,0x00);
	gc2375h_write_cmos_sensor(client,0xfe,0x00);
	gc2375h_write_cmos_sensor(client,0xfe,0x00);
	gc2375h_write_cmos_sensor(client,0xf7,0x01);
	gc2375h_write_cmos_sensor(client,0xf8,0x0c);
	gc2375h_write_cmos_sensor(client,0xf9,0x42);
	gc2375h_write_cmos_sensor(client,0xfa,0x88);
	gc2375h_write_cmos_sensor(client,0xfc,0x8e);
	gc2375h_write_cmos_sensor(client,0xfe,0x00);
	gc2375h_write_cmos_sensor(client,0x88,0x03);


	/*Analog*/
	gc2375h_write_cmos_sensor(client,0x03,0x04);
	gc2375h_write_cmos_sensor(client,0x04,0x65);
	gc2375h_write_cmos_sensor(client,0x05,0x02);
	gc2375h_write_cmos_sensor(client,0x06,0x5a);
	gc2375h_write_cmos_sensor(client,0x07,0x00);
	gc2375h_write_cmos_sensor(client,0x08,0x10);
	gc2375h_write_cmos_sensor(client,0x09,0x00);
	gc2375h_write_cmos_sensor(client,0x0a,0x04);
	gc2375h_write_cmos_sensor(client,0x0b,0x00);
	gc2375h_write_cmos_sensor(client,0x0c,0x14);
	gc2375h_write_cmos_sensor(client,0x0d,0x04);
	gc2375h_write_cmos_sensor(client,0x0e,0xb8);
	gc2375h_write_cmos_sensor(client,0x0f,0x06);
	gc2375h_write_cmos_sensor(client,0x10,0x48);
	gc2375h_write_cmos_sensor(client,0x17,0xd7);
	gc2375h_write_cmos_sensor(client,0x1c,0x10);
	gc2375h_write_cmos_sensor(client,0x1d,0x13);
	gc2375h_write_cmos_sensor(client,0x20,0x0b);
	gc2375h_write_cmos_sensor(client,0x21,0x6d);
	gc2375h_write_cmos_sensor(client,0x22,0x0c);
	gc2375h_write_cmos_sensor(client,0x25,0xc1);
	gc2375h_write_cmos_sensor(client,0x26,0x0e);
	gc2375h_write_cmos_sensor(client,0x27,0x22);
	gc2375h_write_cmos_sensor(client,0x29,0x5f);
	gc2375h_write_cmos_sensor(client,0x2b,0x88);
	gc2375h_write_cmos_sensor(client,0x2f,0x12);
	gc2375h_write_cmos_sensor(client,0x38,0x86);
	gc2375h_write_cmos_sensor(client,0x3d,0x00);
	gc2375h_write_cmos_sensor(client,0xcd,0xa3);
	gc2375h_write_cmos_sensor(client,0xce,0x57);
	gc2375h_write_cmos_sensor(client,0xd0,0x09);
	gc2375h_write_cmos_sensor(client,0xd1,0xca);
	gc2375h_write_cmos_sensor(client,0xd2,0x34);
	gc2375h_write_cmos_sensor(client,0xd3,0xbb);
	gc2375h_write_cmos_sensor(client,0xd8,0x60);
	gc2375h_write_cmos_sensor(client,0xe0,0x08);
	gc2375h_write_cmos_sensor(client,0xe1,0x1f);
	gc2375h_write_cmos_sensor(client,0xe4,0xf8);

	gc2375h_write_cmos_sensor(client,0xe5,0x0c);
	gc2375h_write_cmos_sensor(client,0xe6,0x10);
	gc2375h_write_cmos_sensor(client,0xe7,0xcc);
	gc2375h_write_cmos_sensor(client,0xe8,0x02);
	gc2375h_write_cmos_sensor(client,0xe9,0x01);
	gc2375h_write_cmos_sensor(client,0xea,0x02);
	gc2375h_write_cmos_sensor(client,0xeb,0x01);

	/*Crop*/
	gc2375h_write_cmos_sensor(client,0x90,0x01);
	gc2375h_write_cmos_sensor(client,0x92,0x04);
	gc2375h_write_cmos_sensor(client,0x94,0x04);
	gc2375h_write_cmos_sensor(client,0x95,0x04);
	gc2375h_write_cmos_sensor(client,0x96,0xb0);
	gc2375h_write_cmos_sensor(client,0x97,0x06);
	gc2375h_write_cmos_sensor(client,0x98,0x40);

	/*BLK*/
	gc2375h_write_cmos_sensor(client,0x18,0x02);
	gc2375h_write_cmos_sensor(client,0x1a,0x18);
	gc2375h_write_cmos_sensor(client,0x28,0x00);
	gc2375h_write_cmos_sensor(client,0x3f,0x40);
	gc2375h_write_cmos_sensor(client,0x40,0x26);
	gc2375h_write_cmos_sensor(client,0x41,0x00);
	gc2375h_write_cmos_sensor(client,0x43,0x03);
	gc2375h_write_cmos_sensor(client,0x4a,0x00);
	gc2375h_write_cmos_sensor(client,0x4e,0x3c);
	gc2375h_write_cmos_sensor(client,0x4f,0x00);
	gc2375h_write_cmos_sensor(client,0x66,0xc0);
	gc2375h_write_cmos_sensor(client,0x67,0x00);

	/*Dark sun*/
	gc2375h_write_cmos_sensor(client,0x68,0x00);

	/*Gain*/
	gc2375h_write_cmos_sensor(client,0xb0,0x58);
	gc2375h_write_cmos_sensor(client,0xb1,0x01);
	gc2375h_write_cmos_sensor(client,0xb2,0x00);
	gc2375h_write_cmos_sensor(client,0xb6,0x00);

	/*MIPI*/
	gc2375h_write_cmos_sensor(client,0xef,0x90);
	gc2375h_write_cmos_sensor(client,0xfe,0x03);
	gc2375h_write_cmos_sensor(client,0x01,0x03);
	gc2375h_write_cmos_sensor(client,0x02,0x33);
	gc2375h_write_cmos_sensor(client,0x03,0x90);
	gc2375h_write_cmos_sensor(client,0x04,0x04);
	gc2375h_write_cmos_sensor(client,0x05,0x00);
	gc2375h_write_cmos_sensor(client,0x06,0x80);
	gc2375h_write_cmos_sensor(client,0x11,0x2b);
	gc2375h_write_cmos_sensor(client,0x12,0xd0);
	gc2375h_write_cmos_sensor(client,0x13,0x07);
	gc2375h_write_cmos_sensor(client,0x15,0x00);
	gc2375h_write_cmos_sensor(client,0x21,0x08);
	gc2375h_write_cmos_sensor(client,0x22,0x05);
	gc2375h_write_cmos_sensor(client,0x23,0x13);
	gc2375h_write_cmos_sensor(client,0x24,0x02);
	gc2375h_write_cmos_sensor(client,0x25,0x13);
	gc2375h_write_cmos_sensor(client,0x26,0x08);
	gc2375h_write_cmos_sensor(client,0x29,0x06);
	gc2375h_write_cmos_sensor(client,0x2a,0x08);
	gc2375h_write_cmos_sensor(client,0x2b,0x08);
	gc2375h_write_cmos_sensor(client,0xfe,0x00);

	gc2375h_write_cmos_sensor(client,0x8f, gc2375h_read_cmos_sensor(client,0x8f) | 0x17); // prize add for exact shutter

}


static void gc2375h_stream_on(struct i2c_client *client)
{
	//msleep(150);
	gc2375h_write_cmos_sensor(client,0xFE, 0X00);
	gc2375h_write_cmos_sensor(client,0xEF, 0X90);
	gc2375h_write_cmos_sensor(client,0xFE, 0X00);
    //msleep(50);
}

static unsigned short gc2375h_read_shutter(struct i2c_client *client)
{
	unsigned short shutter;

	shutter = gc2375h_read_cmos_sensor(client,0x3B);

	/* 0- 255 0 dark 255 brigth */
	if(shutter == 0) {
	    shutter = 500;
	} else {
	    shutter = 300 - shutter;
	}
	CAMERA_DBG("%s,shutter %d\n",__FUNCTION__,shutter);

	return shutter;
}

static unsigned int gc2375h_get_sensor_id(struct i2c_client *client,unsigned int *sensorID)
{
    // check if sensor ID correct
    *sensorID=((gc2375h_read_cmos_sensor(client,0xf0)<< 8)|gc2375h_read_cmos_sensor(client,0xf1));
	CAMERA_DBG("GC2375H Read ID %x",*sensorID);

    return 0;
}

static int gc2375h_set_power(struct i2c_client *client,unsigned int enable)
{
	struct spc_data_t *spc_data = i2c_get_clientdata(client);

    int  ret = 0;
	CAMERA_DBG("gc2375h_set_power 2");

	if (enable) {
		gpio_direction_output(spc_data->rst_pin,0);
		mdelay(5);
		gpio_direction_output(spc_data->pdn_pin,1);
		mdelay(5);
		gpio_direction_output(spc_data->avdd_pin,1);
		mdelay(5);
		gpio_direction_output(spc_data->pdn_pin,0);
		mdelay(5);
		gpio_direction_output(spc_data->rst_pin,1);
	} else {
		gpio_direction_output(spc_data->pdn_pin,1);
		mdelay(5);
		gpio_direction_output(spc_data->rst_pin,0);
		mdelay(5);
		gpio_direction_output(spc_data->avdd_pin,0);
		mdelay(10);
		gpio_direction_output(spc_data->pdn_pin,0);
	}
    return ret;
}


static int gc2375h_open(struct i2c_client *client)
{
	int i;
	unsigned short sensor_id=0;
	int id_status = 0;

	CAMERA_DBG("<Jet> gc2375h_open");

	//  Read sensor ID to adjust I2C is OK?
	for (i = 0; i < 3; i++) {
		sensor_id = gc2375h_read_cmos_sensor(client,0xf0) << 8 | gc2375h_read_cmos_sensor(client,0xf1) ;
		CAMERA_DBG("*sensorID=%x %s",sensor_id,__func__);
		if (sensor_id != GC2375H_SENSOR_ID) {
			mdelay(50);
			CAMERA_DBG("Read Sensor ID Fail[open] = 0x%x deley 50ms\n", sensor_id);
		} else {
			id_status = 1;
			break;
		}
	}
	if (!id_status) {
		return -EINVAL;
	}

	CAMERA_DBG("GC2375Hmipi_ Sensor Read ID OK \r\n");
	gc2375h_sensor_init(client);

	return 0;
}

const struct sensor_info_t gc2375h_info = {
	.sensor_type = SENSOR_TYPE_2375,
	.sensor_id = GC2375H_SENSOR_ID,
	.open = gc2375h_open,
	.init = gc2375h_sensor_init,
	.stream_on = gc2375h_stream_on,
	.get_shutter = gc2375h_read_shutter,
	.get_sensor_id = gc2375h_get_sensor_id,
	.set_power = gc2375h_set_power,
};
