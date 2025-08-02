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


#define BF2257CS_SENSOR_ID  0x2257


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

static inline char bf2257cs_write_cmos_sensor(struct i2c_client *client, char addr, char para)
{
    return i2c_write_reg(client, addr, para);
}
static char bf2257cs_read_cmos_sensor(struct i2c_client *client, char addr)
{
	char get_byte=0;

	get_byte = i2c_read_reg(client, addr);
    return get_byte;
}

static void bf2257cs_sensor_init(struct i2c_client *client)
{
	bf2257cs_write_cmos_sensor(client,0xf2, 0x01);
    bf2257cs_write_cmos_sensor(client,0x00, 0x01);//Bit[3]: Mirror Bit[2]: vFlip
	bf2257cs_write_cmos_sensor(client,0x02, 0xb7);
	bf2257cs_write_cmos_sensor(client,0x1e, 0x04);
	bf2257cs_write_cmos_sensor(client,0x24, 0x60);
	bf2257cs_write_cmos_sensor(client,0xe0, 0x00);
	bf2257cs_write_cmos_sensor(client,0xe1, 0x01);
	bf2257cs_write_cmos_sensor(client,0xe2, 0x08);
	bf2257cs_write_cmos_sensor(client,0xe5, 0xe3);
	bf2257cs_write_cmos_sensor(client,0xe6, 0x60);
	bf2257cs_write_cmos_sensor(client,0xe7, 0x33);
	bf2257cs_write_cmos_sensor(client,0xe8, 0x12);
	bf2257cs_write_cmos_sensor(client,0xe9, 0x89);
	bf2257cs_write_cmos_sensor(client,0xea, 0x87);
	bf2257cs_write_cmos_sensor(client,0xeb, 0x80);
	bf2257cs_write_cmos_sensor(client,0xec, 0x91);
	bf2257cs_write_cmos_sensor(client,0xed, 0x60);

	//MIPICLK:672M
	bf2257cs_write_cmos_sensor(client,0xe3, 0x78);
	bf2257cs_write_cmos_sensor(client,0xe4, 0xe0);

	//Dummy
	bf2257cs_write_cmos_sensor(client,0x06, 0x10);
	bf2257cs_write_cmos_sensor(client,0x07, 0x00);
	bf2257cs_write_cmos_sensor(client,0x0b, 0x80);
	bf2257cs_write_cmos_sensor(client,0x0c, 0x03);

	//Black target
	bf2257cs_write_cmos_sensor(client,0x59, 0x40);
	bf2257cs_write_cmos_sensor(client,0x5a, 0x40);
	bf2257cs_write_cmos_sensor(client,0x5b, 0x40);
	bf2257cs_write_cmos_sensor(client,0x5c, 0x40);

	//MIPI Setting
	bf2257cs_write_cmos_sensor(client,0x70, 0x08);
	bf2257cs_write_cmos_sensor(client,0x71, 0x07);
	bf2257cs_write_cmos_sensor(client,0x72, 0x12);
	bf2257cs_write_cmos_sensor(client,0x73, 0x09);
	bf2257cs_write_cmos_sensor(client,0x74, 0x08);
	bf2257cs_write_cmos_sensor(client,0x75, 0x06);
	bf2257cs_write_cmos_sensor(client,0x76, 0x20);
	bf2257cs_write_cmos_sensor(client,0x77, 0x02);
	bf2257cs_write_cmos_sensor(client,0x78, 0x10);
	bf2257cs_write_cmos_sensor(client,0x79, 0x09);
	bf2257cs_write_cmos_sensor(client,0x7a, 0x00);
	bf2257cs_write_cmos_sensor(client,0x7b, 0x00);
	bf2257cs_write_cmos_sensor(client,0x7c, 0x00);
	bf2257cs_write_cmos_sensor(client,0x7d, 0x0f);

	//Window 1600*1200
	bf2257cs_write_cmos_sensor(client,0xca, 0x60);
	bf2257cs_write_cmos_sensor(client,0xcb, 0x40);
	bf2257cs_write_cmos_sensor(client,0xcc, 0x04);
	bf2257cs_write_cmos_sensor(client,0xcd, 0x44);
	bf2257cs_write_cmos_sensor(client,0xce, 0x04);
	bf2257cs_write_cmos_sensor(client,0xcf, 0xb4);

	bf2257cs_write_cmos_sensor(client,0x6a, 0x0f);//Global gain
	bf2257cs_write_cmos_sensor(client,0x6b, 0x04);
	bf2257cs_write_cmos_sensor(client,0x6c, 0xe1);//{0x6b,0x6c}:int_time
	bf2257cs_write_cmos_sensor(client,0x6d, 0x0f);

	bf2257cs_write_cmos_sensor(client,0x01, 0x4b);//stream_on
	bf2257cs_write_cmos_sensor(client,0xf3, 0x00);
	bf2257cs_write_cmos_sensor(client,0xd0, 0x00);
	bf2257cs_write_cmos_sensor(client,0x01, 0x43);
}


static void bf2257cs_stream_on(struct i2c_client *client)
{
	//msleep(150);
	bf2257cs_write_cmos_sensor(client,0x01, 0x4b);
	bf2257cs_write_cmos_sensor(client,0xf3, 0x00);
	bf2257cs_write_cmos_sensor(client,0xd0, 0x00);
	bf2257cs_write_cmos_sensor(client,0x01, 0x43);
    //msleep(50);
}

static unsigned short bf2257cs_read_shutter(struct i2c_client *client)
{
	unsigned short shutter;

	shutter = bf2257cs_read_cmos_sensor(client,0x64);

	/* 15- 255 15 dark 255 brigth */
	if(shutter < 20) {
	    shutter = 500;
	} else {
	    shutter = 300 - shutter;
	}

	CAMERA_DBG("%s,shutter %d\n",__FUNCTION__,shutter);

	return shutter;
}

static unsigned int bf2257cs_get_sensor_id(struct i2c_client *client,unsigned int *sensorID)
{
    // check if sensor ID correct
    *sensorID=(bf2257cs_read_cmos_sensor(client,0xfc) << 8 | bf2257cs_read_cmos_sensor(client,0xfd));
	CAMERA_DBG("BF2257CS Read ID %x",*sensorID);

    return 0;
}

static int bf2257cs_set_power(struct i2c_client *client,unsigned int enable)
{
	struct spc_data_t *spc_data = i2c_get_clientdata(client);

    int  ret = 0;
	CAMERA_DBG("bf2257cs_set_power %d",enable);

	if (enable) {
		gpio_direction_output(spc_data->pdn_pin,0);
		//mdelay(5);
		//gpio_direction_output(spc_data->iovdd_pin,1);
		mdelay(5);
		gpio_direction_output(spc_data->avdd_pin,1);
		mdelay(5);
		gpio_direction_output(spc_data->pdn_pin,1);
	} else {
		gpio_direction_output(spc_data->pdn_pin,0);
		mdelay(5);
		gpio_direction_output(spc_data->avdd_pin,0);
		//mdelay(5);
		//gpio_direction_output(spc_data->iovdd_pin,0);
	}
    return ret;
}


static int bf2257cs_open(struct i2c_client *client)
{
	int i;
	unsigned short sensor_id=0;
	int id_status = 0;

	CAMERA_DBG("<Jet> bf2257cs_open");

	//  Read sensor ID to adjust I2C is OK?
	for (i = 0; i < 3; i++) {
		sensor_id = bf2257cs_read_cmos_sensor(client,0xfc) << 8 | bf2257cs_read_cmos_sensor(client,0xfd) ;
		CAMERA_DBG("*sensorID=%x %s",sensor_id,__func__);
		if (sensor_id != BF2257CS_SENSOR_ID) {
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

	CAMERA_DBG("bf2257csmipi_ Sensor Read ID OK \r\n");
	bf2257cs_sensor_init(client);

	return 0;
}

const struct sensor_info_t bf2257cs_info = {
	.sensor_type = SENSOR_TYPE_2257,
	.sensor_id = BF2257CS_SENSOR_ID,
	.open = bf2257cs_open,
	.init = bf2257cs_sensor_init,
	.stream_on = bf2257cs_stream_on,
	.get_shutter = bf2257cs_read_shutter,
	.get_sensor_id = bf2257cs_get_sensor_id,
	.set_power = bf2257cs_set_power,
};
