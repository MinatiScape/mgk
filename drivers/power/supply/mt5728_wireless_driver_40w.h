#ifndef __MT5728_WIRELESS_DRIVER_40W_H__
#define __MT5728_WIRELESS_DRIVER_40W_H__

//#include "mt5728_wireless_15w.h"


enum mt5728_algo_state {
	MT5728_ALGO_IDLE,
	MT5728_ALGO_INIT,
	MT5728_ALGO_TX_INIT0,
	MT5728_ALGO_TX_INIT1,
	MT5728_ALGO_INCREASE_POWER,
	MT5728_ALGO_INCREASE_POWER_RETRY,
	MT5728_ALGO_POLLING,
	MT5728_ALGO_POWER_EXCEED,
	MT5728_ALGO_OVER_TEMP,
	MT5728_ALGO_TX_DEVIATION,
	MT5728_ALGO_SWITCH_TO_SWCHG,
	MT5728_ALGO_ABOUT_TO_EXIT,
	MT5728_ALGO_STOP,
	MT5728_ALGO_STATE_MAX
};

#ifndef REG_VOUTSET
#define REG_VOUTSET				0x002E
#endif

#ifndef REG_BC				
#define REG_BC					0x0066
#endif


#ifndef POWER_RISING
#define POWER_RISING	1
#endif

#ifndef POWER_FALLING
#define POWER_FALLING	0
#endif



#define SWITCH_TO_SWCHG_UISOC	97
#define UISOC_100				100
#define IOUT_MAX_LINIT_UISOC	90

#define VOUT_MAX  		19000
#define IOUT_MAX  		1350
#define IOUT_MAX_90  	1150
#define IOUT_EOC  		400
#define VBAT_MAX  		4450

#define TDVCHG_TEMP_MAX 	65
#define TDVCHG_TEMP_NOMAL   61

#define TBAT_TEMP_MAX 	 	50
#define TBAT_TEMP_NOMAL   	46

#define POWER_RISING_STEP		25
#define POWER_FALLING_STEP		25

#ifndef MS_TO_NS
#define MS_TO_NS(msec)		((msec) * 1000 * 1000)
#endif


extern int turn_on_5728_wpc_vdd(int en);
extern int set_rx_vout(uint16_t vout);
extern ssize_t Mt5728_get_vout(void);
extern ssize_t Mt5728_get_iout(void);
extern int Mt5728_get_fsk_buf_0_1(void);
extern void mt5728_send_ask_key(void);
extern int mt5728_read(u16 reg, u8* buf, u32 size);
//extern void mt5728_send_pp18_packet(int delay_time);
extern ssize_t Mt5728_get_vsetflag_cep(void);
extern void mt5728_wireless_algo_start(void);
extern void mt5728_wireless_algo_stop(void);
extern int mt5728_wireless_charge_40w_init(struct device* dev);
extern void mt5728_connect_set_over_time(void);
extern void mt5728_connect_over_time_work_schedule(int delay_time);
extern int battery_get_uisoc(void);
#endif


