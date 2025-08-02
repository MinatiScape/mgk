/******************************************************************************
* file  MT5728 40W wireless charge  driver
* Copyright (C) 2023 Coosea.
******************************************************************************/
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/alarmtimer.h>
//#include <mt-plat/mtk_boot.h>
#include "mt5728_wireless_driver_40w.h"
#include "MT5728_proprietary.h"
#include "MT5728_sha1.h"

//#include "mtk_charger_intf.h"
//#include "mtk_switch_charging.h"
//#include "mtk_intf.h"
#include "mtk_charger.h"
#include "mtk_battery.h"
//#include <mt-plat/v1/prop_chgalgo_class.h>
#include "charger_class.h"
//#include <mt-plat/v1/charger_type.h>


#define MT5728_WIRELESS_ALGO_VERSION		"1.0.0"
#define	MT5728_ALGO_INIT_POLLING_INTERVAL	500 //ms
#define	MT5728_ALGO_IDLE_POLLING_INTERVAL	1000 //ms


static int MT5728_DEBUG_ENABLE = 1;

#define MT5728_DEBUG(format, args...) do { \
	if (MT5728_DEBUG_ENABLE) \
	{\
		printk(KERN_WARNING format, ##args);\
	} \
} while (0)


#define DEJITTER_BUFFER_SIZE 5
struct mt5728_algo_dejitter {
	unsigned short buffer[DEJITTER_BUFFER_SIZE];
	unsigned char buffer_index;
	unsigned char data_count;
	unsigned int value;
};

struct mt5728_algo_info
{
	struct device *dev;
	wait_queue_head_t wq;
	struct alarm timer;
	struct task_struct *task;
	struct mutex lock;
	enum mt5728_algo_state state;
	
	atomic_t wakeup_thread;
	atomic_t stop_thread;
	atomic_t stop_algo;
	
	//struct prop_chgalgo_device *dvchg;
	//struct prop_chgalgo_device *dvchg_slave;
	struct power_supply *bms_psy;
	
	struct charger_device *dvchg;
	struct charger_device *swchg;
	
	int bat_vol_init0;
	int bat_vol_init0_temp;
	int bat_vol_init1;
	int bat_vol_init1_temp;
	int bat_vol_init_retry_cnt;
	int bat_vol_init_wait_cnt;
	
	struct mt5728_algo_dejitter aver_iout;
	struct mt5728_algo_dejitter aver_tdvchg;
	struct mt5728_algo_dejitter aver_tbat;
	
	int dvchg_state_err_total_cnt;
	int dvchg_state_err_cnt;
	int over_temp_wait_cnt;
	int rising_cnt_wait_cnt;
	int power_exceed_wait_cnt;
	int tx_deviation_wait_cnt;
	int send_ask_key_cnt;
	int send_vout_5v_cnt;
	int send_vout_5v_check_cnt;
	/*TX状态检测*/	
	int phone_deviation_cnt;
	bool phone_deviation_flag;/*偏移*/
	
	int phone_over_load_cnt;
	bool phone_over_load_flag;/*过载*/
	
	int phone_over_temp_cnt;
	bool phone_over_temp_flag;/*过温*/

};

struct mt5728_algo_info *g_info = NULL;


static const char *const __mt5728_algo_state_name[MT5728_ALGO_STATE_MAX] = {
	"IDLE","INIT", "TX_INIT0","TX_INIT1", "INCREASE_POWER", "INCREASE_POWER_RETRY",
	"POLLING", "POWER_EXCEED","OVER_TEMP","TX_DEVIATION", "SWITCH_TO_SWCHG","ABOUT_TO_EXIT","STOP",
};


static int mt5728_algo_dejitter_value_func(struct mt5728_algo_dejitter *dejitter, int val)
{
	unsigned int  total = 0;
	unsigned char i = 0;

	dejitter->buffer[dejitter->buffer_index] = val;
	dejitter->buffer_index = (dejitter->buffer_index + 1) % DEJITTER_BUFFER_SIZE;

	if (dejitter->data_count < DEJITTER_BUFFER_SIZE)
		dejitter->data_count++;

	for (i = 0; i < dejitter->data_count; i++)
		total += dejitter->buffer[i];

    dejitter->value = total / dejitter->data_count;

	return dejitter->value;
}

int Mt5728_get_fsk_buf_0_1(void) 
{
    u8 fsk[2] = {0};
	
	if(!g_info){
		return -1;
	}
	
    if(mt5728_read(REG_BC,fsk,2) < 0) {
       printk(KERN_ALERT "%s,Mt5728_get_fsk error!\n", __func__);
    } else {
       printk(KERN_ALERT "%s,Mt5728_get_fsk : 0x%x,0x%x KHz !\n", __func__, fsk[0],fsk[1]);
    }

    //mike add , The phone is not properly positioned  偏移中心
    if ((fsk[0] == 0x1A) && (fsk[1] == 0xc3))
    {
        g_info->phone_deviation_cnt++;
        if (g_info->phone_deviation_cnt >= 3)
        {
            g_info->phone_deviation_flag = true;
        }
    }
    else if((fsk[0] == 0x1A) && (fsk[1] == 0xCC))
    {
        g_info->phone_deviation_cnt = 0;
		g_info->phone_deviation_flag = false;
    }
	
	/*TX过载*/
	if ((fsk[0] == 0x1A) && (fsk[1] == 0xAD))
    {
        g_info->phone_over_load_cnt++;
        if (g_info->phone_over_load_cnt >= 3)
        {
            g_info->phone_over_load_flag = true;
        }
    }
    else if((fsk[0] == 0x1A) && (fsk[1] == 0xA2))
    {
        g_info->phone_deviation_cnt = 0;
		g_info->phone_over_load_flag = false;
    }
	
	/*TX过温*/
	if ((fsk[0] == 0x1A) && (fsk[1] == 0xA5))
    {
        g_info->phone_over_temp_cnt++;
        if (g_info->phone_over_temp_cnt >= 3)
        {
            g_info->phone_over_temp_flag = true;
        }
    }
    else if((fsk[0] == 0x1A) && (fsk[1] == 0xAA))
    {
        g_info->phone_over_temp_cnt = 0;
		g_info->phone_over_temp_flag = false;
    }
	
    return 0;
}


static inline void __mt5728_wakeup_algo_thread(struct mt5728_algo_info *info)
{
	MT5728_DEBUG("++\n");
	atomic_set(&info->wakeup_thread, 1);
	wake_up_interruptible(&info->wq);
}


static int __mt5728_algo_stop(struct mt5728_algo_info *info)
{
	info->state = MT5728_ALGO_STOP;
	atomic_set(&info->stop_algo, 1);
	__mt5728_wakeup_algo_thread(info);
	return 0;
}

static void mt5728_algo_regulate_power(int rising,int step)
{
	u8 temp[2] = {0};
	u16 vout = 0;
	mt5728_read(REG_VOUTSET, temp, 2);
	vout = (temp[1] << 8) + temp[0];
	
	if(vout >= VOUT_MAX && rising){
		MT5728_DEBUG("[%s]POWER_RISING vout:%d,over vol!!\n",__func__,vout);
		return;
	}
	
	if(rising){
		vout += step;
		MT5728_DEBUG("[%s]POWER_RISING vout:%d,set vout:%d\n",__func__,vout - step,vout);
	}
	else{
		vout -= POWER_FALLING_STEP;
		MT5728_DEBUG("[%s]POWER_FALLING vout:%d,set vout:%d\n",__func__,vout + POWER_FALLING_STEP,vout);
	}
	set_rx_vout(vout);
}

static void mt5728_algo_get_tbat(struct mt5728_algo_info *info,int * tbat)
{
	union power_supply_propval prop;
	
	if(IS_ERR_OR_NULL(info->bms_psy)){
		info->bms_psy = power_supply_get_by_name("cw-bat");
		if(IS_ERR_OR_NULL(info->bms_psy)){
			pr_err("gezi get bms_psy error\n");
			*tbat = 25;
			return;
		}
		goto to;
	}
to:
	power_supply_get_property(info->bms_psy,POWER_SUPPLY_PROP_TEMP, &prop);
	*tbat = prop.intval / 10;
}

static void mt5728_algo_get_vbat(struct mt5728_algo_info *info,int * vbat)
{
	union power_supply_propval prop;
	
	if(IS_ERR_OR_NULL(info->bms_psy)){
		info->bms_psy = power_supply_get_by_name("cw-bat");
		if(IS_ERR_OR_NULL(info->bms_psy)){
			pr_err("gezi get bms_psy error\n");
			*vbat = 0;
			return;
		}
		goto to;
	}
to:
	power_supply_get_property(info->bms_psy,POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
	*vbat = prop.intval / 1000;
}

static void mt5728_algo_tbat_tdvchg_check(struct mt5728_algo_info *info)
{
	int ret = 0;
	int iout = 0,tbat = 0,tdvchg = 0;
	iout = Mt5728_get_iout();
	ret = charger_dev_get_adc(info->dvchg, ADC_CHANNEL_TEMP_JC, &tdvchg,&tdvchg);
	if (ret < 0) {
		pr_err("%s get tdvchg fail(%d)\n", __func__,ret);
	} else {
		mt5728_algo_dejitter_value_func(&info->aver_tdvchg,tdvchg);
	}
	//ret = charger_dev_get_adc(info->dvchg, PCA_ADCCHAN_TBAT, &tbat,&tbat);
	mt5728_algo_get_tbat(info,&tbat);
	if (ret < 0) {
		pr_err("%s get tbat fail(%d)\n", __func__,ret);
	} else {
		mt5728_algo_dejitter_value_func(&info->aver_tbat, tbat);
	}
	mt5728_algo_dejitter_value_func(&info->aver_iout,iout);
	MT5728_DEBUG("[%s]tdvchg:%d,tbat:%d,iout:%d\n", __func__, \
			info->aver_tdvchg.value,info->aver_tbat.value,info->aver_iout.value);
		
	if((info->aver_tdvchg.value >= TDVCHG_TEMP_MAX) || (info->aver_tbat.value >= TBAT_TEMP_MAX))
	{
		info->state = MT5728_ALGO_OVER_TEMP;
	}
	return;
}

static bool mt5728_algo_uisoc_check(struct mt5728_algo_info *info)
{
	int ui_soc = 0,iout = 0,vbat = 0;

	ui_soc = battery_get_uisoc();
	iout = Mt5728_get_iout();
	mt5728_algo_get_vbat(info,&vbat);
	
	pr_err("[%s]vbat:%d,ui_soc:%d,iout:%d\n",__func__,vbat,ui_soc,iout);
	
	if ((ui_soc >= SWITCH_TO_SWCHG_UISOC && iout <= IOUT_EOC) || (ui_soc >= UISOC_100 && vbat >= VBAT_MAX))
	{
		info->state = MT5728_ALGO_SWITCH_TO_SWCHG;
		return true;
	}
	return false;
}

static bool mt5728_algo_dvchg_state_check(struct mt5728_algo_info *info)
{
	bool en = false;
	
	charger_dev_is_enabled(info->dvchg,&en);
	
	pr_err("[%s]dvchg_en:%d,err_cnt:%d\n",__func__,en,info->dvchg_state_err_cnt);

	if(!en)
	{
		info->dvchg_state_err_cnt++;
	}
	else{
		info->dvchg_state_err_cnt = 0;
	}
	
	if(info->dvchg_state_err_cnt >= 3){
		pr_err("[%s]dvchg state err:%d,total err:%d\n",__func__,info->dvchg_state_err_cnt,info->dvchg_state_err_total_cnt);
		if(info->dvchg_state_err_total_cnt <= 3 && (battery_get_uisoc() < 85)){
			info->dvchg_state_err_total_cnt++;
			info->dvchg_state_err_cnt = 0;
			info->state = MT5728_ALGO_INCREASE_POWER_RETRY;
		}
		else{
			info->state = MT5728_ALGO_SWITCH_TO_SWCHG;
		}
		return true;
	}
	
	return false;
}
static bool  mt5728_algo_tx_state_check(struct mt5728_algo_info *info)
{
	MT5728_DEBUG("[%s]deviation:%d,over_load:%d,over_temp:%d\n", \
			__func__,g_info->phone_deviation_flag,g_info->phone_over_load_flag,g_info->phone_over_temp_flag);
	
	if(g_info->phone_deviation_flag || g_info->phone_over_load_flag || g_info->phone_over_temp_flag)
	{
		info->state = MT5728_ALGO_TX_DEVIATION;
		return true;
	}
	
	return false;
}

static void mt5728_algo_polling(struct mt5728_algo_info *info)
{
	int uisoc = 0,vbat = 0;
	
	mt5728_algo_get_vbat(info,&vbat);
	uisoc = battery_get_uisoc();
	
	MT5728_DEBUG("[%s]uisoc:%d,aver_iout:%d,vbat:%d\n",__func__,uisoc,info->aver_iout.value,vbat);
	
	 /*若当前平均iout电流大于IOUT_MAX + 20,则降低功率*/
	 if(uisoc >= IOUT_MAX_LINIT_UISOC){
		 if ((info->aver_iout.value > IOUT_MAX_90) || (vbat > VBAT_MAX)){
			info->state = MT5728_ALGO_POWER_EXCEED;
			return;
		}
	 }
	 else{
		if ((info->aver_iout.value > IOUT_MAX) || (vbat > VBAT_MAX)){
			info->state = MT5728_ALGO_POWER_EXCEED;
			return;
		}
	}
	
	/*若当前功率偏低(iout < 1700)，且辅充温度 < 60度时，重新提高功率*/
	if((info->aver_iout.value <= (IOUT_MAX - 500)) && (info->aver_tdvchg.value < (TDVCHG_TEMP_MAX - 5)) \
		&& (battery_get_uisoc() < (SWITCH_TO_SWCHG_UISOC - 3))){
		info->state = MT5728_ALGO_INCREASE_POWER;
		return;
	}
	
}

static void mt5728_algo_power_exceed(struct mt5728_algo_info *info)
{
	int vbat = 0,uisoc = 0;
	mt5728_algo_get_vbat(info,&vbat);
	uisoc = battery_get_uisoc();
	
	MT5728_DEBUG("[%s]uisoc:%d,aver_iout:%d,vbat:%d,wait_cnt:%d\n",__func__,uisoc,info->aver_iout.value,vbat,info->power_exceed_wait_cnt);
	
	if(uisoc >= IOUT_MAX_LINIT_UISOC){
		if((info->aver_iout.value) < IOUT_MAX_90 && (vbat <= VBAT_MAX)){
			info->state = MT5728_ALGO_POLLING;
			return;
		}
	}
	else{
		if((info->aver_iout.value) < IOUT_MAX && (vbat <= VBAT_MAX)){
			info->state = MT5728_ALGO_POLLING;
			return;
		}
	}
	
	if(info->power_exceed_wait_cnt > 0){
		info->power_exceed_wait_cnt--;
		return;
	}
	info->power_exceed_wait_cnt = 5;
	mt5728_algo_regulate_power(POWER_FALLING,POWER_FALLING_STEP);
}

static void mt5728_algo_switch_to_swchg(struct mt5728_algo_info *info)
{
	int vout = 0,iout = 0;
	
	vout = Mt5728_get_vout();
	iout = Mt5728_get_iout();
	
	mt5728_algo_dejitter_value_func(&info->aver_iout,iout);
	
	MT5728_DEBUG("[%s]v5c:%d,v5cc:%d,asc:%d,vout:%d,iout:%d,av_iout:%d\n", \
		__func__,info->send_vout_5v_cnt,info->send_vout_5v_check_cnt,info->send_ask_key_cnt,vout,iout,info->aver_iout.value);
	
	
	if(info->aver_iout.value > IOUT_EOC)
	{
		if(info->power_exceed_wait_cnt > 0)
		{
			info->power_exceed_wait_cnt--;
			return;
		}
		info->power_exceed_wait_cnt = 3;
		mt5728_algo_regulate_power(POWER_FALLING,POWER_FALLING_STEP);
		MT5728_DEBUG("mt5728 switch_to_swchg,wating iout falling\n");
		return;
	}
	
	 /*关闭辅充*/
    charger_dev_enable(info->dvchg, 0);
	//prop_chgalgo_enable_charging(pca_dvchg_slave, 0);
	msleep(200);
	/*关闭WPC VDD*/
	//turn_on_5728_wpc_vdd(0);
	
    /*设置TX输出电压*/
	set_rx_vout(5000);
	msleep(200);
	
	Mt5728_get_vsetflag_cep();
	info->send_vout_5v_cnt++;
	
	if(Mt5728_get_vout() <= 9000){
		info->send_vout_5v_check_cnt++;
	}
	else{
		info->send_vout_5v_check_cnt = 0;
	}
	
	if(info->send_vout_5v_check_cnt >= 3){
		goto out;
	}
	
	if(info->send_vout_5v_cnt <= 15){
		return;
	}
	else{
		MT5728_DEBUG("[%s] over cnt:%d !!!\n",__func__,info->send_vout_5v_cnt);
	}
	
out:
	info->send_vout_5v_check_cnt = 0;
	info->send_vout_5v_cnt = 0;
	info->state = MT5728_ALGO_ABOUT_TO_EXIT;

	MT5728_DEBUG("[%s]------ok!\n",__func__);
}

static void mt5728_algo_about_to_exit_power(struct mt5728_algo_info *info)
{
	mt5728_send_ask_key();
	info->send_ask_key_cnt++;
	if(info->send_ask_key_cnt < 3){
		return;
	}
	  /*打开主充*/
	//mt5728_connect_set_over_time();
	mt5728_connect_over_time_work_schedule(5000);
	//charger_dev_enable_hz(info->swchg, 0);
	charger_dev_enable(info->swchg, 1);
	info->power_exceed_wait_cnt = 0;
	info->send_ask_key_cnt = 0;
	__mt5728_algo_stop(info);
	MT5728_DEBUG("[%s]------ok!\n",__func__);
}

static void mt5728_algo_increase_power(struct mt5728_algo_info *info)
{
	int vout = 0,iout = 0,step = 25;
	int ui_soc = 0;
	vout = Mt5728_get_vout();
	iout = Mt5728_get_iout();
	ui_soc = battery_get_uisoc();
	MT5728_DEBUG("[%s]ui_soc:%d,vout:%d,iout:%d,cnt:%d,power:%d\n", \
			__func__,ui_soc,vout,iout,info->rising_cnt_wait_cnt,vout * iout / 100000);
			
	/* 若当前VOUT或IOUT达到最大值，停止提升功率 */
	if(ui_soc >= IOUT_MAX_LINIT_UISOC){
		if (iout >= IOUT_MAX_90 || vout >= VOUT_MAX || (ui_soc >= (SWITCH_TO_SWCHG_UISOC - 3))){
			info->state = MT5728_ALGO_POLLING;
			return;
		}
	}
	else{
		if (iout >= IOUT_MAX || vout >= VOUT_MAX || (ui_soc >= (SWITCH_TO_SWCHG_UISOC - 3))){
			info->state = MT5728_ALGO_POLLING;
			return;
		}
	}
	
	if(iout < 1800){
		step = 50;
	}
	else{
		step = 25;
	}
	if(info->rising_cnt_wait_cnt > 0){
		info->rising_cnt_wait_cnt--;
		MT5728_DEBUG("[%s] waiting cnt:%d\n",__func__,info->rising_cnt_wait_cnt);
		return;
	}
	info->rising_cnt_wait_cnt = 3;
	mt5728_algo_regulate_power(POWER_RISING,step);
}

static void mt5728_algo_increase_power_retry(struct mt5728_algo_info *info)
{
	    /*设置TX输出电压*/
	int vout = 0,iout = 0;

	vout = Mt5728_get_vout();
	iout = Mt5728_get_iout();
	
	MT5728_DEBUG("[%s]start---vout:%d,iout:%d\n",__func__,vout,iout);
	
	set_rx_vout(5000);
	msleep(150);
	
	MT5728_DEBUG("[%s]end---vout:%d,iout:%d\n",__func__,vout,iout);
	
	info->state = MT5728_ALGO_TX_INIT0;
}

static void mt5728_algo_over_temp(struct mt5728_algo_info *info)
{
	int bat_vol = 0;
	int bat_chg_vol = 0;
	int vout = Mt5728_get_vout();
	
	/*辅充温度大于65度或电池温度大于50度，开始降低功率*/
	if((info->aver_tdvchg.value >= TDVCHG_TEMP_MAX) || (info->aver_tbat.value >= TBAT_TEMP_MAX)){
		charger_dev_get_adc(info->dvchg, ADC_CHANNEL_VBAT,&bat_vol, &bat_vol);
		bat_vol /= 1000;
		bat_chg_vol = bat_vol * 2 + 200;
		MT5728_DEBUG("[%s]reduce power..vout:%d,vbat:%d,bav:%d\n",__func__,vout,bat_vol,bat_chg_vol);
		if(info->over_temp_wait_cnt > 0){
			info->over_temp_wait_cnt--;
			MT5728_DEBUG("[%s]reduce power,waiting...%d\n",__func__,info->over_temp_wait_cnt);
			return;
		}
		if(vout > bat_chg_vol){
			mt5728_algo_regulate_power(POWER_FALLING,POWER_FALLING_STEP);
			info->over_temp_wait_cnt = 15;
		}
	}
	if((info->aver_tdvchg.value <= TDVCHG_TEMP_NOMAL) && (info->aver_tbat.value <= TBAT_TEMP_NOMAL)){
		info->state = MT5728_ALGO_POLLING;
	}
}

static void mt5728_algo_tx_deviation(struct mt5728_algo_info *info)
{
	int iout = Mt5728_get_iout();
	
	MT5728_DEBUG("[%s]deviation:%d,over_load:%d,over_temp:%d,iout:%d,wait_cnt:%d\n", \
			__func__,info->phone_deviation_flag,info->phone_over_load_flag,info->phone_over_temp_flag,iout,info->tx_deviation_wait_cnt);
	
	if(info->tx_deviation_wait_cnt > 0){
		info->tx_deviation_wait_cnt--;
		return;
	}
	
	if(info->phone_deviation_flag || info->phone_over_load_flag || info->phone_over_temp_flag)
	{
		if(iout > 1000)
		{
			info->tx_deviation_wait_cnt = 3;
			mt5728_algo_regulate_power(POWER_FALLING,POWER_FALLING_STEP);
		}
		else if(iout <= 400){
			info->tx_deviation_wait_cnt = 5;
			mt5728_algo_regulate_power(POWER_RISING,POWER_RISING_STEP);
		}
		return;
	}
	else{
		info->state = MT5728_ALGO_POLLING;
		info->tx_deviation_wait_cnt = 0;
	}
	
	return;
}


static void mt5728_algo_reset(struct mt5728_algo_info *info)
{
	MT5728_DEBUG("[%s]\n",__func__);
	info->aver_iout.buffer_index = 0;
	info->aver_iout.data_count = 0;
	memset(info->aver_iout.buffer, 0, DEJITTER_BUFFER_SIZE);

	info->aver_tdvchg.buffer_index = 0;
	info->aver_tdvchg.data_count = 0;
	memset(info->aver_tdvchg.buffer, 0, DEJITTER_BUFFER_SIZE);
	
	info->aver_tbat.buffer_index = 0;
	info->aver_tbat.data_count = 0;
	memset(info->aver_tbat.buffer, 0, DEJITTER_BUFFER_SIZE);
	
	/*偏移中心*/
	info->phone_deviation_cnt = 0;
	info->phone_deviation_flag = false;
	/*tx过载*/
	info->phone_over_load_cnt = 0;
	info->phone_over_load_flag = false;
	/*tx过温*/
	info->phone_over_temp_cnt = 0;
	info->phone_over_temp_flag = false;	
	
	info->dvchg_state_err_total_cnt = 0;
	info->dvchg_state_err_cnt = 0;
	info->over_temp_wait_cnt = 0;
	info->rising_cnt_wait_cnt = 0;
	info->power_exceed_wait_cnt = 0;
	info->tx_deviation_wait_cnt = 0;
	info->send_vout_5v_cnt = 0;
	info->send_vout_5v_check_cnt = 0;
	info->send_ask_key_cnt = 0;
	info->bat_vol_init_wait_cnt = 0;
	
}


static void mt5728_algo_tx_init0(struct mt5728_algo_info *info)
{
	int vout = 0,step = 0;
	
	vout = Mt5728_get_vout();
	
	info->bat_vol_init_retry_cnt++;
	if(info->bat_vol_init_retry_cnt % 5 == 0){
		step += (POWER_RISING_STEP * (info->bat_vol_init_retry_cnt / 5));
	}
	MT5728_DEBUG("[%s]vout:%d,bat_vol_init0:%d,temp:%d,retry_cnt:%d,step:%d\n", \
		__func__,vout,info->bat_vol_init0,info->bat_vol_init0_temp,info->bat_vol_init_retry_cnt,step);
		
	if(info->bat_vol_init_retry_cnt >= 15){
		info->bat_vol_init_retry_cnt = 0;
		info->state = MT5728_ALGO_SWITCH_TO_SWCHG;
		return;
	}
	if(vout < info->bat_vol_init0_temp){
		set_rx_vout(info->bat_vol_init0 + step);
	}
	else if(vout < info->bat_vol_init0){
		mt5728_algo_regulate_power(POWER_RISING,step);
	}
	else{
		set_rx_vout(info->bat_vol_init1);
		info->bat_vol_init_retry_cnt = 0;
		info->state = MT5728_ALGO_TX_INIT1;
	}
	return;
}

static void mt5728_algo_tx_init1(struct mt5728_algo_info *info)
{
	int vout = 0,ret = 0,step = 0;
	u32 vbusovp = 22000000;
	u32 vbusovp_alam = 20000000;
	
	vout = Mt5728_get_vout();
	
	info->bat_vol_init_retry_cnt++;
	if(info->bat_vol_init_retry_cnt % 5 == 0){
		step += (POWER_RISING_STEP * (info->bat_vol_init_retry_cnt / 5));
	}
	
	MT5728_DEBUG("[%s]vout:%d,bat_vol_init1:%d,temp:%d,retry_cnt:%d,wait_cnt:%d,step:%d\n", \
		__func__,vout,info->bat_vol_init1,info->bat_vol_init1_temp,info->bat_vol_init_retry_cnt,info->bat_vol_init_wait_cnt,step);

	if(info->bat_vol_init_retry_cnt >= 15){
		info->bat_vol_init_retry_cnt = 0;
		info->state = MT5728_ALGO_SWITCH_TO_SWCHG;
		return;
	}
	
	if(vout < info->bat_vol_init1_temp){
		//mt5728_algo_regulate_power(POWER_RISING,step);
		set_rx_vout(info->bat_vol_init1 + step);
		info->bat_vol_init_wait_cnt = 0;
		msleep(200);
		return;
	}
	else if(vout < info->bat_vol_init1){
		mt5728_algo_regulate_power(POWER_RISING,step);
		info->bat_vol_init_wait_cnt = 0;
		msleep(200);
	}
	else{
		info->bat_vol_init_wait_cnt++;
		info->bat_vol_init_retry_cnt = 0;
		if(info->bat_vol_init_wait_cnt <= 3){
			msleep(100);
			return;
		}
	}
	info->bat_vol_init_wait_cnt = 0;
	info->bat_vol_init_retry_cnt = 0;
	/*打开charge pump*/
	pr_err("gezi----%s----vbusovp:%ld,vbusovp_alam:%ld\n",__func__,vbusovp,vbusovp_alam);
	charger_dev_set_mivr(info->dvchg,0);
	charger_dev_set_vbusovp(info->dvchg,vbusovp);
	charger_dev_set_vbusovp_alarm(info->dvchg,vbusovp_alam);
	ret = charger_dev_enable(info->dvchg, 1);
	if (ret < 0) {
		pr_err("[%s]en dv chg fail(%d)\n", __func__,ret);
		//__mt5728_algo_stop(info);
		info->state = MT5728_ALGO_SWITCH_TO_SWCHG;
		return;
	}
	info->state = MT5728_ALGO_INCREASE_POWER;
	
}

static void mt5728_algo_idle(struct mt5728_algo_info *info)
{
	int temp = 0;
	int uisoc = battery_get_uisoc();
	//temp = battery_get_bat_temperature();
	mt5728_algo_get_tbat(info,&temp);
	MT5728_DEBUG("[%s]uisoc:%d,temp:%d,boot_mode:%d\n",__func__,uisoc,temp,gezi_boot_mode);
	if(temp >= 15 && temp <= 50){
		if((gezi_boot_mode == 8 || gezi_boot_mode == 9)){
			if(uisoc >= 3){
				info->state = MT5728_ALGO_INIT;
			}
			else{
				MT5728_DEBUG("[%s]uisoc < 3(%d),waiting...\n",__func__,uisoc);
			}
		}
		else{
			info->state = MT5728_ALGO_INIT;
		}
	}
	else{
		MT5728_DEBUG("[%s]temp not in range(%d),waiting...\n",__func__,temp);
	}
}
static void mt5728_algo_init(struct mt5728_algo_info *info)
{
	int bat_vol = 0;
	
	if(IS_ERR_OR_NULL(info->dvchg)){
		info->dvchg = get_charger_by_name("primary_dvchg");
		if(IS_ERR_OR_NULL(info->dvchg)){
			pr_err("gezi get chg_dvchg error\n");
			goto init_err;
		}
	}
	
	if(IS_ERR_OR_NULL(info->bms_psy)){
		info->bms_psy = power_supply_get_by_name("cw-bat");
		if(IS_ERR_OR_NULL(info->bms_psy)){
			pr_err("gezi get bms_psy error\n");
			//goto init_err;
		}
	}

	if(IS_ERR_OR_NULL(info->swchg)){
		info->swchg = get_charger_by_name("primary_chg");
		if(IS_ERR_OR_NULL(info->swchg)){
			pr_err("gezi get swchg error\n");
			goto init_err;
		}
	}
	/*获取vbat电压*/
	charger_dev_get_adc(info->dvchg, ADC_CHANNEL_VBAT,&bat_vol, &bat_vol);
	bat_vol /= 1000;
	MT5728_DEBUG("gezi:vbat = %d\n",bat_vol);
	info->bat_vol_init0 = bat_vol * 4;
	info->bat_vol_init0_temp = info->bat_vol_init0 * 9 / 10;
	if(battery_get_uisoc() > 50){
		bat_vol = bat_vol * 4 + 100;
	}
	else{
		bat_vol = bat_vol * 4 + 200;
	}
	info->bat_vol_init1 = bat_vol;
	info->bat_vol_init1_temp = info->bat_vol_init1 * 9 / 10;
	/*关闭主充*/
	charger_dev_enable(info->swchg, 0);
	//charger_dev_enable_hz(info->swchg, 1);
	/*打开WPC VDD*/
	turn_on_5728_wpc_vdd(1);
	MT5728_DEBUG("set_rx_vout = %d\n",info->bat_vol_init0);
	/*设置初始电压*/
	set_rx_vout(info->bat_vol_init0);
	msleep(500);
	info->state = MT5728_ALGO_TX_INIT0;
	return;
init_err:
	info->state = MT5728_ALGO_SWITCH_TO_SWCHG;
}

static enum alarmtimer_restart
__mt5728_algo_timer_cb(struct alarm *alarm, ktime_t now)
{
	struct mt5728_algo_info *info =
		container_of(alarm, struct mt5728_algo_info, timer);

	MT5728_DEBUG("++\n");
	__mt5728_wakeup_algo_thread(info);
	return ALARMTIMER_NORESTART;
}


static int __mt5728_algo_threadfn(void *param)
{
	struct mt5728_algo_info *info = param;
	u32 sec, ms, polling_interval;
	ktime_t ktime;

	while (!kthread_should_stop()) {
		wait_event_interruptible(info->wq,
					 atomic_read(&info->wakeup_thread));
		pm_stay_awake(info->dev);
		if (atomic_read(&info->stop_thread)) {
			pm_relax(info->dev);
			break;
		}
		atomic_set(&info->wakeup_thread, 0);
		mutex_lock(&info->lock);
		if (atomic_read(&info->stop_algo)){
			info->state = MT5728_ALGO_STOP;
		}
		MT5728_DEBUG("state = %d,%s\n",info->state,__mt5728_algo_state_name[info->state]);
		//MT5728_DEBUG("[%s]state = %d\n", __func__,info->state);
		switch (info->state) {
		case MT5728_ALGO_IDLE:
			mt5728_algo_idle(info);
			break;
		case MT5728_ALGO_INIT:
			mt5728_algo_init(info);
			break;
		case MT5728_ALGO_TX_INIT0:
			mt5728_algo_tx_init0(info);
			break;
		case MT5728_ALGO_TX_INIT1:
			mt5728_algo_tx_init1(info);
			break;
		case MT5728_ALGO_INCREASE_POWER:
			mt5728_algo_increase_power(info);
			break;
		case MT5728_ALGO_INCREASE_POWER_RETRY:
			mt5728_algo_increase_power_retry(info);
			break;
		case MT5728_ALGO_POLLING:
			mt5728_algo_polling(info);
			break;
		case MT5728_ALGO_POWER_EXCEED:
			mt5728_algo_power_exceed(info);
			break;
		case MT5728_ALGO_SWITCH_TO_SWCHG:
			mt5728_algo_switch_to_swchg(info);
			break;
		case MT5728_ALGO_OVER_TEMP:
			mt5728_algo_over_temp(info);
			break;
		case MT5728_ALGO_TX_DEVIATION:
			mt5728_algo_tx_deviation(info);
			break;
		case MT5728_ALGO_ABOUT_TO_EXIT:
			mt5728_algo_about_to_exit_power(info);
			break;
		case MT5728_ALGO_STOP:
			mt5728_algo_reset(info);
			break;		
		default:
			break;
		}
		
		if((info->state < MT5728_ALGO_SWITCH_TO_SWCHG) && (info->state >= MT5728_ALGO_INCREASE_POWER)){
			mt5728_algo_tbat_tdvchg_check(info);
			mt5728_algo_dvchg_state_check(info);
			mt5728_algo_tx_state_check(info);
			mt5728_algo_uisoc_check(info);
		}
		
		if (info->state != MT5728_ALGO_STOP) {
			if(info->state == MT5728_ALGO_IDLE || info->state == MT5728_ALGO_POLLING){
				polling_interval = MT5728_ALGO_IDLE_POLLING_INTERVAL;
			}
			else{
				polling_interval = MT5728_ALGO_INIT_POLLING_INTERVAL;
			}
			sec = polling_interval / 1000;
			ms = polling_interval % 1000;
			ktime = ktime_set(sec, MS_TO_NS(ms));
			alarm_start_relative(&info->timer, ktime);
		}
		mutex_unlock(&info->lock);
		pm_relax(info->dev);
	}
	return 0;
}

void mt5728_wireless_algo_start(void)
{	
	if(!g_info){
		return;
	}
	MT5728_DEBUG("%s\n", __func__);
	atomic_set(&g_info->stop_algo, 0);
	g_info->state = MT5728_ALGO_IDLE;
	__mt5728_wakeup_algo_thread(g_info);
}

void mt5728_wireless_algo_stop(void)
{	
	if(!g_info){
		return;
	}
	MT5728_DEBUG("%s\n", __func__);
	__mt5728_algo_stop(g_info);
}

int mt5728_wireless_charge_40w_init(struct device* dev)
{
	int ret = 0;
	struct mt5728_algo_info *info;
	
	MT5728_DEBUG("%s [%s]\n", __func__, MT5728_WIRELESS_ALGO_VERSION);
	
	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info){
		return -ENOMEM;
	}
	
	info->dev = dev;
	/* init algo thread & timer */
	mutex_init(&info->lock);
	init_waitqueue_head(&info->wq);
	atomic_set(&info->wakeup_thread, 0);
	atomic_set(&info->stop_thread, 0);
	atomic_set(&info->stop_algo, 0);
	info->state = MT5728_ALGO_STOP;
	alarm_init(&info->timer, ALARM_REALTIME, __mt5728_algo_timer_cb);
	info->task = kthread_run(__mt5728_algo_threadfn, info, "5728_algo_task");
	if (IS_ERR(info->task)) {
		ret = PTR_ERR(info->task);
		MT5728_DEBUG("%s run task fail(%d)\n", __func__, ret);
		goto err;
	}
	g_info = info;
	device_init_wakeup(info->dev, true);
	MT5728_DEBUG("%s successfully\n", __func__);
	return 1;
err:
	MT5728_DEBUG("%s fail!\n", __func__);
	__mt5728_algo_stop(info);
	return 0;
}

