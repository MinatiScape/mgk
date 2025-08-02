###
# Copyright (C) Fourier Semiconductor Inc. 2016-2021. All rights reserved.
#

# TOPLEVEL=$(PWD)/..
# EXTRA_CFLAGS += -I$(TOPLEVEL)

# EXTRA_CFLAGS += -DDEBUG -DFSM_DEBUG
# EXTRA_CFLAGS += -Wall -Werror
MODFLAGS = -fno-pic
CFLAGS_MODULE = $(MODFLAGS)
AFLAGS_MODULE = $(MODFLAGS)

snd-soc-aw87xxx-objs += aw87xxx.o
snd-soc-aw87xxx-objs += aw87xxx_device.o
snd-soc-aw87xxx-objs += aw87xxx_monitor.o
snd-soc-aw87xxx-objs += aw87xxx_bin_parse.o
snd-soc-aw87xxx-objs += aw87xxx_dsp.o
snd-soc-aw87xxx-objs += aw87xxx_acf_bin.o

obj-$(CONFIG_SND_SOC_AW87XXX) += snd-soc-aw87xxx.o
