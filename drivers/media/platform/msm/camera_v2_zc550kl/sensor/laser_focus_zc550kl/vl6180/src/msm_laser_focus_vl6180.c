/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include "msm_sd.h"
#include "msm_laser_focus.h"
#include "show_sysfs.h"
#include "msm_cci.h"
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "msm_laser_focus_vl6180x_api.h"
#include "kernel_driver_timer.h"
#include <linux/of_gpio.h>


#include "vl6180x_api.h"

#define VL6180_API_ERROR_COUNT_MAX 5
#define LOG_DISPLAY_COUNT 50

#define ASUS_RETRY_WAF 20
#define RETRY_WAF_TIMEOUT 500

//DEFINE_MSM_MUTEX(msm_laser_focus_mutex);

//static struct v4l2_file_operations msm_laser_focus_v4l2_subdev_fops;
static int32_t VL6180x_power_up(struct msm_laser_focus_ctrl_t *a_ctrl);
static int32_t VL6180x_power_down(struct msm_laser_focus_ctrl_t *a_ctrl);
static int VL6180x_match_id(struct msm_laser_focus_ctrl_t *s_ctrl);
static int VL6180x_init(struct msm_laser_focus_ctrl_t *a_ctrl);
static int VL6180x_deinit(struct msm_laser_focus_ctrl_t *a_ctrl);

static struct i2c_driver vl6180x_i2c_driver;

struct msm_laser_focus_ctrl_t *vl6180x_t = NULL;
struct timeval timer, timer2;
bool camera_on_flag = false;
int vl6180x_check_status = 0;

int log_count = 0;
static int laser_focus_enforce_ctrl = 0;

static int DMax = 0;
static int errorStatus = 16;

struct mutex vl6180x_mutex;

/*For LaserFocus STATUS Controll+++*/
#define	STATUS_PROC_FILE				"driver/LaserFocus_Status"
#define	STATUS_PROC_FILE_FOR_CAMERA	"driver/LaserFocus_Status_For_Camera"
#define	DEVICE_TURN_ON_FILE			"driver/LaserFocus_on"
#define	DEVICE_GET_VALUE				"driver/LaserFocus_value"
#define	DEVICE_SET_CALIBRATION			"driver/LaserFocus_CalStart"
#define	DEVICE_DUMP_REGISTER_VALUE	"driver/LaserFocus_register_dump"
#define	DEVICE_DUMP_DEBUG_REGISTER_VALUE        "driver/LaserFocus_debug_dump"
#define	LASER_FOCUS_ENFORCE			"driver/LaserFocus_enforce"
#define   DEVICE_GET_VALUE_MORE_INFO   "driver/LaserFocus_value_more_info"
#define	I2C_STATUS_FILE				"driver/LaserFocus_I2c_Status_VL6180x"
static struct proc_dir_entry *status_proc_file;
static struct proc_dir_entry *device_trun_on_file;
static struct proc_dir_entry *device_get_value_file;
static struct proc_dir_entry *device_set_calibration_file;
static struct proc_dir_entry *dump_laser_focus_register_file;
static struct proc_dir_entry *dump_laser_focus_debug_file;
static struct proc_dir_entry *enforce_proc_file;
static struct proc_dir_entry *i2c_status_file;
static int ATD_status;


#ifdef CONFIG_I2C_STRESS_TEST

#include <linux/i2c_testcase.h>

#define I2C_TEST_LASERFOCUS_FAIL (-1)

static int TestLaserFocusI2C(struct i2c_client *apClient)
{
	int32_t rc ;

	i2c_log_in_test_case("TestLaserFocusI2C ++\n");
	mutex_lock(&vl6180x_mutex);

    if (!camera_on_flag)
    {//camera not running
		pr_err("czw, laser full test");
		rc = VL6180x_power_up(vl6180x_t);
		if (rc < 0) {
			pr_err("%s VL6180x_power_up failed %d\n", __func__, __LINE__);
			goto FAIL;
		}

		VL6180x_init(vl6180x_t);
		if (rc < 0) {
			pr_err("%s VL6180x_init failed %d\n", __func__, __LINE__);
			goto FAIL;
		}

		rc = VL6180x_match_id(vl6180x_t);
		if (rc < 0) {
			pr_err("%s VL6180x_match_id failed %d\n", __func__, __LINE__);

			rc = VL6180x_deinit(vl6180x_t);
			if (rc < 0) {
				pr_err("%s VL6180x_deinit failed %d\n", __func__, __LINE__);
			}

			rc = VL6180x_power_down(vl6180x_t);
			if (rc < 0) {
				pr_err("%s VL6180x_power_down failed %d\n", __func__, __LINE__);
			}

			goto FAIL;
		}

		rc = VL6180x_deinit(vl6180x_t);
		if (rc < 0) {
			pr_err("%s VL6180x_deinit failed %d\n", __func__, __LINE__);
			goto FAIL;
		}

		rc = VL6180x_power_down(vl6180x_t);
		if (rc < 0) {
			pr_err("%s VL6180x_power_down failed %d\n", __func__, __LINE__);
			goto FAIL;
		}
	}
	else
	{//camera on
		pr_err("czw, laser read test");
		rc = VL6180x_match_id(vl6180x_t);
		if (rc < 0) {
			pr_err("%s VL6180x_match_id failed %d\n", __func__, __LINE__);
			goto FAIL;
		}
	}

	mutex_unlock(&vl6180x_mutex);

	i2c_log_in_test_case("TestLaserFocusI2C --\n");
	return I2C_TEST_PASS;
FAIL:
	mutex_unlock(&vl6180x_mutex);
	i2c_log_in_test_case("TestLaserFocusI2C failed --\n");
	return I2C_TEST_LASERFOCUS_FAIL;
}

static struct i2c_test_case_info LaserFocusTestCaseInfo[] =
{
     __I2C_STRESS_TEST_CASE_ATTR(TestLaserFocusI2C),
};
#endif

int ASUS_VL6180x_WrByte(uint32_t register_addr, uint16_t i2c_write_data)
{
	int status;
	/* Setting i2c client */
	struct msm_camera_i2c_client *sensor_i2c_client;
	sensor_i2c_client = vl6180x_t->i2c_client;
	if (!sensor_i2c_client) {
		pr_err("%s:%d failed: %p \n", __func__, __LINE__, sensor_i2c_client);
		return -EINVAL;
	}

	status = sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, register_addr,
				i2c_write_data, MSM_CAMERA_I2C_BYTE_DATA);
	if (status < 0) {
		pr_err("%s: wirte register(0x%x) failed\n", __func__, register_addr);
		return status;
	}
	status =0;
	REG_RW_DBG("%s: wirte register(0x%x) : 0x%x \n", __func__, register_addr, i2c_write_data);
	return status;
}

int ASUS_VL6180x_RdByte(uint32_t register_addr, uint16_t *i2c_read_data)
{
        int status;
        
        /* Setting i2c client */
        struct msm_camera_i2c_client *sensor_i2c_client;
        sensor_i2c_client = vl6180x_t->i2c_client;
        if (!sensor_i2c_client) {
                pr_err("%s:%d failed: %p \n", __func__, __LINE__, sensor_i2c_client);
                return -EINVAL;
        }

        status = sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, register_addr,
                        i2c_read_data, MSM_CAMERA_I2C_BYTE_DATA);
        if (status < 0) {
                pr_err("%s: read register(0x%x) failed\n", __func__, register_addr);
                return status;
        }
        REG_RW_DBG("%s: read register(0x%x) : 0x%x \n", __func__, register_addr, *i2c_read_data);
		status =0;
        return status;
}

int ASUS_VL6180x_WrWord(uint32_t register_addr, uint16_t i2c_write_data)
{
	int status;
	/* Setting i2c client */
	struct msm_camera_i2c_client *sensor_i2c_client;
	sensor_i2c_client = vl6180x_t->i2c_client;
	if (!sensor_i2c_client) {
		pr_err("%s:%d failed: %p \n", __func__, __LINE__, sensor_i2c_client);
		return -EINVAL;
	}

	status = sensor_i2c_client->i2c_func_tbl->i2c_write(sensor_i2c_client, register_addr,
				i2c_write_data, MSM_CAMERA_I2C_WORD_DATA);
	if (status < 0) {
		pr_err("%s: wirte register(0x%x) failed\n", __func__, register_addr);
		return status;
	}
	REG_RW_DBG("%s: wirte register(0x%x) : 0x%x \n", __func__, register_addr, i2c_write_data);
	status =0;
	return status;
}

int ASUS_VL6180x_RdWord(uint32_t register_addr, uint16_t *i2c_read_data)
{
	int status;
	/* Setting i2c client */
	struct msm_camera_i2c_client *sensor_i2c_client;
	sensor_i2c_client = vl6180x_t->i2c_client;
	if (!sensor_i2c_client) {
		pr_err("%s:%d failed: %p \n", __func__, __LINE__, sensor_i2c_client);
		return -EINVAL;
	}

	status = sensor_i2c_client->i2c_func_tbl->i2c_read(sensor_i2c_client, register_addr,
		i2c_read_data, MSM_CAMERA_I2C_WORD_DATA);
	if (status < 0) {
		pr_err("%s: read register(0x%x) failed\n", __func__, register_addr);
		return status;
	}
	REG_RW_DBG("%s: read register(0x%x) : 0x%x \n", __func__, register_addr, *i2c_read_data);
	status =0;
	return status;
}

int ASUS_VL6180x_WrDWord(uint32_t register_addr, uint32_t i2c_write_data){
	int status;
	uint16_t high_val, low_val;
	
	high_val = (uint16_t)((i2c_write_data&0xFFFF0000)>>16);
	low_val = (uint16_t)(i2c_write_data&0x0000FFFF);
	
	status = ASUS_VL6180x_WrWord(register_addr, high_val);
	if (status < 0) {
		pr_err("%s: wirte register(0x%x) failed\n", __func__, register_addr);
		return status;
	}
	
	status = ASUS_VL6180x_WrWord(register_addr+2, low_val);
	if (status < 0) {
		pr_err("%s: wirte register(0x%x) failed\n", __func__, register_addr+2);
		return status;
	}

	REG_RW_DBG("%s: wirte register(0x%x) : 0x%x\n", __func__, register_addr, i2c_write_data);	
	status =0;
	return status;
}

#if 0
int ASUS_VL6180x_WrDWord(uint32_t register_addr, uint32_t i2c_write_data, uint16_t num_byte)
{
	int status;
	uint16_t Data_value = 0;
	struct msm_camera_i2c_seq_reg_array reg_setting;
	
	/* Setting i2c client */
	struct msm_camera_i2c_client *sensor_i2c_client;
	sensor_i2c_client = vl6180x_t->i2c_client;
	if (!sensor_i2c_client) {
		pr_err("%s:%d failed: %p \n", __func__, __LINE__, sensor_i2c_client);
		return -EINVAL;
	}

	reg_setting.reg_addr = register_addr;
	reg_setting.reg_data[0] = (uint8_t)((i2c_write_data & 0xFF000000) >> 24);
	reg_setting.reg_data[1] = (uint8_t)((i2c_write_data & 0x00FF0000) >> 16);
	reg_setting.reg_data[2] = (uint8_t)((i2c_write_data & 0x0000FF00) >> 8);
	reg_setting.reg_data[3] = (uint8_t)(i2c_write_data & 0x000000FF);
	reg_setting.reg_data_size = num_byte;

	printk("[Debug] write reg_data[0]:0x%x ; reg_data[1]:0x%x ; reg_data[2]:0x%x ; reg_data[3]:0x%x\n", 
		reg_setting.reg_data[0], reg_setting.reg_data[1], reg_setting.reg_data[2], reg_setting.reg_data[3]);
	
	//status = (int)msm_camera_cci_i2c_write_seq(sensor_i2c_client, register_addr, 
	//	reg_setting.reg_data, reg_setting.reg_data_size);
	status = sensor_i2c_client->i2c_func_tbl->i2c_write_seq(sensor_i2c_client, register_addr, 
		reg_setting.reg_data, reg_setting.reg_data_size);
	
	status = sensor_i2c_client->i2c_func_tbl->i2c_read_seq(sensor_i2c_client, register_addr, 
		reg_setting.reg_data, reg_setting.reg_data_size);
	printk("[Debug] read reg_data[0]:0x%x ; reg_data[1]:0x%x ; reg_data[2]:0x%x ; reg_data[3]:0x%x\n", 
		reg_setting.reg_data[0], reg_setting.reg_data[1], reg_setting.reg_data[2], reg_setting.reg_data[3]);
	ASUS_VL6180x_RdWord(register_addr, &Data_value);
	
	if (status < 0) {
		pr_err("%s: wirte register(0x%x) failed\n", __func__, register_addr);
		return status;
	}
	REG_RW_DBG("%s: wirte register(0x%x) : 0x%x\n", __func__, register_addr, i2c_write_data);
	status =0;
	return status;
}
#endif

int ASUS_VL6180x_RdDWord(uint32_t register_addr, uint32_t *i2c_read_data, uint16_t num_byte)
{
	int status;
	struct msm_camera_i2c_seq_reg_array reg_setting;
	
	/* Setting i2c client */
	struct msm_camera_i2c_client *sensor_i2c_client;
	sensor_i2c_client = vl6180x_t->i2c_client;
	if (!sensor_i2c_client) {
		pr_err("%s:%d failed: %p \n", __func__, __LINE__, sensor_i2c_client);
		return -EINVAL;
	}

	reg_setting.reg_data_size = num_byte;

	status = (int)sensor_i2c_client->i2c_func_tbl->i2c_read_seq(sensor_i2c_client, register_addr, 
		reg_setting.reg_data, reg_setting.reg_data_size);
	
	if (status < 0) {
		pr_err("%s: read register(0x%x) failed\n", __func__, register_addr);
		return status;
	}
	
	*i2c_read_data=((uint32_t)reg_setting.reg_data[0]<<24)|((uint32_t)reg_setting.reg_data[1]<<16)|((uint32_t)reg_setting.reg_data[2]<<8)|((uint32_t)reg_setting.reg_data[3]);
	REG_RW_DBG("%s: read register(0x%x) : 0x%x \n", __func__, register_addr, *i2c_read_data);
	status =0;
	return status;
}

int ASUS_VL6180x_RdMulti(uint32_t register_addr, uint8_t *i2c_read_data, uint16_t num_byte)
{
	int status;
	struct msm_camera_i2c_seq_reg_array reg_setting;
	int i;
	/* Setting i2c client */
	struct msm_camera_i2c_client *sensor_i2c_client;
	sensor_i2c_client = vl6180x_t->i2c_client;
	if (!sensor_i2c_client) {
		pr_err("%s:%d failed: %p \n", __func__, __LINE__, sensor_i2c_client);
		return -EINVAL;
	}

	reg_setting.reg_data_size = num_byte;

	status = (int)sensor_i2c_client->i2c_func_tbl->i2c_read_seq(sensor_i2c_client, register_addr,
		i2c_read_data, num_byte);

	if (status < 0) {
		pr_err("%s: read register(0x%x) failed\n", __func__, register_addr);
		return status;
	}

	REG_RW_DBG("%s: read register(0x%x) : %d bytes\n", __func__, register_addr, num_byte);
	REG_RW_DBG("START\n");
	for(i=0;i<num_byte;i++)
	{
		REG_RW_DBG("0x%x\n",i2c_read_data[i]);
	}
	REG_RW_DBG("END\n");
	status =0;
	return status;
}


int ASUS_VL6180x_UpdateByte(uint32_t register_addr, uint8_t AndData, uint8_t OrData){
	int status;
	uint16_t i2c_read_data, i2c_write_data;

	status = ASUS_VL6180x_RdByte(register_addr, &i2c_read_data);
	if (status < 0) {
		pr_err("%s: read register(0x%x) failed\n", __func__, register_addr);
		return status;
	}

	i2c_write_data = ((uint8_t)(i2c_read_data&0x00FF)&AndData) | OrData;

	status = ASUS_VL6180x_WrByte(register_addr, i2c_write_data);

	if (status < 0) {
		pr_err("%s: wirte register(0x%x) failed\n", __func__, register_addr);
		return status;
	}

 	REG_RW_DBG("%s: update register(0x%x) from 0x%x to 0x%x(AndData:0x%x;OrData:0x%x)\n", __func__, register_addr, i2c_read_data, i2c_write_data,AndData,OrData);
	status =0;
	return status;
}

#if 0
static bool sysfs_write(char *filename, int calvalue)
{
       struct file *fp = NULL;
       mm_segment_t old_fs;
       loff_t pos_lsts = 0;
       char buf[8];

       sprintf(buf, "%d", calvalue);

       fp = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
       if (IS_ERR_OR_NULL(fp)) {
               pr_err("[S_DEBUG] File open (%s) fail\n", filename);
               return false;
       }

       /*For purpose that can use read/write system call*/
       old_fs = get_fs();
       set_fs(KERNEL_DS);

       if (fp->f_op != NULL && fp->f_op->write != NULL) {
               pos_lsts = 0;
               fp->f_op->write(fp, buf, strlen(buf), &fp->f_pos);
       } else {
               pr_err("[S_DEBUG] File strlen: f_op=NULL or op->write=NULL\n");
               return false;
       }
       set_fs(old_fs);
       filp_close(fp, NULL);

       printk("[S_DEBUG] write value %s to %s\n", buf, filename);

       return true;
}


static void debug_read_range(VL6180x_RangeData_t *pRangeData){

       uint16_t reg_data = 0;

       ASUS_VL6180x_RdWord(SYSRANGE_CROSSTALK_COMPENSATION_RATE, &reg_data);
       //printk("[S_DEBUG] register(0x%x) : 0x%x\n", SYSRANGE_CROSSTALK_COMPENSATION_RATE, reg_data);
       sysfs_write("/factory/LaserFocus_0x1e.txt",reg_data);
       ASUS_VL6180x_RdByte(SYSRANGE_PART_TO_PART_RANGE_OFFSET, &reg_data);
       //printk("[S_DEBUG] register(0x%x) : 0x%x\n", SYSRANGE_PART_TO_PART_RANGE_OFFSET, reg_data);
       sysfs_write("/factory/LaserFocus_0x24.txt",reg_data);
       ASUS_VL6180x_RdByte(RESULT_RANGE_STATUS, &reg_data);
       //printk("[S_DEBUG] register(0x%x) : 0x%x\n", RESULT_RANGE_STATUS, reg_data);
       sysfs_write("/factory/LaserFocus_0x4d.txt",reg_data);
       ASUS_VL6180x_RdByte(RESULT_RANGE_VAL, &reg_data);
       //printk("[S_DEBUG] register(0x%x) : 0x%x\n", RESULT_RANGE_VAL, reg_data);
       sysfs_write("/factory/LaserFocus_0x62.txt",reg_data);
       ASUS_VL6180x_RdByte(RESULT_RANGE_RAW, &reg_data);
       //printk("[S_DEBUG] register(0x%x) : 0x%x\n", RESULT_RANGE_RAW, reg_data);
       sysfs_write("/factory/LaserFocus_0x64.txt",reg_data);
       ASUS_VL6180x_RdWord(RESULT_RANGE_SIGNAL_RATE, &reg_data);
       //printk("[S_DEBUG] register(0x%x) : 0x%x\n", RESULT_RANGE_SIGNAL_RATE, reg_data);
       sysfs_write("/factory/LaserFocus_0x66.txt",reg_data);
       //printk("[S_DEBUG] DMax : %d0\n", pRangeData->DMax);
       sysfs_write("/factory/LaserFocus_DMax.txt",pRangeData->DMax);
       //printk("[S_DEBUG] errorCode : %d\n", pRangeData->errorStatus);
       sysfs_write("/factory/LaserFocus_errorStatus.txt",pRangeData->errorStatus);
}
#endif

static int VL6180x_device_Load_Calibration_Value(void){
	int status = 0;
	int offset = 0, cross_talk = 0;

	if (vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_APPLY_CALIBRATION ||
		vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_INIT_CCI){
		/* Read Calibration data */
		offset = Laser_Forcus_sysfs_read_offset();
		cross_talk = Laser_Forcus_sysfs_read_cross_talk_offset();
		vl6180x_t->laser_focus_offset_value = offset;
		vl6180x_t->laser_focus_cross_talk_offset_value = cross_talk; 

		/* Apply Calibration value */
		if(offset>=0){
			API_DBG("%s: VL6180x_SetOffsetCalibrationData Start\n", __func__);
			VL6180x_SetOffsetCalibrationData(0, vl6180x_t->laser_focus_offset_value);
			API_DBG("%s: VL6180x_SetOffsetCalibrationData Success\n", __func__);
#if 0
			status = ASUS_VL6180x_WrByte(SYSRANGE_PART_TO_PART_RANGE_OFFSET, vl6180x_t->laser_focus_offset_value);
			if (status < 0) {
				pr_err("%s: wirte register(0x%x) failed\n", __func__, SYSRANGE_PART_TO_PART_RANGE_OFFSET);
				return status;
			}
#endif
		}
		if(cross_talk>=0){
			API_DBG("%s: VL6180x_SetXTalkCompensationRate Start\n", __func__);
			status = VL6180x_SetXTalkCompensationRate(0, vl6180x_t->laser_focus_cross_talk_offset_value);
			if (status < 0){
				   pr_err("%s Device trun on fail !!\n", __func__);
				   return -EIO;
			}
			API_DBG("%s: VL6180x_SetXTalkCompensationRate Success\n", __func__);
#if 0
			status = ASUS_VL6180x_WrWord(SYSRANGE_CROSSTALK_COMPENSATION_RATE, vl6180x_t->laser_focus_cross_talk_offset_value);
			if (status < 0) {
				pr_err("%s: wirte register(0x%x) failed\n", __func__, SYSRANGE_CROSSTALK_COMPENSATION_RATE);
				return status;
			}
#endif
		}
	}
	
	return status;
}

static ssize_t ATD_VL6180x_device_enable_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
	int val, rc = 0;
	char messages[8];
	memset(messages, 0, 8);
	if (len > 8) {
		len = 8;
	}
	if (copy_from_user(messages, buff, len)) {
		printk("%s commond fail !!\n", __func__);
		return -EFAULT;
	}

	val = (int)simple_strtol(messages, NULL, 10);
	if (vl6180x_t->device_state == val)	{
		printk("%s Setting same commond (%d) !!\n", __func__, val);
		return -EINVAL;
	}
	switch (val) {
		case MSM_LASER_FOCUS_DEVICE_OFF:
			mutex_lock(&vl6180x_mutex);
			if(camera_on_flag){
				CDBG("%s: %d Camera is running, do nothing!!\n ", __func__, __LINE__);
				mutex_unlock(&vl6180x_mutex);
				break;
			}
			rc = VL6180x_deinit(vl6180x_t);
			rc = VL6180x_power_down(vl6180x_t);
			vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
			mutex_unlock(&vl6180x_mutex);
			break;
		case MSM_LASER_FOCUS_DEVICE_APPLY_CALIBRATION:
			if(camera_on_flag){
				CDBG("%s: %d Camera is running, do nothing!!\n ", __func__, __LINE__);
				break;
			}
			if (vl6180x_t->device_state != MSM_LASER_FOCUS_DEVICE_OFF)	{
				rc = VL6180x_deinit(vl6180x_t);
				rc = VL6180x_power_down(vl6180x_t);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
			}
			vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_APPLY_CALIBRATION;
			rc = VL6180x_power_up(vl6180x_t);
			rc = VL6180x_init(vl6180x_t);
			if (rc < 0){
				pr_err("%s Device trun on fail !!\n", __func__);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
				return -EIO;
			} 

			API_DBG("%s: VL6180x_InitData Start\n", __func__);
			rc = VL6180x_InitData(0);
			if (rc < 0){
				pr_err("%s Device trun on fail !!\n", __func__);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
				//return -EIO;
				goto DEVICE_TURN_ON_ERROR;
			}
			API_DBG("%s: VL6180x_InitData Success\n", __func__);

			API_DBG("%s: VL6180x_Prepare Start\n", __func__);
			rc = VL6180x_Prepare(0);
			if (rc < 0){
				pr_err("%s Device trun on fail !!\n", __func__);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
				//return -EIO;
				goto DEVICE_TURN_ON_ERROR;
			}
			API_DBG("%s: VL6180x_Prepare Success\n", __func__);


			rc = VL6180x_device_Load_Calibration_Value();
			if (rc < 0){
				pr_err("%s Device trun on fail !!\n", __func__);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
				//return -EIO;
				goto DEVICE_TURN_ON_ERROR;
			}
#if 0
			API_DBG("%s: VL6180x_RangeSetSystemMode Start\n", __func__);
			rc = VL6180x_RangeSetSystemMode(0, MODE_START_STOP|MODE_SINGLESHOT);
			if (rc < 0){
				pr_err("%s Device trun on fail !!\n", __func__);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
				//return -EIO;
				goto DEVICE_TURN_ON_ERROR;
			}
			API_DBG("%s: VL6180x_RangeSetSystemMode Success\n", __func__);
#endif
			vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_APPLY_CALIBRATION;
			printk("%s Init Device (%d)\n", __func__, vl6180x_t->device_state);
		
			break;
		case MSM_LASER_FOCUS_DEVICE_NO_APPLY_CALIBRATION:
			if(camera_on_flag){
				CDBG("%s: %d Camera is running, do nothing!!\n ", __func__, __LINE__);
				break;
			}
			if (vl6180x_t->device_state != MSM_LASER_FOCUS_DEVICE_OFF)	{
				rc = VL6180x_deinit(vl6180x_t);
				rc = VL6180x_power_down(vl6180x_t);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
			}
			vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_NO_APPLY_CALIBRATION;
			rc = VL6180x_power_up(vl6180x_t);
			rc = VL6180x_init(vl6180x_t);
			if (rc < 0){
				pr_err("%s Device trun on fail !!\n", __func__);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
				return -EIO;
			} 

			API_DBG("%s: VL6180x_InitData Start\n", __func__);
			rc = VL6180x_InitData(0);
			if (rc < 0){
				pr_err("%s Device trun on fail !!\n", __func__);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
				//return -EIO;
				goto DEVICE_TURN_ON_ERROR;
			}
			API_DBG("%s: VL6180x_InitData Success\n", __func__);

			API_DBG("%s: VL6180x_Prepare Start\n", __func__);
			rc = VL6180x_Prepare(0);
			if (rc < 0){
				pr_err("%s Device trun on fail !!\n", __func__);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
				//return -EIO;
				goto DEVICE_TURN_ON_ERROR;
			}
			API_DBG("%s: VL6180x_Prepare Success\n", __func__);

#if 0
			API_DBG("%s: VL6180x_RangeSetSystemMode Start\n", __func__);
			rc = VL6180x_RangeSetSystemMode(0, MODE_START_STOP|MODE_SINGLESHOT);
			if (rc < 0){
				pr_err("%s Device trun on fail !!\n", __func__);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
				//return -EIO;
				goto DEVICE_TURN_ON_ERROR;
			}
			API_DBG("%s: VL6180x_RangeSetSystemMode Success\n", __func__);
#endif
			vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_NO_APPLY_CALIBRATION;
			printk("%s Init Device (%d)\n", __func__, vl6180x_t->device_state);
		
			break;
		case MSM_LASER_FOCUS_DEVICE_INIT_CCI:
			mutex_lock(&vl6180x_mutex);
			vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_INIT_CCI;
			rc = VL6180x_init(vl6180x_t);
			if (rc < 0){
				pr_err("%s Device trun on fail !!\n", __func__);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
				mutex_unlock(&vl6180x_mutex);
				return -EIO;
			} 

			API_DBG("%s: VL6180x_InitData Start\n", __func__);
			rc = VL6180x_InitData(0);
			if (rc < 0){
				pr_err("%s Device trun on fail !!\n", __func__);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
				mutex_unlock(&vl6180x_mutex);
				return -EIO;
			}
			API_DBG("%s: VL6180x_InitData Success\n", __func__);

			API_DBG("%s: VL6180x_Prepare Start\n", __func__);
			rc = VL6180x_Prepare(0);
			if (rc < 0){
				pr_err("%s Device trun on fail !!\n", __func__);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
				mutex_unlock(&vl6180x_mutex);
				return -EIO;
			}
			API_DBG("%s: VL6180x_Prepare Success\n", __func__);

			rc = VL6180x_device_Load_Calibration_Value();
			if (rc < 0){
				pr_err("%s Device trun on fail !!\n", __func__);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
				mutex_unlock(&vl6180x_mutex);
				return -EIO;
			}
				
			API_DBG("%s: VL6180x_RangeSetSystemMode Start\n", __func__);
			rc = VL6180x_RangeSetSystemMode(0, MODE_START_STOP|MODE_SINGLESHOT);
			if (rc < 0){
				pr_err("%s Device trun on fail !!\n", __func__);
				vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_OFF;
				mutex_unlock(&vl6180x_mutex);
				return -EIO;
			}
			API_DBG("%s: VL6180x_RangeSetSystemMode Success\n", __func__);

			vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_INIT_CCI;
			printk("%s Init Device (%d)\n", __func__, vl6180x_t->device_state);
			camera_on_flag = true;
			mutex_unlock(&vl6180x_mutex);
			break;
		case MSM_LASER_FOCUS_DEVICE_DEINIT_CCI:
			mutex_lock(&vl6180x_mutex);
			rc = VL6180x_deinit(vl6180x_t);
			vl6180x_t->device_state = MSM_LASER_FOCUS_DEVICE_DEINIT_CCI;
			camera_on_flag = false;
			mutex_unlock(&vl6180x_mutex);
			break;
		default:
			printk("%s commond fail !!\n", __func__);
			break;
	}
	return len;

DEVICE_TURN_ON_ERROR:
	rc = VL6180x_deinit(vl6180x_t);
	if (rc < 0) {
		//kfree(vl6180x_t);
		pr_err("%s VL6180x_deinit failed %d\n", __func__, __LINE__);
	}
		
	rc = VL6180x_power_down(vl6180x_t);
	if (rc < 0) {
		//kfree(vl6180x_t);
		pr_err("%s VL6180x_power_down failed %d\n", __func__, __LINE__);
	}
	return -EIO;
}

static int ATD_VL6180x_device_enable_read(struct seq_file *buf, void *v)
{
	seq_printf(buf, "%d\n", vl6180x_t->device_state);
	return 0;
}

static int ATD_VL6180x_device_enable_open(struct inode *inode, struct  file *file)
{
	return single_open(file, ATD_VL6180x_device_enable_read, NULL);
}

static const struct file_operations ATD_laser_focus_device_enable_fops = {
	.owner = THIS_MODULE,
	.open = ATD_VL6180x_device_enable_open,
	.write = ATD_VL6180x_device_enable_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//ASUS_BSP Sam add retry for vl6180 WAF +++
static uint64_t asus_get_time(void)
{
	struct timeval t;
	t.tv_sec = t.tv_usec = 0;
	t = get_current_time();
	return (t.tv_sec*1000000 + t.tv_usec)/1000;
}
//ASUS_BSP Sam add retry for vl6180 WAF ---

static int ATD_VL6180x_device_read_range(VL6180x_RangeData_t *pRangeData)
{
	int status, i = 0;
	//struct msm_camera_i2c_client *sensor_i2c_client;
	uint8_t intStatus;
	//VL6180x_RangeData_t RangeData;
	int16_t RawRange;

//ASUS_BSP Sam add retry for vl6180 WAF +++
	int j = 0;
	uint16_t reg_status = 0;
	uint64_t start_time = 0, end_time = 0, diff_time = 0;
//ASUS_BSP Sam add retry for vl6180 WAF ---

	timer = get_current_time();

	log_count ++;

#if 0
	/* Setting i2c client */
	sensor_i2c_client = vl6180x_t->i2c_client;
	if (!sensor_i2c_client) {
		pr_err("%s:%d failed: %p \n", __func__, __LINE__, sensor_i2c_client);
		return -EINVAL;
	}
#endif

	/* Setting Range meansurement in single-shot mode */	
	API_DBG("%s: VL6180x_RangeSetSystemMode Start\n", __func__);
	status = VL6180x_RangeClearInterrupt(0);
	if (status < 0) {
		pr_err("%s: VL6180x_RangeClearInterrupt failed\n", __func__);
		return status;
	}
	API_DBG("%s: VL6180x_RangeSetSystemMode Success\n", __func__);

	API_DBG("%s: VL6180x_RangeSetSystemMode Start\n", __func__);
	status = VL6180x_RangeSetSystemMode(0, MODE_START_STOP|MODE_SINGLESHOT);
	if (status < 0) {
		pr_err("%s: VL6180x_RangeSetSystemMode failed\n", __func__);
		return status;
	}
	API_DBG("%s: VL6180x_RangeSetSystemMode Success\n", __func__);

	RawRange = 0;

	/* Delay: waitting laser sensor to be triggered */
	msleep(8);

	/* Get Sensor detect distance */
	for (i = 0; i <1000; i++)	{
		/* Check RESULT_INTERRUPT_STATUS_GPIO */
		API_DBG("%s: VL6180x_RangeGetInterruptStatus Start\n", __func__);
		status = VL6180x_RangeGetInterruptStatus(0, &intStatus);
		if (status < 0) {
			pr_err("%s: VL6180x_RangeGetInterruptStatus failed\n", __func__);
			return status;
		}
		API_DBG("%s: VL6180x_RangeGetInterruptStatus Success\n", __func__);

		if (intStatus == RES_INT_STAT_GPIO_NEW_SAMPLE_READY){
			//ASUS_BSP Sam add retry for vl6180 WAF +++
			//pr_err("[S-DBEUG]%s: VL6180x_RangeGetMeasurement Start\n", __func__);
			start_time = asus_get_time();
			for(j = 0; j < ASUS_RETRY_WAF; j++)
			{
				status = VL6180x_RangeGetMeasurement(0, pRangeData);
				ASUS_VL6180x_RdByte(RESULT_RANGE_STATUS, &reg_status);
				end_time = asus_get_time();
				diff_time = end_time - start_time;
				if((int)pRangeData->range_mm !=765 || pRangeData->errorStatus != 17 || (reg_status & 0xF0) !=0)
					break;
				if(diff_time > RETRY_WAF_TIMEOUT)
				{
					printk("[S-DEBUG]%s: WAF is timeout, quit the loop!!\n", __func__);
					break;
				}
			}
			//pr_err("[S-DBEUG]%s: VL6180x_RangeGetMeasurement end diff:%llu\n", __func__, diff_time);
			//ASUS_BSP Sam add retry for vl6180 WAF ---
			if (status < 0) {
				pr_err("%s: Read range failed\n", __func__);
				goto ERR_OUT_OF_RANGE;
			}
			if (pRangeData->errorStatus == 0){
				if(log_count >= LOG_DISPLAY_COUNT){
					   log_count = 0;
					   DBG_LOG("%s: Read range:%d\n", __func__, (int)pRangeData->range_mm);
				}
				API_DBG("%s: VL6180x_RangeGetMeasurement Success\n", __func__);
				break;
			}
			else{
				API_DBG("%s: VL6180x_RangeGetMeasurement Failed: errorStatus(%d)\n", __func__, pRangeData->errorStatus);
				goto ERR_OUT_OF_RANGE;
			}
		}
		timer2 = get_current_time();
		if((((timer2.tv_sec*1000000)+timer2.tv_usec)-((timer.tv_sec*1000000)+timer.tv_usec)) > (TIMEOUT_VAL*1000)){
			printk("%s: Timeout: Out Of Range!!\n", __func__);
			//return OUT_OF_RANGE;
			goto ERR_OUT_OF_RANGE;
		}

		/* Delay: waitting laser sensor sample ready */
		msleep(5);
	}

#if 0
	if (intStatus == RES_INT_STAT_GPIO_NEW_SAMPLE_READY){
	   API_DBG("%s: VL6180x_RangeGetMeasurement Start\n", __func__);
	   status = VL6180x_RangeGetMeasurement(0, pRangeData);
	   if (status < 0) {
		   pr_err("%s: RangeGetMeasurement failed\n", __func__);
		   return status;
		}
	   API_DBG("%s: VL6180x_RangeGetMeasurement Success\n", __func__);
	}
#endif

	/* Setting SYSTEM_INTERRUPT_CLEAR to 0x01 */
	API_DBG("%s: VL6180x_RangeClearInterrupt Start\n", __func__);
	status = VL6180x_RangeClearInterrupt(0);
	if (status < 0) {
		pr_err("%s: VL6180x_RangeClearInterrupt failed\n", __func__);
		return status;
	}
	API_DBG("%s: VL6180x_RangeClearInterrupt Success\n", __func__);

	return (int)pRangeData->range_mm;
ERR_OUT_OF_RANGE:
	/* Setting SYSTEM_INTERRUPT_CLEAR to 0x01 */
	API_DBG("%s: VL6180x_RangeClearInterrupt Start\n", __func__);
	status = VL6180x_RangeClearInterrupt(0);
	if (status < 0) {
		pr_err("%s: VL6180x_RangeClearInterrupt failed\n", __func__);
		return status;
	}
	API_DBG("%s: VL6180x_RangeClearInterrupt Success\n", __func__);

	return OUT_OF_RANGE;
}

static int ATD_VL6180x_device_get_range_read(struct seq_file *buf, void *v)
{
	int RawRange = 0;
	VL6180x_RangeData_t RangeData;

	mutex_lock(&vl6180x_mutex);

	if (vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_OFF ||
		vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_DEINIT_CCI) {
		pr_err("%s:%d Device without turn on: (%d) \n", __func__, __LINE__, vl6180x_t->device_state);
		seq_printf(buf, "%d\n", 0);
		mutex_unlock(&vl6180x_mutex);
		return -EBUSY;
	}

	if(laser_focus_enforce_ctrl != 0){
		seq_printf(buf, "%d\n", laser_focus_enforce_ctrl);
		mutex_unlock(&vl6180x_mutex);
		return 0;
	}
	
	RawRange = ATD_VL6180x_device_read_range(&RangeData);
	//RawRange = RawRange*3;
	pr_err("get RawRange=%d\n",RawRange);
	//DBG_LOG("%s Test Data (%d)  Device (%d)\n", __func__, RawRange , vl6180x_t->device_state);

	if (RawRange < 0) {
		pr_err("%s: read_range(%d) failed\n", __func__, RawRange);
		RawRange = 0;
	}

	DMax = RangeData.DMax;
	errorStatus = RangeData.errorStatus;
	pr_err("%s: read_range(%d)\n", __func__, RawRange);
	seq_printf(buf, "%d\n", RawRange);
#if 0
	if(camera_on_flag==false){
		   debug_read_range(&RangeData);
	}
#endif
	
	mutex_unlock(&vl6180x_mutex);

	return 0;
}
 
static int ATD_VL6180x_device_get_range_open(struct inode *inode, struct  file *file)
{
	return single_open(file, ATD_VL6180x_device_get_range_read, NULL);
}

static const struct file_operations ATD_laser_focus_device_get_range_fos = {
	.owner = THIS_MODULE,
	.open = ATD_VL6180x_device_get_range_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ATD_VL6180x_device_get_range_more_info_read(struct seq_file *buf, void *v)
{
       int RawRange = 0;
       VL6180x_RangeData_t RangeData;

       mutex_lock(&vl6180x_mutex);

       if (vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_OFF ||
               vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_DEINIT_CCI) {
               pr_err("%s:%d Device without turn on: (%d) \n", __func__, __LINE__, vl6180x_t->device_state);
               seq_printf(buf, "%d\n", 0);
               mutex_unlock(&vl6180x_mutex);
               return -EBUSY;
       }

       if(laser_focus_enforce_ctrl != 0){
               seq_printf(buf, "%d\n", laser_focus_enforce_ctrl);
               mutex_unlock(&vl6180x_mutex);
               return 0;
       }

       RawRange = ATD_VL6180x_device_read_range(&RangeData);
       //RawRange = RawRange*3;

       //DBG_LOG("%s Test Data (%d)  Device (%d)\n", __func__, RawRange , vl6180x_t->device_state);

       if (RawRange < 0) {
             pr_err("%s: read_range(%d) failed\n", __func__, RawRange);
               RawRange = 0;
       }

       DMax = RangeData.DMax;
       errorStatus = RangeData.errorStatus;

       seq_printf(buf, "%d#%d#%d\n", RawRange, RangeData.DMax, RangeData.errorStatus);
#if 0
       if(camera_on_flag==false){
               debug_read_range(&RangeData);
       }
#endif

       mutex_unlock(&vl6180x_mutex);

       return 0;
}

static int ATD_VL6180x_device_get_range_more_info_open(struct inode *inode, struct  file *file)
{
       return single_open(file, ATD_VL6180x_device_get_range_more_info_read, NULL);
}

static const struct file_operations ATD_laser_focus_device_get_range_more_info_fos = {
       .owner = THIS_MODULE,
       .open = ATD_VL6180x_device_get_range_more_info_open,
       .read = seq_read,
       .llseek = seq_lseek,
       .release = single_release,
};

static int ATD_VL6180x_device_clibration_offset(void)
{
	int i = 0, RawRange = 0, sum = 0, status, errorCount = 0;
	uint16_t distance/*, Data_value*/;
	int16_t offset;
	VL6180x_RangeData_t RangeData;

	mutex_lock(&vl6180x_mutex);

	if (vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_OFF ||
		vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_DEINIT_CCI) {
		pr_err("%s:%d Device without turn on: (%d) \n", __func__, __LINE__, vl6180x_t->device_state);
		mutex_unlock(&vl6180x_mutex);
		return -EBUSY;
	}
	
	/* Clean system offset */
	API_DBG("%s: VL6180x_SetOffsetCalibrationData Start\n", __func__);
	VL6180x_SetOffsetCalibrationData(0,0);
	API_DBG("%s: VL6180x_SetOffsetCalibrationData Success\n", __func__);

	API_DBG("%s: VL6180x_SetXTalkCompensationRate Start\n", __func__);
	status = VL6180x_SetXTalkCompensationRate(0,0);
	if (status < 0) {
		pr_err("%s: VL6180x_SetXTalkCompensationRate failed\n", __func__);
		mutex_unlock(&vl6180x_mutex);
		return status;
	}
	
	API_DBG("%s: VL6180x_SetXTalkCompensationRate Success\n", __func__);
#if 0
	for(i=0; i<6; i++){
		   ATD_VL6180x_device_read_range(&RangeData);
	}
#endif

	for(i=0; i<STMVL6180_RUNTIMES_OFFSET_CAL; i++)
	{
		RawRange = (int)ATD_VL6180x_device_read_range(&RangeData);
		//msleep(50);
	   if(RangeData.errorStatus != 0){
			   errorCount++;
			   if(i>0){
					   i--;
			   }

			   if(errorCount > VL6180_API_ERROR_COUNT_MAX){
					   printk("Too many out of range(765) detected in xtalk calibration (%d)\n", errorCount);
					   goto error_offset;
			   }
			   continue;
	   }
		sum += RawRange;
	}
	distance = (uint16_t)(sum / STMVL6180_RUNTIMES_OFFSET_CAL);
	//DBG_LOG("The measure distanec is %d mm\n", distance);
#if 0
	if((VL6180_OFFSET_CAL_RANGE - distance)<0){
		offset = (VL6180_OFFSET_CAL_RANGE - distance)/3;
		offset = 256+offset;
	}else{
		offset = (VL6180_OFFSET_CAL_RANGE - distance)/3;
	}
#endif
	if((VL6180_OFFSET_CAL_RANGE - distance)<0){
		   offset = (VL6180_OFFSET_CAL_RANGE - distance);
		   offset = 256+offset;
	}else{
		   offset = (VL6180_OFFSET_CAL_RANGE - distance);
	}

#if 0
	if((distance>=(100+3)||distance<=(100-3)))
	{	
		status = ASUS_VL6180x_WrByte(SYSRANGE_PART_TO_PART_RANGE_OFFSET, offset);
		if (status < 0) {
			pr_err("%s: write register(0x%x) failed\n", __func__, SYSRANGE_PART_TO_PART_RANGE_OFFSET);
			mutex_unlock(&vl6180x_mutex);
			return status;
		}

		/* Write calibration file */
		vl6180x_t->laser_focus_offset_value = offset;
		if (Laser_Forcus_sysfs_write_offset(offset) == false){
			mutex_unlock(&vl6180x_mutex);
			return -ENOENT;
		}
	}
#endif

	API_DBG("%s: VL6180x_SetOffsetCalibrationData Start\n", __func__);
	VL6180x_SetOffsetCalibrationData(0, offset);
	API_DBG("%s: VL6180x_SetOffsetCalibrationData Success\n", __func__);

#if 0
	status = ASUS_VL6180x_RdByte(SYSRANGE_PART_TO_PART_RANGE_OFFSET,&Data_value);
	if (status < 0) {
		   pr_err("%s: read register(0x%x) failed\n", __func__,SYSRANGE_PART_TO_PART_RANGE_OFFSET);
		   mutex_unlock(&vl6180x_mutex);
		   return status;
	}
	printk("[S_DEBUG] read register(0x%x):0x%x\n",SYSRANGE_PART_TO_PART_RANGE_OFFSET,Data_value);
#endif

	/* Write calibration file */
	vl6180x_t->laser_focus_offset_value = offset;
	if (Laser_Forcus_sysfs_write_offset(offset) == false){
		   mutex_unlock(&vl6180x_mutex);
		   return -ENOENT;
	}

	DBG_LOG("The measure distance is %d mm; The offset value is %d\n", distance, offset);

	mutex_unlock(&vl6180x_mutex);

	return 0;

error_offset:

	API_DBG("%s: VL6180x_SetOffsetCalibrationData Start\n", __func__);
	VL6180x_SetOffsetCalibrationData(0, 0);
	API_DBG("%s: VL6180x_SetOffsetCalibrationData Success\n", __func__);

	/* Write calibration file */
	vl6180x_t->laser_focus_offset_value = 0;
	if (Laser_Forcus_sysfs_write_offset(0) == false){
		   mutex_unlock(&vl6180x_mutex);
		   return -ENOENT;
	}

	mutex_unlock(&vl6180x_mutex);
	return -ENOENT;
}

static int ATD_VL6180x_device_clibration_crosstalkoffset(void)
{
	int i = 0, RawRange = 0, status;
	int xtalk_sum  = 0, xrtn_sum = 0;
	int32_t XtalkCompRate;
	uint16_t rtnRate = 0;
	uint16_t Data_value = 0;

	VL6180x_RangeData_t RangeData;
    int errorCount = 0;

	mutex_lock(&vl6180x_mutex);

	if (vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_OFF ||
		vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_DEINIT_CCI) {
		pr_err("%s:%d Device without turn on: (%d) \n", __func__, __LINE__, vl6180x_t->device_state);
		mutex_unlock(&vl6180x_mutex);
		return -EBUSY;
	}

	/* Clean crosstalk offset */
	API_DBG("%s: VL6180x_SetXTalkCompensationRate Start\n", __func__);
	VL6180x_SetXTalkCompensationRate(0,0);
	API_DBG("%s: VL6180x_SetXTalkCompensationRate Success\n", __func__);


	for(i = 0; i < STMVL6180_RUNTIMES_OFFSET_CAL; i++)
	{	
		RawRange = ATD_VL6180x_device_read_range(&RangeData);

		if(RangeData.errorStatus != 0){
			   errorCount++;
			   if(i>0){
					   i--;
			   }

			   if(errorCount > VL6180_API_ERROR_COUNT_MAX){
					   printk("Too many out of range(765) detected in xtalk calibration (%d)\n", errorCount);
					   goto error_xtalk;
			   }
			   continue;
		}

		rtnRate = RangeData.signalRate_mcps;

		xtalk_sum += RawRange;
		xrtn_sum += (int)rtnRate;

	}
	
	//XtalkCompRate = (uint16_t)(xrtn_sum/STMVL6180_RUNTIMES_OFFSET_CAL) * (1000-((xtalk_sum/STMVL6180_RUNTIMES_OFFSET_CAL)*1000/VL6180_CROSSTALK_CAL_RANGE));
	XtalkCompRate = 1000-((xtalk_sum/STMVL6180_RUNTIMES_OFFSET_CAL)*1000/VL6180_CROSSTALK_CAL_RANGE);

	if(XtalkCompRate < 0){
		XtalkCompRate = 0;
	}
	else{
		XtalkCompRate = (xrtn_sum/STMVL6180_RUNTIMES_OFFSET_CAL) * XtalkCompRate;
		XtalkCompRate = XtalkCompRate/1000;
	}

	status = ASUS_VL6180x_RdByte(0x21,&Data_value);
	if (status < 0) {
		pr_err("%s: read register(0x21) failed\n", __func__);
		mutex_unlock(&vl6180x_mutex);
		return status;
	}

	if(Data_value==20)
	{
		status = ASUS_VL6180x_WrByte(0x21,0x6);
		if (status < 0) {
			pr_err("%s: write register(0x21) failed\n", __func__);
			mutex_unlock(&vl6180x_mutex);
			return status;
		}
	}
	
	//printk("Crosstalk compensation rate is %d\n", XtalkCompRate);

	API_DBG("%s: VL6180x_SetXTalkCompensationRate Start\n", __func__);
	status = VL6180x_SetXTalkCompensationRate(0, (uint16_t)XtalkCompRate);
	if (status < 0) {
		pr_err("%s: VL6180x_SetXTalkCompensationRate failed\n", __func__);
		mutex_unlock(&vl6180x_mutex);
		return status;
	}
	API_DBG("%s: VL6180x_SetXTalkCompensationRate Success\n", __func__);

	/* Write calibration file */
	vl6180x_t->laser_focus_cross_talk_offset_value = (uint16_t)XtalkCompRate;
	if (Laser_Forcus_sysfs_write_cross_talk_offset((uint16_t)XtalkCompRate) == false){
		mutex_unlock(&vl6180x_mutex);
		return -ENOENT;
	}

	DBG_LOG("Crosstalk compensation rate is %d\n", (uint16_t)XtalkCompRate);

	mutex_unlock(&vl6180x_mutex);

	return 0;

error_xtalk:
	API_DBG("%s: VL6180x_SetXTalkCompensationRate Start\n", __func__);
	status = VL6180x_SetXTalkCompensationRate(0,0);
	if (status < 0) {
		   pr_err("%s: VL6180x_SetXTalkCompensationRate failed\n", __func__);
		   mutex_unlock(&vl6180x_mutex);
		   return status;
	}
	API_DBG("%s: VL6180x_SetXTalkCompensationRate Success\n", __func__);

	vl6180x_t->laser_focus_cross_talk_offset_value = 0;
	if (Laser_Forcus_sysfs_write_cross_talk_offset(0) == false){
		   mutex_unlock(&vl6180x_mutex);
		   return -ENOENT;
	}

	mutex_unlock(&vl6180x_mutex);
	return -ENOENT;
}
 
static ssize_t ATD_VL6180x_device_calibration_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
	int val, ret = 0;
	char messages[8];
	memset(messages, 0, 8);
	if (len > 8) {
		len = 8;
	}
	if (copy_from_user(messages, buff, len)) {
		printk("%s commond fail !!\n", __func__);
		return -EFAULT;
	}

	val = (int)simple_strtol(messages, NULL, 10);

	printk("%s commond : %d\n", __func__, val);
	switch (val) {
	case MSM_LASER_FOCUS_APPLY_OFFSET_CALIBRATION:
		ret = ATD_VL6180x_device_clibration_offset();
		if (ret < 0)
			return ret;
		break;
	case MSM_LASER_FOCUS_APPLY_CROSSTALK_CALIBRATION:
		ret = ATD_VL6180x_device_clibration_crosstalkoffset();
		if (ret < 0)
			return ret;
		break;
	default:
		printk("%s commond fail(%d) !!\n", __func__, val);
		return -EINVAL;
		break;
	}
	return len;
}

static const struct file_operations ATD_laser_focus_device_calibration_fops = {
	.owner = THIS_MODULE,
	.open = ATD_VL6180x_device_get_range_open,
	.write = ATD_VL6180x_device_calibration_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ATD_VL6180x_I2C_status_check(struct msm_laser_focus_ctrl_t *s_ctrl){
	int32_t rc ;
	rc = VL6180x_power_up(vl6180x_t);
	if (rc < 0) {
		//kfree(vl6180x_t);
		pr_err("%s VL6180x_power_up failed %d\n", __func__, __LINE__);
		return 0;
	}

	VL6180x_init(vl6180x_t);
	if (rc < 0) {
		//kfree(vl6180x_t);
		pr_err("%s VL6180x_init failed %d\n", __func__, __LINE__);
		return 0;
	}
	
	rc = VL6180x_match_id(vl6180x_t);
	if (rc < 0) {
		//kfree(vl6180x_t);
		pr_err("%s VL6180x_match_id failed %d\n", __func__, __LINE__);

		rc = VL6180x_deinit(vl6180x_t);
		if (rc < 0) {
			//kfree(vl6180x_t);
			pr_err("%s VL6180x_deinit failed %d\n", __func__, __LINE__);
		}
		
		rc = VL6180x_power_down(vl6180x_t);
		if (rc < 0) {
			//kfree(vl6180x_t);
			pr_err("%s VL6180x_power_down failed %d\n", __func__, __LINE__);
		}
		
		return 0;
	}
	
	rc = VL6180x_deinit(vl6180x_t);
	if (rc < 0) {
		//kfree(vl6180x_t);
		pr_err("%s VL6180x_deinit failed %d\n", __func__, __LINE__);
		return 0;
	}
	
	rc = VL6180x_power_down(vl6180x_t);
	if (rc < 0) {
		//kfree(vl6180x_t);
		pr_err("%s VL6180x_power_down failed %d\n", __func__, __LINE__);
		return 0;
	}

	vl6180x_check_status = 1;

	return 1;
}

static int ATD_VL6180x_I2C_status_check_proc_read(struct seq_file *buf, void *v)
{
	ATD_status = ATD_VL6180x_I2C_status_check(vl6180x_t);
	
	seq_printf(buf, "%d\n", ATD_status);
	return 0;
}

static int ATD_VL6180x_I2C_status_check_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, ATD_VL6180x_I2C_status_check_proc_read, NULL);
}

static const struct file_operations ATD_I2C_status_check_fops = {
	.owner = THIS_MODULE,
	.open = ATD_VL6180x_I2C_status_check_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//ASUS_BSP Sam add i2c monitor +++
static int ASUS_VL6180x_I2C_status_check_proc_read(struct seq_file *buf, void *v)
{
	int i2c_status  = 0;

	pr_err("[S-DEBUG]vl6180x_check_status+++:%d\n", vl6180x_check_status);
	if(ATD_status == 1)
		i2c_status = ATD_VL6180x_I2C_status_check(vl6180x_t);

	pr_err("[S-DEBUG]vl6180x_check_status---:%d\n", vl6180x_check_status);

	seq_printf(buf, "%d\n", i2c_status);
	return 0;
}

static int ASUS_VL6180x_I2C_status_check_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, ASUS_VL6180x_I2C_status_check_proc_read, NULL);
}

static const struct file_operations ASUS_I2C_status_check_fops = {
	.owner = THIS_MODULE,
	.open = ASUS_VL6180x_I2C_status_check_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
//ASUS_BSP Sam add i2c monitor ---

static int VL6180x_I2C_status_check_via_prob(struct msm_laser_focus_ctrl_t *s_ctrl){
	return vl6180x_check_status;
}

static int VL6180x_I2C_status_check_proc_read(struct seq_file *buf, void *v)
{
	ATD_status = VL6180x_I2C_status_check_via_prob(vl6180x_t);
	
	seq_printf(buf, "%d\n", ATD_status);
	return 0;
}

static int VL6180x_I2C_status_check_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, VL6180x_I2C_status_check_proc_read, NULL);
}

static const struct file_operations I2C_status_check_fops = {
	.owner = THIS_MODULE,
	.open = VL6180x_I2C_status_check_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dump_VL6180x_register_read(struct seq_file *buf, void *v)
{
	int status, i = 0;
	uint16_t register_value = 0;

	if (vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_OFF ||
		   vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_DEINIT_CCI) {
		   pr_err("%s:%d Device without turn on: (%d) \n", __func__, __LINE__, vl6180x_t->device_state);
		   //mutex_unlock(&vl6180x_mutex);
		   return -EBUSY;
	}

	for (i = 0; i <0x100; i++)	{
		register_value = 0;
		status = ASUS_VL6180x_RdWord(i, &register_value);
		printk("%s: read register(0x%x): 0x%x for word\n",__func__, i, register_value);
		if (status < 0) {
			pr_err("%s: read register(0x%x) failed\n", __func__, i);
			return status;
		}
	}
	seq_printf(buf, "%d\n", 0);
	return 0;
}

static int dump_VL6180x_laser_focus_register_open(struct inode *inode, struct  file *file)
{
	return single_open(file, dump_VL6180x_register_read, NULL);
}

static const struct file_operations dump_laser_focus_register_fops = {
	.owner = THIS_MODULE,
	.open = dump_VL6180x_laser_focus_register_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dump_VL6180x_debug_register_read(struct seq_file *buf, void *v)
{
       uint16_t reg_data = 0;

       if (vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_OFF ||
               vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_DEINIT_CCI) {
               pr_err("%s:%d Device without turn on: (%d) \n", __func__, __LINE__, vl6180x_t->device_state);
               //mutex_unlock(&vl6180x_mutex);
               return -EBUSY;
       }

       ASUS_VL6180x_RdWord(SYSRANGE_CROSSTALK_COMPENSATION_RATE, &reg_data);
       seq_printf(buf, "register(0x%x) : 0x%x\n", SYSRANGE_CROSSTALK_COMPENSATION_RATE, reg_data);
       //printk("[S_DEBUG] register(0x%x) : 0x%x\n", SYSRANGE_CROSSTALK_COMPENSATION_RATE, reg_data);
       ASUS_VL6180x_RdByte(SYSRANGE_PART_TO_PART_RANGE_OFFSET, &reg_data);
       seq_printf(buf, "register(0x%x) : 0x%x\n", SYSRANGE_PART_TO_PART_RANGE_OFFSET, reg_data);
       //printk("[S_DEBUG] register(0x%x) : 0x%x\n", SYSRANGE_PART_TO_PART_RANGE_OFFSET, reg_data);
       ASUS_VL6180x_RdByte(RESULT_RANGE_STATUS, &reg_data);
       seq_printf(buf, "register(0x%x) : 0x%x\n", RESULT_RANGE_STATUS, reg_data);
       //printk("[S_DEBUG] register(0x%x) : 0x%x\n", RESULT_RANGE_STATUS, reg_data);
       ASUS_VL6180x_RdByte(RESULT_RANGE_VAL, &reg_data);
       seq_printf(buf, "register(0x%x) : 0x%x\n", RESULT_RANGE_VAL, reg_data);
       //printk("[S_DEBUG] register(0x%x) : 0x%x\n", RESULT_RANGE_VAL, reg_data);
       ASUS_VL6180x_RdByte(RESULT_RANGE_RAW, &reg_data);
       seq_printf(buf, "register(0x%x) : 0x%x\n", RESULT_RANGE_RAW, reg_data);
       //printk("[S_DEBUG] register(0x%x) : 0x%x\n", RESULT_RANGE_RAW, reg_data);
       ASUS_VL6180x_RdWord(RESULT_RANGE_SIGNAL_RATE, &reg_data);
       seq_printf(buf, "register(0x%x) : 0x%x\n", RESULT_RANGE_SIGNAL_RATE, reg_data);
       //printk("[S_DEBUG] register(0x%x) : 0x%x\n", RESULT_RANGE_SIGNAL_RATE, reg_data);
       seq_printf(buf, "DMax : %d\n", DMax);
       //printk("[S_DEBUG] DMax : %d0\n", pRangeData->DMax);
       seq_printf(buf, "errorCode : %d\n", errorStatus);
       //printk("[S_DEBUG] errorCode : %d\n", pRangeData->errorStatus);
       return 0;
}

static int dump_VL6180x_laser_focus_debug_register_open(struct inode *inode, struct  file *file)
{
       return single_open(file, dump_VL6180x_debug_register_read, NULL);
}

static const struct file_operations dump_laser_focus_debug_register_fops = {
       .owner = THIS_MODULE,
       .open = dump_VL6180x_laser_focus_debug_register_open,
       .read = seq_read,
       .llseek = seq_lseek,
       .release = single_release,
};

static int VL6180x_laser_focus_enforce_read(struct seq_file *buf, void *v)
{
	return 0;
}

static int VL6180x_laser_focus_enforce_open(struct inode *inode, struct  file *file)
{
	return single_open(file, VL6180x_laser_focus_enforce_read, NULL);
}

static ssize_t VL6180x_laser_focus_enforce_write(struct file *filp, const char __user *buff, size_t len, loff_t *data)
{
	char messages[8];
	memset(messages, 0, 8);
	if (vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_OFF ||
		vl6180x_t->device_state == MSM_LASER_FOCUS_DEVICE_DEINIT_CCI) {
		pr_err("%s:%d Device without turn on: (%d) \n", __func__, __LINE__, vl6180x_t->device_state);
		return -EBUSY;
	}

	if (len > 8) {
		len = 8;
	}

	if (copy_from_user(messages, buff, len)) {
		printk("%s commond fail !!\n", __func__);
		return -EFAULT;
	}
	
	laser_focus_enforce_ctrl = (int)simple_strtol(messages, NULL, 10);
	
	return len;
}

static const struct file_operations laser_focus_enforce_fops = {
	.owner = THIS_MODULE,
	.open = VL6180x_laser_focus_enforce_open,
	.write = VL6180x_laser_focus_enforce_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//ASUS_BSP Sam add i2c monitor +++
static void VL6180x_create_proc_file_only_for_i2c(void)
{
	i2c_status_file = proc_create(I2C_STATUS_FILE, 0664, NULL, &ASUS_I2C_status_check_fops);

	if (i2c_status_file) {
		printk("%s i2c_status_file sucessed!\n", __func__);
	} else {
		printk("%s i2c_status_file failed!\n", __func__);
	}
}
//ASUS_BSP Sam add i2c monitor ---

static void VL6180x_create_proc_file(void)
{
	status_proc_file = proc_create(STATUS_PROC_FILE, 0664, NULL, &ATD_I2C_status_check_fops);
	if (status_proc_file) {
		printk("%s status_proc_file sucessed!\n", __func__);
	} else {
		printk("%s status_proc_file failed!\n", __func__);
	}
	
	device_trun_on_file = proc_create(DEVICE_TURN_ON_FILE, 0660, NULL, &ATD_laser_focus_device_enable_fops);
	if (device_trun_on_file) {
		printk("%s device_trun_on_file sucessed!\n", __func__);
	} else {
		printk("%s device_trun_on_file failed!\n", __func__);
	}

	device_get_value_file = proc_create(DEVICE_GET_VALUE, 0664, NULL, &ATD_laser_focus_device_get_range_fos);
	if (device_get_value_file) {
		printk("%s device_get_value_file sucessed!\n", __func__);
	} else {
		printk("%s device_get_value_file failed!\n", __func__);
	}

   device_get_value_file = proc_create(DEVICE_GET_VALUE_MORE_INFO, 0664, NULL, &ATD_laser_focus_device_get_range_more_info_fos);
   if (device_get_value_file) {
	   printk("%s device_get_value_more_info_file sucessed!\n", __func__);
   } else {
	   printk("%s device_get_value_more_info_file failed!\n", __func__);
   }

	device_set_calibration_file = proc_create(DEVICE_SET_CALIBRATION, 0660, NULL, &ATD_laser_focus_device_calibration_fops);
	if (device_set_calibration_file) {
		printk("%s device_set_calibration_file sucessed!\n", __func__);
	} else {
		printk("%s device_set_calibration_file failed!\n", __func__);
	}

	dump_laser_focus_register_file = proc_create(DEVICE_DUMP_REGISTER_VALUE, 0664, NULL, &dump_laser_focus_register_fops);
	if (dump_laser_focus_register_file) {
		printk("%s dump_laser_focus_register_file sucessed!\n", __func__);
	} else {
		printk("%s dump_laser_focus_register_file failed!\n", __func__);
	}

	dump_laser_focus_debug_file = proc_create(DEVICE_DUMP_DEBUG_REGISTER_VALUE, 0664, NULL, &dump_laser_focus_debug_register_fops);
	if (dump_laser_focus_debug_file) {
		printk("%s dump_laser_focus_debug_register_file sucessed!\n", __func__);
	} else {
		printk("%s dump_laser_focus_debug_register_file failed!\n", __func__);
	}

	status_proc_file = proc_create(STATUS_PROC_FILE_FOR_CAMERA, 0664, NULL, &I2C_status_check_fops);
	if (status_proc_file) {
		printk("%s status_proc_file sucessed!\n", __func__);
	} else {
		printk("%s status_proc_file failed!\n", __func__);
	}

	enforce_proc_file = proc_create(LASER_FOCUS_ENFORCE, 0660, NULL, &laser_focus_enforce_fops);
	if (status_proc_file) {
		printk("%s enforce_proc_file sucessed!\n", __func__);
	} else {
		printk("%s enforce_proc_file failed!\n", __func__);
	}
}

int VL6180x_match_id(struct msm_laser_focus_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint16_t chipid = 0;
	struct msm_camera_i2c_client *sensor_i2c_client;
	struct msm_camera_slave_info *slave_info;
	const char *sensor_name;

	if (!s_ctrl) {
		pr_err("%s:%d failed: %p\n",
			__func__, __LINE__, s_ctrl);
		return -EINVAL;
	}
	sensor_i2c_client = s_ctrl->i2c_client;
	slave_info = s_ctrl->sensordata->slave_info;
	sensor_name = s_ctrl->sensordata->sensor_name;

	if (!sensor_i2c_client || !slave_info || !sensor_name) {
		pr_err("%s:%d failed: %p %p %p\n",
			__func__, __LINE__, sensor_i2c_client, slave_info,
			sensor_name);
		return -EINVAL;
	}

	pr_err("[S-DEBUG]%s: adderss: 0x%x reg 0x%x:\n", __func__, slave_info->sensor_slave_addr,
		slave_info->sensor_id_reg_addr);

	rc = sensor_i2c_client->i2c_func_tbl->i2c_read(
		sensor_i2c_client, slave_info->sensor_id_reg_addr,
		&chipid, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__, sensor_name);
		return rc;
	}

	pr_err("[S-DEBUG]%s: read id: 0x%x expected id 0x%x:\n", __func__, chipid,
		slave_info->sensor_id);
	if (chipid != slave_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id does not match\n");
		return -ENODEV;
	}
	return rc;
}

static int32_t VL6180x_vreg_control(struct msm_laser_focus_ctrl_t *a_ctrl,
							int config)
{
	int rc = 0, i, cnt;
	struct msm_laser_focus_vreg *vreg_cfg;
	struct msm_camera_power_ctrl_t *power_info;

	power_info = &vl6180x_t->sensordata->power_info;

	if (!power_info)
	{
		pr_err("%s:%d failed: %p\n", __func__, __LINE__, power_info);
		return -EINVAL;
	}
	vreg_cfg = &vl6180x_t->vreg_cfg;
	cnt = vreg_cfg->num_vreg;
	if (!cnt)
		return 0;

	if (cnt >= CAM_VREG_MAX) {
		pr_err("%s failed %d cnt %d\n", __func__, __LINE__, cnt);
		return -EINVAL;
	}

	pr_err("%s %d cnt %d, name:%s, %p\n", __func__, __LINE__, cnt, (&vreg_cfg->cam_vreg[i])->reg_name, power_info->dev);

	for (i = 0; i < cnt; i++) {
/*		rc =	msm_camera_config_single_vreg(ctrl->dev,
				&ctrl->cam_vreg[power_setting->seq_val],
				(struct regulator **)&power_setting->data[0],
				1);
		rc = msm_camera_config_single_vreg(&(a_ctrl->pdev->dev),
			&vreg_cfg->cam_vreg[i],
			(struct regulator **)&vreg_cfg->data[i],
			config);*/
		rc =	msm_camera_config_single_vreg(power_info->dev,
				&vreg_cfg->cam_vreg[i],
				(struct regulator **)&vreg_cfg->data[i],
				config);
	}
	return rc;
}
#if 0
static int VL6180x_GPIO_High(struct msm_laser_focus_ctrl_t *a_ctrl){
	int rc = 0;
	
	struct msm_camera_sensor_board_info *sensordata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	
	CDBG("Enter\n");

	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	
	sensordata = a_ctrl->sensordata;
	power_info = &sensordata->power_info;

	if(power_info->gpio_conf->cam_gpiomux_conf_tbl != NULL){
		pr_err("%s:%d mux install\n", __func__, __LINE__);
	}

	rc = msm_camera_request_gpio_table(
		power_info->gpio_conf->cam_gpio_req_tbl,
		power_info->gpio_conf->cam_gpio_req_tbl_size, 1);
	if(rc < 0){
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_VDIG],
		GPIO_OUT_HIGH
	);
	
	CDBG("Exit\n");
	return rc;
}

static int VL6180x_GPIO_Low(struct msm_laser_focus_ctrl_t *a_ctrl){
	int rc = 0;
	
	struct msm_camera_sensor_board_info *sensordata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	
	CDBG("Enter\n");

	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	
	sensordata = a_ctrl->sensordata;
	power_info = &sensordata->power_info;

	if(power_info->gpio_conf->cam_gpiomux_conf_tbl != NULL){
		pr_err("%s:%d mux install\n", __func__, __LINE__);
	}

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->gpio_num[SENSOR_GPIO_VDIG],
		GPIO_OUT_LOW
	);

	rc = msm_camera_request_gpio_table(
		power_info->gpio_conf->cam_gpio_req_tbl,
		power_info->gpio_conf->cam_gpio_req_tbl_size, 0);
	if(rc < 0){
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}
	
	CDBG("Exit\n");
	return rc;
}
#endif
static int32_t VL6180x_power_down(struct msm_laser_focus_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	CDBG("Enter,state=%d\n",a_ctrl->laser_focus_state);

	if (a_ctrl->laser_focus_state != LASER_FOCUS_POWER_DOWN) {

		CDBG("call VL6180x_vreg_control\n");
		rc = VL6180x_vreg_control(a_ctrl, 0);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}

		kfree(a_ctrl->i2c_reg_tbl);
		a_ctrl->i2c_reg_tbl = NULL;
		a_ctrl->i2c_tbl_index = 0;
		a_ctrl->laser_focus_state = LASER_FOCUS_POWER_DOWN;
	}

	//VL6180x_GPIO_Low(a_ctrl);
	CDBG("Exit,state=%d\n",a_ctrl->laser_focus_state);

	return rc;
}

static int VL6180x_init(struct msm_laser_focus_ctrl_t *a_ctrl)
{
	int rc = 0;
	CDBG("Enter\n");
	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	
	// CCI Init 
	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = a_ctrl->i2c_client->i2c_func_tbl->i2c_util(
			a_ctrl->i2c_client, MSM_CCI_INIT);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	CDBG("Exit\n");
	return rc;
}

static int VL6180x_deinit(struct msm_laser_focus_ctrl_t *a_ctrl) 
{
	int rc = 0;
	CDBG("Enter\n");
	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = a_ctrl->i2c_client->i2c_func_tbl->i2c_util(
			a_ctrl->i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_deinit failed\n");
		
	}

	CDBG("Exit\n");
	return rc;
}

static int32_t VL6180x_get_dt_data(struct device_node *of_node,
		struct msm_laser_focus_ctrl_t *fctrl)
{
	int i = 0;
	int32_t rc = 0;
	struct msm_camera_sensor_board_info *sensordata = NULL;
	uint32_t id_info[3];
	struct msm_laser_focus_vreg *vreg_cfg = NULL;
	
	struct msm_camera_gpio_conf *gconf = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	uint16_t *gpio_array = NULL;
	uint16_t gpio_array_size = 0;

	CDBG("called\n");

	if (!of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	fctrl->sensordata = kzalloc(sizeof(
		struct msm_camera_sensor_board_info),
		GFP_KERNEL);
	if (!fctrl->sensordata) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	sensordata = fctrl->sensordata;

	rc = of_property_read_u32(of_node, "cell-index", &fctrl->subdev_id);
	if (rc < 0) {
		pr_err("failed\n");
		return -EINVAL;
	}

	pr_err("subdev id %d\n", fctrl->subdev_id);

	rc = of_property_read_string(of_node, "label",
		&sensordata->sensor_name);
	DBG_LOG("%s label %s, rc %d\n", __func__,
		sensordata->sensor_name, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR;
	}
#if 0
	rc = of_property_read_u32(of_node, "qcom,cci-master",
		&fctrl->cci_master);
	DBG_LOG("%s qcom,cci-master %d, rc %d\n", __func__, fctrl->cci_master,
		rc);
	if (rc < 0) {
		/* Set default master 0 */
		fctrl->cci_master = MASTER_0;
		rc = 0;
	}
#endif
	if (of_find_property(of_node,
			"qcom,cam-vreg-name", NULL)) {
		vreg_cfg = &fctrl->vreg_cfg;
		rc = msm_camera_get_dt_vreg_data(of_node,
			&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
		if (rc < 0) {
			kfree(fctrl);
			pr_err("failed rc %d\n", rc);
			return rc;
		}
	}

	sensordata->slave_info =
		kzalloc(sizeof(struct msm_camera_slave_info),
			GFP_KERNEL);
	if (!sensordata->slave_info) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR;
	}

	rc = of_property_read_u32_array(of_node, "qcom,slave-id",
		id_info, 3);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR;
	}
	fctrl->sensordata->slave_info->sensor_slave_addr = id_info[0];
	fctrl->sensordata->slave_info->sensor_id_reg_addr = id_info[1];
	fctrl->sensordata->slave_info->sensor_id = id_info[2];

	CDBG("%s:%d slave addr 0x%x sensor reg 0x%x id 0x%x\n",
		__func__, __LINE__,
		fctrl->sensordata->slave_info->sensor_slave_addr,
		fctrl->sensordata->slave_info->sensor_id_reg_addr,
		fctrl->sensordata->slave_info->sensor_id);

	/* Handle GPIO (CAM_1V2_EN) */
	power_info = &sensordata->power_info;
	
	power_info->gpio_conf = kzalloc(sizeof(struct msm_camera_gpio_conf), GFP_KERNEL);
	if(!power_info->gpio_conf){
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		return rc;
	}

	gconf = power_info->gpio_conf;
	
	gpio_array_size = of_gpio_count(of_node);
	CDBG("%s gpio count %d\n", __func__, gpio_array_size);

	if(gpio_array_size){
		gpio_array = kzalloc(sizeof(uint16_t) * gpio_array_size, GFP_KERNEL);
		if(!gpio_array){
			pr_err("%s failed %d\n", __func__, __LINE__);
			kfree(gconf);
			rc = -ENOMEM;
			goto ERROR;
		}
		
		for(i=0; i < gpio_array_size; i++){
			gpio_array[i] = of_get_gpio(of_node, i);
			CDBG("%s gpio_array[%d] = %d\n", __func__, i, gpio_array[i]);
		}

		rc = msm_camera_get_dt_gpio_req_tbl(of_node, gconf, gpio_array, gpio_array_size);
		if(rc < 0){
			pr_err("%s failed %d\n", __func__, __LINE__);
			kfree(gconf);
			goto ERROR;
		}

		rc = msm_camera_get_dt_gpio_set_tbl(of_node, gconf, gpio_array, gpio_array_size);
		if(rc < 0){
			pr_err("%s failed %d\n", __func__, __LINE__);
			kfree(gconf->cam_gpio_req_tbl);
			goto ERROR;
		}

		rc = msm_camera_init_gpio_pin_tbl(of_node, gconf, gpio_array, gpio_array_size);
		if(rc < 0){
			pr_err("%s failed %d\n", __func__, __LINE__);
			kfree(gconf->cam_gpio_set_tbl);
			goto ERROR;
		}
	}
	kfree(gpio_array);

	return rc;

ERROR:
		kfree(fctrl->sensordata->slave_info);
	return rc;
}

#if 0
static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	//.i2c_write_seq = msm_camera_cci_i2c_write_seq,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll =  msm_camera_cci_i2c_poll,
};
#endif
static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	//.i2c_write_seq = msm_camera_qup_i2c_write_seq,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
	//.i2c_util = msm_sensor_qup_i2c_util,
	.i2c_poll = msm_camera_qup_i2c_poll,
};

static const struct v4l2_subdev_internal_ops msm_laser_focus_internal_ops; 

static int32_t VL6180x_power_up(struct msm_laser_focus_ctrl_t *a_ctrl)
{
	int rc = 0;
	CDBG("%s called\n", __func__);
	if (a_ctrl->laser_focus_state != LASER_FOCUS_POWER_UP) {
		rc = VL6180x_vreg_control(a_ctrl, 1);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}

		a_ctrl->laser_focus_state = LASER_FOCUS_POWER_UP;

		//VL6180x_GPIO_High(a_ctrl);
		msleep(1);
	}
	CDBG("Exit,state=%d\n",a_ctrl->laser_focus_state);
	return rc;
}

static struct v4l2_subdev_core_ops msm_laser_focus_subdev_core_ops = {
	.ioctl = NULL,
	//.s_power = msm_laser_focus_power,
};

static struct v4l2_subdev_ops msm_laser_focus_subdev_ops = {
	.core = &msm_laser_focus_subdev_core_ops,
};

static const struct i2c_device_id vl6180x_i2c_id[] = {
	{"qcom,vl6180", (kernel_ulong_t)NULL},
	{ }
};

static const struct of_device_id vl6180x_i2c_dt_match[] = {
	{.compatible = "qcom,vl6180", .data = NULL},
	{}
};

MODULE_DEVICE_TABLE(of, vl6180x_i2c_dt_match);

static int vl6180x_i2c_remove(struct i2c_client *client)
{
		return 0;
}

static struct msm_camera_i2c_client msm_laser_focus_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int32_t vl6180x_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	int retry = 0;
	int check_result = 0;
	//struct msm_laser_focus_ctrl_t *vl6180_ctrl_t = NULL;
	CDBG("Enter\n");

	if (client == NULL) {
		pr_err("vl6180x_i2c_probe: client is null\n");
		rc = -EINVAL;
		goto probe_failure;
	}

	vl6180x_t = kzalloc(sizeof(struct msm_laser_focus_ctrl_t),
		GFP_KERNEL);
	if (!vl6180x_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}
	//pr_err("client = 0x%p\n",  client);

	/*rc = of_property_read_u32(client->dev.of_node, "cell-index",
		&vl6180x_t->subdev_id);
	CDBG("cell-index %d, rc %d\n", vl6180x_t->subdev_id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		return rc;
	}*/

	rc = VL6180x_get_dt_data(client->dev.of_node, vl6180x_t);
	if (rc < 0) {
		pr_err("%s failed line %d rc = %d\n", __func__, __LINE__, rc);
		return rc;
	}

	vl6180x_t->i2c_driver = &vl6180x_i2c_driver;
	vl6180x_t->i2c_client = &msm_laser_focus_i2c_client;
	if (NULL == vl6180x_t->i2c_client) {
		pr_err("[S-DEBUG]%s i2c_client NULL\n", __func__);
		rc = -EFAULT;
		goto probe_failure;
	}
	vl6180x_t->i2c_client->client = client;
	vl6180x_t->sensordata->power_info.dev = &client->dev;

	pr_err("client dev :%p\n", &client->dev);

	/* Set device type as I2C */
	vl6180x_t->act_device_type = MSM_CAMERA_I2C_DEVICE;
	vl6180x_t->i2c_client->i2c_func_tbl = &msm_sensor_qup_func_tbl;
	vl6180x_t->act_v4l2_subdev_ops = &msm_laser_focus_subdev_ops;
	//vl6180x_t->mutex = &vl6180x_mutex;

	vl6180x_t->laser_focus_state = LASER_FOCUS_POWER_DOWN;
	rc = -EFAULT;

	/* Init data struct */
	vl6180x_t->laser_focus_cross_talk_offset_value = 0;
	vl6180x_t->laser_focus_offset_value = 0;
	vl6180x_t->laser_focus_state = LASER_FOCUS_POWER_DOWN;
	
	VL6180x_create_proc_file_only_for_i2c();//ASUS_BSP Sam add i2c monitor
	for(retry=0;retry<3;retry++)
	{
		check_result = ATD_VL6180x_I2C_status_check(vl6180x_t);
		if(check_result == 0)
		{
			pr_err("%s: Laser check status failed, retry, count %d\n",__func__,retry);
			msleep(20);
		}
		else
		{
			break;
		}
	}
	if(check_result == 0)
	{
		rc = -EFAULT;
		goto Error_Early;
	}

	/* Assign name for sub devVL6180x_driver_exitice */
	snprintf(vl6180x_t->msm_sd.sd.name, sizeof(vl6180x_t->msm_sd.sd.name),
		"%s", vl6180x_t->i2c_driver->driver.name);
	/* Initialize sub device */
	v4l2_i2c_subdev_init(&vl6180x_t->msm_sd.sd,
		vl6180x_t->i2c_client->client,
		vl6180x_t->act_v4l2_subdev_ops);
	v4l2_set_subdevdata(&vl6180x_t->msm_sd.sd, vl6180x_t);
	vl6180x_t->msm_sd.sd.internal_ops = &msm_laser_focus_internal_ops;
	vl6180x_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(vl6180x_t->msm_sd.sd.name,
		ARRAY_SIZE(vl6180x_t->msm_sd.sd.name), "msm_laser_focus");
	media_entity_init(&vl6180x_t->msm_sd.sd.entity, 0, NULL, 0);
	vl6180x_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	vl6180x_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_LASER_FOCUS;
	vl6180x_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&vl6180x_t->msm_sd);

	mutex_init(&vl6180x_mutex);
	ATD_status = 1;
	VL6180x_create_proc_file();
#ifdef CONFIG_I2C_STRESS_TEST
	pr_err("Camera LaserFocus add test case+\n");
	i2c_add_test_case(client, "camera_laser_focus",ARRAY_AND_SIZE(LaserFocusTestCaseInfo));
	pr_err("Camera LaserFocus add test case-\n");
#endif
	rc = 0;
	pr_info("vl6180x_i2c_probe: succeeded, rc=%d\n",rc);
	CDBG("Exit\n");
	return rc;
Error_Early:
	if(vl6180x_t->sensordata)
	{
		if(vl6180x_t->sensordata->slave_info)
			kfree(vl6180x_t->sensordata->slave_info);
		if(vl6180x_t->sensordata->power_info.gpio_conf)
			kfree(vl6180x_t->sensordata->power_info.gpio_conf);
		kfree(vl6180x_t->sensordata);
	}
	kfree(vl6180x_t);
probe_failure:
	pr_err("vl6180x_i2c_probe: failed, rc=%d\n",rc);
	return rc;
}

static struct i2c_driver vl6180x_i2c_driver = {
	.id_table = vl6180x_i2c_id,
	.probe  = vl6180x_i2c_probe,
	.remove = vl6180x_i2c_remove,
	.driver = {
		.name = "qcom,vl6180",
		.owner = THIS_MODULE,
		.of_match_table = vl6180x_i2c_dt_match,
	},
};

static const struct of_device_id msm_laser_focus_dt_match[] = {
	{.compatible = "qcom,vl6180", .data = NULL},
	{}
};

MODULE_DEVICE_TABLE(of, msm_laser_focus_dt_match);
#if 0
static int32_t VL6180x_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;

	const struct of_device_id *match;
	struct msm_camera_cci_client *cci_client = NULL;

	CDBG("Probe Start\n");
	ATD_status = 0;

	match = of_match_device(msm_laser_focus_dt_match, &pdev->dev);
	if (!match) {
		pr_err("device not match\n");
		return -EFAULT;
	}

	if (!pdev->dev.of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}
	vl6180x_t = kzalloc(sizeof(struct msm_laser_focus_ctrl_t),
		GFP_KERNEL);
	if (!vl6180x_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}
	/* Set platform device handle */
	vl6180x_t->pdev = pdev;

	rc = VL6180x_get_dt_data(pdev->dev.of_node, vl6180x_t);
	if (rc < 0) {
		pr_err("%s failed line %d rc = %d\n", __func__, __LINE__, rc);
		return rc;
	}

	/* Assign name for sub device */
	snprintf(vl6180x_t->msm_sd.sd.name, sizeof(vl6180x_t->msm_sd.sd.name),
			"%s", vl6180x_t->sensordata->sensor_name);

	vl6180x_t->act_v4l2_subdev_ops = &msm_laser_focus_subdev_ops;

	/* Set device type as platform device */
	vl6180x_t->act_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	vl6180x_t->i2c_client = &msm_laser_focus_i2c_client;
	if (NULL == vl6180x_t->i2c_client) {
		pr_err("%s i2c_client NULL\n",
			__func__);
		rc = -EFAULT;
		goto probe_failure;
	}
	if (!vl6180x_t->i2c_client->i2c_func_tbl)
		vl6180x_t->i2c_client->i2c_func_tbl = &msm_sensor_cci_func_tbl;

	vl6180x_t->i2c_client->cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!vl6180x_t->i2c_client->cci_client) {
		kfree(vl6180x_t->vreg_cfg.cam_vreg);
		kfree(vl6180x_t);
		pr_err("failed no memory\n");
		return -ENOMEM;
	}
	//vl6180x_t->i2c_client->addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	cci_client = vl6180x_t->i2c_client->cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = vl6180x_t->cci_master;
	if (vl6180x_t->sensordata->slave_info->sensor_slave_addr)
		cci_client->sid = vl6180x_t->sensordata->slave_info->sensor_slave_addr >> 1;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->i2c_freq_mode = I2C_FAST_MODE;
	v4l2_subdev_init(&vl6180x_t->msm_sd.sd,
		vl6180x_t->act_v4l2_subdev_ops);
	v4l2_set_subdevdata(&vl6180x_t->msm_sd.sd, vl6180x_t);
	vl6180x_t->msm_sd.sd.internal_ops = &msm_laser_focus_internal_ops;
	vl6180x_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(vl6180x_t->msm_sd.sd.name,
		ARRAY_SIZE(vl6180x_t->msm_sd.sd.name), "msm_laser_focus");
	media_entity_init(&vl6180x_t->msm_sd.sd.entity, 0, NULL, 0);
	vl6180x_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	vl6180x_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_LASER_FOCUS;
	vl6180x_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&vl6180x_t->msm_sd);
	vl6180x_t->laser_focus_state = LASER_FOCUS_POWER_DOWN;

	/* Init data struct */
	vl6180x_t->laser_focus_cross_talk_offset_value = 0;
	vl6180x_t->laser_focus_offset_value = 0;
	vl6180x_t->laser_focus_state = LASER_FOCUS_POWER_DOWN;
	
	if(ATD_VL6180x_I2C_status_check(vl6180x_t) == 0)
		goto probe_failure;
	
	ATD_status = 1;
	VL6180x_create_proc_file();
	CDBG("Probe Success\n");
	return 0;
probe_failure:
	CDBG("%s Probe failed\n", __func__);
	return rc;
}

static struct platform_driver msm_laser_focus_platform_driver = {
	.driver = {
		.name = "qcom,vl6180",
		.owner = THIS_MODULE,
		.of_match_table = msm_laser_focus_dt_match,
	},
};
#endif
static int __init VL6180x_init_module(void)
{
	int32_t rc = 0;
	pr_err("VL6180x_init_module Enter\n");
#if 0
	rc = platform_driver_probe(&msm_laser_focus_platform_driver,
		VL6180x_platform_probe);
	pr_err("[S-DEBUG]%s:%d rc %d\n", __func__, __LINE__, rc);
	if (!rc)
		return rc;
#endif
	rc = i2c_add_driver(&vl6180x_i2c_driver);

	return rc;
}
static void __exit VL6180x_driver_exit(void)
{
	pr_err("VL6180x_driver_exit Enter");
	//platform_driver_unregister(&msm_laser_focus_platform_driver);
	i2c_del_driver(&vl6180x_i2c_driver);
	return;
}


module_init(VL6180x_init_module);
module_exit(VL6180x_driver_exit);
MODULE_DESCRIPTION("MSM LASER_FOCUS");
MODULE_LICENSE("GPL v2");
