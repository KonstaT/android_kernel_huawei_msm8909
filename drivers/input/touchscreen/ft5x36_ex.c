/*
 *drivers/input/touchscreen/FocalTech_ex.c
 *
 *FocalTech IC driver expand function for debug.
 *
 *Copyright (c) 2010  Focal tech Ltd.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *Note:the error code of EIO is the general error in this file.
 */


#include <linux/mount.h>
#include <linux/netdevice.h>
//#include <linux/proc_fs.h>
#include "../../fs/proc/internal.h"
#include "ft5x36_ex.h"
#include "ft5x36_vendor.h"
#include "ft5x36_ts.h"
//added by pangle for FT gesture 20140917 begin
#include <linux/wakelock.h>
bool gesture_enable;
extern bool gesture_enable_dtsi;
//added by pangle for FT gesture 20140917 end

struct Upgrade_Info {
	u16 delay_aa;		/*delay of write FT_UPGRADE_AA */
	u16 delay_55;		/*delay of write FT_UPGRADE_55 */
	u8 upgrade_id_1;	/*upgrade id 1 */
	u8 upgrade_id_2;	/*upgrade id 2 */
	u16 delay_readid;	/*delay of read id */
	u16 delay_earse_flash; /*delay of earse flash*/
};


int fts_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf, u32 dw_lenth);
int fts_read_project_code(struct i2c_client * client, char * pProjectCode);
	
static DEFINE_MUTEX(g_device_mutex);

int fts_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue)
{
	unsigned char buf[2] = {0};
	buf[0] = regaddr;
	buf[1] = regvalue;

	return fts_i2c_Write(client, buf, sizeof(buf));
}


int fts_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue)
{
	return fts_i2c_Read(client, &regaddr, 1, regvalue, 1);
}

int fts_ctpm_auto_clb(struct i2c_client *client)
{
	unsigned char uc_temp;
	unsigned char i ;
	 struct fts_ts_data *ft5x36_ts = i2c_get_clientdata(client);

	/*start auto CLB */
	msleep(200);

	fts_write_reg(client, 0, FTS_FACTORYMODE_VALUE);
	/*make sure already enter factory mode */
	msleep(100);
	/*write command to start calibration */
	fts_write_reg(client, 2, 0x4);
	msleep(300);
	if (ft5x36_ts->pdata->family_id==6) {
		for(i=0;i<100;i++)
		{
			fts_read_reg(client, 0x02, &uc_temp);
			if (0x02 == uc_temp ||
				0xFF == uc_temp)
			{
				/*if 0x02, then auto clb ok, else 0xff, auto clb failure*/
			    break;
			}
			msleep(20);	    
		}
	} else {
		for (i = 0; i < 100; i++) {
			fts_read_reg(client, 0, &uc_temp);
			if (0x0 == ((uc_temp&0x70)>>4))  /*return to normal mode, calibration finish*/
			{
				break;
			}
			msleep(20);	    
		}
	}
	/*calibration OK */
	msleep(300);
	fts_write_reg(client, 0, FTS_FACTORYMODE_VALUE);	/*goto factory mode for store */
	msleep(200);	/*make sure already enter factory mode */
	fts_write_reg(client, 2, 0x5);	/*store CLB result */
	msleep(300);
	fts_write_reg(client, 0, FTS_WORKMODE_VALUE);	/*return to normal mode */
	msleep(300);

	/*store CLB result OK */
	return 0;
}

/*
upgrade with *.i file
*/
int fts_ctpm_fw_upgrade_with_i_file(struct i2c_client *client)
{
	u8 *pbt_buf = NULL;
	int i_ret = 0;
	int fw_len;
	u8 uc_tp_fm_TP_ID;
	u8 temp[40];

       struct fts_ts_data *ft5x36_ts = i2c_get_clientdata(client);

	/*modify by pangle for ft6436 at 2014.09.30 begin*/
	fts_read_reg(client, FTS_REG_VENDOR_ID, &uc_tp_fm_TP_ID);
       printk(KERN_INFO "FT5x36:module vendor id = 0x%x\n", uc_tp_fm_TP_ID);
	
	if(uc_tp_fm_TP_ID == 0 || uc_tp_fm_TP_ID != 0xA0){
		uc_tp_fm_TP_ID = fts_read_project_code(client, temp);
		printk(KERN_INFO "get maker id failed, reget module vendor id = 0x%x\n", uc_tp_fm_TP_ID);
	}
	/*modify by pangle for ft6436 at 2014.09.30 end*/
	switch (uc_tp_fm_TP_ID) {

	case FT5X36_VENDOR_JTOUCH:
/*modify by pangle for QW806 CTP 2014-09-10 begin*/
	#if defined(CONFIG_PRODUCT_NAME_NONE) ||defined(CONFIG_PRODUCT_QC601B)
		printk(KERN_INFO "FT5X36:upgrade with huaxinda firmware\n");
		
		fw_len = sizeof(FT5X36_FIRMWARE_HUAXINDA);
			
		/*judge the fw that will be upgraded
		* if illegal, then stop upgrade and return.
		*/
		if (fw_len < 8 || fw_len > 32 * 1024) {
			dev_err(&client->dev, "%s:FW length error\n", __func__);
			return -EIO;
		}
	
		if ((FT5X36_FIRMWARE_HUAXINDA[fw_len - 8] ^ FT5X36_FIRMWARE_HUAXINDA[fw_len - 6]) == 0xFF
			&& (FT5X36_FIRMWARE_HUAXINDA[fw_len - 7] ^ FT5X36_FIRMWARE_HUAXINDA[fw_len - 5]) == 0xFF
			&& (FT5X36_FIRMWARE_HUAXINDA[fw_len - 3] ^ FT5X36_FIRMWARE_HUAXINDA[fw_len - 4]) == 0xFF) {
			/*FW upgrade */
			pbt_buf = FT5X36_FIRMWARE_HUAXINDA;
			/*call the upgrade function */
			i_ret = fts_ctpm_fw_upgrade(client, pbt_buf, sizeof(FT5X36_FIRMWARE_HUAXINDA));
			if (i_ret != 0)
				dev_err(&client->dev, "%s:upgrade failed. err.\n",
						__func__);
			else{
							if(ft5x36_ts->pdata->auto_clb)
					   fts_ctpm_auto_clb(client);	/*start auto CLB */
			}
		}	
	#endif
		break;
		
	
//added by chenchen for boway 20140724 begin	
	case FT5X36_VENDOR_TOPTOUCH:
	#if defined(CONFIG_Y560_CL_512)
			printk(KERN_INFO "FT5X36:upgrade with TOPTPUCH firmware\n");
			fw_len = sizeof(FT6X36_FIRMWARE0x0A_TOPTOUCH);	
			if (fw_len < 8 || fw_len > 32 * 1024) {
				dev_err(&client->dev, "%s:FW length error\n", __func__);
				return -EIO;
			}
			/*FW upgrade */
			pbt_buf = FT6X36_FIRMWARE0x0A_TOPTOUCH;
			/*call the upgrade function */
			i_ret = fts_ctpm_fw_upgrade(client, pbt_buf, sizeof(FT6X36_FIRMWARE0x0A_TOPTOUCH));
			if (i_ret != 0)
				dev_err(&client->dev, "%s:upgrade failed. err.\n",
						__func__);
			else{
							if(ft5x36_ts->pdata->auto_clb)
					   fts_ctpm_auto_clb(client);	/*start auto CLB */
			}
			
	#endif
//added by pangle for 702 fw upgrade at 20150413 begin	
//added by pangle for 860 fw upgrade at 20150413 begin	
	case FT6X36_VENDOR0x9b_JINLONG:
	#if defined(CONFIG_QL860_BASE)
				printk(KERN_INFO "FT5X36:upgrade with JINLONG firmware\n");
				fw_len = sizeof(FT6X36_FIRMWARE0x9b_JINLONG);	
				if (fw_len < 8 || fw_len > 32 * 1024) {
					dev_err(&client->dev, "%s:FW length error\n", __func__);
					return -EIO;
				}
				/*FW upgrade */
				pbt_buf = FT6X36_FIRMWARE0x9b_JINLONG;
				/*call the upgrade function */
				i_ret = fts_ctpm_fw_upgrade(client, pbt_buf, sizeof(FT6X36_FIRMWARE0x9b_JINLONG));
				if (i_ret != 0)
					dev_err(&client->dev, "%s:upgrade failed. err.\n",
							__func__);
				else{
								if(ft5x36_ts->pdata->auto_clb)
						   fts_ctpm_auto_clb(client);	/*start auto CLB */
				}
				
	#endif
//added by pangle for 860 fw upgrade at 20150413 end	
//added by pangle for 702 fw upgrade at 20150303 begin	
	case FT6X36_VENDOR0x11_JINLONG:
	#if defined(CONFIG_QL860_BASE)
				printk(KERN_INFO "FT5X36:upgrade with JINLONG firmware\n");
				fw_len = sizeof(FT6X36_FIRMWARE0x9b_JINLONG);	
				if (fw_len < 8 || fw_len > 32 * 1024) {
					dev_err(&client->dev, "%s:FW length error\n", __func__);
					return -EIO;
				}
				/*FW upgrade */
				pbt_buf = FT6X36_FIRMWARE0x9b_JINLONG;
				/*call the upgrade function */
				i_ret = fts_ctpm_fw_upgrade(client, pbt_buf, sizeof(FT6X36_FIRMWARE0x9b_JINLONG));
				if (i_ret != 0)
					dev_err(&client->dev, "%s:upgrade failed. err.\n",
							__func__);
				else{
								if(ft5x36_ts->pdata->auto_clb)
						   fts_ctpm_auto_clb(client);	/*start auto CLB */
				}
				
	#endif


#if defined(CONFIG_Y560_U23_BASE) || defined(CONFIG_Y560_L01_BASE)|| defined(CONFIG_Y560_L02_BASE) //modify by pangle at 20150311
			printk(KERN_INFO "FT5X36:upgrade with TOPTPUCH firmware\n");
			fw_len = sizeof(FT6X36_FIRMWARE0xA0_TOPTOUCH);	
			if (fw_len < 8 || fw_len > 32 * 1024) {
				dev_err(&client->dev, "%s:FW length error\n", __func__);
				return -EIO;
			}
			/*FW upgrade */
			pbt_buf = FT6X36_FIRMWARE0xA0_TOPTOUCH;
			/*call the upgrade function */
			i_ret = fts_ctpm_fw_upgrade(client, pbt_buf, sizeof(FT6X36_FIRMWARE0xA0_TOPTOUCH));
			if (i_ret != 0)
				dev_err(&client->dev, "%s:upgrade failed. err.\n",
						__func__);
			else{
							if(ft5x36_ts->pdata->auto_clb)
					   fts_ctpm_auto_clb(client);	/*start auto CLB */
			}
			
#endif
//added by pangle for 702 fw upgrade at 20150303 begin

	      break;
	default:
		fw_len = 0;
		pbt_buf  = 0;
		printk(KERN_INFO "invalid vendor id 0x%x,upgrade abort\n", uc_tp_fm_TP_ID);
       break;

	}
	
	return i_ret;
}
/*modify by pangle for QW806 CTP 2014-09-10 end*/
u8 fts_ctpm_get_i_file_ver(struct i2c_client *client)
{
	u16 ui_sz;
	u8 uc_tp_fm_TP_ID;
	u8 temp[40];

	/*modify by pangle for ft6436 at 2014.09.30 begin*/
	fts_read_reg(client, FTS_REG_VENDOR_ID, &uc_tp_fm_TP_ID);
	
	if(uc_tp_fm_TP_ID == 0 || uc_tp_fm_TP_ID != 0xA0){
		uc_tp_fm_TP_ID = fts_read_project_code(client, temp);
		printk(KERN_INFO "read vendor id failed, the vendor id is 0x%x after read again\n", uc_tp_fm_TP_ID);
	}
	/*modify by pangle for ft6436 at 2014.09.30 end*/

	switch (uc_tp_fm_TP_ID) {

	case FT5X36_VENDOR_JTOUCH:

	#if defined(CONFIG_PRODUCT_NAME_NONE) ||defined(CONFIG_QL1005B_DOOV_T3000)
	ui_sz = sizeof(FT5X36_FIRMWARE_JTOUCH);
	if (ui_sz > 2)
		return FT5X36_FIRMWARE_JTOUCH[ui_sz - 2];
	#endif
	break;
	
	case FT5X36_VENDOR_TRULY:

	#if defined(CONFIG_PRODUCT_NAME_NONE) ||defined(CONFIG_QL1005B_DOOV_T3000) ||defined(CONFIG_QL1005B_DOOV_NIKITA)
	ui_sz = sizeof(FT5X36_FIRMWARE_TRULY);
	if (ui_sz > 2)
		return FT5X36_FIRMWARE_TRULY[ui_sz - 2];
	#endif
	break;

	case FT5X36_VENDOR_CHAOSHENG:

	#if defined(CONFIG_PRODUCT_NAME_NONE) ||defined(CONFIG_QL1005B_DOOV_NIKITA)
	ui_sz = sizeof(FT5X36_FIRMWARE_CHAOSHENG);
	if (ui_sz > 2)
		return FT5X36_FIRMWARE_CHAOSHENG[ui_sz - 2];
	#endif
	break;
    /*modified by shihuijun QL701 platform  ctp TOPTOUCH vendor id 0xoA 20140114 begin*/
	case FT5X36_VENDOR_TOPTOUCH:

	#if defined(CONFIG_Y560_CL_512)
	ui_sz = sizeof(FT6X36_FIRMWARE0x0A_TOPTOUCH);
	ui_sz = 0x10a;
	if (ui_sz > 2)
		return FT6X36_FIRMWARE0x0A_TOPTOUCH[ui_sz];
	#endif
	/*modified by shihuijun QL701 platform	ctp TOPTOUCH vendor id 0xoA 20140114 end*/
//added by pangle for 860 fw upgrade at 20150413 begin
	case FT6X36_VENDOR0x9b_JINLONG:

	#if defined(CONFIG_QL860_BASE)
	ui_sz = sizeof(FT6X36_FIRMWARE0x9b_JINLONG);
	ui_sz = 0x10a;
	if (ui_sz > 2)
		return FT6X36_FIRMWARE0x9b_JINLONG[ui_sz];
	#endif
//added by pangle for 860 fw upgrade at 20150413 begin

	case FT6X36_VENDOR0x11_JINLONG:

	#if defined(CONFIG_QL860_BASE)
	ui_sz = sizeof(FT6X36_FIRMWARE0x9b_JINLONG);
	ui_sz = 0x10a;
	if (ui_sz > 2)
		return FT6X36_FIRMWARE0x9b_JINLONG[ui_sz];
	#endif

//added by pangle for 702 fw upgrade at 20150303 begin
	#if defined(CONFIG_Y560_U23_BASE) || defined(CONFIG_Y560_L01_BASE) || defined(CONFIG_Y560_L02_BASE) //modify by pangle at 20150311
	ui_sz = sizeof(FT6X36_FIRMWARE0xA0_TOPTOUCH);
	ui_sz = 0x10a;
	if (ui_sz > 2)
		return FT6X36_FIRMWARE0xA0_TOPTOUCH[ui_sz];
	#endif
	break;	
//added by pangle for 702 fw upgrade at 20150303 end

	case FT5X36_VENDOR_DAWOSI:

	#if defined(CONFIG_PRODUCT_NAME_NONE) ||defined(CONFIG_QL1005C_BOWAY)
	ui_sz = sizeof(FT5X36_FIRMWARE_DAWOSI);
	if (ui_sz > 2)
		return FT5X36_FIRMWARE_DAWOSI[ui_sz - 2];
	#endif
	break;

	case FT6X06_VENDOR0x11_HONGZHAN:

	#if defined(CONFIG_QL1005C_WASAM)
	ui_sz = sizeof(FT6X06_FIRMWARE0x11_HONGZHAN);
	if (ui_sz > 2)
		return FT6X06_FIRMWARE0x11_HONGZHAN[ui_sz - 2];
	#endif

	/*modified for L100 yeji vendor id =0x11*/
	#if defined(CONFIG_QL1005C_BIRD)
	ui_sz = sizeof(FT6X06_FIRMWARE0x80_EACH);
	if (ui_sz > 2)
		return FT6X06_FIRMWARE0x80_EACH[ui_sz - 2];
	#endif

	/*modified for L71 yuxin vendor id =0x11*/
	#if defined(CONFIG_QL1005C_YUSUN)
	ui_sz = sizeof(FT6X06_FIRMWARE0x59_BYD);
	if (ui_sz > 2)
		return FT6X06_FIRMWARE0x59_BYD[ui_sz - 2];
	#endif
	
	break;

	case FT6X06_VENDOR0xD2_HONGZHAN:

	#if defined(CONFIG_QL1005C_NOAIN)
	ui_sz = sizeof(FT6X06_FIRMWARE0xD2_HONGZHAN);
	if (ui_sz > 2)
		return FT6X06_FIRMWARE0xD2_HONGZHAN[ui_sz - 2];
	#endif

	#if defined(CONFIG_QL1005C_WASAM)
	ui_sz = sizeof(FT6X06_FIRMWARE0xD2_HONGZHAN);
	if (ui_sz > 2)
		return FT6X06_FIRMWARE0xD2_HONGZHAN[ui_sz - 2];
	#endif

	break;

	case FT6X06_VENDOR0x86_EKEY:

	#if defined(CONFIG_QL1005C_OUKI)
	ui_sz = sizeof(FT6X06_FIRMWARE0x86_EKEY);
	if (ui_sz > 2)
		return FT6X06_FIRMWARE0x86_EKEY[ui_sz - 2];
	#endif
	break;

	case FT6X06_VENDOR0x80_EACH:

	#if defined(CONFIG_QL1005C_BIRD)
	ui_sz = sizeof(FT6X06_FIRMWARE0x80_EACH);
	if (ui_sz > 2)
		return FT6X06_FIRMWARE0x80_EACH[ui_sz - 2];
	#endif
	break;

	case FT6X06_VENDOR0xD7_TONGHAO:

	#if defined(CONFIG_QL1005C_BIRD)
	ui_sz = sizeof(FT6X06_FIRMWARE0xD7_TONGHAO);
	if (ui_sz > 2)
		return FT6X06_FIRMWARE0xD7_TONGHAO[ui_sz - 2];
	#endif
	break;

	case FT5X36_VENDOR_BYD:

	#if defined(CONFIG_QL1005C_YUSUN)
	ui_sz = sizeof(FT6X06_FIRMWARE0x59_BYD);
	if (ui_sz > 2)
		return FT6X06_FIRMWARE0x59_BYD[ui_sz - 2];
	#endif
	break;
//added by chenchen for noain jinchen ctp 20140624 begin
	case FT6X06_VENDOR_JINCHEN:

	#if defined(CONFIG_QL1005C_NOAIN)
	ui_sz = sizeof(FT6X06_FIRMWARE_JINCHEN);
	if (ui_sz > 2)
		return FT6X06_FIRMWARE_JINCHEN[ui_sz - 2];
	#endif
	break;
//added by chenchen for noain jinchen ctp 20140624 end

	default:
		ui_sz=0;
		printk(KERN_INFO "invalid vendor id 0x%x,upgrade abort\n", uc_tp_fm_TP_ID);
	break;
	}
	
	return 0x00;	/*default value */
}

/*update project setting
*only update these settings for COB project, or for some special case
*/
int fts_ctpm_update_project_setting(struct i2c_client *client)
{
	u8 uc_i2c_addr;	/*I2C slave address (7 bit address)*/
	u8 uc_io_voltage;	/*IO Voltage 0---3.3v;	1----1.8v*/
	u8 uc_panel_factory_id;	/*TP panel factory ID*/
	u8 buf[FTS_SETTING_BUF_LEN];
	u8 reg_val[2] = {0};
	u8 auc_i2c_write_buf[10] = {0};
	u8 packet_buf[FTS_SETTING_BUF_LEN + 6];
	u32 i = 0;
	int i_ret;

	uc_i2c_addr = client->addr;
	uc_io_voltage = 0x0;
	uc_panel_factory_id = 0x5a;


	/*Step 1:Reset  CTPM
	*write 0xaa to register 0xfc
	*/
	fts_write_reg(client, 0xfc, 0xaa);
	msleep(50);

	/*write 0x55 to register 0xfc */
	fts_write_reg(client, 0xfc, 0x55);
	msleep(30);

	/*********Step 2:Enter upgrade mode *****/
	auc_i2c_write_buf[0] = 0x55;
	auc_i2c_write_buf[1] = 0xaa;
	do {
		i++;
		i_ret = fts_i2c_Write(client, auc_i2c_write_buf, 2);
		msleep(5);
	} while (i_ret <= 0 && i < 5);


	/*********Step 3:check READ-ID***********************/
	auc_i2c_write_buf[0] = 0x90;
	auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] =
			0x00;

	fts_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);

	if (reg_val[0] == 0x79 && reg_val[1] == 0x3)
		dev_dbg(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
			 reg_val[0], reg_val[1]);
	else
		return -EIO;

	auc_i2c_write_buf[0] = 0xcd;
	fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
	dev_dbg(&client->dev, "bootloader version = 0x%x\n", reg_val[0]);

	/*--------- read current project setting  ---------- */
	/*set read start address */
	buf[0] = 0x3;
	buf[1] = 0x0;
	buf[2] = 0x78;
	buf[3] = 0x0;

	fts_i2c_Read(client, buf, 4, buf, FTS_SETTING_BUF_LEN);
	dev_dbg(&client->dev, "[FTS] old setting: uc_i2c_addr = 0x%x,\
			uc_io_voltage = %d, uc_panel_factory_id = 0x%x\n",
			buf[0], buf[2], buf[4]);

	 /*--------- Step 4:erase project setting --------------*/
	auc_i2c_write_buf[0] = 0x63;
	fts_i2c_Write(client, auc_i2c_write_buf, 1);
	msleep(100);

	/*----------  Set new settings ---------------*/
	buf[0] = uc_i2c_addr;
	buf[1] = ~uc_i2c_addr;
	buf[2] = uc_io_voltage;
	buf[3] = ~uc_io_voltage;
	buf[4] = uc_panel_factory_id;
	buf[5] = ~uc_panel_factory_id;
	packet_buf[0] = 0xbf;
	packet_buf[1] = 0x00;
	packet_buf[2] = 0x78;
	packet_buf[3] = 0x0;
	packet_buf[4] = 0;
	packet_buf[5] = FTS_SETTING_BUF_LEN;

	for (i = 0; i < FTS_SETTING_BUF_LEN; i++)
		packet_buf[6 + i] = buf[i];

	fts_i2c_Write(client, packet_buf, FTS_SETTING_BUF_LEN + 6);
	msleep(100);

	/********* reset the new FW***********************/
	auc_i2c_write_buf[0] = 0x07;
	fts_i2c_Write(client, auc_i2c_write_buf, 1);

	msleep(200);
	return 0;
}

int fts_ctpm_auto_upgrade(struct i2c_client *client)
{
	u8 uc_host_fm_ver = FTS_REG_FW_VER;
	u8 uc_tp_fm_ver;
	int i_ret = 0; 	//modify by pangle for QW806 CTP 2014-09-09 

	fts_read_reg(client, FTS_REG_FW_VER, &uc_tp_fm_ver);
	uc_host_fm_ver = fts_ctpm_get_i_file_ver(client);

       printk(KERN_INFO "FT5x36:firmware version in tp mudule = 0x%x\n",uc_tp_fm_ver);
       printk(KERN_INFO "FT5x36:firmware version on host side = 0x%x\n", uc_host_fm_ver);
	   
	if (/*the firmware in touch panel maybe corrupted */
		uc_tp_fm_ver == FTS_REG_FW_VER ||
		/*the firmware in host flash is new, need upgrade */
	     uc_tp_fm_ver < uc_host_fm_ver
	    ) {
		msleep(100);
		dev_dbg(&client->dev, "[FTS] uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x\n",
				uc_tp_fm_ver, uc_host_fm_ver);
		i_ret = fts_ctpm_fw_upgrade_with_i_file(client);
		if (i_ret == 0)	{
			msleep(300);
			uc_host_fm_ver = fts_ctpm_get_i_file_ver(client);
			dev_dbg(&client->dev, "[FTS] upgrade to new version 0x%x\n",uc_host_fm_ver);
		} else {
			pr_err("[FTS] upgrade failed ret=%d.\n", i_ret);
			return -EIO;
		}
	}

	return 0;
}

/*
*get upgrade information depend on the ic type
*/
static void fts_get_upgrade_info(struct Upgrade_Info *upgrade_info,struct i2c_client *client)
{
	struct fts_ts_data *ft5x36_ts = i2c_get_clientdata(client);
	   
	switch (ft5x36_ts->pdata->family_id) {
	case IC_FT5X06:
		upgrade_info->delay_55 = FT5X06_UPGRADE_55_DELAY;
		upgrade_info->delay_aa = FT5X06_UPGRADE_AA_DELAY;
		upgrade_info->upgrade_id_1 = FT5X06_UPGRADE_ID_1;
		upgrade_info->upgrade_id_2 = FT5X06_UPGRADE_ID_2;
		upgrade_info->delay_readid = FT5X06_UPGRADE_READID_DELAY;
		upgrade_info->delay_earse_flash = FT5X06_UPGRADE_EARSE_DELAY;
		break;
	case IC_FT5606:
		upgrade_info->delay_55 = FT5606_UPGRADE_55_DELAY;
		upgrade_info->delay_aa = FT5606_UPGRADE_AA_DELAY;
		upgrade_info->upgrade_id_1 = FT5606_UPGRADE_ID_1;
		upgrade_info->upgrade_id_2 = FT5606_UPGRADE_ID_2;
		upgrade_info->delay_readid = FT5606_UPGRADE_READID_DELAY;
		upgrade_info->delay_earse_flash = FT5606_UPGRADE_EARSE_DELAY;
		break;
	case IC_FT5316:
		upgrade_info->delay_55 = FT5316_UPGRADE_55_DELAY;
		upgrade_info->delay_aa = FT5316_UPGRADE_AA_DELAY;
		upgrade_info->upgrade_id_1 = FT5316_UPGRADE_ID_1;
		upgrade_info->upgrade_id_2 = FT5316_UPGRADE_ID_2;
		upgrade_info->delay_readid = FT5316_UPGRADE_READID_DELAY;
		upgrade_info->delay_earse_flash = FT5316_UPGRADE_EARSE_DELAY;
		break;
	case IC_FT6208:
		upgrade_info->delay_55 = FT6208_UPGRADE_55_DELAY;
		upgrade_info->delay_aa = FT6208_UPGRADE_AA_DELAY;
		upgrade_info->upgrade_id_1 = FT6208_UPGRADE_ID_1;
		upgrade_info->upgrade_id_2 = FT6208_UPGRADE_ID_2;
		upgrade_info->delay_readid = FT6208_UPGRADE_READID_DELAY;
		upgrade_info->delay_earse_flash = FT6208_UPGRADE_EARSE_DELAY;
		break;
	case IC_FT6x06:
		upgrade_info->delay_55 = FT6X06_UPGRADE_55_DELAY;
		upgrade_info->delay_aa = FT6X06_UPGRADE_AA_DELAY;
		upgrade_info->upgrade_id_1 = FT6X06_UPGRADE_ID_1;
		upgrade_info->upgrade_id_2 = FT6X06_UPGRADE_ID_2;
		upgrade_info->delay_readid = FT6X06_UPGRADE_READID_DELAY;
		upgrade_info->delay_earse_flash = FT6X06_UPGRADE_EARSE_DELAY;
		break;
	case IC_FT5x06i:
		upgrade_info->delay_55 = FT5X06_UPGRADE_55_DELAY;
		upgrade_info->delay_aa = FT5X06_UPGRADE_AA_DELAY;
		upgrade_info->upgrade_id_1 = FT5X06_UPGRADE_ID_1;
		upgrade_info->upgrade_id_2 = FT5X06_UPGRADE_ID_2;
		upgrade_info->delay_readid = FT5X06_UPGRADE_READID_DELAY;
		upgrade_info->delay_earse_flash = FT5X06_UPGRADE_EARSE_DELAY;
		break;
	case IC_FT5x36:
		upgrade_info->delay_55 = FT5X36_UPGRADE_55_DELAY;
		upgrade_info->delay_aa = FT5X36_UPGRADE_AA_DELAY;
		upgrade_info->upgrade_id_1 = FT5X36_UPGRADE_ID_1;
		upgrade_info->upgrade_id_2 = FT5X36_UPGRADE_ID_2;
		upgrade_info->delay_readid = FT5X36_UPGRADE_READID_DELAY;
		upgrade_info->delay_earse_flash = FT5X36_UPGRADE_EARSE_DELAY;
		break;
//added by chenchen for focaltech FT6336 20140902 begin
	case IC_FT6x36:
		upgrade_info->delay_55 = FT6X36_UPGRADE_55_DELAY;
		upgrade_info->delay_aa = FT6X36_UPGRADE_AA_DELAY;
		upgrade_info->upgrade_id_1 = FT6X36_UPGRADE_ID_1;
		upgrade_info->upgrade_id_2 = FT6X36_UPGRADE_ID_2;
		upgrade_info->delay_readid = FT6X36_UPGRADE_READID_DELAY;
		upgrade_info->delay_earse_flash = FT6X36_UPGRADE_EARSE_DELAY;
		break;		
//added by chenchen for focaltech FT6336 20140902 end
	default:
		break;
	}
}

int fts_read_project_code(struct i2c_client * client, char * pProjectCode) {
	u8 reg_val[2] = {0};
	u32 i = 0;
	u32  j;
	u32  temp;
	u8 	packet_buf[4];
	u8  	auc_i2c_write_buf[10];
	//int      i_ret;
	u8 is_5336_new_bootloader = BL_VERSION_LZ4;
	struct Upgrade_Info upgradeinfo;
	struct fts_ts_data *ft5x36_ts = i2c_get_clientdata(client);

	fts_get_upgrade_info(&upgradeinfo,client);

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		msleep(100);
		printk(KERN_INFO "[FTS] Step 1:Reset	CTPM\n");
		/*********Step 1:Reset	CTPM *****/
		/*write 0xaa to register 0xfc */
		if ((ft5x36_ts->pdata->family_id==3) ||(ft5x36_ts->pdata->family_id==7))  //modify family_id by pangle at 2014.09.15
			fts_write_reg(client, 0xbc, FT_UPGRADE_AA);  //6x36 use 0xbc addr
		else
			fts_write_reg(client, 0xfc, FT_UPGRADE_AA);	 //5x36 use 0xfc addr
		//msleep(upgradeinfo.delay_aa);
		msleep(i*5+10);		//modify  for delay by pangle at 2014.09.15

		/*write 0x55 to register 0xfc */
		if((ft5x36_ts->pdata->family_id==3) ||(ft5x36_ts->pdata->family_id==7))   //modify family_id by pangle at 2014.09.15
			fts_write_reg(client, 0xbc, FT_UPGRADE_55);
		else
			fts_write_reg(client, 0xfc, FT_UPGRADE_55);

		//msleep(upgradeinfo.delay_55);
		msleep(i*5+10);		//modify  for delay by pangle at 2014.09.15
		/*********Step 2:Enter upgrade mode *****/
		printk(KERN_INFO "[FTS] Step 2:Enter upgrade mode \n");
		#if 0
			auc_i2c_write_buf[0] = FT_UPGRADE_55;
			auc_i2c_write_buf[1] = FT_UPGRADE_AA;
		
			fts_i2c_Write(client, auc_i2c_write_buf, 2);
		#else
			auc_i2c_write_buf[0] = FT_UPGRADE_55;
			fts_i2c_Write(client, auc_i2c_write_buf, 1);
			msleep(5);
			auc_i2c_write_buf[0] = FT_UPGRADE_AA;
			fts_i2c_Write(client, auc_i2c_write_buf, 1);
		#endif


		/*********Step 3:check READ-ID***********************/
		msleep(upgradeinfo.delay_readid);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] =0x00;
		fts_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);

		printk(KERN_INFO "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0], reg_val[1]);
		if (reg_val[0] == upgradeinfo.upgrade_id_1
			&& reg_val[1] == upgradeinfo.upgrade_id_2) {
			//dev_dbg(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
				//reg_val[0], reg_val[1]);
			DBG("[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0], reg_val[1]);
			break;
		} else {
			dev_err(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0], reg_val[1]);
		}
	}
	if (i >= FTS_UPGRADE_LOOP)
		return -EIO;

	auc_i2c_write_buf[0] = 0xcd;
	fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
	if ( ft5x36_ts->pdata->family_id==7 ) 	//modif by pangle at 20140923
	{
		if (reg_val[0] <= 4)
			is_5336_new_bootloader = BL_VERSION_LZ4 ;
		else if(reg_val[0] == 7)
			is_5336_new_bootloader = BL_VERSION_Z7 ;	
		else if(reg_val[0] >= 0x0f)
			is_5336_new_bootloader = BL_VERSION_GZF ;
	}
	DBG("bootloader version:%d\n", reg_val[0]);

	/*read project code*/
	packet_buf[0] = 0x03;
	packet_buf[1] = 0x00;
	for (j=0;j<33;j++)
	{   
//modify by chenchen for focaltech FT6336 20140902
		if (is_5336_new_bootloader == BL_VERSION_Z7 || is_5336_new_bootloader == BL_VERSION_GZF ||ft5x36_ts->pdata->family_id==7)
			temp = 0x07b4 + j;
		else
			temp = 0x7804 + j;
		packet_buf[2] = (u8)(temp>>8);
		packet_buf[3] = (u8)temp;

		fts_i2c_Read(client, packet_buf, sizeof(packet_buf), 
			pProjectCode+j, 1);
		if (*(pProjectCode+j) == '\0')
			break;
	}
	DBG("project code = 0x%x \n", pProjectCode[0]);	// modify by pangle at 20140923


	printk(KERN_INFO"[FTS] Step 7: reset the new FW\n");
	/*********Step 7: reset the new FW***********************/
	DBG("Step 7: reset the new FW\n");
	auc_i2c_write_buf[0] = 0x07;
	fts_i2c_Write(client, auc_i2c_write_buf, 1);
	msleep(300);	/*make sure CTP startup normally */

	return pProjectCode[0];
}


int fts_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf, u32 dw_lenth)
{
	u8 reg_val[2] = {0};
	u32 i = 0;
	u8 is_5336_new_bootloader = BL_VERSION_LZ4;
	u8 is_5336_fwsize_30 = 0;
	u32 packet_number;
	u32 j;
	u32 temp = 0;	//modify by pangle for QW806 CTP 2014-09-09 
	u32 lenght;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 auc_i2c_write_buf[10];
	u8 bt_ecc;
//added by chenchen for focaltech FT6336 20140902
	u32 fw_length;
	//int i_ret;
	struct Upgrade_Info upgradeinfo;
	struct fts_ts_data *ft5x36_ts = i2c_get_clientdata(client);	//add by zg
	
//added by chenchen for focaltech FT6336 20140902 begin
	if(pbt_buf[0] != 0x02 && ft5x36_ts->pdata->family_id == 7)
		{
			DBG("[FTS] FW first byte is not 0x02. so it is invalid \n");
			return -1;
		}

	if(dw_lenth > 0x11f && ft5x36_ts->pdata->family_id == 7)
		{
			fw_length = ((u32)pbt_buf[0x100]<<8) + pbt_buf[0x101];
			if(dw_lenth < fw_length)
			{
				DBG("[FTS] Fw length is invalid \n");
				return -1;
			}
		}
	if(dw_lenth <= 0x11f && ft5x36_ts->pdata->family_id == 7)
		{
			DBG("[FTS] Fw length is invalid \n");
			return -1;
		}
//added by chenchen for focaltech FT6336 20140902 end

	fts_get_upgrade_info(&upgradeinfo,client);
	
      if(ft5x36_ts->pdata->family_id==6)
      {
		is_5336_fwsize_30 = 0;
		if((*(pbt_buf+dw_lenth-12)) == 30)
		{
			is_5336_fwsize_30 = 1;
      		}
	
      }
	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
	  	msleep(100);
		printk(KERN_INFO "[FTS] Step 1:Reset  CTPM\n");
		/*********Step 1:Reset  CTPM *****/
		/*write ft5x36 0xaa to register 0xfc ,write ft6x06 0xaa to register 0xbc */
	printk(KERN_INFO"[FTS]Step 1: family id =%d\n",ft5x36_ts->pdata->family_id);
	 if(ft5x36_ts->pdata->family_id==6)
	 {
	 	printk(KERN_INFO "[FTS] Step 1:fts_write_reg  0xfc   CTPM\n");
		fts_write_reg(client, 0xfc, FT_UPGRADE_AA);
        //msleep(100);	
        msleep(i*5+20);  //modify  for delay by pangle at 2014.09.15
		fts_write_reg(client, 0xfc, FT_UPGRADE_55);
	 }
//modify by pangle for focaltech FT6x36 20140910
	 else if( ft5x36_ts->pdata->family_id==4 || ft5x36_ts->pdata->family_id==7)
	 {
	 	printk(KERN_INFO "[FTS] Step 1:fts_write_reg  0xbc  CTPM\n");
	 	fts_write_reg(client, 0xbc, FT_UPGRADE_AA);
        // msleep(100);
        msleep(i*5+20);    //modify  for delay by pangle at 2014.09.15
		fts_write_reg(client, 0xbc, FT_UPGRADE_55);
	 }
		msleep(20);
		
		/*********Step 2:Enter upgrade mode *****/
		printk(KERN_INFO "[FTS] Step 2:Enter upgrade mode \n");
		#if 1
			auc_i2c_write_buf[0] = FT_UPGRADE_55;
                    fts_i2c_Write(client, auc_i2c_write_buf, 1);
			msleep(10);		 
			auc_i2c_write_buf[0] = FT_UPGRADE_AA;
		       fts_i2c_Write(client, auc_i2c_write_buf, 1);
	    	//i_ret=fts_i2c_Write(client, auc_i2c_write_buf, 2);
		#else
			auc_i2c_write_buf[0] = FT_UPGRADE_55;
			fts_i2c_Write(client, auc_i2c_write_buf, 1);
			msleep(5);
			auc_i2c_write_buf[0] = FT_UPGRADE_AA;
			fts_i2c_Write(client, auc_i2c_write_buf, 1);
		#endif


		/*********Step 3:check READ-ID***********************/
			msleep(50);		//add by zg for debug
			//msleep(upgradeinfo.delay_readid);
			auc_i2c_write_buf[0] = 0x90;
			auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] =0x00;
		
		fts_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);

		printk(KERN_INFO "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0], reg_val[1]);
		if (reg_val[0] == upgradeinfo.upgrade_id_1
			&& reg_val[1] == upgradeinfo.upgrade_id_2) {
			//dev_dbg(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
				//reg_val[0], reg_val[1]);
			DBG("[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
				reg_val[0], reg_val[1]);
			break;
		} else {
			dev_err(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
				reg_val[0], reg_val[1]);
		}
	}
	if (i >= FTS_UPGRADE_LOOP)
		return -EIO;
	
//added by chenchen for focaltech FT6336 20140902 begin
	if(ft5x36_ts->pdata->family_id == 7)
		{
			auc_i2c_write_buf[0] = 0x90;
			auc_i2c_write_buf[1] = 0x00;
			auc_i2c_write_buf[2] = 0x00;
			auc_i2c_write_buf[3] = 0x00;
			auc_i2c_write_buf[4] = 0x00;
			fts_i2c_Write(client, auc_i2c_write_buf, 5);
		}
	else{
			auc_i2c_write_buf[0] = 0xcd;
			fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
		}
//added by chenchen for focaltech FT6336 20140902 end
	
	if (ft5x36_ts->pdata->family_id==6)
	{
		if (reg_val[0] <= 4)
			is_5336_new_bootloader = BL_VERSION_LZ4 ;
		else if(reg_val[0] == 7)
			is_5336_new_bootloader = BL_VERSION_Z7 ;	
		else if(reg_val[0] >= 0x0f)
			is_5336_new_bootloader = BL_VERSION_GZF ;
	}
	DBG("bootloader version:%d\n", reg_val[0]);

	/*Step 4:erase app and panel paramenter area*/
	DBG("Step 4:erase app and panel paramenter area\n");
	auc_i2c_write_buf[0] = 0x61;
	fts_i2c_Write(client, auc_i2c_write_buf, 1);	/*erase app area */
	msleep(upgradeinfo.delay_earse_flash);
//added by chenchen for focaltech FT6336 20140902 begin
	if(ft5x36_ts->pdata->family_id == 7)
		{
			for(i = 0;i < 200;i++)
			{
				auc_i2c_write_buf[0] = 0x6a;
				auc_i2c_write_buf[1] = 0x00;
				auc_i2c_write_buf[2] = 0x00;
				auc_i2c_write_buf[3] = 0x00;
				reg_val[0] = 0x00;
				reg_val[1] = 0x00;
				fts_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);
				if(0xb0 == reg_val[0] && 0x02 == reg_val[1])
				{
					DBG("[FTS] erase app finished \n");
					break;
				}
				msleep(50);
			}
		}
//added by chenchen for focaltech FT6336 20140902 end

	if(is_5336_fwsize_30)
	{
		 auc_i2c_write_buf[0] = 0x63;
		 fts_i2c_Write(client, auc_i2c_write_buf, 1); /*erase app area*/	
   		 msleep(50);
	}

	printk(KERN_INFO "[FTS] Step 5:write firmware(FW) to ctpm flash\n");
	/*********Step 5:write firmware(FW) to ctpm flash*********/
	bt_ecc = 0;
	DBG("Step 5:write firmware(FW) to ctpm flash\n");
//added by chenchen for focaltech FT6336 20140902 begin
	
	if(ft5x36_ts->pdata->family_id == 7)
		{
			dw_lenth = fw_length;
		}
	else{	
			dw_lenth = dw_lenth - 8;
		}
//added by chenchen for focaltech FT6336 20140902 end
	
	if(is_5336_new_bootloader == BL_VERSION_GZF) 
		dw_lenth = dw_lenth - 6;
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
	packet_buf[0] = 0xbf;
	packet_buf[1] = 0x00;

	for (j = 0; j < packet_number; j++) {
		temp = j * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		lenght = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (lenght >> 8);
		packet_buf[5] = (u8) lenght;

		for (i = 0; i < FTS_PACKET_LENGTH; i++) {
			packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		
		fts_i2c_Write(client, packet_buf, FTS_PACKET_LENGTH + 6);
//added by chenchen for focaltech FT6336 20140902 begin
	if(ft5x36_ts->pdata->family_id == 7)
		{
			for(i = 0;i < 30;i++)
			{
				auc_i2c_write_buf[0] = 0x6a;
				auc_i2c_write_buf[1] = 0x00;
				auc_i2c_write_buf[2] = 0x00;
				auc_i2c_write_buf[3] = 0x00;
				reg_val[0] = 0x00;
				reg_val[1] = 0x00;
				fts_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);
				if(0xb0 == (reg_val[0] & 0xf0) && (0x03 + (j % 0x0ffd)) == (((reg_val[0] & 0x0f) << 8) |reg_val[1]))
				{
					DBG("[FTS] write a block data finished \n");
					break;
				}
				msleep(1);
			}
		}
	else{
			msleep(FTS_PACKET_LENGTH / 6 + 1);
		}
//added by chenchen for focaltech FT6336 20140902 end
		if((((j+1) * FTS_PACKET_LENGTH)%1024)==0)
		DBG("write bytes:0x%04x\n", (j+1) * FTS_PACKET_LENGTH);
		//delay_qt_ms(FTS_PACKET_LENGTH / 6 + 1);
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0) {
		temp = packet_number * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		temp = (dw_lenth) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (temp >> 8);
		packet_buf[5] = (u8) temp;

		for (i = 0; i < temp; i++) {
			packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}

		fts_i2c_Write(client, packet_buf, temp + 6);
//added by chenchen for focaltech FT6336 20140902 begin
	if(ft5x36_ts->pdata->family_id == 7)
		{
			for(i = 0;i < 30;i++)
			{
				auc_i2c_write_buf[0] = 0x6a;
				auc_i2c_write_buf[1] = 0x00;
				auc_i2c_write_buf[2] = 0x00;
				auc_i2c_write_buf[3] = 0x00;
				reg_val[0] = 0x00;
				reg_val[1] = 0x00;
				fts_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);
				if(0xb0 == (reg_val[0] & 0xf0) && (0x03 + (j % 0x0ffd)) == (((reg_val[0] & 0x0f) << 8) |reg_val[1]))
				{
					DBG("[FTS] write a block data finished \n");
					break;
				}
				msleep(1);
			}
		}
	else{
			msleep(20);
		}
//added by chenchen for focaltech FT6336 20140902 end
	}


	/*send the last six byte */
//modify by chenchen for focaltech FT6336 20140902 begin
		if(ft5x36_ts->pdata->family_id != 7)
		{
		if(is_5336_new_bootloader == BL_VERSION_LZ4 || is_5336_new_bootloader == BL_VERSION_Z7 )
		{
			for (i = 0; i < 6; i++) {
				if ((is_5336_new_bootloader == BL_VERSION_Z7) &&(ft5x36_ts->pdata->family_id==6)) 
					temp = 0x7bfa + i;
				else
					temp = 0x6ffa + i;
				packet_buf[2] = (u8) (temp >> 8);
				packet_buf[3] = (u8) temp;
				temp = 1;
				packet_buf[4] = (u8) (temp >> 8);
				packet_buf[5] = (u8) temp;
				packet_buf[6] = pbt_buf[dw_lenth + i];
				bt_ecc ^= packet_buf[6];
				fts_i2c_Write(client, packet_buf, 7);
				msleep(20);
			}
		}
		else if(is_5336_new_bootloader == BL_VERSION_GZF)
		{
			for (i = 0; i<12; i++)
			{
				if (is_5336_fwsize_30 && (ft5x36_ts->pdata->family_id==6)) 
				{
					temp = 0x7ff4 + i;
				}
				else if (ft5x36_ts->pdata->family_id==6) 
				{
					temp = 0x7bf4 + i;
				}
				packet_buf[2] = (u8)(temp>>8);
				packet_buf[3] = (u8)temp;
				temp =1;
				packet_buf[4] = (u8)(temp>>8);
				packet_buf[5] = (u8)temp;
				packet_buf[6] = pbt_buf[ dw_lenth + i]; 
				bt_ecc ^= packet_buf[6];
	  
				fts_i2c_Write(client, packet_buf, 7);
				msleep(20);

			}
		}
	}
//modify by chenchen for focaltech FT6336 20140902 end
	printk(KERN_INFO "[FTS] Step 6: read out checksum\n");
	/*********Step 6: read out checksum***********************/
	/*send the opration head */
	DBG("Step 6: read out checksum\n");
	auc_i2c_write_buf[0] = 0xcc;
	fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
	if (reg_val[0] != bt_ecc) {
		dev_err(&client->dev, "[FTS]--ecc error! FW=%02x bt_ecc=%02x\n",reg_val[0],	bt_ecc);
		return -EIO;
	}

	printk(KERN_INFO "[FTS] Step 7: reset the new FW\n");
	/*********Step 7: reset the new FW***********************/
	DBG("Step 7: reset the new FW\n");
	auc_i2c_write_buf[0] = 0x07;
	fts_i2c_Write(client, auc_i2c_write_buf, 1);
	msleep(300);	/*make sure CTP startup normally */

	return 0;
}

/*sysfs debug*/

/*
*get firmware size

@firmware_name:firmware name
*note:the firmware default path is sdcard.
	if you want to change the dir, please modify by yourself.
*/
static int fts_GetFirmwareSize(char *firmware_name)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize = 0;
	char filepath[128];
	memset(filepath, 0, sizeof(filepath));

	sprintf(filepath, "%s", firmware_name);

	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);

	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	filp_close(pfile, NULL);
	return fsize;
}



/*
*read firmware buf for .bin file.

@firmware_name: fireware name
@firmware_buf: data buf of fireware

note:the firmware default path is sdcard.
	if you want to change the dir, please modify by yourself.
*/
static int fts_ReadFirmware(char *firmware_name, unsigned char *firmware_buf)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize;
	char filepath[128];
	loff_t pos;
	mm_segment_t old_fs;

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s", firmware_name);
	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
	}

	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, firmware_buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);

	return 0;
}



/*
upgrade with *.bin file
*/

int fts_ctpm_fw_upgrade_with_app_file(struct i2c_client *client, char *firmware_name)
{
	u8 *pbt_buf = NULL;
	int i_ret=0;
	int fwsize = fts_GetFirmwareSize(firmware_name);

       struct fts_ts_data *ft5x36_ts = i2c_get_clientdata(client);

	if (fwsize <= 0) {
		dev_err(&client->dev, "%s ERROR:Get firmware size failed\n", __func__);
		return -EIO;
	}

	if (fwsize < 8 || fwsize > 32 * 1024) {
		dev_dbg(&client->dev, "%s:FW length error\n", __func__);
		return -EIO;
	}

	/*=========FW upgrade========================*/
	pbt_buf = kmalloc(fwsize + 1, GFP_ATOMIC);

	if (fts_ReadFirmware(firmware_name, pbt_buf)) {
		dev_err(&client->dev, "%s() - ERROR: request_firmware failed\n",
					__func__);
		//kfree(pbt_buf);
		//return -EIO;
		i_ret = -EIO;
		goto err_ret;
	}
	
	if ((pbt_buf[fwsize - 8] ^ pbt_buf[fwsize - 6]) == 0xFF
		&& (pbt_buf[fwsize - 7] ^ pbt_buf[fwsize - 5]) == 0xFF
		&& (pbt_buf[fwsize - 3] ^ pbt_buf[fwsize - 4]) == 0xFF) {
		/*call the upgrade function */
		i_ret = fts_ctpm_fw_upgrade(client, pbt_buf, fwsize);
		if (i_ret != 0)
			dev_err(&client->dev, "%s() - ERROR:[FTS] upgrade failed..\n",__func__);
		else {
                 if(ft5x36_ts->pdata->auto_clb)
			fts_ctpm_auto_clb(client);
		 }
	} else {
		dev_dbg(&client->dev, "%s:FW format error\n", __func__);
	}

err_ret:	
	kfree(pbt_buf);
    
	return i_ret;
}

static ssize_t fts_tpfwver_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	ssize_t num_read_chars = 0;
	u8 fwver = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	mutex_lock(&g_device_mutex);

	if (fts_read_reg(client, FTS_REG_FW_VER, &fwver) < 0)
		num_read_chars = snprintf(buf, PAGE_SIZE, "get tp fw version fail!\n");
	else
		num_read_chars = snprintf(buf, PAGE_SIZE, "%02X\n", fwver);

	mutex_unlock(&g_device_mutex);

	return num_read_chars;
}

static ssize_t fts_tpfwver_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	/*place holder for future use*/
	return -EPERM;
}



static ssize_t fts_tprwreg_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	/*place holder for future use*/
	return -EPERM;
}

static ssize_t fts_tprwreg_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	ssize_t num_read_chars = 0;
	int retval;
	long unsigned int wmreg = 0;
	u8 regaddr = 0xff, regvalue = 0xff;
	u8 valbuf[5] = {0};

	memset(valbuf, 0, sizeof(valbuf));
	mutex_lock(&g_device_mutex);
	num_read_chars = count - 1;

	if (num_read_chars != 2) {
		if (num_read_chars != 4) {
			pr_info("please input 2 or 4 character\n");
			goto error_return;
		}
	}

	memcpy(valbuf, buf, num_read_chars);
	retval = strict_strtoul(valbuf, 16, &wmreg);

	if (0 != retval) {
		dev_err(&client->dev, "%s() - ERROR: Could not convert the "\
						"given input to a number." \
						"The given input was: \"%s\"\n",
						__func__, buf);
		goto error_return;
	}

	if (2 == num_read_chars) {
		/*read register*/
		regaddr = wmreg;
		if (fts_read_reg(client, regaddr, &regvalue) < 0)
			dev_err(&client->dev, "Could not read the register(0x%02x)\n",
						regaddr);
		else
			pr_info("the register(0x%02x) is 0x%02x\n",
					regaddr, regvalue);
	} else {
		regaddr = wmreg >> 8;
		regvalue = wmreg;
		if (fts_write_reg(client, regaddr, regvalue) < 0)
			dev_err(&client->dev, "Could not write the register(0x%02x)\n",
							regaddr);
		else
			dev_err(&client->dev, "Write 0x%02x into register(0x%02x) successful\n",
							regvalue, regaddr);
	}

error_return:
	mutex_unlock(&g_device_mutex);

	return count;
}

static ssize_t fts_fwupdate_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	/* place holder for future use */
	return -EPERM;
}

/*upgrade from *.i*/
static ssize_t fts_fwupdate_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	u8 uc_host_fm_ver;
	int i_ret;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	struct fts_ts_data *data = i2c_get_clientdata(client);

	mutex_lock(&g_device_mutex);

	fts_disable_irq(data);
	i_ret = fts_ctpm_fw_upgrade_with_i_file(client);
	if (i_ret == 0) {
		msleep(300);
		uc_host_fm_ver = fts_ctpm_get_i_file_ver(client);
		pr_info("%s [FTS] upgrade to new version 0x%x\n", __func__,
					 uc_host_fm_ver);
	} else
		dev_err(&client->dev, "%s ERROR:[FTS] upgrade failed.\n",
					__func__);

	fts_enable_irq(data);
	mutex_unlock(&g_device_mutex);

	return count;
}

static ssize_t fts_fwupgradeapp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	/*place holder for future use*/
	return -EPERM;
}


/*upgrade from app.bin*/
static ssize_t fts_fwupgradeapp_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	ssize_t num_read_chars = 0;
	char fwname[128];
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct fts_ts_data *data = i2c_get_clientdata(client);
	char * info="";

	memset(fwname, 0, sizeof(fwname));
	sprintf(fwname, "%s", buf);
	fwname[count - 1] = '\0';

	mutex_lock(&g_device_mutex);
	fts_disable_irq(data);

	if(0==fts_ctpm_fw_upgrade_with_app_file(client, fwname))
	{
		num_read_chars = snprintf(info, PAGE_SIZE, "%s\n", "FTP firmware upgrade success!\n");
	}
	else
	{
		num_read_chars = snprintf(info, PAGE_SIZE, "%s\n", "FTP firmware upgrade fail!\n");	
	}

	buf=info;

	fts_enable_irq(data);
	mutex_unlock(&g_device_mutex);

	return num_read_chars;
}

static ssize_t fts_ftsgetprojectcode_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	ssize_t num_read_chars = 0;
	char projectcode[40];   // modify for SW00081304 by pangle at 20140923 
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	memset(projectcode, 0, sizeof(projectcode));
	mutex_lock(&g_device_mutex);
	if(fts_read_project_code(client, projectcode) < 0)
		num_read_chars = snprintf(buf, PAGE_SIZE, "get projcet code fail!\n");
	else
		num_read_chars = snprintf(buf, PAGE_SIZE, "projcet code = 0x%x\n", projectcode[0]);  // modify by pangle at 20140923

	mutex_unlock(&g_device_mutex);
	return num_read_chars;
}

static ssize_t fts_ftsgetprojectcode_store(struct device *dev,
									struct device_attribute *attr,
									const char *buf, size_t count)
{
	/* place holder for future use */
    return -EPERM;
}
//added by pangle for gesture 20140917 begin
static ssize_t fts_gesture_show(struct device *dev,struct device_attribute *attr, char *buf)
{
    	ssize_t num_read_chars = 0;
	mutex_lock(&g_device_mutex);

	num_read_chars = snprintf(buf,PAGE_SIZE, "%d\n", gesture_enable);

	mutex_unlock(&g_device_mutex);

	return num_read_chars;			
}

static ssize_t fts_gesture_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	unsigned long  val = 0;
	mutex_lock(&g_device_mutex);
	val = simple_strtoul(buf, NULL, 10);;
	if(val)
		gesture_enable=true;
	else
		gesture_enable=false;
	mutex_unlock(&g_device_mutex);
 	return -EPERM;
}
//added by pangle for gesture 20140917 end

/*sysfs */
/*get the fw version
*example:cat ftstpfwver
*/
static DEVICE_ATTR(ftstpfwver, S_IRUGO | S_IWUSR, fts_tpfwver_show,	fts_tpfwver_store);

/*upgrade from *.i
*example: echo 1 > ftsfwupdate
*/
static DEVICE_ATTR(ftsfwupdate, S_IRUGO | S_IWUSR, fts_fwupdate_show, fts_fwupdate_store);

/*read and write register
*read example: echo 88 > ftstprwreg ---read register 0x88
*write example:echo 8807 > ftstprwreg ---write 0x07 into register 0x88
*
*note:the number of input must be 2 or 4.if it not enough,please fill in the 0.
*/
static DEVICE_ATTR(ftstprwreg, S_IRUGO | S_IWUSR, fts_tprwreg_show,	fts_tprwreg_store);


/*upgrade from app.bin
*example:echo "*_app.bin" > ftsfwupgradeapp
*/
static DEVICE_ATTR(ftsfwupgradeapp, S_IRUGO | S_IWUSR, fts_fwupgradeapp_show, fts_fwupgradeapp_store);
/*show project code
*example:cat ftsgetprojectcode
*/
static DEVICE_ATTR(ftsgetprojectcode, S_IRUGO|S_IWUSR, fts_ftsgetprojectcode_show, fts_ftsgetprojectcode_store);

//added by pangle for gesture 20140917
static DEVICE_ATTR(ftsgestureenable, S_IRUGO|S_IWUSR, fts_gesture_show, fts_gesture_store);

/*add your attr in here*/
static struct attribute *fts_attributes[] = {
	&dev_attr_ftstpfwver.attr,
	&dev_attr_ftsfwupdate.attr,
	&dev_attr_ftstprwreg.attr,
	&dev_attr_ftsfwupgradeapp.attr,
	&dev_attr_ftsgetprojectcode.attr,
	NULL
};

static struct attribute_group fts_attribute_group = {
	.attrs = fts_attributes
};

//added by pangle for gesture 20140917 begin
static struct attribute *fts_gesture[] = {
	&dev_attr_ftsgestureenable.attr,
	NULL
};
static struct attribute_group fts_gesture_group = {
	.attrs = fts_gesture
};
//added by pangle for gesture 20140917 end

//added  public sys node for gesture switch 20140918 begin
static ssize_t gestureenable_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	//char *s = buf;

	ssize_t num_read_chars = 0;
	mutex_lock(&g_device_mutex);

	num_read_chars = snprintf(buf,PAGE_SIZE, "%d\n", gesture_enable);

	mutex_unlock(&g_device_mutex);

	return num_read_chars;	

}

static ssize_t gestureenable_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	unsigned long  val = 0;
	mutex_lock(&g_device_mutex);
	val = simple_strtoul(buf, NULL, 10);;
	if(val)
		gesture_enable=true;
	else
		gesture_enable=false;
	mutex_unlock(&g_device_mutex);
 	return -EPERM;
}


struct kobject *tpswitch_ctrl_kobj;

//change Permission of gestureenable by pangle at 20140925
#define tpswitch_ctrl_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0444,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}


tpswitch_ctrl_attr(gestureenable);

static struct attribute *g_attr[] = {
	&gestureenable_attr.attr,	
	NULL,
};

static struct attribute_group tpswitch_attr_group = {
	.attrs = g_attr,
};

static int tpswitch_sysfs_init(void)
{ 
	tpswitch_ctrl_kobj = kobject_create_and_add("tp_gesture", NULL);
	if (!tpswitch_ctrl_kobj)
		return -ENOMEM;

	return sysfs_create_group(tpswitch_ctrl_kobj, &tpswitch_attr_group);
}

//static struct attribute_group fts_attribute_group = {
//	.attrs = fts_attributes
//};

static void tpswitch_sysfs_exit(void)
{
	sysfs_remove_group(tpswitch_ctrl_kobj, &tpswitch_attr_group);

	kobject_put(tpswitch_ctrl_kobj);
}

//added  public sys node for gesture switch 20140918 end
// in /sys/tp_gesture/gestureenable

/*create sysfs for debug*/
int fts_create_sysfs(struct i2c_client *client)
{
	int err;
//added by pangle for gesture 20140917 begin
	if(gesture_enable_dtsi){
		err = sysfs_create_group(&client->dev.kobj, &fts_gesture_group);
		tpswitch_sysfs_init();	//add by pangle for public sys node for gesture switch 20140918
		if ( 0 != err ) {
			dev_err(&client->dev,
					 "%s() - ERROR: sysfs_create_gesture() failed.\n",
					 __func__);
			sysfs_remove_group(&client->dev.kobj, &fts_gesture_group);
		} else 
			pr_info("FocalTech:%s() - sysfs_create_gesture() succeeded.\n",
				__func__);
	}
//added by pangle for gesture 20140917 end
	err = sysfs_create_group(&client->dev.kobj, &fts_attribute_group);
	if (0 != err) {
		dev_err(&client->dev,
					 "%s() - ERROR: sysfs_create_group() failed.\n",
					 __func__);
		sysfs_remove_group(&client->dev.kobj, &fts_attribute_group);
		tpswitch_sysfs_exit();	//add by pangle for public sys node for gesture switch 20140918
		return -EIO;
	} else {
		mutex_init(&g_device_mutex);
		pr_info("FocalTech:%s() - sysfs_create_group() succeeded.\n",
				__func__);
	}
	return err;
}

void fts_release_sysfs(struct i2c_client *client)
{
	sysfs_remove_group(&client->dev.kobj, &fts_attribute_group);
//added by pangle for gesture	20140917
	sysfs_remove_group(&client->dev.kobj, &fts_gesture_group);
	mutex_destroy(&g_device_mutex);
}

/*create apk debug channel*/
#ifdef FTS_APK_DEBUG
#define PROC_UPGRADE			0
#define PROC_READ_REGISTER		1
#define PROC_WRITE_REGISTER	    2
#define PROC_RAWDATA			3
#define PROC_AUTOCLB			4
#define PROC_UPGRADE_INFO 		5
#define PROC_WRITE_DATA               6
#define PROC_READ_DATA                 7
//added by shihuijun for gesture display 20150113 begin
#define PROC_READ_GESTRUEX		8
#define PROC_READ_GESTRUEY			9
#define PROC_READ_DRAWDATA                     10
//added by shihuijun for gesture display 20150113 end

#define PROC_NAME	"ft5x0x-debug"
static unsigned char proc_operate_mode = PROC_UPGRADE;
static struct proc_dir_entry *fts_proc_entry;
static int g_upgrade_successful = 0;
/*interface of write proc*/
//change function argument for debug write by pangle at 2014.10.29
ssize_t fts_debug_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
	struct i2c_client *client = (struct i2c_client *)fts_proc_entry->data;
	struct fts_ts_data *data = i2c_get_clientdata(client);
	unsigned char writebuf[FTS_PACKET_LENGTH];
	int buflen = len;
	int writelen = 0;
	int ret = 0;
	
	if (copy_from_user(&writebuf, buff, buflen)) {
		dev_err(&client->dev, "%s:copy from user error\n", __func__);
		return -EFAULT;
	}
	proc_operate_mode = writebuf[0];
	
	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		{
			char upgrade_file_path[128];
			memset(upgrade_file_path, 0, sizeof(upgrade_file_path));
			sprintf(upgrade_file_path, "%s", writebuf + 1);
			upgrade_file_path[buflen-1] = '\0';
			DBG("%s\n", upgrade_file_path);
			fts_disable_irq(data);

			g_upgrade_successful = 0;
			ret = fts_ctpm_fw_upgrade_with_app_file(client, upgrade_file_path);

			fts_enable_irq(data);
			if (ret < 0) {
				dev_err(&client->dev, "%s:upgrade failed.\n", __func__);
				return ret;
			}
			else
			{
				g_upgrade_successful = 1;
			}
		}
		break;
	case PROC_READ_REGISTER:
		writelen = 1;
		DBG("%s:register addr=0x%02x\n", __func__, writebuf[1]);
		ret = fts_i2c_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_WRITE_REGISTER:
		writelen = 2;
		ret = fts_i2c_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_RAWDATA:
		break;
	case PROC_AUTOCLB:
		fts_ctpm_auto_clb(client);
		break;
	case PROC_UPGRADE_INFO:
		break;
//added by pangle for gesture display 20141031 begin
	case PROC_READ_GESTRUEX:
		break;
	case PROC_READ_GESTRUEY:
		break;	
//added by pangle for gesture display 20141031 end
	case PROC_READ_DATA:
       case PROC_WRITE_DATA:
           writelen = len - 1;
           ret = fts_i2c_Write(client, writebuf + 1, writelen);
           if (ret < 0) {
                 dev_err(&client->dev, "%s:write iic error\n", __func__);
                 return ret;
           }
           break;

	default:
		break;
	}
	

	return len;
}

/*interface of read proc*/
/*modified for create  focal proc by zengguang 2014.6.26*/
//change function argument for debug write by pangle at 2014.10.29
static ssize_t fts_debug_read(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	struct i2c_client *client = (struct i2c_client *)fts_proc_entry->data;

	int ret = 0;
	//u8 tx = 0, rx = 0;
	//int i, j;
//added by shihuijun for gesture display 20141223  
	//unsigned char readbuf[PAGE_SIZE];
	int i = 0;
	unsigned char buf[1016];
	int num_read_chars = 0;
	int readlen = 0;
	u8 regvalue = 0x00, regaddr = 0x00;
	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		/*after calling fts_debug_write to upgrade*/
		regaddr = 0xA6;
		ret = fts_read_reg(client, regaddr, &regvalue);
		if (ret < 0)
			num_read_chars = sprintf(buf, "%s", "get fw version failed.\n");
		else
			num_read_chars = sprintf(buf, "current fw version:0x%02x\n", regvalue);
		break;
	case PROC_READ_REGISTER:
		readlen = 1;
		ret = fts_i2c_Read(client, NULL, 0, buf, readlen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:read iic error\n", __func__);
			return ret;
		} else
			DBG("%s:value=0x%02x\n", __func__, buf[0]);
		num_read_chars = 1;
		break;
	case PROC_RAWDATA:
		break;
	case PROC_UPGRADE_INFO:
		if(1 == g_upgrade_successful)
			buf[0] = 1;
		else
			buf[0] = 0;
		DBG("%s:value=0x%02x\n", __func__, buf[0]);
		num_read_chars = 1;
		break;
	case PROC_READ_DATA:
           readlen = size;		//modify for create  focal proc by pangle 2014.10.29
           ret = fts_i2c_Read(client, NULL, 0, buf, readlen);
           if (ret < 0) {
                 dev_err(&client->dev, "%s:read iic error\n", __func__);
                 return ret;
           }
           
           num_read_chars = readlen;
           break;
/*//delete by shihuijun for gesture display 20150115 begin
	case PROC_READ_GESTRUEX:
		buf[0] = pointnum;
		for(i = 0;i < pointnum;i++)
		{
			buf[1 + 2 * i] = (char)((coordinate_x[i] >> 8) & 0xff);
			buf[2 + 2 * i] = (char)(coordinate_x[i] & 0xff);
		}
		num_read_chars = (1 + 2 * pointnum);
		break;
	case PROC_READ_GESTRUEY:
		buf[0] = pointnum;
		for(i = 0;i < pointnum;i++)
		{
			buf[1 + 2 * i] = (char)((coordinate_y[i] >> 8) & 0xff);
			buf[2 + 2 * i] = (char)(coordinate_y[i] & 0xff);
		}
		num_read_chars = (1 + 2 * pointnum);
		break;		   
//delete by shihuijun for gesture display 20150115 end*/
      case PROC_WRITE_DATA:
           break;
 //add  by shihuijun for gesture display 20150115 begin
	  case PROC_READ_DRAWDATA:
	       buf[0] = gesture_id;
		   //printk(KERN_ERR"readbuf[0]= %x\n",buf[0]);
			for(i = 0;i < 28;i++)
			   {
	            buf[1 + i] = buf_gesture[i];
				//printk(KERN_ERR"buf[%d]= %x\n",i,buf[i]);
	            }
				num_read_chars = 29;
	   //num_read_chars = (1 + 2 * pointnum);			 
	  	   break;
 //add by shihuijun for gesture display 20150115 begin

	default:
		break;
	}
	
	memcpy(page, buf, num_read_chars);

	return num_read_chars;
}
/*modified for create  focal proc by pangle 2014.10.29 start*/
static const struct file_operations focal_tool_ops = {
    .owner = THIS_MODULE,
    .read = fts_debug_read,
    .write = fts_debug_write,
};
/*modified for create  focal proc by pangle 2014.10.29 end*/
int fts_create_apk_debug_channel(struct i2c_client * client)
{
#ifdef TPD_AUTO_UPGRADE
	u8 uc_host_fm_ver;
	int i_ret;
	u8 uc_host_fm_ver1 = FTS_REG_FW_VER;
	u8 uc_tp_fm_ver;
#endif

    struct fts_ts_data *ft5x36_ts = i2c_get_clientdata(client);
    printk(KERN_ERR "ENTER %s\n",__func__);


//       ft5x36_ts = i2c_get_clientdata(client);

	printk(KERN_ERR "FT5X36:IC  family id is 0x%x\n",ft5x36_ts->pdata->family_id);
	printk("pangle trace 00000\n");
/*modified for create  focal proc by pangle 2014.10.29 start*/
	fts_proc_entry = proc_create(PROC_NAME, 0666,NULL, &focal_tool_ops);
	if (NULL == fts_proc_entry) {
		dev_err(&client->dev, "Couldn't create proc entry!\n");
		return -ENOMEM;
	} else {
		dev_info(&client->dev, "Create proc entry success!\n");
		fts_proc_entry->data = client;
		//fts_proc_entry->write_proc = fts_debug_write;
		//fts_proc_entry->read_proc = fts_debug_read;
	}
/*modified for create  focal proc by pangle 2014.10.29 end*/
#ifdef TPD_AUTO_UPGRADE

	fts_read_reg(client, FTS_REG_FW_VER, &uc_tp_fm_ver);
	uc_host_fm_ver1 = fts_ctpm_get_i_file_ver(client);
	
       printk(KERN_INFO "FT5x36:firmware version in tp mudule = 0x%x\n",uc_tp_fm_ver);
       printk(KERN_INFO "FT5x36:firmware version on host side = 0x%x\n", uc_host_fm_ver1);

	 //uc_host_fm_ver1=0xb;  
	if (uc_tp_fm_ver < uc_host_fm_ver1)
	{ 
		mutex_lock(&g_device_mutex);

		fts_disable_irq(ft5x36_ts);
		i_ret = fts_ctpm_fw_upgrade_with_i_file(client);
		if (i_ret == 0) 
		{
			msleep(300);
			uc_host_fm_ver = fts_ctpm_get_i_file_ver(client);
			pr_info("%s [FTS] upgrade to new version 0x%x\n", __func__,
					 uc_host_fm_ver);
		} 
		else
			dev_err(&client->dev, "%s ERROR:[FTS] upgrade failed.\n",
					__func__);

		fts_enable_irq(ft5x36_ts);
		mutex_unlock(&g_device_mutex);
	}
#endif
	return 0;
}

void fts_release_apk_debug_channel(void)
{
	if (fts_proc_entry)
		remove_proc_entry(PROC_NAME, NULL);
}
#endif
