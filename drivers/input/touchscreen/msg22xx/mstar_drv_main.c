////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006-2012 MStar Semiconductor, Inc.
// All rights reserved.
//
// Unless otherwise stipulated in writing, any and all information contained
// herein regardless in any format shall remain the sole proprietary of
// MStar Semiconductor Inc. and be kept in strict confidence
// (??MStar Confidential Information??) by the recipient.
// Any unauthorized act including without limitation unauthorized disclosure,
// copying, use, reproduction, sale, distribution, modification, disassembling,
// reverse engineering and compiling of the contents of MStar Confidential
// Information is unlawful and strictly prohibited. MStar hereby reserves the
// rights to any and all damages, losses, costs and expenses resulting therefrom.
//
////////////////////////////////////////////////////////////////////////////////

/**
 *
 * @file    mstar_drv_main.c
 *
 * @brief   This file defines the interface of touch screen
 *
 * @version v2.2.0.0
 *
 */

/*=============================================================*/
// INCLUDE FILE
/*=============================================================*/

#include "mstar_drv_main.h"
#include "mstar_drv_utility_adaption.h"
#include "mstar_drv_platform_porting_layer.h"
#include "mstar_drv_ic_fw_porting_layer.h"

/*=============================================================*/
// CONSTANT VALUE DEFINITION
/*=============================================================*/


/*=============================================================*/
// EXTERN VARIABLE DECLARATION
/*=============================================================*/

#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
extern FirmwareInfo_t g_FirmwareInfo;
extern u8 *g_LogModePacket;
extern u16 g_FirmwareMode;
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG

//modify by pangle for msg22xx add hardware info at 20150321 begin
#ifdef CONFIG_GET_HARDWARE_INFO
#include <mach/hardware_info.h>
static char tmp_str[50];
#endif
//modify by pangle for msg22xx add hardware info at 20150321 end

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
extern u16 g_GestureWakeupMode;
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
extern u32 g_LogGestureInfor[GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH];
#define PROC_NAME	"ft5x0x-debug"
#endif //CONFIG_ENABLE_GESTURE_INFORMATION_MODE
#endif //CONFIG_ENABLE_GESTURE_WAKEUP

extern u8 g_ChipType;
//merged by pangle at 20150228 begin
#ifdef CONFIG_ENABLE_ITO_MP_TEST
#if defined(CONFIG_ENABLE_CHIP_MSG26XXM)
extern TestScopeInfo_t g_TestScopeInfo;
#endif //CONFIG_ENABLE_CHIP_MSG26XXM
#endif //CONFIG_ENABLE_ITO_MP_TEST
//merged by pangle at 20150228 end
/*=============================================================*/
// LOCAL VARIABLE DEFINITION
/*=============================================================*/

static u16 _gDebugReg[MAX_DEBUG_REGISTER_NUM] = {0};
static u32 _gDebugRegCount = 0;

static u8 *_gPlatformFwVersion = NULL; // internal use firmware version for MStar

#ifdef CONFIG_ENABLE_ITO_MP_TEST
static ItoTestMode_e _gItoTestMode = 0;
#endif //CONFIG_ENABLE_ITO_MP_TEST

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
//static u32 _gLogGestureCount = 0;
static u8 _gLogGestureInforType = 0;
#endif //CONFIG_ENABLE_GESTURE_INFORMATION_MODE

#endif //CONFIG_ENABLE_GESTURE_WAKEUP
static u32 _gIsUpdateComplete = 0;

static u8 *_gFwVersion = NULL; // customer firmware version

static struct class *_gFirmwareClass = NULL;
static struct device *_gFirmwareCmdDev = NULL;

/*=============================================================*/
// GLOBAL VARIABLE DEFINITION
/*=============================================================*/

#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
struct kset *g_TouchKSet = NULL;
struct kobject *g_TouchKObj = NULL;
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG

u8 g_FwData[94][1024];
u32 g_FwDataCount = 0;

/*=============================================================*/
// LOCAL FUNCTION DEFINITION
/*=============================================================*/


/*=============================================================*/
// GLOBAL FUNCTION DEFINITION
/*=============================================================*/

ssize_t DrvMainFirmwareChipTypeShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    DBG("*** %s() ***\n", __func__);

    return sprintf(pBuf, "%d", g_ChipType);
}

ssize_t DrvMainFirmwareChipTypeStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    DBG("*** %s() ***\n", __func__);

//    g_ChipType = DrvIcFwLyrGetChipType();

    return nSize;
}

static DEVICE_ATTR(chip_type, SYSFS_AUTHORITY, DrvMainFirmwareChipTypeShow, DrvMainFirmwareChipTypeStore);

ssize_t DrvMainFirmwareDriverVersionShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    DBG("*** %s() ***\n", __func__);

    return sprintf(pBuf, "%s", DEVICE_DRIVER_RELEASE_VERSION);
}

ssize_t DrvMainFirmwareDriverVersionStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    DBG("*** %s() ***\n", __func__);

    return nSize;
}

static DEVICE_ATTR(driver_version, SYSFS_AUTHORITY, DrvMainFirmwareDriverVersionShow, DrvMainFirmwareDriverVersionStore);

/*--------------------------------------------------------------------------*/

ssize_t DrvMainFirmwareUpdateShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    DBG("*** %s() _gFwVersion = %s ***\n", __func__, _gFwVersion);

    return sprintf(pBuf, "%s\n", _gFwVersion);
}

ssize_t DrvMainFirmwareUpdateStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    DrvPlatformLyrDisableFingerTouchReport();

    DBG("*** %s() g_FwDataCount = %d ***\n", __func__, g_FwDataCount);

    if (0 != DrvIcFwLyrUpdateFirmware(g_FwData, EMEM_ALL))
    {
        _gIsUpdateComplete = 0;
        DBG("Update FAILED\n");
    }
    else
    {
        _gIsUpdateComplete = 1;
        DBG("Update SUCCESS\n");
    }

#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
//    DrvIcFwLyrRestoreFirmwareModeToLogDataMode();    
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG

    DrvPlatformLyrEnableFingerTouchReport();
  
    return nSize;
}

static DEVICE_ATTR(update, SYSFS_AUTHORITY, DrvMainFirmwareUpdateShow, DrvMainFirmwareUpdateStore);

ssize_t DrvMainFirmwareVersionShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    DBG("*** %s() _gFwVersion = %s ***\n", __func__, _gFwVersion);

    return sprintf(pBuf, "%s\n", _gFwVersion);
}

ssize_t DrvMainFirmwareVersionStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    u16 nMajor = 0, nMinor = 0;
    
    DrvIcFwLyrGetCustomerFirmwareVersion(&nMajor, &nMinor, &_gFwVersion);

    DBG("*** %s() _gFwVersion = %s ***\n", __func__, _gFwVersion);

    return nSize;
}

static DEVICE_ATTR(version, SYSFS_AUTHORITY, DrvMainFirmwareVersionShow, DrvMainFirmwareVersionStore);

ssize_t DrvMainFirmwareDataShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    DBG("*** %s() g_FwDataCount = %d ***\n", __func__, g_FwDataCount);
    
    return g_FwDataCount;
}

ssize_t DrvMainFirmwareDataStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    u32 nCount = nSize / 1024;
    u32 nRemainder = nSize % 1024;
    u32 i;

    DBG("*** %s() ***\n", __func__);

    if (nCount > 0) // nSize >= 1024
   	{
        for (i = 0; i < nCount; i ++)
        {
            memcpy(g_FwData[g_FwDataCount], pBuf+(i*1024), 1024);

            g_FwDataCount ++;
        }

        if (nRemainder > 0) // Handle special firmware size like MSG22XX(48.5KB)
        {
            DBG("nRemainder = %d\n", nRemainder);

            memcpy(g_FwData[g_FwDataCount], pBuf+(i*1024), nRemainder);

            g_FwDataCount ++;
        }
    }
    else // nSize < 1024
    {
    		if (nSize > 0)
    		{
            memcpy(g_FwData[g_FwDataCount], pBuf, nSize);

            g_FwDataCount ++;
    		}
    }

    DBG("*** g_FwDataCount = %d ***\n", g_FwDataCount);

    if (pBuf != NULL)
    {
        DBG("*** buf[0] = %c ***\n", pBuf[0]);
    }
   
    return nSize;
}

static DEVICE_ATTR(data, SYSFS_AUTHORITY, DrvMainFirmwareDataShow, DrvMainFirmwareDataStore);

/*--------------------------------------------------------------------------*/

#ifdef CONFIG_ENABLE_ITO_MP_TEST
ssize_t DrvMainFirmwareTestShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    DBG("*** %s() ***\n", __func__);
    DBG("*** ctp mp test status = %d ***\n", DrvIcFwLyrGetMpTestResult());
    
    return sprintf(pBuf, "%d", DrvIcFwLyrGetMpTestResult());
}

ssize_t DrvMainFirmwareTestStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    u32 nMode = 0;

    DBG("*** %s() ***\n", __func__);
    
    if (pBuf != NULL)
    {
        sscanf(pBuf, "%x", &nMode);   

        DBG("Mp Test Mode = 0x%x\n", nMode);

        if (nMode == ITO_TEST_MODE_OPEN_TEST) //open test
        {
            _gItoTestMode = ITO_TEST_MODE_OPEN_TEST;
            DrvIcFwLyrScheduleMpTestWork(ITO_TEST_MODE_OPEN_TEST);
        }
        else if (nMode == ITO_TEST_MODE_SHORT_TEST) //short test
        {
            _gItoTestMode = ITO_TEST_MODE_SHORT_TEST;
            DrvIcFwLyrScheduleMpTestWork(ITO_TEST_MODE_SHORT_TEST);
        }
        else
        {
            DBG("*** Undefined MP Test Mode ***\n");
        }
    }

    return nSize;
}

static DEVICE_ATTR(test, SYSFS_AUTHORITY, DrvMainFirmwareTestShow, DrvMainFirmwareTestStore);

ssize_t DrvMainFirmwareTestLogShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    u32 nLength = 0;
    
    DBG("*** %s() ***\n", __func__);
    
    DrvIcFwLyrGetMpTestDataLog(_gItoTestMode, pBuf, &nLength);
    
    return nLength;
}

ssize_t DrvMainFirmwareTestLogStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    DBG("*** %s() ***\n", __func__);

    return nSize;
}

static DEVICE_ATTR(test_log, SYSFS_AUTHORITY, DrvMainFirmwareTestLogShow, DrvMainFirmwareTestLogStore);

ssize_t DrvMainFirmwareTestFailChannelShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    u32 nCount = 0;

    DBG("*** %s() ***\n", __func__);
    
    DrvIcFwLyrGetMpTestFailChannel(_gItoTestMode, pBuf, &nCount);
    
    return nCount;
}

ssize_t DrvMainFirmwareTestFailChannelStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    DBG("*** %s() ***\n", __func__);

    return nSize;
}

static DEVICE_ATTR(test_fail_channel, SYSFS_AUTHORITY, DrvMainFirmwareTestFailChannelShow, DrvMainFirmwareTestFailChannelStore);
//merged by pangle at 20150228 begin
ssize_t DrvMainFirmwareTestScopeShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    DBG("*** %s() ***\n", __func__);

#if defined(CONFIG_ENABLE_CHIP_MSG26XXM)
    DrvIcFwLyrGetMpTestScope(&g_TestScopeInfo);
    
    return sprintf(pBuf, "%d,%d", g_TestScopeInfo.nMx, g_TestScopeInfo.nMy);
#else
    return 0;    
#endif //CONFIG_ENABLE_CHIP_MSG26XXM
}

ssize_t DrvMainFirmwareTestScopeStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    DBG("*** %s() ***\n", __func__);

    return nSize;
}

static DEVICE_ATTR(test_scope, SYSFS_AUTHORITY, DrvMainFirmwareTestScopeShow, DrvMainFirmwareTestScopeStore);
//merged by pangle at 20150228 end
#endif //CONFIG_ENABLE_ITO_MP_TEST

/*--------------------------------------------------------------------------*/

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP

ssize_t DrvMainFirmwareGestureWakeupModeShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    DBG("*** %s() ***\n", __func__);
    DBG("g_GestureWakeupMode = 0x%x\n", g_GestureWakeupMode);

    return sprintf(pBuf, "%x", g_GestureWakeupMode);
}

ssize_t DrvMainFirmwareGestureWakeupModeStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    u32 nLength, nWakeupMode;
    
    DBG("*** %s() ***\n", __func__);

    if (pBuf != NULL)
    {
        sscanf(pBuf, "%x", &nWakeupMode);   
        DBG("nWakeupMode = 0x%x\n", nWakeupMode);

        nLength = nSize;
        DBG("nLength = %d\n", nLength);

        if ((nWakeupMode & GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG) == GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG)
        {
            g_GestureWakeupMode = g_GestureWakeupMode | GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG;
        }
        else
        {
            g_GestureWakeupMode = g_GestureWakeupMode & (~GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG);
        }

        if ((nWakeupMode & GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG) == GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG)
        {
            g_GestureWakeupMode = g_GestureWakeupMode | GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG;
        }
        else
        {
            g_GestureWakeupMode = g_GestureWakeupMode & (~GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG);
        }

        if ((nWakeupMode & GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG) == GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG)
        {
            g_GestureWakeupMode = g_GestureWakeupMode | GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG;
        }
        else
        {
            g_GestureWakeupMode = g_GestureWakeupMode & (~GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG);
        }

        if ((nWakeupMode & GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG) == GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG)
        {
            g_GestureWakeupMode = g_GestureWakeupMode | GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG;
        }
        else
        {
            g_GestureWakeupMode = g_GestureWakeupMode & (~GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG);
        }

        if ((nWakeupMode & GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG) == GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG)
        {
            g_GestureWakeupMode = g_GestureWakeupMode | GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG;
        }
        else
        {
            g_GestureWakeupMode = g_GestureWakeupMode & (~GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG);
        }
//modify  by pangle for gesture display 20150304 begin
#ifdef GESTURE_ALL_SWITCH
        if ((nWakeupMode & GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG)
        {
            g_GestureWakeupMode = g_GestureWakeupMode | GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG;
        }
        else
        {
            g_GestureWakeupMode = g_GestureWakeupMode & (~GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG);
        }

        if ((nWakeupMode & GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG)
        {
            g_GestureWakeupMode = g_GestureWakeupMode | GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG;
        }
        else
        {
            g_GestureWakeupMode = g_GestureWakeupMode & (~GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG);
        }

        if ((nWakeupMode & GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG)
        {
            g_GestureWakeupMode = g_GestureWakeupMode | GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG;
        }
        else
        {
            g_GestureWakeupMode = g_GestureWakeupMode & (~GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG);
        }
       
        if ((nWakeupMode & GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG)
        {
            g_GestureWakeupMode = g_GestureWakeupMode | GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG;
        }
        else
        {
            g_GestureWakeupMode = g_GestureWakeupMode & (~GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG);
        }

        if ((nWakeupMode & GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG)
        {
            g_GestureWakeupMode = g_GestureWakeupMode | GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG;
        }
        else
        {
            g_GestureWakeupMode = g_GestureWakeupMode & (~GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG);
        }

        if ((nWakeupMode & GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG)
        {
            g_GestureWakeupMode = g_GestureWakeupMode | GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG;
        }
        else
        {
            g_GestureWakeupMode = g_GestureWakeupMode & (~GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG);
        }

        if ((nWakeupMode & GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG)
        {
            g_GestureWakeupMode = g_GestureWakeupMode | GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG;
        }
        else
        {
            g_GestureWakeupMode = g_GestureWakeupMode & (~GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG);
        }

        if ((nWakeupMode & GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG)
        {
            g_GestureWakeupMode = g_GestureWakeupMode | GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG;
        }
        else
        {
            g_GestureWakeupMode = g_GestureWakeupMode & (~GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG);
        }
#endif
//modify  by pangle for gesture display 20150304 end

        DBG("g_GestureWakeupMode = 0x%x\n", g_GestureWakeupMode);
    }
        
    return nSize;
}

static DEVICE_ATTR(gesture_wakeup_mode, SYSFS_AUTHORITY, DrvMainFirmwareGestureWakeupModeShow, DrvMainFirmwareGestureWakeupModeStore);
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
ssize_t DrvMainFirmwareGestureInforShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    u8 szOut[15*5] = {0}, szValue[10] = {0};
    u32 szLogGestureInfo[15] = {0};
    u32 i = 0;
    DBG("*** %s() ***\n", __func__);

   		//gesture
   		szLogGestureInfo[0] = g_LogGestureInfor[4];
		
		//left_up 
 		szLogGestureInfo[1] = g_LogGestureInfor[6];
 		szLogGestureInfo[2] = TOUCH_SCREEN_Y_MAX-g_LogGestureInfor[7];

		//right_down
 		szLogGestureInfo[3] = g_LogGestureInfor[8];
 		szLogGestureInfo[4] = TOUCH_SCREEN_Y_MAX-g_LogGestureInfor[9];

		//Xst Yst
 		szLogGestureInfo[5] = g_LogGestureInfor[10];
		szLogGestureInfo[6] = TOUCH_SCREEN_Y_MAX-g_LogGestureInfor[11];

		//Xend Yend
 		szLogGestureInfo[7] = g_LogGestureInfor[12];
		szLogGestureInfo[8] = TOUCH_SCREEN_Y_MAX-g_LogGestureInfor[13];
		
		//X0 Y0
 		szLogGestureInfo[9] = g_LogGestureInfor[14];
 		szLogGestureInfo[10] = TOUCH_SCREEN_Y_MAX-g_LogGestureInfor[15];

		//X1 Y1
 		szLogGestureInfo[11] = g_LogGestureInfor[16];
 		szLogGestureInfo[12] = TOUCH_SCREEN_Y_MAX-g_LogGestureInfor[17];

		//X2 Y2
 		szLogGestureInfo[13] = g_LogGestureInfor[18];
 		szLogGestureInfo[14] = TOUCH_SCREEN_Y_MAX-g_LogGestureInfor[19];

		for (i = 0; i < 15; i ++)
    	{
       		sprintf(szValue, "%d", szLogGestureInfo[i]);
        	strcat(szOut, szValue);
        	strcat(szOut, ",");
    	}
		
	/*
    if (_gLogGestureInforType == FIRMWARE_GESTURE_INFORMATION_MODE_A) //FIRMWARE_GESTURE_INFORMATION_MODE_A
    {
        for (i = 0; i < 2; i ++)//0 EventFlag; 1 RecordNum
        {
            szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[4 + i];
            _gLogGestureCount ++;
        }

        for (i = 2; i < 11; i ++)//2~3 Xst Yst; 4~5 Xend Yend; 6~7 char_width char_height
        {
            szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[4 + i];
            _gLogGestureCount ++;
        }
    }
    else if (_gLogGestureInforType == FIRMWARE_GESTURE_INFORMATION_MODE_B) //FIRMWARE_GESTURE_INFORMATION_MODE_B
    {
        for (i = 0; i < 2; i ++)//0 EventFlag; 1 RecordNum
        {
            szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[4 + i];
            _gLogGestureCount ++;
        }

        for (i = 0; i< g_LogGestureInfor[5]*2 ; i ++)//(X and Y)*RecordNum
        {
            szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[12 + i];
            _gLogGestureCount ++;
        }
    }
    else if (_gLogGestureInforType == FIRMWARE_GESTURE_INFORMATION_MODE_C) //FIRMWARE_GESTURE_INFORMATION_MODE_C
    {
        for (i = 0; i < 6; i ++)//header
        {
            szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[i];
            _gLogGestureCount ++;
        }

        for (i = 6; i < 86; i ++)
        {
            szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[i];
            _gLogGestureCount ++;
        }

        szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[86];//dummy
        _gLogGestureCount ++;
        szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[87];//checksum
        _gLogGestureCount++;
    }
    else
    {
        DBG("*** Undefined GESTURE INFORMATION MODE ***\n");
    }

    for (i = 0; i < _gLogGestureCount; i ++)
    {
        sprintf(szValue, "%d", szLogGestureInfo[i]);
        strcat(szOut, szValue);
        strcat(szOut, ",");
    }
*/
    return sprintf(pBuf, "%s\n", szOut);
}

ssize_t DrvMainFirmwareGestureInforStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    u32 nMode;

    DBG("*** %s() ***\n", __func__);

    if (pBuf != NULL)
    {
        sscanf(pBuf, "%x", &nMode);
        _gLogGestureInforType = nMode;
    }

    DBG("*** _gLogGestureInforType type = 0x%x ***\n", _gLogGestureInforType);

    return nSize;
}

static DEVICE_ATTR(gesture_infor, SYSFS_AUTHORITY, DrvMainFirmwareGestureInforShow, DrvMainFirmwareGestureInforStore);

//added by litao for gesture proc note 20150119 begin
static struct proc_dir_entry *mstar_gesture_info;

ssize_t mstar_gesture_info_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
	unsigned char writebuf[128];
	int buflen = len;
//	int writelen = 0;
	
	if (copy_from_user(&writebuf, buff, buflen)) {
		printk(KERN_ERR"%s:copy from user error\n", __func__);
		return -EFAULT;
	}
	return len;
}

static ssize_t mstar_gesture_info_read(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	u32 szLogGestureInfo[15] = {0};
	unsigned char buf[29];
	
	DBG("*** %s() ***\n", __func__);

	memset(buf, 0, 29);

	//gesture
	szLogGestureInfo[0] = g_LogGestureInfor[4];
	buf[0] = szLogGestureInfo[0]& 0xff;
	
	//left_up 
	szLogGestureInfo[1] = g_LogGestureInfor[6];
	buf[2] = szLogGestureInfo[1] & 0xff;
	buf[1] = szLogGestureInfo[1] >> 8;
	
	szLogGestureInfo[2] = TOUCH_SCREEN_Y_MAX-g_LogGestureInfor[7];
	buf[4] = szLogGestureInfo[2] & 0xff;
	buf[3] = szLogGestureInfo[2] >> 8;
	
	//right_down
	szLogGestureInfo[3] = g_LogGestureInfor[8];
	buf[6] = szLogGestureInfo[3] & 0xff;
	buf[5] = szLogGestureInfo[3] >> 8;

	szLogGestureInfo[4] = TOUCH_SCREEN_Y_MAX-g_LogGestureInfor[9];
	buf[8] = szLogGestureInfo[4] & 0xff;
	buf[7] = szLogGestureInfo[4] >> 8;
	
	//Xst Yst
	szLogGestureInfo[5] = g_LogGestureInfor[10];
	buf[10] = szLogGestureInfo[5] & 0xff;
	buf[9] = szLogGestureInfo[5] >> 8;
	
	szLogGestureInfo[6] = TOUCH_SCREEN_Y_MAX-g_LogGestureInfor[11];
	buf[12] = szLogGestureInfo[6] & 0xff;
	buf[11] = szLogGestureInfo[6] >> 8;
	
	//Xend Yend
	szLogGestureInfo[7] = g_LogGestureInfor[12];
	buf[14] = szLogGestureInfo[7] & 0xff;
	buf[13] = szLogGestureInfo[7] >> 8;
	
	szLogGestureInfo[8] = TOUCH_SCREEN_Y_MAX-g_LogGestureInfor[13];
	buf[16] = szLogGestureInfo[8] & 0xff;
	buf[15] = szLogGestureInfo[8] >> 8;
	
	//X0 Y0
	szLogGestureInfo[9] = g_LogGestureInfor[14];
	buf[18] = szLogGestureInfo[9] & 0xff;
	buf[17] = szLogGestureInfo[9] >> 8;
	
	szLogGestureInfo[10] = TOUCH_SCREEN_Y_MAX-g_LogGestureInfor[15];
	buf[20] = szLogGestureInfo[10] & 0xff;
	buf[19] = szLogGestureInfo[10] >> 8;
	
	//X1 Y1
	szLogGestureInfo[11] = g_LogGestureInfor[16];
	buf[22] = szLogGestureInfo[11] & 0xff;
	buf[21] = szLogGestureInfo[11] >> 8;
	
	szLogGestureInfo[12] = TOUCH_SCREEN_Y_MAX-g_LogGestureInfor[17];
	buf[24] = szLogGestureInfo[12] & 0xff;
	buf[23] = szLogGestureInfo[12] >> 8;	
	
	//X2 Y2
	szLogGestureInfo[13] = g_LogGestureInfor[18];
	buf[26] = szLogGestureInfo[13] & 0xff;
	buf[25] = szLogGestureInfo[13] >> 8;
	
	szLogGestureInfo[14] = TOUCH_SCREEN_Y_MAX-g_LogGestureInfor[19];

	buf[28] = szLogGestureInfo[14] & 0xff;
	buf[27] = szLogGestureInfo[14] >> 8;
	
	memcpy(page, buf, 29);

	return 29;
}

static const struct file_operations mstar_gesture_info_ops = {
    .owner = THIS_MODULE,
    .read = mstar_gesture_info_read,
    .write = mstar_gesture_info_write,
};

int mstar_create_proc(void)
{
	mstar_gesture_info = proc_create(PROC_NAME, 0777,NULL, &mstar_gesture_info_ops);
	if (NULL == mstar_gesture_info) {
		printk(KERN_ERR "Couldn't create proc entry!\n");
		return -ENOMEM;
	} else {
		printk(KERN_ERR "Create proc entry success!\n");
	}
	return 0;
}
//added by litao for gesture proc note 20150119 end

#endif //CONFIG_ENABLE_GESTURE_INFORMATION_MODE

#endif //CONFIG_ENABLE_GESTURE_WAKEUP

//add by shihuijun for factory test 20150309 start 
#if defined(CONFIG_ITO_TEST_BY_PROC)
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#define ITO_TEST_AUTHORITY 0777 
static struct proc_dir_entry *msg_class_proc = NULL;
static struct proc_dir_entry *msg_msg20xx_proc = NULL;
static struct proc_dir_entry *msg_device_proc = NULL;
static struct proc_dir_entry *chip_type = NULL;
static struct proc_dir_entry *test = NULL;
static struct proc_dir_entry *test_log = NULL;
static struct proc_dir_entry *test_fail_channel = NULL;
static struct proc_dir_entry *test_scop = NULL;
#define PROC_MSG_CLASS      "class"
#define PROC_MSG_TOUCHSCREEN_MSG20XX      "ms-touchscreen-msg20xx"
#define PROC_MSG_DEVICE      "device"
#define PROC_MSG_CHIP_TYPE      "chip_type"
#define PROC_MSG_ITO_TEST      "test"
#define PROC_MSG_ITO_TEST_LOG     "test_log"
#define PROC_MSG_ITO_TEST_FAIL_CHANNEL      "test_fail_channel"
#define PROC_MSG_ITO_TEST_SCOP     "test_scop"
//add by shihuijun for factory test 20150309 end

//creat  chiptype/test/testlog/testfailchannel/test scope node  under proc by shihuijun for  factory test updatedate 20150309 start 
ssize_t mstar_chiptype_info_write(struct file * filp,const char __user * buff,size_t len,loff_t * off)
{
	 DBG("*** %s() ***\n", __func__);

//    g_ChipType = DrvIcFwLyrGetChipType();

    return len;
}
static ssize_t mstar_chiptype_info_read(struct file * file,char __user * page,size_t size,loff_t * ppos)
{
   DBG("*** %s() ***\n", __func__);
   DBG("g_ChipType = %d \n", g_ChipType);
   g_ChipType = DrvIcFwLyrGetChipType();
   return sprintf(page, "%d", g_ChipType);
}

static const struct file_operations mstar_chiptype_info_ops = {
    .owner = THIS_MODULE,
    .read = mstar_chiptype_info_read,
    .write = mstar_chiptype_info_write,
};
ssize_t mstar_test_info_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
	int buflen = len;
	char *buf1;
	u32 nMode = 0;

    DBG("*** %s() ***\n", __func__);
    if (len < 1)
	return -EINVAL;
	buf1 = kmalloc(len, GFP_KERNEL);
	if (!buf1)
	return -ENOMEM;
	
        //sscanf(buff, "%x", &nMode);  
        if (copy_from_user(buf1, buff, buflen)) 
		{
		printk(KERN_ERR"%s:copy from user error\n", __func__);
		return -EFAULT;
	    }
		if (buf1[0] == '1') {
				nMode=1;
			} else if (buf1[0] == '2') {
				nMode=2;
			} else {
				kfree(buf1);
				DBG("WRONG TEST TYPE buf1 =%s\n", buf1);
				return -EINVAL;
			}
			DBG("*** %s() ***\n", __func__);

        if (nMode == ITO_TEST_MODE_OPEN_TEST) //open test
        {
            _gItoTestMode = ITO_TEST_MODE_OPEN_TEST;
            DrvIcFwLyrScheduleMpTestWork(ITO_TEST_MODE_OPEN_TEST);
        }
        else if (nMode == ITO_TEST_MODE_SHORT_TEST) //short test
        {
            _gItoTestMode = ITO_TEST_MODE_SHORT_TEST;
            DrvIcFwLyrScheduleMpTestWork(ITO_TEST_MODE_SHORT_TEST);
        }
        else
        {
            DBG("*** Undefined MP Test Mode ***\n");
        }
    kfree(buf1);
    return len;
}
static ssize_t mstar_test_info_read(struct file * file,char __user * page,size_t size,loff_t * ppos)
{

	//unsigned char buf[16];
    DBG("*** %s() ***\n", __func__);
    DBG("*** ctp mp test status = %d ***\n", DrvIcFwLyrGetMpTestResult());
   return sprintf(page, "%d", DrvIcFwLyrGetMpTestResult());
}

static const struct file_operations mstar_test_info_ops = {
    .owner = THIS_MODULE,
    .read = mstar_test_info_read,
    .write = mstar_test_info_write,
};

ssize_t mstar_test_log_info_write(struct file * filp,const char __user * buff,size_t len,loff_t * off)
{
  DBG("*** %s() ***\n", __func__);

    return len;  
}

static ssize_t mstar_test_log_info_read(struct file * file,char __user * page,size_t size,loff_t * ppos)
{
    u32 nLength = 0;
    
    DBG("*** %s() ***\n", __func__);
    
    DrvIcFwLyrGetMpTestDataLog(_gItoTestMode, page, &nLength);
 //   *(ppos+size)=1;
    return nLength;
}

static const struct file_operations mstar_test_log_info_ops = {
    .owner = THIS_MODULE,
    .read = mstar_test_log_info_read,
    .write = mstar_test_log_info_write,
};

ssize_t mstar_test_fail_channel_info_write(struct file * filp,const char __user * buff,size_t len,loff_t * off)
{
  DBG("*** %s() ***\n", __func__);

    return len;  
}

static ssize_t mstar_test_fail_channel_info_read(struct file * file,char __user * page,size_t size,loff_t * ppos)
{
    u32 nCount = 0;
    
    DBG("*** %s() ***\n", __func__);
    
    DrvIcFwLyrGetMpTestDataLog(_gItoTestMode, page, &nCount);
   // *(ppos+size)=1;
    return nCount;
}

static const struct file_operations mstar_test_fail_channel_info_ops = {
    .owner = THIS_MODULE,
    .read = mstar_test_fail_channel_info_read,
    .write = mstar_test_fail_channel_info_write,
};

ssize_t mstar_test_scope_info_write(struct file * filp,const char __user * buff,size_t len,loff_t * off)
{
    DBG("*** %s() ***\n", __func__);

    return len;  
}
static ssize_t mstar_test_scope_info_read(struct file * file,char __user * page,size_t size,loff_t * ppos)
{
    DBG("*** %s() ***\n", __func__);

    #if defined(CONFIG_ENABLE_CHIP_MSG26XXM)
    DrvIcFwLyrGetMpTestScope(&g_TestScopeInfo);
    return sprintf(page, "%d,%d", g_TestScopeInfo.nMx, g_TestScopeInfo.nMy);
    #else
    return 0;    
    #endif //CONFIG_ENABLE_CHIP_MSG26XXM
}

static const struct file_operations mstar_test_scope_info_ops = {
    .owner = THIS_MODULE,
    .read = mstar_test_scope_info_read,
    .write = mstar_test_scope_info_write,
};
//creat  chiptype/test/testlog/testfailchannel/test scope node  under proc by shihuijun for  factory test updatedate 20150309 end 


/*********************************
//name:       MsgCreateFileForTestProc
//author:      shihuijun
//date:         20150309
//function:    creat factory test node under proc file(chiptype,test,test log,test fail channel,test scope)
//arguments: none
//update:      20150309         
**********************************/
static void MsgCreateFileForTestProc(void)
{
	msg_class_proc = proc_mkdir(PROC_MSG_CLASS, NULL);
	msg_msg20xx_proc = proc_mkdir(PROC_MSG_TOUCHSCREEN_MSG20XX, msg_class_proc);
	msg_device_proc = proc_mkdir(PROC_MSG_DEVICE, msg_msg20xx_proc);

    chip_type = proc_create(PROC_MSG_CHIP_TYPE, 0777,msg_device_proc, &mstar_chiptype_info_ops);
	test = proc_create(PROC_MSG_ITO_TEST, 0777,msg_device_proc, &mstar_test_info_ops);
	test_log = proc_create(PROC_MSG_ITO_TEST_LOG, 0777,msg_device_proc, &mstar_test_log_info_ops);
	test_fail_channel = proc_create(PROC_MSG_ITO_TEST_FAIL_CHANNEL, 0777,msg_device_proc, &mstar_test_fail_channel_info_ops);
	test_scop = proc_create(PROC_MSG_ITO_TEST_SCOP, 0777,msg_device_proc, &mstar_test_scope_info_ops);

	if (NULL==chip_type) 
	{
		DBG("create_proc_entry CHIP TYPE failed\n");
	} 
	else 
	{
	
	    DBG("create_proc_entry CHIP TYPE  OK\n");
	}
	if (NULL==test) 
	{
		DBG("create_proc_entry ITO TEST failed\n");
	} 
	else 
	{
		
		DBG("create_proc_entry ITO TEST  OK\n");
	}
	if (NULL==test_log) 
	{
		DBG("create_proc_entry ITO TEST LOG failed\n");
	} 
	else 
	{
		
		DBG("create_proc_entry ITO TEST  LOG OK\n");
	}	
	if (NULL==test_fail_channel) 
	{
		DBG("create_proc_entry ITO TEST FAIL CHANNEL failed\n");
	} 
	else 
	{
		
		DBG("create_proc_entry ITO TEST FAIL CHANNEL  OK\n");
	}
	if (NULL==test_scop) 
	{
		DBG("create_proc_entry ITO TEST SCOP failed\n");
	} 
	else 
	{
		
		DBG("create_proc_entry ITO TEST SCOP  OK\n");
	}
}
#endif
/*--------------------------------------------------------------------------*/

ssize_t DrvMainFirmwareDebugShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    u32 i;
    u8 nBank, nAddr;
    u16 szRegData[MAX_DEBUG_REGISTER_NUM] = {0};
    u8 szOut[MAX_DEBUG_REGISTER_NUM*25] = {0};
	u8 szValue[10] = {0};

    DBG("*** %s() ***\n", __func__);
    
    DbBusEnterSerialDebugMode();
    DbBusStopMCU();
    DbBusIICUseBus();
    DbBusIICReshape();
    mdelay(300);
    
    for (i = 0; i < _gDebugRegCount; i ++)
    {
        szRegData[i] = RegGet16BitValue(_gDebugReg[i]);
    }

    DbBusIICNotUseBus();
    DbBusNotStopMCU();
    DbBusExitSerialDebugMode();

    for (i = 0; i < _gDebugRegCount; i ++)
    {
    	  nBank = (_gDebugReg[i] >> 8) & 0xFF;
    	  nAddr = _gDebugReg[i] & 0xFF;
    	  
        DBG("reg(0x%X,0x%X)=0x%04X\n", nBank, nAddr, szRegData[i]);

        strcat(szOut, "reg(");
        sprintf(szValue, "0x%X", nBank);
        strcat(szOut, szValue);
        strcat(szOut, ",");
        sprintf(szValue, "0x%X", nAddr);
        strcat(szOut, szValue);
        strcat(szOut, ")=");
        sprintf(szValue, "0x%04X", szRegData[i]);
        strcat(szOut, szValue);
        strcat(szOut, "\n");
    }

    return sprintf(pBuf, "%s\n", szOut);
}

ssize_t DrvMainFirmwareDebugStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    u32 i;
    char *pCh;

    DBG("*** %s() ***\n", __func__);

    if (pBuf != NULL)
    {
        DBG("*** %s() pBuf[0] = %c ***\n", __func__, pBuf[0]);
        DBG("*** %s() pBuf[1] = %c ***\n", __func__, pBuf[1]);
        DBG("*** %s() pBuf[2] = %c ***\n", __func__, pBuf[2]);
        DBG("*** %s() pBuf[3] = %c ***\n", __func__, pBuf[3]);
        DBG("*** %s() pBuf[4] = %c ***\n", __func__, pBuf[4]);
        DBG("*** %s() pBuf[5] = %c ***\n", __func__, pBuf[5]);

        DBG("nSize = %d\n", nSize);
       
        i = 0;
        while ((pCh = strsep((char **)&pBuf, " ,")) && (i < MAX_DEBUG_REGISTER_NUM))
        {
            DBG("pCh = %s\n", pCh);
            
            _gDebugReg[i] = DrvCommonConvertCharToHexDigit(pCh, strlen(pCh));

            DBG("_gDebugReg[%d] = 0x%04X\n", i, _gDebugReg[i]);
            i ++;
        }
        _gDebugRegCount = i;
        
        DBG("_gDebugRegCount = %d\n", _gDebugRegCount);
    }

    return nSize;
}

static DEVICE_ATTR(debug, SYSFS_AUTHORITY, DrvMainFirmwareDebugShow, DrvMainFirmwareDebugStore);

/*--------------------------------------------------------------------------*/

ssize_t DrvMainFirmwarePlatformVersionShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    DBG("*** %s() _gPlatformFwVersion = %s ***\n", __func__, _gPlatformFwVersion);

    return sprintf(pBuf, "%s\n", _gPlatformFwVersion);
}

ssize_t DrvMainFirmwarePlatformVersionStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    DrvIcFwLyrGetPlatformFirmwareVersion(&_gPlatformFwVersion);

#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
    DrvIcFwLyrRestoreFirmwareModeToLogDataMode();    
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG

    DBG("*** %s() _gPlatformFwVersion = %s ***\n", __func__, _gPlatformFwVersion);

    return nSize;
}

static DEVICE_ATTR(platform_version, SYSFS_AUTHORITY, DrvMainFirmwarePlatformVersionShow, DrvMainFirmwarePlatformVersionStore);

/*--------------------------------------------------------------------------*/

#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
ssize_t DrvMainFirmwareModeShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
#if defined(CONFIG_ENABLE_CHIP_MSG26XXM)
    DrvPlatformLyrDisableFingerTouchReport();

    g_FirmwareMode = DrvIcFwLyrGetFirmwareMode();
    
    DrvPlatformLyrEnableFingerTouchReport();

    DBG("%s() firmware mode = 0x%x\n", __func__, g_FirmwareMode);

    return sprintf(pBuf, "%x", g_FirmwareMode);
#elif defined(CONFIG_ENABLE_CHIP_MSG21XXA) || defined(CONFIG_ENABLE_CHIP_MSG22XX)
    DrvPlatformLyrDisableFingerTouchReport();

    DrvIcFwLyrGetFirmwareInfo(&g_FirmwareInfo);
    g_FirmwareMode = g_FirmwareInfo.nFirmwareMode;

    DrvPlatformLyrEnableFingerTouchReport();

    DBG("%s() firmware mode = 0x%x, can change firmware mode = %d\n", __func__, g_FirmwareInfo.nFirmwareMode, g_FirmwareInfo.nIsCanChangeFirmwareMode);

    return sprintf(pBuf, "%x,%d", g_FirmwareInfo.nFirmwareMode, g_FirmwareInfo.nIsCanChangeFirmwareMode);
#endif
}

ssize_t DrvMainFirmwareModeStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{


    unsigned int  nMode;
    DBG("*** %s() ***\n", __func__);    
    if (pBuf != NULL)
    {
        sscanf(pBuf, "%x", &nMode);   
        DBG("firmware mode = 0x%x\n", nMode);

        DrvPlatformLyrDisableFingerTouchReport(); 

        if (nMode == FIRMWARE_MODE_DEMO_MODE) //demo mode
        {
            g_FirmwareMode = DrvIcFwLyrChangeFirmwareMode(FIRMWARE_MODE_DEMO_MODE);
        }
        else if (nMode == FIRMWARE_MODE_DEBUG_MODE) //debug mode
        {
            g_FirmwareMode = DrvIcFwLyrChangeFirmwareMode(FIRMWARE_MODE_DEBUG_MODE);
        }
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA) || defined(CONFIG_ENABLE_CHIP_MSG22XX)
        else if (nMode == FIRMWARE_MODE_RAW_DATA_MODE) //raw data mode
        {
            g_FirmwareMode = DrvIcFwLyrChangeFirmwareMode(FIRMWARE_MODE_RAW_DATA_MODE);
        }
#endif
        else
        {
            DBG("*** Undefined Firmware Mode ***\n");
        }

        DrvPlatformLyrEnableFingerTouchReport(); 
    }

    DBG("*** g_FirmwareMode = 0x%x ***\n", g_FirmwareMode);

    return nSize;
}

static DEVICE_ATTR(mode, SYSFS_AUTHORITY, DrvMainFirmwareModeShow, DrvMainFirmwareModeStore);
/*
ssize_t DrvMainFirmwarePacketShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    u32 i = 0;
    u32 nLength = 0;
    
    DBG("*** %s() ***\n", __func__);
    
    if (g_LogModePacket != NULL)
    {
        DBG("g_FirmwareMode=%x, g_LogModePacket[0]=%x, g_LogModePacket[1]=%x\n", g_FirmwareMode, g_LogModePacket[0], g_LogModePacket[1]);
        DBG("g_LogModePacket[2]=%x, g_LogModePacket[3]=%x\n", g_LogModePacket[2], g_LogModePacket[3]);
        DBG("g_LogModePacket[4]=%x, g_LogModePacket[5]=%x\n", g_LogModePacket[4], g_LogModePacket[5]);

#if defined(CONFIG_ENABLE_CHIP_MSG26XXM)
        if ((g_FirmwareMode == FIRMWARE_MODE_DEBUG_MODE) && (g_LogModePacket[0] == 0xA5 || g_LogModePacket[0] == 0xAB || g_LogModePacket[0] == 0xA7))
#elif defined(CONFIG_ENABLE_CHIP_MSG21XXA) || defined(CONFIG_ENABLE_CHIP_MSG22XX)
        if ((g_FirmwareMode == FIRMWARE_MODE_DEBUG_MODE || g_FirmwareMode == FIRMWARE_MODE_RAW_DATA_MODE) && (g_LogModePacket[0] == 0x62))
#endif
        {
            for (i = 0; i < g_FirmwareInfo.nLogModePacketLength; i ++)
            {
                pBuf[i] = g_LogModePacket[i];
            }

            nLength = g_FirmwareInfo.nLogModePacketLength;
            DBG("nLength = %d\n", nLength);
        }
        else
        {
            DBG("CURRENT MODE IS NOT DEBUG MODE/WRONG DEBUG MODE HEADER\n");
//        nLength = 0;
        }
    }
    else
    {
        DBG("g_LogModePacket is NULL\n");
//        nLength = 0;
    }

    return nLength;
}

ssize_t DrvMainFirmwarePacketStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    DBG("*** %s() ***\n", __func__);

    return nSize;
}

static DEVICE_ATTR(packet, SYSFS_AUTHORITY, DrvMainFirmwarePacketShow, DrvMainFirmwarePacketStore);
*/
ssize_t DrvMainFirmwareSensorShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    DBG("*** %s() ***\n", __func__);

#if defined(CONFIG_ENABLE_CHIP_MSG26XXM)
    if (g_FirmwareInfo.nLogModePacketHeader == 0xA5 || g_FirmwareInfo.nLogModePacketHeader == 0xAB)
    {
        return sprintf(pBuf, "%d,%d", g_FirmwareInfo.nMx, g_FirmwareInfo.nMy);
    }
    else if (g_FirmwareInfo.nLogModePacketHeader == 0xA7)
    {
        return sprintf(pBuf, "%d,%d,%d,%d", g_FirmwareInfo.nMx, g_FirmwareInfo.nMy, g_FirmwareInfo.nSs, g_FirmwareInfo.nSd);
    }
    else
    {
        DBG("Undefined debug mode packet format : 0x%x\n", g_FirmwareInfo.nLogModePacketHeader);
        return 0;
    }
#elif defined(CONFIG_ENABLE_CHIP_MSG21XXA) || defined(CONFIG_ENABLE_CHIP_MSG22XX)
    return sprintf(pBuf, "%d", g_FirmwareInfo.nLogModePacketLength);
#endif
}

ssize_t DrvMainFirmwareSensorStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    DBG("*** %s() ***\n", __func__);
/*
    DrvIcFwLyrGetFirmwareInfo(&g_FirmwareInfo);
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA) || defined(CONFIG_ENABLE_CHIP_MSG22XX)
    g_FirmwareMode = g_FirmwareInfo.nFirmwareMode;
#endif 
*/    
    return nSize;
}

static DEVICE_ATTR(sensor, SYSFS_AUTHORITY, DrvMainFirmwareSensorShow, DrvMainFirmwareSensorStore);

ssize_t DrvMainFirmwareHeaderShow(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
    DBG("*** %s() ***\n", __func__);

    return sprintf(pBuf, "%d", g_FirmwareInfo.nLogModePacketHeader);
}

ssize_t DrvMainFirmwareHeaderStore(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
    DBG("*** %s() ***\n", __func__);
/*
    DrvIcFwLyrGetFirmwareInfo(&g_FirmwareInfo);
#if defined(CONFIG_ENABLE_CHIP_MSG21XXA) || defined(CONFIG_ENABLE_CHIP_MSG22XX)
    g_FirmwareMode = g_FirmwareInfo.nFirmwareMode;
#endif
*/    
    return nSize;
}

static DEVICE_ATTR(header, SYSFS_AUTHORITY, DrvMainFirmwareHeaderShow, DrvMainFirmwareHeaderStore);

//------------------------------------------------------------------------------//

ssize_t DrvMainKObjectPacketShow(struct kobject *pKObj, struct kobj_attribute *pAttr, char *pBuf)
{
    u32 i = 0;
    u32 nLength = 0;

    DBG("*** %s() ***\n", __func__);

    if (strcmp(pAttr->attr.name, "packet") == 0)
    {
        if (g_LogModePacket != NULL)
        {
            DBG("g_FirmwareMode=%x, g_LogModePacket[0]=%x, g_LogModePacket[1]=%x\n", g_FirmwareMode, g_LogModePacket[0], g_LogModePacket[1]);
            DBG("g_LogModePacket[2]=%x, g_LogModePacket[3]=%x\n", g_LogModePacket[2], g_LogModePacket[3]);
            DBG("g_LogModePacket[4]=%x, g_LogModePacket[5]=%x\n", g_LogModePacket[4], g_LogModePacket[5]);

#if defined(CONFIG_ENABLE_CHIP_MSG26XXM)
            if ((g_FirmwareMode == FIRMWARE_MODE_DEBUG_MODE) && (g_LogModePacket[0] == 0xA5 || g_LogModePacket[0] == 0xAB || g_LogModePacket[0] == 0xA7))
#elif defined(CONFIG_ENABLE_CHIP_MSG21XXA) || defined(CONFIG_ENABLE_CHIP_MSG22XX)
            if ((g_FirmwareMode == FIRMWARE_MODE_DEBUG_MODE || g_FirmwareMode == FIRMWARE_MODE_RAW_DATA_MODE) && (g_LogModePacket[0] == 0x62))
#endif
            {
                for (i = 0; i < g_FirmwareInfo.nLogModePacketLength; i ++)
                {
                    pBuf[i] = g_LogModePacket[i];
                }

                nLength = g_FirmwareInfo.nLogModePacketLength;
                DBG("nLength = %d\n", nLength);
            }
            else
            {
                DBG("CURRENT MODE IS NOT DEBUG MODE/WRONG DEBUG MODE HEADER\n");
//            nLength = 0;
            }
        }
        else
        {
            DBG("g_LogModePacket is NULL\n");
//            nLength = 0;
        }
    }
    else
    {
        DBG("pAttr->attr.name = %s \n", pAttr->attr.name);
//        nLength = 0;
    }

    return nLength;
}

ssize_t DrvMainKObjectPacketStore(struct kobject *pKObj, struct kobj_attribute *pAttr, const char *pBuf, size_t nCount)
{
    DBG("*** %s() ***\n", __func__);
/*
    if (strcmp(pAttr->attr.name, "packet") == 0)
    {

    }
*/    	
    return nCount;
}

static struct kobj_attribute packet_attr = __ATTR(packet, 0666, DrvMainKObjectPacketShow, DrvMainKObjectPacketStore);

/* Create a group of attributes so that we can create and destroy them all at once. */
static struct attribute *attrs[] = {
    &packet_attr.attr,
    NULL,	/* need to NULL terminate the list of attributes */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory. If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
struct attribute_group attr_group = {
    .attrs = attrs,
};
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG

//------------------------------------------------------------------------------//

s32 DrvMainTouchDeviceInitialize(void)
{
    s32 nRetVal = 0;
#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
    u8 *pDevicePath = NULL;
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG
//modify by pangle for msg22xx add hardware info at 20150321 begin
#ifdef CONFIG_GET_HARDWARE_INFO
    u16 nMajor = 0, nMinor = 0;
    u8 *pVersion = NULL;
#endif
//modify by pangle for msg22xx add hardware info at 20150321 begin

    DBG("*** %s() ***\n", __func__);

    /* set sysfs for firmware */
    _gFirmwareClass = class_create(THIS_MODULE, "ms-touchscreen-msg20xx");
    if (IS_ERR(_gFirmwareClass))
        DBG("Failed to create class(firmware)!\n");

    _gFirmwareCmdDev = device_create(_gFirmwareClass, NULL, 0, NULL, "device");
    if (IS_ERR(_gFirmwareCmdDev))
        DBG("Failed to create device(_gFirmwareCmdDev)!\n");

    // version
    if (device_create_file(_gFirmwareCmdDev, &dev_attr_version) < 0)
        DBG("Failed to create device file(%s)!\n", dev_attr_version.attr.name);
    // update
    if (device_create_file(_gFirmwareCmdDev, &dev_attr_update) < 0)
        DBG("Failed to create device file(%s)!\n", dev_attr_update.attr.name);
    // data
    if (device_create_file(_gFirmwareCmdDev, &dev_attr_data) < 0)
        DBG("Failed to create device file(%s)!\n", dev_attr_data.attr.name);
#ifdef CONFIG_ENABLE_ITO_MP_TEST
    // test
    if (device_create_file(_gFirmwareCmdDev, &dev_attr_test) < 0)
        pr_err("Failed to create device file(%s)!\n", dev_attr_test.attr.name);
    // test_log
    if (device_create_file(_gFirmwareCmdDev, &dev_attr_test_log) < 0)
        pr_err("Failed to create device file(%s)!\n", dev_attr_test_log.attr.name);
    // test_fail_channel
    if (device_create_file(_gFirmwareCmdDev, &dev_attr_test_fail_channel) < 0)
        pr_err("Failed to create device file(%s)!\n", dev_attr_test_fail_channel.attr.name);
	//add test_scope by shihuijun 20150205 start
	//test_scope
	 if (device_create_file(_gFirmwareCmdDev, &dev_attr_test_scope) < 0)
        pr_err("Failed to create device file(%s)!\n", dev_attr_test_scope.attr.name);
	 //add test_scope by shihuijun 20150205 end
#endif //CONFIG_ENABLE_ITO_MP_TEST

#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
    // mode
    if (device_create_file(_gFirmwareCmdDev, &dev_attr_mode) < 0)
        DBG("Failed to create device file(%s)!\n", dev_attr_mode.attr.name);
    // packet
//    if (device_create_file(_gFirmwareCmdDev, &dev_attr_packet) < 0)
//        DBG("Failed to create device file(%s)!\n", dev_attr_packet.attr.name);
    // sensor
    if (device_create_file(_gFirmwareCmdDev, &dev_attr_sensor) < 0)
        DBG("Failed to create device file(%s)!\n", dev_attr_sensor.attr.name);
    // header
    if (device_create_file(_gFirmwareCmdDev, &dev_attr_header) < 0)
        DBG("Failed to create device file(%s)!\n", dev_attr_header.attr.name);
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG

    // debug
    if (device_create_file(_gFirmwareCmdDev, &dev_attr_debug) < 0)
        DBG("Failed to create device file(%s)!\n", dev_attr_debug.attr.name);
    // platform version
    if (device_create_file(_gFirmwareCmdDev, &dev_attr_platform_version) < 0)
        DBG("Failed to create device file(%s)!\n", dev_attr_platform_version.attr.name);
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    // gesture wakeup mode
    if (device_create_file(_gFirmwareCmdDev, &dev_attr_gesture_wakeup_mode) < 0)
        DBG("Failed to create device file(%s)!\n", dev_attr_gesture_wakeup_mode.attr.name);
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
	// gesture information mode
if (device_create_file(_gFirmwareCmdDev, &dev_attr_gesture_infor) < 0)
	DBG("Failed to create device file(%s)!\n", dev_attr_gesture_infor.attr.name);
#endif //CONFIG_ENABLE_GESTURE_INFORMATION_MODE
#endif //CONFIG_ENABLE_GESTURE_WAKEUP
    // chip type
    if (device_create_file(_gFirmwareCmdDev, &dev_attr_chip_type) < 0)
        DBG("Failed to create device file(%s)!\n", dev_attr_chip_type.attr.name);
    // driver version
    if (device_create_file(_gFirmwareCmdDev, &dev_attr_driver_version) < 0)
        DBG("Failed to create device file(%s)!\n", dev_attr_driver_version.attr.name);

    dev_set_drvdata(_gFirmwareCmdDev, NULL);

#ifdef CONFIG_ENABLE_ITO_MP_TEST
    DrvIcFwLyrCreateMpTestWorkQueue();

//add by shihuijun for factory test 20150309 start
    #if defined(CONFIG_ITO_TEST_BY_PROC)
    	MsgCreateFileForTestProc();
    #endif
#endif //CONFIG_ENABLE_ITO_MP_TEST
//add by shihuijun for factory test 20150309 end 

#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
    /* create a kset with the name of "kset_example" which is located under /sys/kernel/ */
    g_TouchKSet = kset_create_and_add("kset_example", NULL, kernel_kobj);
    if (!g_TouchKSet)
    {
        DBG("*** kset_create_and_add() failed, nRetVal = %d ***\n", nRetVal);

        nRetVal = -ENOMEM;
    }

    g_TouchKObj = kobject_create();
    if (!g_TouchKObj)
    {
        DBG("*** kobject_create() failed, nRetVal = %d ***\n", nRetVal);

        nRetVal = -ENOMEM;
		    kset_unregister(g_TouchKSet);
		    g_TouchKSet = NULL;
    }

    g_TouchKObj->kset = g_TouchKSet;

    nRetVal = kobject_add(g_TouchKObj, NULL, "%s", "kobject_example");
    if (nRetVal != 0)
    {
        DBG("*** kobject_add() failed, nRetVal = %d ***\n", nRetVal);

		    kobject_put(g_TouchKObj);
		    g_TouchKObj = NULL;
		    kset_unregister(g_TouchKSet);
		    g_TouchKSet = NULL;
    }
    
    /* create the files associated with this kobject */
    nRetVal = sysfs_create_group(g_TouchKObj, &attr_group);
    if (nRetVal != 0)
    {
        DBG("*** sysfs_create_file() failed, nRetVal = %d ***\n", nRetVal);

        kobject_put(g_TouchKObj);
		    g_TouchKObj = NULL;
		    kset_unregister(g_TouchKSet);
		    g_TouchKSet = NULL;
    }
    
    pDevicePath = kobject_get_path(g_TouchKObj, GFP_KERNEL);
    DBG("DEVPATH = %s\n", pDevicePath);
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG

    g_ChipType = DrvIcFwLyrGetChipType();

    if (g_ChipType != 0) // To make sure TP is attached on cell phone.
    {
#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
#if defined(CONFIG_ENABLE_CHIP_MSG26XXM)
    // get firmware mode for parsing packet judgement.
    g_FirmwareMode = DrvIcFwLyrGetFirmwareMode();
#endif //CONFIG_ENABLE_CHIP_MSG26XXM

    memset(&g_FirmwareInfo, 0x0, sizeof(FirmwareInfo_t));

    DrvIcFwLyrGetFirmwareInfo(&g_FirmwareInfo);

#if defined(CONFIG_ENABLE_CHIP_MSG21XXA) || defined(CONFIG_ENABLE_CHIP_MSG22XX)
    g_FirmwareMode = g_FirmwareInfo.nFirmwareMode;
#endif
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG

#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
    DrvIcFwLyrCheckFirmwareUpdateBySwId();
#endif //CONFIG_UPDATE_FIRMWARE_BY_SW_ID
    }
    else
    {
    		nRetVal = -ENODEV;
    }
//modify by pangle for msg22xx add hardware info at 20150321 begin
#ifdef CONFIG_GET_HARDWARE_INFO

DrvFwCtrlGetCustomerFirmwareVersion(&nMajor, &nMinor, &pVersion);
	if((nMajor == 3)||(nMajor == 2))	// major == 3 Yeji
	{
		memset(tmp_str, 0, sizeof(tmp_str));
		strcpy(tmp_str, "msg2238 Yeji CTP-");
		sprintf(tmp_str+strlen("msg2238 Yeji CTP-"), "%03d%03d",
		nMajor, nMinor);
		register_hardware_info(CTP, tmp_str);
	}
	else if(nMajor == 4)	// major == 3 Yeji
	{
		memset(tmp_str, 0, sizeof(tmp_str));
		strcpy(tmp_str, "msg2238 Mudong CTP-");
		sprintf(tmp_str+strlen("msg2238 Mudong CTP-"), "%03d%03d",
		nMajor, nMinor);
		register_hardware_info(CTP, tmp_str);
	}
	//modify by shihuijun for hardware_info,add judge Yeji vendor id = 3 20141029 end
#endif
//modify by pangle for msg22xx add hardware info at 20150321 end


    return nRetVal;
}

//------------------------------------------------------------------------------//