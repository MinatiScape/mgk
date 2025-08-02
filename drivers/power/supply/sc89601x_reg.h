/* SPDX-License-Identifier: GPL-2.0*/
/*
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#ifndef __SC89601X_HEADER__
#define __SC89601X_HEADER__

/* Register 00h */
#define SC89601S_REG_00      		0x00
#define SC89601S_ENHIZ_MASK		    0x80
#define SC89601S_ENHIZ_SHIFT		    7
#define	SC89601S_HIZ_ENABLE			1
#define	SC89601S_HIZ_DISABLE			0

#define	SC89601S_STAT_CTRL_MASK		0x60
#define SC89601S_STAT_CTRL_SHIFT		5
#define	SC89601S_STAT_CTRL_STAT		0
#define	SC89601S_STAT_CTRL_DISABLE		3

#define SC89601S_IINLIM_MASK		    0x1F
#define SC89601S_IINLIM_SHIFT			0
#define	SC89601S_IINLIM_LSB			100
#define	SC89601S_IINLIM_BASE			100

/* Register 01h */
#define SC89601S_REG_01		    	0x01
#define SC89601S_PFM_DIS_MASK	      	0x80
#define	SC89601S_PFM_DIS_SHIFT			7
#define	SC89601S_PFM_ENABLE			0
#define	SC89601S_PFM_DISABLE			1

#define SC89601S_WDT_RESET_MASK		0x40
#define SC89601S_WDT_RESET_SHIFT		6
#define SC89601S_WDT_RESET				1

#define	SC89601S_OTG_CONFIG_MASK		0x20
#define	SC89601S_OTG_CONFIG_SHIFT		5
#define	SC89601S_OTG_ENABLE			1
#define	SC89601S_OTG_DISABLE			0

#define SC89601S_CHG_CONFIG_MASK     	0x10
#define SC89601S_CHG_CONFIG_SHIFT    	4
#define SC89601S_CHG_DISABLE        	0
#define SC89601S_CHG_ENABLE         	1

#define SC89601S_SYS_MINV_MASK       	0x0E
#define SC89601S_SYS_MINV_SHIFT      	1

#define	SC89601S_MIN_VBAT_SEL_MASK		0x01
#define	SC89601S_MIN_VBAT_SEL_SHIFT	0
#define	SC89601S_MIN_VBAT_2P8V			0
#define	SC89601S_MIN_VBAT_2P5V			1


/* Register 0x02*/
#define SC89601S_REG_02              0x02
#define	SC89601S_BOOST_LIM_MASK		0x80
#define	SC89601S_BOOST_LIM_SHIFT		7
#define	SC89601S_BOOST_LIM_0P5A		0
#define	SC89601S_BOOST_LIM_1P2A		1

#define SC89601S_ICHG_MASK           	0x3F
#define SC89601S_ICHG_SHIFT          	0
#define SC89601S_ICHG_BASE           	0
#define SC89601S_ICHG_LSB            	60

/* Register 0x03*/
#define SC89601S_REG_03              0x03
#define SC89601S_IPRECHG_MASK        	0xF0
#define SC89601S_IPRECHG_SHIFT       	4
#define SC89601S_IPRECHG_BASE        	60
#define SC89601S_IPRECHG_LSB         	60

#define SC89601S_ITERM_MASK          	0x0F
#define SC89601S_ITERM_SHIFT         	0
#define SC89601S_ITERM_BASE          	60
#define SC89601S_ITERM_LSB           	60


/* Register 0x04*/
#define SC89601S_REG_04              0x04
#define SC89601S_VREG_MASK           	0xF8
#define SC89601S_VREG_SHIFT          	3
#define SC89601S_VREG_BASE           	3848
#define SC89601S_VREG_LSB            	32

#define	SC89601S_TOPOFF_TIMER_MASK		0x06
#define	SC89601S_TOPOFF_TIMER_SHIFT	1
#define	SC89601S_TOPOFF_TIMER_DISABLE	0
#define	SC89601S_TOPOFF_TIMER_15M		1
#define	SC89601S_TOPOFF_TIMER_30M		2
#define	SC89601S_TOPOFF_TIMER_45M		3


#define SC89601S_VRECHG_MASK         	0x01
#define SC89601S_VRECHG_SHIFT        	0
#define SC89601S_VRECHG_100MV        	0
#define SC89601S_VRECHG_200MV        	1

/* Register 0x05*/
#define SC89601S_REG_05             	0x05
#define SC89601S_EN_TERM_MASK        	0x80
#define SC89601S_EN_TERM_SHIFT       	7
#define SC89601S_TERM_ENABLE         	1
#define SC89601S_TERM_DISABLE        	0

#define SC89601S_WDT_MASK            	0x30
#define SC89601S_WDT_SHIFT           	4
#define SC89601S_WDT_DISABLE         	0
#define SC89601S_WDT_40S             	1
#define SC89601S_WDT_80S             	2
#define SC89601S_WDT_160S            	3
#define SC89601S_WDT_BASE            	0
#define SC89601S_WDT_LSB             	40

#define SC89601S_EN_TIMER_MASK       	0x08
#define SC89601S_EN_TIMER_SHIFT      	3
#define SC89601S_CHG_TIMER_ENABLE    	1
#define SC89601S_CHG_TIMER_DISABLE   	0

#define SC89601S_CHG_TIMER_MASK      	0x04
#define SC89601S_CHG_TIMER_SHIFT     	2
#define SC89601S_CHG_TIMER_5HOURS    	0
#define SC89601S_CHG_TIMER_10HOURS   	1

#define	SC89601S_TREG_MASK				0x02
#define	SC89601S_TREG_SHIFT			1
#define	SC89601S_TREG_90C				0
#define	SC89601S_TREG_110C				1

#define SC89601S_JEITA_ISET_MASK     	0x01
#define SC89601S_JEITA_ISET_SHIFT    	0
#define SC89601S_JEITA_ISET_50PCT    	0
#define SC89601S_JEITA_ISET_20PCT    	1


/* Register 0x06*/
#define SC89601S_REG_06              0x06
#define	SC89601S_OVP_MASK				0xC0
#define	SC89601S_OVP_SHIFT				0x6
#define	SC89601S_OVP_5P8V				0
#define	SC89601S_OVP_6P4V				1
#define	SC89601S_OVP_11V				2
#define	SC89601S_OVP_14P2V				3

#define	SC89601S_BOOSTV_MASK			0x30
#define	SC89601S_BOOSTV_SHIFT			4
#define	SC89601S_BOOSTV_4P7V			0
#define	SC89601S_BOOSTV_4P9V			1
#define	SC89601S_BOOSTV_5P1V			2
#define	SC89601S_BOOSTV_5P3V			3

#define	SC89601S_VINDPM_MASK			0x0F
#define SC89601S_VINDPM_SHIFT			0
#define	SC89601S_VINDPM_BASE			3900
#define	SC89601S_VINDPM_LSB			100

/* Register 0x07*/
#define SC89601S_REG_07              0x07
#define SC89601S_FORCE_DPDM_MASK     	0x80
#define SC89601S_FORCE_DPDM_SHIFT    	7
#define SC89601S_FORCE_DPDM          	1

#define SC89601S_TMR2X_EN_MASK       	0x40
#define SC89601S_TMR2X_EN_SHIFT      	6
#define SC89601S_TMR2X_ENABLE        	1
#define SC89601S_TMR2X_DISABLE       	0

#define SC89601S_BATFET_DIS_MASK     	0x20
#define SC89601S_BATFET_DIS_SHIFT    	5
#define SC89601S_BATFET_OFF          	1
#define SC89601S_BATFET_ON          	0

#define SC89601S_JEITA_VSET_MASK     	0x10
#define SC89601S_JEITA_VSET_SHIFT    	4
#define SC89601S_JEITA_VSET_4100     	0
#define SC89601S_JEITA_VSET_VREG     	1

#define	SC89601S_BATFET_DLY_MASK		0x08
#define	SC89601S_BATFET_DLY_SHIFT		3
#define	SC89601S_BATFET_DLY_0S			0
#define	SC89601S_BATFET_DLY_10S		1

#define	SC89601S_BATFET_RST_EN_MASK	0x04
#define	SC89601S_BATFET_RST_EN_SHIFT	2
#define	SC89601S_BATFET_RST_DISABLE	0
#define	SC89601S_BATFET_RST_ENABLE		1

#define	SC89601S_VDPM_BAT_TRACK_MASK	0x03
#define	SC89601S_VDPM_BAT_TRACK_SHIFT 	0
#define	SC89601S_VDPM_BAT_TRACK_DISABLE	0
#define	SC89601S_VDPM_BAT_TRACK_200MV	1
#define	SC89601S_VDPM_BAT_TRACK_250MV	2
#define	SC89601S_VDPM_BAT_TRACK_300MV	3

/* Register 0x08*/
#define SC89601S_REG_08              0x08
#define SC89601S_VBUS_STAT_MASK        0xE0           
#define SC89601S_VBUS_STAT_SHIFT       5
#define SC89601S_VBUS_TYPE_NONE	    0
#define SC89601S_VBUS_TYPE_SDP	        1
#define SC89601S_VBUS_TYPE_CDP	        2
#define SC89601S_VBUS_TYPE_DCP	        3
#define SC89601S_VBUS_TYPE_UNKNOWN	    5
#define SC89601S_VBUS_TYPE_NON_STD	    6
#define SC89601S_VBUS_TYPE_OTG	        7

#define SC89601S_VBUS_TYPE_USB         1
#define SC89601S_VBUS_TYPE_ADAPTER     3
#define SC89601S_VBUS_TYPE_OTG         7

#define SC89601S_CHRG_STAT_MASK        0x18
#define SC89601S_CHRG_STAT_SHIFT       3
#define SC89601S_CHRG_STAT_IDLE        0
#define SC89601S_CHRG_STAT_PRECHG      1
#define SC89601S_CHRG_STAT_FASTCHG     2
#define SC89601S_CHRG_STAT_CHGDONE     3

#define SC89601S_PG_STAT_MASK          0x04
#define SC89601S_PG_STAT_SHIFT         2
#define SC89601S_POWER_GOOD            1

#define SC89601S_THERM_STAT_MASK       0x02
#define SC89601S_THERM_STAT_SHIFT      1

#define SC89601S_VSYS_STAT_MASK        0x01
#define SC89601S_VSYS_STAT_SHIFT       0
#define SC89601S_IN_VSYS_STAT          1


/* Register 0x09*/
#define SC89601S_REG_09              0x09
#define SC89601S_FAULT_WDT_MASK        0x80
#define SC89601S_FAULT_WDT_SHIFT       7
#define SC89601S_FAULT_WDT             1

#define SC89601S_FAULT_BOOST_MASK      0x40
#define SC89601S_FAULT_BOOST_SHIFT     6

#define SC89601S_FAULT_CHRG_MASK       0x30
#define SC89601S_FAULT_CHRG_SHIFT      4
#define SC89601S_FAULT_CHRG_NORMAL     0
#define SC89601S_FAULT_CHRG_INPUT      1
#define SC89601S_FAULT_CHRG_THERMAL    2
#define SC89601S_FAULT_CHRG_TIMER      3

#define SC89601S_FAULT_BAT_MASK        0x08
#define SC89601S_FAULT_BAT_SHIFT       3
#define	SC89601S_FAULT_BAT_OVP		    1

#define SC89601S_FAULT_NTC_MASK        0x07
#define SC89601S_FAULT_NTC_SHIFT       0
#define	SC89601S_FAULT_NTC_NORMAL		0
#define SC89601S_FAULT_NTC_WARM        2
#define SC89601S_FAULT_NTC_COOL        3
#define SC89601S_FAULT_NTC_COLD        5
#define SC89601S_FAULT_NTC_HOT         6


/* Register 0x0A */
#define SC89601S_REG_0A              0x0A
#define	SC89601S_VBUS_GD_MASK			0x80
#define	SC89601S_VBUS_GD_SHIFT			7
#define	SC89601S_VBUS_GD				1

#define	SC89601S_VINDPM_STAT_MASK		0x40
#define	SC89601S_VINDPM_STAT_SHIFT		6
#define	SC89601S_VINDPM_ACTIVE			1

#define	SC89601S_IINDPM_STAT_MASK		0x20
#define	SC89601S_IINDPM_STAT_SHIFT		5
#define	SC89601S_IINDPM_ACTIVE			1

#define	SC89601S_TOPOFF_ACTIVE_MASK	0x08
#define	SC89601S_TOPOFF_ACTIVE_SHIFT	3
#define	SC89601S_TOPOFF_ACTIVE			1

#define	SC89601S_ACOV_STAT_MASK		0x04
#define	SC89601S_ACOV_STAT_SHIFT		2
#define	SC89601S_ACOV_ACTIVE			1

#define	SC89601S_VINDPM_INT_MASK		0x02
#define	SC89601S_VINDPM_INT_SHIFT		1
#define	SC89601S_VINDPM_INT_ENABLE		0
#define	SC89601S_VINDPM_INT_DISABLE	1

#define	SC89601S_IINDPM_INT_MASK		0x01
#define	SC89601S_IINDPM_INT_SHIFT		0
#define	SC89601S_IINDPM_INT_ENABLE		0
#define	SC89601S_IINDPM_INT_DISABLE	1

#define	SC89601S_INT_MASK_MASK			0x03
#define	SC89601S_INT_MASK_SHIFT		0


#define	SC89601S_REG_0B				0x0B
#define	SC89601S_REG_RESET_MASK		0x80
#define	SC89601S_REG_RESET_SHIFT		7
#define	SC89601S_REG_RESET				1

#define SC89601S_PN_MASK             	0x78
#define SC89601S_PN_SHIFT            	3

#define SC89601S_DEV_REV_MASK        	0x03
#define SC89601S_DEV_REV_SHIFT       	0

#define SC89601S_REG_0C                  0x0C
#define SC89601S_JEITA_COOL_ISET2_MASK   0x80 
#define SC89601S_JEITA_COOL_ISET2_SHIFT  7

#define SC89601S_JEITA_WARM_VSET2_MASK   0x40 
#define SC89601S_JEITA_WARM_VSET2_SHIFT  6

#define SC89601S_JEITA_WARM_ISET_MASK    0x30
#define SC89601S_JEITA_WARM_ISET_SHIFT   4

#define SC89601S_JEITA_COOL_TEMP_MASK    0x0C 
#define SC89601S_JEITA_COOL_TEMP_SHIFT   2

#define SC89601S_JEITA_WARM_TEMP_MASK    0x03
#define SC89601S_JEITA_WARM_TEMP_SHIFT   0


#define SC89601S_REG_0D                  0x0D
#define SC89601S_VBAT_REG_FT_MASK        0xC0
#define SC89601S_VBAT_REG_FT_SHIFT       6
#define SC89601S_VREG_FT_DEFAULT			0
#define SC89601S_VREG_FT_INC8MV			1
#define SC89601S_VREG_FT_INC16MV			2
#define SC89601S_VREG_FT_INC24MV			3

#define SC89601S_BOOST_NTC_HOT_TEMP_MASK   0x30
#define SC89601S_BOOST_NTC_HOT_TEMP_SHIFT  4

#define SC89601S_BOOST_NTC_COOL_TEMP_MASK  0x08
#define SC89601S_BOOST_NTC_COOL_TEMP_SHIFT 3

#define SC89601S_BOOSTV3_MASK            0x04
#define SC89601S_BOOSTV3_SHIFT           2

#define SC89601S_BOOSTV0_MASK            0x02
#define SC89601S_BOOSTV0_SHIFT           1

#define SC89601S_ISHORT_MASK             0x01
#define SC89601S_ISHORT_SHIFT            0


#define SC89601S_REG_0E                  0x0E
#define SC89601S_VTC_MASK                0x80
#define SC89601S_VTC_SHIFT               7
#define SC89601S_VTC_2V8                 1
#define SC89601S_VTC_3V0                 0

#define SC89601S_INPUT_DET_DONE_MASK     0x40
#define SC89601S_INPUT_DET_DONE_SHIFT    6

#define SC89601S_AUTO_DPDM_EN_MASK       0x20
#define SC89601S_AUTO_DPDM_EN_SHIFT      5

#define SC89601S_BUCK_FREQ_MASK          0x10
#define SC89601S_BUCK_FREQ_SHIFT         4

#define SC89601S_BOOST_FREQ_MASK         0x08
#define SC89601S_BOOST_FREQ_SHIFT        3

#define SC89601S_VSYSOVP_MASK            0x06
#define SC89601S_VSYSOVP_SHIFT           1

#define SC89601S_NTC_DIS_MASK            0x01
#define SC89601S_NTC_DIS_SHIFT           0

#define SC89601S_REG_7F                0x7F
#define SC89601S_KEY1                    0x5A
#define SC89601S_KEY2                    0x68
#define SC89601S_KEY3                    0x65
#define SC89601S_KEY4                    0x6E
#define SC89601S_KEY5                    0x67
#define SC89601S_KEY6                    0x4C
#define SC89601S_KEY7                    0x69
#define SC89601S_KEY8                    0x6E

#define SC89601S_REG_81                0x81
#define SC89601S_HVDCP_EN_MASK           0x02
#define SC89601S_HVDCP_EN_SHIFT          1
#define SC89601S_HVDCP_ENABLE            1
#define SC89601S_HVDCP_DISABLE           0


#define SC89601S_REG_92                0x92
#define SC89601S_PFM_VAL                 0x71

#define SC89601S_REG_99                0x99

#endif


