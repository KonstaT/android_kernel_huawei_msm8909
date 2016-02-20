#ifndef __FOCALTECH_TS_H__
#define __FOCALTECH_TS_H__
//add by pangle at 20141129
#include <linux/wakelock.h>

/* -- dirver configure -- */
#define PRESS_MAX					0xFF
#define FT_PRESS					0x7F

#define FT_MAX_ID					0x0F
#define FT_TOUCH_STEP				6
#define FT_TOUCH_X_H_POS			3
#define FT_TOUCH_X_L_POS			4
#define FT_TOUCH_Y_H_POS			5
#define FT_TOUCH_Y_L_POS			6
#define FT_TOUCH_EVENT_POS			3
#define FT_TOUCH_ID_POS				5

#define FT_COORDS_ARR_SIZE	4
#define MAX_BUTTONS		4
#define FT_FW_NAME_MAX_LEN	50

#define FT_VTG_MIN_UV		2600000
#define FT_VTG_MAX_UV		3300000
#define FT_I2C_VTG_MIN_UV	1800000
#define FT_I2C_VTG_MAX_UV	1800000

#define FT_STARTUP_DLY		60//250 /*Modifyed by liumx for tp 2013.11.20*/
#define FT_RESET_DLY		20

 #define FTS_NAME						"FT5x36"
 #define CFG_MAX_TOUCH_POINTS 				5
#define POINT_READ_BUF					(3 + FT_TOUCH_STEP * CFG_MAX_TOUCH_POINTS)

/* The platform data for the Focaltech touchscreen driver */
struct fts_platform_data {
	u32 irqflags;
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 reset_gpio;
	u32 reset_gpio_flags;
	u32 family_id;
	u8 max_touch_point;
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 panel_minx;
	u32 panel_miny;
	u32 panel_maxx;
	u32 panel_maxy;
	bool no_force_update;
	bool i2c_pull_up;
	bool auto_clb;
	int (*power_init) (bool);
	int (*power_on) (bool);
};
struct ts_event {
	u16 au16_x[CFG_MAX_TOUCH_POINTS];	/*x coordinate */
	u16 au16_y[CFG_MAX_TOUCH_POINTS];	/*y coordinate */
	u8 au8_touch_event[CFG_MAX_TOUCH_POINTS];	/*touch event:
					0 -- down; 1-- contact; 2 -- contact */
	u8 au8_finger_id[CFG_MAX_TOUCH_POINTS];	/*touch ID */
	u16 pressure;
	u8 touch_point;
};


struct fts_ts_data {
	unsigned int irq;
	unsigned int x_max;
	unsigned int y_max;
	struct workqueue_struct *fts_wq;
	struct work_struct	work;
	/*
	//yuquan added begin for suspend and resume
	struct workqueue_struct *fts_pm_wq;
	struct delayed_work	fts_pm_work;
	int pm_status;
	//yuquan added end for suspend and resume
	//yuquan added timeout wakelock at 20141129
       struct wake_lock fts_wake_lock;
	   */
//added by pangle for ft gesture 20140917 begin	
	struct work_struct	fts_work;
       struct mutex fts_mutex;
	spinlock_t		lock;
	struct wake_lock tp_wake_lock;
//added by pangle for ft gesture 20140917 end	
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct ts_event event;
	struct fts_platform_data *pdata;
	struct regulator *vdd;
	struct regulator *vcc_i2c;

/*
Added by Litao for enable irq or disable irq protect,avoid to make logic error,the irq was always disabled.
*/
	s32 irq_is_disable;
	s32 irq_wake_is_disable;
	spinlock_t irq_lock;
	bool loading_fw;
	bool suspended;
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
/*modified for pin used touch start.zengguang 2014.08.22*/
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
/*modified for pin used touch end.zengguang 2014.08.22*/
};

/******************************************************************************/
/*Chip Device Type*/
#define IC_FT5X06						0	/*x=2,3,4*/
#define IC_FT5606						1	/*ft5506/FT5606/FT5816*/
#define IC_FT5316						2	/*ft5x16*/
#define IC_FT6208						3  	/*ft6208*/
#define IC_FT6x06     					4	/*ft6206/FT6306*/
#define IC_FT5x06i     					5	/*ft5306i*/
#define IC_FT5x36     					6	/*ft5336/ft5436/FT5436i*/
//added by chenchen for focaltech FT6336 20140902
#define IC_FT6x36     					7	/*ft6336/ft6436/FT6536*/

//#define DEVICE_IC_TYPE					IC_FT6x06//IC_FT5x36 //modifyed by liumx

/*register address*/
#define FTS_REG_FW_VER					0xA6
#define FTS_REG_VENDOR_ID				0xA8
#define FTS_REG_POINT_RATE	                     0x88
#define FTS_READ_ID_REG		0x90
#define FTS_REG_PMODE		0xA5
#define FTS_PMODE_HIBERNATE	0x03


#if 0
#if ( DEVICE_IC_TYPE == IC_FT5X06 )
  #define FTS_NAME						"FT5x06"
  #define CFG_MAX_TOUCH_POINTS 				5
  #define AUTO_CLB
#elif ( DEVICE_IC_TYPE == IC_FT5606 )
  #define FTS_NAME						"FT5606"
  #define CFG_MAX_TOUCH_POINTS 				5//10
  #define AUTO_CLB
#elif ( DEVICE_IC_TYPE == IC_FT5316 )
  #define FTS_NAME						"FT5316"
  #define CFG_MAX_TOUCH_POINTS 				5
  #define AUTO_CLB
#elif ( DEVICE_IC_TYPE == IC_FT6208 )
  #define FTS_NAME						"FT6208"
  #define CFG_MAX_TOUCH_POINTS 				2
  //#define AUTO_CLB
#elif ( DEVICE_IC_TYPE == IC_FT6x06 )
  #define FTS_NAME						"FT5x36"//"FT6x06"//modifyed by liumx
  #define CFG_MAX_TOUCH_POINTS 				2
  #define AUTO_CLB
#elif ( DEVICE_IC_TYPE == IC_FT5x06i )
  #define FTS_NAME						"FT5x06i"
  #define CFG_MAX_TOUCH_POINTS 				5
  #define    AUTO_CLB
#elif ( DEVICE_IC_TYPE == IC_FT5x36 )
  #define FTS_NAME						"FT5x36"
  #define CFG_MAX_TOUCH_POINTS 				5
  #define AUTO_CLB
#endif
#endif

//#define FTS_CTL_IIC
#define FTS_SYSFS_DEBUG
#define FTS_APK_DEBUG
//#define FTS_AUTO_UPDATE

#ifdef FTS_SYSFS_DEBUG
//#define FTS_APK_DEBUG
#define TPD_AUTO_UPGRADE				// if need upgrade CTP FW when POWER ON,pls enable this MACRO
#endif

#define FTS_DBG
#ifdef FTS_DBG
#define DBG(fmt, args...) 				printk("[FTS]" fmt, ## args)
#else
#define DBG(fmt, args...) 				do{}while(0)
#endif

//added by shihuijun for gesture display 20141223 begin
#define FTS_GESTRUE_POINTS 255
extern int gesture_id;
extern unsigned char buf_gesture[FTS_GESTRUE_POINTS * 2];
extern short pointnum;
extern unsigned short coordinate_x[150];
extern unsigned short coordinate_y[150];
//added by pangle for gesture display 20141031 end

int fts_i2c_Read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen);
int fts_i2c_Write(struct i2c_client *client, char *writebuf, int writelen);
void fts_disable_irq(struct fts_ts_data *data);
void fts_enable_irq(struct fts_ts_data *data);
void fts_disable_irq_wake(struct fts_ts_data *data);
void fts_enable_irq_wake(struct fts_ts_data *data);

#endif
