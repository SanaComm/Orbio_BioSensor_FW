#ifndef ACCEL_H
#define ACCEL_H

// defines
#define ACCEL_ID					0x33
#define ACCEL_I2C_ADDRESS           0x30

// Accelerometer Registers
#define ACCEL_REG_READBIT			0x80  	// Turn on read bit in reg addr to indicate read
#define ACCEL_REG_AUTOINC			0x40	// auto increment register addresses on read

#define ACCEL_REG_STATUS_REG_AUX	0x07
#define ACCEL_REG_OUT_ADC1_L		0x08
#define ACCEL_REG_OUT_ADC1_H		0x09
#define ACCEL_REG_OUT_ADC2_L		0x0A
#define ACCEL_REG_OUT_ADC2_H		0x0B
#define ACCEL_REG_OUT_ADC3_L		0x0C
#define ACCEL_REG_OUT_ADC3_H		0x0D
#define ACCEL_REG_INT_COUNTER_REG	0x0E
#define ACCEL_REG_WHOAMI			0x0F
#define ACCEL_REG_CTRL_REG0   		0x1E	//rw
#define ACCEL_REG_TEMP_CFG_REG		0x1F	//rw
#define ACCEL_REG_CTRL_REG1			0x20	//rw
#define ACCEL_REG_CTRL_REG2			0x21	//rw
#define ACCEL_REG_CTRL_REG3			0x22	//rw
#define ACCEL_REG_CTRL_REG4			0x23	//rw
#define ACCEL_REG_CTRL_REG5			0x24	//rw
#define ACCEL_REG_CTRL_REG6			0x25	//rw
#define ACCEL_REG_REFERENCE			0x26	//rw
#define ACCEL_REG_HP_FILTER_RESET	0x26	//r - this is the same as the reference register
#define ACCEL_REG_STATUS_REG2		0x27
#define ACCEL_REG_OUT_X_L			0x28
#define ACCEL_REG_OUT_X_H			0x29
#define ACCEL_REG_OUT_Y_L			0x2A
#define ACCEL_REG_OUT_Y_H			0x2B
#define ACCEL_REG_OUT_Z_L			0x2C
#define ACCEL_REG_OUT_Z_H			0x2D
#define ACCEL_REG_FIFO_CTRL_REG		0x2E	//rw
#define ACCEL_REG_FIFO_SRC_REG		0x2F
#define ACCEL_REG_INT1_CFG			0x30	//rw
#define ACCEL_REG_INT1_SOURCE		0x31
#define ACCEL_REG_INT1_THS			0x32	//rw
#define ACCEL_REG_INT1_DURATION		0x33	//rw
#define ACCEL_REG_INT2_CFG			0x34	//rw
#define ACCEL_REG_INT2_SOURCE		0x35
#define ACCEL_REG_INT2_THS			0x36	//rw
#define ACCEL_REG_INT2_DURATION		0x37	//rw
#define ACCEL_REG_CLICK_CFG			0x38	//rw
#define ACCEL_REG_CLICK_SRC			0x39
#define ACCEL_REG_CLICK_THS			0x3A	//rw
#define ACCEL_REG_TIME_LIMIT		0x3B	//rw
#define ACCEL_REG_TIME_LATENCY		0x3C	//rw
#define ACCEL_REG_TIME_WINDOW		0x3D	//rw
#define ACCEL_REG_ACT_THS			0x3E
#define ACCEL_REG_INACT_DUR			0x3F

// Specific bits
#define INT1_SRC_IA                 0x40
#define INT1_SRC_ZH                 0x20
#define INT1_SRC_YH                 0x08
#define INT1_SRC_XH                 0x02
#define INT1_SRC_ZL                 0x10
#define INT1_SRC_YL                 0x04
#define INT1_SRC_XL                 0x01

#define USE_25Hz
#undef USE_10Hz
#undef USE_1Hz

// prototypes

// initialize accelerometer
int accelInit(void);
// Get how many accelerometer X/Y/Z data are available in FIFO
int accelGetNumData(void);
// Get accelerometer X/Y/Z data
void accelGetData(uint8_t *output);
// Get Int1 sources
uint8_t accelGetSrc1(void);
// Get Int2 sources
uint8_t accelGetSrc2(void);
// Put into power-on mode
void accelPowerOn(void);
// Put into power-off mode
void accelPowerOff(void);
// Clear FIFO
void accelClearFifo(void);

#endif
