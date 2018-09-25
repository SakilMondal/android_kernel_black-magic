ccflags-y += -Idrivers/media/platform/msm/camera_v2_zc550kl
ccflags-y += -Idrivers/media/platform/msm/camera_v2_zc550kl/msm_vb2
ccflags-y += -Idrivers/media/platform/msm/camera_v2_zc550kl/camera
ccflags-y += -Idrivers/media/platform/msm/camera_v2_zc550kl/sensor/io
ccflags-y += -Idrivers/media/platform/msm/camera_v2_zc550kl/sensor/cci
ifneq ($(CONFIG_ASUS_ZC550KL_PROJECT),y)
	obj-$(CONFIG_MSMB_CAMERA) += cci/ io/ csiphy/ csid/ actuator/  eeprom/ flash/ laser_focus/
else
	obj-$(CONFIG_MSMB_CAMERA) += cci/ io/ csiphy/ csid/ actuator/  eeprom/ flash_zc550kl/ laser_focus_zc550kl/
endif
obj-$(CONFIG_MSM_CAMERA_SENSOR) += msm_sensor_init.o msm_sensor_driver.o msm_sensor.o
obj-$(CONFIG_MT9M114) += mt9m114.o
obj-$(CONFIG_OV5645) += ov5645.o
obj-$(CONFIG_OV5645) += ov7695.o
obj-$(CONFIG_GC0310) += gc0310.o
