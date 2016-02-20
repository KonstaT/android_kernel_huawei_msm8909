#ifndef __FOCALTECH_EX_H__
#define __FOCALTECH_EX_H__

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>

#include <linux/syscalls.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include "ft5x36_ts.h"

#define FT_UPGRADE_AA					0xAA
#define FT_UPGRADE_55 					0x55

/*upgrade config of FT5606*/
#define FT5606_UPGRADE_AA_DELAY 		50
#define FT5606_UPGRADE_55_DELAY 		10
#define FT5606_UPGRADE_ID_1				0x79
#define FT5606_UPGRADE_ID_2				0x06
#define FT5606_UPGRADE_READID_DELAY 	100
#define FT5606_UPGRADE_EARSE_DELAY		2000

/*upgrade config of FT5316*/
#define FT5316_UPGRADE_AA_DELAY 		50
#define FT5316_UPGRADE_55_DELAY 		30
#define FT5316_UPGRADE_ID_1				0x79
#define FT5316_UPGRADE_ID_2				0x07
#define FT5316_UPGRADE_READID_DELAY 	1
#define FT5316_UPGRADE_EARSE_DELAY		1500

/*upgrade config of FT5x06(x=2,3,4)*/
#define FT5X06_UPGRADE_AA_DELAY 		50
#define FT5X06_UPGRADE_55_DELAY 		30
#define FT5X06_UPGRADE_ID_1				0x79
#define FT5X06_UPGRADE_ID_2				0x03
#define FT5X06_UPGRADE_READID_DELAY 	1
#define FT5X06_UPGRADE_EARSE_DELAY		2000

/*upgrade config of FT6208*/
#define FT6208_UPGRADE_AA_DELAY 		60
#define FT6208_UPGRADE_55_DELAY 		10
#define FT6208_UPGRADE_ID_1				0x79
#define FT6208_UPGRADE_ID_2				0x05
#define FT6208_UPGRADE_READID_DELAY 	10
#define FT6208_UPGRADE_EARSE_DELAY		2000

/*upgrade config of FT6X06*/
#define FT6X06_UPGRADE_AA_DELAY 		80
#define FT6X06_UPGRADE_55_DELAY 		5
#define FT6X06_UPGRADE_ID_1				0x79
#define FT6X06_UPGRADE_ID_2				0x08
#define FT6X06_UPGRADE_READID_DELAY 	10
#define FT6X06_UPGRADE_EARSE_DELAY		2000

/*upgrade config of FT5X36*/
#define FT5X36_UPGRADE_AA_DELAY 		10//30
#define FT5X36_UPGRADE_55_DELAY 		30
#define FT5X36_UPGRADE_ID_1				0x79
#define FT5X36_UPGRADE_ID_2				0x11
#define FT5X36_UPGRADE_READID_DELAY 	10
#define FT5X36_UPGRADE_EARSE_DELAY		2000

//added by chenchen for focaltech FT6336 20140902 begin
/*upgrade config of FT6X36*/
#define FT6X36_UPGRADE_AA_DELAY 		10//30
#define FT6X36_UPGRADE_55_DELAY 		10
#define FT6X36_UPGRADE_ID_1				0x79
#define FT6X36_UPGRADE_ID_2				0x18
#define FT6X36_UPGRADE_READID_DELAY 	10
#define FT6X36_UPGRADE_EARSE_DELAY		2000
//added by chenchen for focaltech FT6336 20140902 end

#define BL_VERSION_LZ4        			0
#define BL_VERSION_Z7        			1
#define BL_VERSION_GZF        			2
/*****************************************************************************/
#define FTS_PACKET_LENGTH        		128
#define FTS_SETTING_BUF_LEN      		128
#define FTS_DMA_BUF_SIZE 				1024

#define FTS_UPGRADE_LOOP				5

#define FTS_FACTORYMODE_VALUE			0x40
#define FTS_WORKMODE_VALUE				0x00

/*create sysfs for debug*/
int fts_create_sysfs(struct i2c_client * client);

void fts_release_sysfs(struct i2c_client * client);

int fts_create_apk_debug_channel(struct i2c_client *client);
void fts_release_apk_debug_channel(void);

int fts_ctpm_auto_upgrade(struct i2c_client *client);

/*
*FocalTech_write_reg- write register
*@client: handle of i2c
*@regaddr: register address
*@regvalue: register value
*
*/
int fts_write_reg(struct i2c_client * client,u8 regaddr, u8 regvalue);

int fts_read_reg(struct i2c_client * client,u8 regaddr, u8 *regvalue);

#endif
