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


#define GC6133_SENSOR_ID 0xBA


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

static inline char gc6133_write_cmos_sensor(struct i2c_client *client, char addr, char para)
{
    return i2c_write_reg(client, addr, para);
}
static char gc6133_read_cmos_sensor(struct i2c_client *client, char addr)
{
	char get_byte=0;
   
	get_byte = i2c_read_reg(client, addr);
    return get_byte;
}

static void gc6133_sensor_init(struct i2c_client *client)
{
	gc6133_write_cmos_sensor(client,0xfe, 0xa0);
	gc6133_write_cmos_sensor(client,0xfe, 0xa0);
	gc6133_write_cmos_sensor(client,0xfe, 0xa0);

	gc6133_write_cmos_sensor(client,0xf6, 0x00);
	gc6133_write_cmos_sensor(client,0xfa, 0x11);
	gc6133_write_cmos_sensor(client,0xfc, 0x12); //clock enable

	gc6133_write_cmos_sensor(client,0xfe,0x00);
	gc6133_write_cmos_sensor(client,0x49, 0x70);  //AWB r gain
	gc6133_write_cmos_sensor(client,0x4a, 0x40);  //AWB g gain
	gc6133_write_cmos_sensor(client,0x4b, 0x5d);  //AWB b gain
	/////////////////////////////////////////////////////
	////////////////   ANALOG & CISCTL	 ////////////////
	/////////////////////////////////////////////////////
	gc6133_write_cmos_sensor(client,0x03, 0x00);
	gc6133_write_cmos_sensor(client,0x04, 0xfa);
	gc6133_write_cmos_sensor(client,0x01, 0x41); //hb
	gc6133_write_cmos_sensor(client,0x02, 0x12); //vb
	gc6133_write_cmos_sensor(client,0x0f, 0x01);
	gc6133_write_cmos_sensor(client,0x0d, 0x30);
	gc6133_write_cmos_sensor(client,0x12, 0xc8);
	gc6133_write_cmos_sensor(client,0x14, 0x54); //dark CFA
	gc6133_write_cmos_sensor(client,0x15, 0x32); //1:sdark 0:ndark
	gc6133_write_cmos_sensor(client,0x16, 0x04);
	gc6133_write_cmos_sensor(client,0x17, 0x19);
	gc6133_write_cmos_sensor(client,0x1d, 0xb9);
	gc6133_write_cmos_sensor(client,0x1f, 0x15); //PAD_drv
	gc6133_write_cmos_sensor(client,0x7a, 0x00);
	gc6133_write_cmos_sensor(client,0x7b, 0x14);
	gc6133_write_cmos_sensor(client,0x7d, 0x36);
	gc6133_write_cmos_sensor(client,0xfe, 0x10);  //add by 20160217 CISCTL rst [4]
	/////////////////////////////////////////////////////
	//////////////////////   ISP   //////////////////////
	/////////////////////////////////////////////////////
	gc6133_write_cmos_sensor(client,0x20, 0x7e);
	gc6133_write_cmos_sensor(client,0x22, 0xb8);
	gc6133_write_cmos_sensor(client,0x24, 0x54); //output_format
	gc6133_write_cmos_sensor(client,0x26, 0x87); //[5]Y_switch [4]UV_switch [2]skin_en
	//gc6133_write_cmos_sensor(client,0x29, 0x10);// disable isp quiet mode

	gc6133_write_cmos_sensor(client,0x39, 0x00); //crop window
	gc6133_write_cmos_sensor(client,0x3a, 0x80); 
	gc6133_write_cmos_sensor(client,0x3b, 0x01);  //width
	gc6133_write_cmos_sensor(client,0x3c, 0x40); 
	gc6133_write_cmos_sensor(client,0x3e, 0xf0); //height
	/////////////////////////////////////////////////////
	//////////////////////   BLK   //////////////////////
	/////////////////////////////////////////////////////
	gc6133_write_cmos_sensor(client,0x2a, 0x2f);
	gc6133_write_cmos_sensor(client,0x37, 0x46); //[4:0]blk_select_row

	/////////////////////////////////////////////////////
	//////////////////////   GAIN   /////////////////////
	/////////////////////////////////////////////////////
	gc6133_write_cmos_sensor(client,0x3f, 0x18); //global gain 20160901


	/////////////////////////////////////////////////////
	//////////////////////   DNDD   /////////////////////
	/////////////////////////////////////////////////////
	gc6133_write_cmos_sensor(client,0x50, 0x3c); 
	gc6133_write_cmos_sensor(client,0x52, 0x4f); 
	gc6133_write_cmos_sensor(client,0x53, 0x81);
	gc6133_write_cmos_sensor(client,0x54, 0x43);
	gc6133_write_cmos_sensor(client,0x56, 0x78); 
	gc6133_write_cmos_sensor(client,0x57, 0xaa);//20160901 
	gc6133_write_cmos_sensor(client,0x58, 0xff);//20160901

	/////////////////////////////////////////////////////
	//////////////////////   ASDE   /////////////////////
	/////////////////////////////////////////////////////
	gc6133_write_cmos_sensor(client,0x5b, 0x60); //dd&ee th
	gc6133_write_cmos_sensor(client,0x5c, 0x80); //60/OT_th
	gc6133_write_cmos_sensor(client,0xab, 0x28); 
	gc6133_write_cmos_sensor(client,0xac, 0xb5);

	/////////////////////////////////////////////////////
	/////////////////////   INTPEE   ////////////////////
	/////////////////////////////////////////////////////
	gc6133_write_cmos_sensor(client,0x60, 0x45); 
	gc6133_write_cmos_sensor(client,0x62, 0x68); //20160901
	gc6133_write_cmos_sensor(client,0x63, 0x13); //edge effect
	gc6133_write_cmos_sensor(client,0x64, 0x43);

	/////////////////////////////////////////////////////
	//////////////////////   CC   ///////////////////////
	/////////////////////////////////////////////////////
	gc6133_write_cmos_sensor(client,0x65, 0x13); //Y
	gc6133_write_cmos_sensor(client,0x66, 0x26);
	gc6133_write_cmos_sensor(client,0x67, 0x07);
	gc6133_write_cmos_sensor(client,0x68, 0xf5); //Cb
	gc6133_write_cmos_sensor(client,0x69, 0xea);
	gc6133_write_cmos_sensor(client,0x6a, 0x21);
	gc6133_write_cmos_sensor(client,0x6b, 0x21); //Cr
	gc6133_write_cmos_sensor(client,0x6c, 0xe4);
	gc6133_write_cmos_sensor(client,0x6d, 0xfb);

	/////////////////////////////////////////////////////
	//////////////////////   YCP   //////////////////////
	/////////////////////////////////////////////////////
	gc6133_write_cmos_sensor(client,0x81, 0x30); //cb
	gc6133_write_cmos_sensor(client,0x82, 0x30); //cr
	gc6133_write_cmos_sensor(client,0x83, 0x4a); //luma contrast
	gc6133_write_cmos_sensor(client,0x85, 0x06);  //luma offset
	gc6133_write_cmos_sensor(client,0x8d, 0x78); //edge dec sa
	gc6133_write_cmos_sensor(client,0x8e, 0x25); //autogray

	/////////////////////////////////////////////////////
	//////////////////////   AEC   //////////////////////
	/////////////////////////////////////////////////////
	gc6133_write_cmos_sensor(client,0x90, 0x38);//20160901
	gc6133_write_cmos_sensor(client,0x92, 0x50); //target
	gc6133_write_cmos_sensor(client,0x9d, 0x32);//STEP
	gc6133_write_cmos_sensor(client,0x9e, 0x61);//[7:4]margin  10fps
	gc6133_write_cmos_sensor(client,0x9f, 0xf4);
	gc6133_write_cmos_sensor(client,0xa3, 0x28); //pregain
	gc6133_write_cmos_sensor(client,0xa4, 0x01); 

	/////////////////////////////////////////////////////
	//////////////////////   AWB   //////////////////////
	/////////////////////////////////////////////////////
#if 0//AWB2
	gc6133_write_cmos_sensor(client,0xb0, 0xf2); //Y_to_C_diff
	gc6133_write_cmos_sensor(client,0xb1, 0x10); //Y_to_C_diff
	gc6133_write_cmos_sensor(client,0xb2, 0x08); //AWB_Y_to_C_diff_big
	gc6133_write_cmos_sensor(client,0xb3, 0x30); //C_max   //20
	gc6133_write_cmos_sensor(client,0xb4, 0x40); 
	gc6133_write_cmos_sensor(client,0xb5, 0x20); //inter
	gc6133_write_cmos_sensor(client,0xb6, 0x34); //inter2
	gc6133_write_cmos_sensor(client,0xb7, 0x48); //AWB_C_inter3	 18
	gc6133_write_cmos_sensor(client,0xba, 0x40);   // big c  20
	gc6133_write_cmos_sensor(client,0xbb, 0x71); //62//AWB adjust   72
	gc6133_write_cmos_sensor(client,0xbd, 0x7a); //R_limit  80
	gc6133_write_cmos_sensor(client,0xbe, 0x40); //G_limit   58
	gc6133_write_cmos_sensor(client,0xbf, 0x80); //B_limit    a0	
#else
	gc6133_write_cmos_sensor(client,0xb1, 0x1e); //Y_to_C_diff
	gc6133_write_cmos_sensor(client,0xb3, 0x20); //C_max
	gc6133_write_cmos_sensor(client,0xbd, 0x70); //R_limit
	gc6133_write_cmos_sensor(client,0xbe, 0x58); //G_limit
	gc6133_write_cmos_sensor(client,0xbf, 0xa0); //B_limit

	gc6133_write_cmos_sensor(client,0xfe, 0x00);	//20160901 update for AWB
	gc6133_write_cmos_sensor(client,0x43, 0xa8); 
	gc6133_write_cmos_sensor(client,0xb0, 0xf2); 
	gc6133_write_cmos_sensor(client,0xb5, 0x40); 
	gc6133_write_cmos_sensor(client,0xb8, 0x05); 
	gc6133_write_cmos_sensor(client,0xba, 0x60); 	
#endif

	/////////////////////////////////////////////////////
	////////////////////   Banding   ////////////////////
	/////////////////////////////////////////////////////
	//gc6133_write_cmos_sensor(client,0x01, 0x41); //hb
	//gc6133_write_cmos_sensor(client,0x02, 0x12); //vb
	//gc6133_write_cmos_sensor(client,0x0f, 0x01);
	//gc6133_write_cmos_sensor(client,0x9d, 0x32); //step
	//gc6133_write_cmos_sensor(client,0x9e, 0x61); //[7:4]margin  10fps
	//gc6133_write_cmos_sensor(client,0x9f, 0xf4); 
	/////////////////////////////////////////////////////
	//////////////////////   SPI   //////////////////////
	/////////////////////////////////////////////////////
	gc6133_write_cmos_sensor(client,0xfe,0x02);
	gc6133_write_cmos_sensor(client,0x01, 0x01); //spi enable
	gc6133_write_cmos_sensor(client,0x02, 0x02);   //LSB & Falling edge sample; ddr disable
	gc6133_write_cmos_sensor(client,0x03, 0x20);	//1-wire
	gc6133_write_cmos_sensor(client,0x04, 0x20);   //[4] master_outformat
	gc6133_write_cmos_sensor(client,0x0a, 0x00);   //Data ID, 0x00-YUV422, 0x01-RGB565
	gc6133_write_cmos_sensor(client,0x13, 0x10);
	gc6133_write_cmos_sensor(client,0x24, 0x00); //[1]sck_always [0]BT656
	gc6133_write_cmos_sensor(client,0x28, 0x03); //clock_div_spi
	
	gc6133_write_cmos_sensor(client,0xfe,0x00);
	////////////////////////////////////////////////////
	///////////////////////output//////////////////////
	///////////////////////////////////////////////////
	gc6133_write_cmos_sensor(client,0x22, 0xf8); //open awb
	gc6133_write_cmos_sensor(client,0xf1, 0x03); //output enable
	
}

static void gc6133_stream_on(struct i2c_client *client)
{
	//msleep(150);
    gc6133_write_cmos_sensor(client,0xfe,0x03);
    gc6133_write_cmos_sensor(client,0x10,0x94);
    gc6133_write_cmos_sensor(client,0xfe,0x00);
    //msleep(50);
}

static unsigned short gc6133_read_shutter(struct i2c_client *client)
{
    unsigned char temp_reg1, temp_reg2;
	unsigned short shutter;
	
	temp_reg1 = gc6133_read_cmos_sensor(client,0x04);
	temp_reg2 = gc6133_read_cmos_sensor(client,0x03);

	shutter = (temp_reg1 & 0xFF) | (temp_reg2 << 8);
	//CAMERA_DBG("GC6133MIPI_Read_Shutter %d\r\n",shutter);
	return shutter;
}

static unsigned int gc6133_get_sensor_id(struct i2c_client *client,unsigned int *sensorID)
{
    // check if sensor ID correct
    *sensorID=((gc6133_read_cmos_sensor(client,0xf0)<< 8)|gc6133_read_cmos_sensor(client,0xf1));
	CAMERA_DBG("GC6133 Read ID %x",*sensorID);

    return 0;    
}

static int gc6133_set_power(struct i2c_client *client,unsigned int enable)
{
	struct spc_data_t *spc_data = i2c_get_clientdata(client);
	
    int  ret = 0;
	CAMERA_DBG("gc6133_set_power 2");
	if (enable){
		gpio_direction_output(spc_data->pdn_pin,1);
		mdelay(10);
		gpio_direction_output(spc_data->avdd_pin,1);
		mdelay(15);
		gpio_direction_output(spc_data->pdn_pin,0);
		mdelay(5);
		gpio_direction_output(spc_data->pdn_pin,1);
		mdelay(5);
		gpio_direction_output(spc_data->pdn_pin,0);
		mdelay(15);
	}else{
		gpio_direction_output(spc_data->pdn_pin,1);
		mdelay(5);
		gpio_direction_output(spc_data->avdd_pin,0);
		mdelay(5);
		gpio_direction_output(spc_data->pdn_pin,0);
	}
    return ret;    
}


static int gc6133_open(struct i2c_client *client)
{
	int i;
	unsigned short sensor_id=0;
	int id_status = 0;

	CAMERA_DBG("<Jet> gc6133_open");

	//  Read sensor ID to adjust I2C is OK?
	for(i=0;i<3;i++)
	{
		sensor_id = gc6133_read_cmos_sensor(client,0xf0);
		CAMERA_DBG("*sensorID=%x %s",sensor_id,__func__);
		if(sensor_id != GC6133_SENSOR_ID)  
		{
			CAMERA_DBG("Read Sensor ID Fail[open] = 0x%x\n", sensor_id); 
			return -EINVAL;
		}else{
			id_status = 1;
			break;
		}
	}
	if (!id_status){
		return -EINVAL;
	}
	
	CAMERA_DBG("GC6133mipi_ Sensor Read ID OK \r\n");
	gc6133_sensor_init(client);

	return 0;
}

const struct sensor_info_t gc6133_info = {
	.sensor_type = SENSOR_TYPE_0310,
	.sensor_id = GC6133_SENSOR_ID,
	.open = gc6133_open,
	.init = gc6133_sensor_init,
	.stream_on = gc6133_stream_on,
	.get_shutter = gc6133_read_shutter,
	.get_sensor_id = gc6133_get_sensor_id,
	.set_power = gc6133_set_power,
};
