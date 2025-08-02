ccflags-y += \
		-DAW_HAVE_SNK \
		-DAW_HAVE_SRC \
		-DAW_HAVE_DRP \
		-DAW_HAVE_VBUS_ONLY \
		-DAW_HAVE_ACCMODE \
		-DAW_HAVE_EXT_MSG \
		-DAW_HAVE_VDM \
		-DAW_HAVE_DP

subdir-ccflags-y += -Wall -Werror
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/rt-regmap/
subdir-ccflags-y += -I$(srctree)/drivers/mfd/

obj-$(CONFIG_TCPC_MT6360)		+= tcpc_mt6360.o
obj-$(CONFIG_TCPC_MT6362)		+= tcpc_mt6362.o
obj-$(CONFIG_TCPC_MT6370)		+= tcpc_mt6370.o
obj-$(CONFIG_TCPC_MT6375)		+= tcpc_mt6375.o
obj-$(CONFIG_TCPC_RT1711H)		+= tcpc_rt1711h.o
obj-$(CONFIG_TCPC_AW35616)		+= tcpc_aw35616.o
obj-$(CONFIG_TCPC_AW35615_PD)		+= aw35615_whole.o

tcpc_class-objs := tcpci_core.o tcpci_typec.o tcpci_timer.o tcpm.o tcpci.o tcpci_alert.o

ifdef CONFIG_USB_POWER_DELIVERY
tcpc_class-objs += tcpci_event.o \
		pd_core.o pd_policy_engine.o pd_process_evt.o \
		pd_dpm_core.o pd_dpm_uvdm.o pd_dpm_alt_mode_dp.o pd_dpm_pdo_select.o\
		pd_dpm_reaction.o \
		pd_process_evt_snk.o pd_process_evt_src.o pd_process_evt_vdm.o \
		pd_process_evt_drs.o pd_process_evt_prs.o pd_process_evt_vcs.o \
		pd_process_evt_dbg.o pd_process_evt_tcp.o pd_process_evt_com.o \
		pd_policy_engine_src.o pd_policy_engine_snk.o pd_policy_engine_ufp.o pd_policy_engine_vcs.o \
		pd_policy_engine_dfp.o pd_policy_engine_dr.o pd_policy_engine_drs.o pd_policy_engine_prs.o \
		pd_policy_engine_dbg.o pd_policy_engine_com.o pd_dpm_alt_mode_dc.o
endif
obj-$(CONFIG_TCPC_CLASS) += tcpc_class.o tcpci_late_sync.o pd_dbg_info.o rt_pd_manager.o

aw35615_whole-objs := \
		aw35615_driver.o \
		aw35615_global.o \
		platform_helpers.o \
		dfs.o \
		core.o \
		PDPolicy.o \
		timer.o \
		PDProtocol.o \
		TypeC.o \
		Port.o \
		vendor_info.o \
		modules/dpm.o \
		modules/observer.o \
		vdm/bitfield_translators.o \
		vdm/vdm.o \
		vdm/vdm_callbacks.o \
		vdm/DisplayPort/dp.o \
