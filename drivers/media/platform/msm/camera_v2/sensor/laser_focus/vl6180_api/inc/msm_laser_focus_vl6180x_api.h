#ifndef MSM_LASER_FOCUS_VL6180X_API_H
#define MSM_LASER_FOCUS_VL6180X_API_H

#undef CDBG
#define CDBG(fmt, args...) pr_info(fmt, ##args)

#undef DBG_LOG
#define DBG_LOG(fmt, args...) pr_info(fmt, ##args)

#undef REG_RW_DBG
#define REG_RW_DBG(fmt, args...) pr_debug(fmt, ##args)

#undef API_DBG
#define API_DBG(fmt, args...) pr_debug(fmt, ##args)

/* Out of range */
#define OUT_OF_RANGE 765

/* Time out value: mm */
#define TIMEOUT_VAL 80

int ASUS_VL6180x_WrByte(uint32_t register_addr, uint16_t i2c_write_data);
int ASUS_VL6180x_RdByte(uint32_t register_addr, uint16_t *i2c_read_data);
int ASUS_VL6180x_WrWord(uint32_t register_addr, uint16_t i2c_write_data);
int ASUS_VL6180x_RdWord(uint32_t register_addr, uint16_t *i2c_read_data);
int ASUS_VL6180x_WrDWord(uint32_t register_addr, uint32_t i2c_write_data);
int ASUS_VL6180x_RdDWord(uint32_t register_addr, uint32_t *i2c_read_data, uint16_t num_byte);
int ASUS_VL6180x_UpdateByte(uint32_t register_addr, uint8_t AndData, uint8_t OrDat);

#endif
