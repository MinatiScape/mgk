#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
obj-$(CONFIG_PRIZE_HARDWARE_INFO) += hardware_info/
obj-$(CONFIG_CAMERA_GC0301)		+= prize_dual_camera/
#prize add by xuejian for gesture 20230210 start
obj-$(CONFIG_PRIZE_COMMON_NODE) += prize_common_node/
#prize add by xuejian for gesture 20230210 end
#add by driver for pixel_manager 20231124 start
obj-$(CONFIG_PRIZE_PIXEL_MANAGER) += pixel_manager/
#add by driver for pixel_manager 20231124 end

#drv-add by shenwenbin for oled panel 20231213 start
obj-$(CONFIG_PRIZE_OLED_NODE) += prize_oled_node/
#drv-add by shenwenbin for oled panel 20231213 end

obj-$(CONFIG_PRIZE_LEPTON_FILR)		+= FILR/
#drv add by lipengpeng 20240307 gms adc start
obj-$(CONFIG_DRV_ADC_GMS_DETECT) += gms_adc/
obj-$(CONFIG_DRV_LEPTON_POWER) += lepton_power/
#drv add by lipengpeng 20240307 gms adc end
obj-$(CONFIG_PRIZE_AW2016A)		+= aw2016a/

# Makefile for the board id drivers.
obj-$(CONFIG_PRIZE_BOARD_ID) += board_id/
# prize liuyong, add for sim control, 20231128, start
obj-$(CONFIG_PRIZE_SIM_SWITCH) += sim_switch/
# prize liuyong, add for sim control, 20231128, end
# prize liuyong, Add SIM card slot detection, 20240103, start
obj-$(CONFIG_PRIZE_SIM_SLOT_CHECK) += sim_slot/
# prize liuyong, Add SIM card slot detection, 20240103, end
#drv-add by yaozhipeng for fm ant 20240420 start
obj-$(CONFIG_PRIZE_FM_ANT_SWITCH) += fm_ant/
#drv-add by yaozhipeng for fm ant 20240420 end
#drv add by situziyin for 20240521 for uhf start
obj-$(CONFIG_PRIZE_UHF) += uhf/
#drv add by situziyin for 20240521 for uhf end
obj-$(CONFIG_PRIZE_CAMPING_LIGHT) += camping_light/
obj-$(CONFIG_DRV_FLASH_LIGHT) += flash_light/
#Driv: add for LAK-131 jinzhao 20240223
obj-$(CONFIG_PRIZE_SIM_STATE) += prize_sim_state/
#drv added by yaozhipeng, zkw beidou power enable, 20240918-start
obj-$(CONFIG_DRV_ZKWBD_EN) += beidou/
#drv added by yaozhipeng, zkw beidou power enable, 20240918-end