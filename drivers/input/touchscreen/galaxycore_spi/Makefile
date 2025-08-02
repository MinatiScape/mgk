#
# Makefile for the galaxycore_spi touchscreen driver.
#

# Each configuration option enables a list of files.
KBUILD_CFLAGS += -Wno-unused-function

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
obj-m += galaxycore_spi.o
galaxycore_spi-objs := gcore_drv_fw_update.o gcore_drv_interface.o gcore_drv_main.o \
gcore_drv_sprd.o