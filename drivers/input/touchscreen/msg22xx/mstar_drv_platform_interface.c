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
 * @file    mstar_drv_platform_interface.c
 *
 * @brief   This file defines the interface of touch screen
 *
 * @version v2.2.0.0
 *
 */

/*=============================================================*/
// INCLUDE FILE
/*=============================================================*/

#include "mstar_drv_platform_interface.h"
#include "mstar_drv_main.h"
#include "mstar_drv_ic_fw_porting_layer.h"
#include "mstar_drv_platform_porting_layer.h"

/*=============================================================*/
// EXTERN VARIABLE DECLARATION
/*=============================================================*/

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
extern u16 g_GestureWakeupMode;
extern u8 g_GestureWakeupFlag;
#endif //CONFIG_ENABLE_GESTURE_WAKEUP

extern int _gIrq;
/*=============================================================*/
// GLOBAL VARIABLE DEFINITION
/*=============================================================*/
int tp_suspend_flag = 0;
#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
extern int program_over;
#endif
extern struct input_dev *g_InputDevice;
//add by shihuijun for modifing resume and suspend after TP 20150403 start 
#if defined(CONFIG_FB)
static struct delayed_work pm_work;
static struct workqueue_struct *pm_wq;
static spinlock_t pm_lock;
static int mstar_blank = 0;
#endif
//add by shihuijun for modifing resume and suspend after TP 20150403 end
/*=============================================================*/
// GLOBAL FUNCTION DEFINITION
/*=============================================================*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
void MsDrvInterfaceTouchDeviceSuspend(struct early_suspend *pSuspend)
{
    DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
//    g_GestureWakeupMode = 0x1FFF; // Enable all gesture wakeup mode for testing 

    if (g_GestureWakeupMode != 0x0000)
    {
        DrvIcFwLyrOpenGestureWakeup(g_GestureWakeupMode);
        return;
    }
#endif //CONFIG_ENABLE_GESTURE_WAKEUP

    DrvPlatformLyrFingerTouchReleased(0, 0); // Send touch end for clearing point touch
    input_sync(g_InputDevice);

    DrvPlatformLyrDisableFingerTouchReport();
    DrvPlatformLyrTouchDevicePowerOff(); 
}

void MsDrvInterfaceTouchDeviceResume(struct early_suspend *pSuspend)
{
    DBG("*** %s() ***\n", __func__);

    if(program_over ==0)
		return;
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    if (g_GestureWakeupFlag == 1)
    {
        DrvIcFwLyrCloseGestureWakeup();
    }
    else
    {
        DrvPlatformLyrEnableFingerTouchReport(); 
    }
#endif //CONFIG_ENABLE_GESTURE_WAKEUP
    
    DrvPlatformLyrTouchDevicePowerOn();
/*
    DrvPlatformLyrFingerTouchReleased(0, 0);
    input_sync(g_InputDevice);
*/    
#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
    DrvIcFwLyrRestoreFirmwareModeToLogDataMode();
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG

#ifndef CONFIG_ENABLE_GESTURE_WAKEUP
    DrvPlatformLyrEnableFingerTouchReport(); 
#endif //CONFIG_ENABLE_GESTURE_WAKEUP
}
#endif

//added by litao for fb 2014-11-17 begin
#if defined(CONFIG_FB)
void MsDrvInterfaceTouchDeviceSuspend(void)
{
    
    DBG("*** %s() ***\n", __func__);
	
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	u16 wkup_mode = 0;
#endif
#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
    if(program_over ==0)
		return;
#endif
    if(tp_suspend_flag){
        DBG("*** Mstar is already suspended! ***\n");
        return;
    }
    tp_suspend_flag = 1;
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
//    g_GestureWakeupMode = 0x1FFF; // litao Enable all gesture wakeup mode for testing 20150120

    if (g_GestureWakeupMode != 0x0000)
    {
	 wkup_mode = 0x1FFF;
	 DrvPlatformLyrDisableFingerTouchReport();
        DrvIcFwLyrOpenGestureWakeup(wkup_mode);
        return;
    }
#endif //CONFIG_ENABLE_GESTURE_WAKEUP

    DrvPlatformLyrFingerTouchReleased(0, 0); // Send touch end for clearing point touch
    input_sync(g_InputDevice);

    DrvPlatformLyrDisableFingerTouchReport();
    DrvPlatformLyrTouchDevicePowerOff(); 
}

void MsDrvInterfaceTouchDeviceResume(void)
{
    DBG("*** %s() ***\n", __func__);
#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
    if(program_over ==0)
		return;
#endif
    if(!tp_suspend_flag){
        DBG("*** Mstar is already resumed! ***\n");
        return;
    }
    tp_suspend_flag = 0;
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    if (g_GestureWakeupFlag == 1)
    {
        DrvIcFwLyrCloseGestureWakeup();
//	 printk(KERN_ERR"Msg2238 enter DrvIcFwLyrCloseGestureWakeup\n");
    }
    else
    {
        DrvPlatformLyrEnableFingerTouchReport(); 
    }
#endif //CONFIG_ENABLE_GESTURE_WAKEUP
    
    DrvPlatformLyrTouchDevicePowerOn();
/*
    DrvPlatformLyrFingerTouchReleased(0, 0);
    input_sync(g_InputDevice);
*/    
#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
    DrvIcFwLyrRestoreFirmwareModeToLogDataMode();
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG

#ifndef CONFIG_ENABLE_GESTURE_WAKEUP
    DrvPlatformLyrEnableFingerTouchReport(); 
#endif //CONFIG_ENABLE_GESTURE_WAKEUP
}
//add by shihuijun for modifing resume and suspend after TP 20150403 start 
static void mstar_pm_worker(struct work_struct *work)
{
    DBG("*** %s() ***\n", __func__);
	if (mstar_blank == FB_BLANK_UNBLANK)
		MsDrvInterfaceTouchDeviceResume();
	else if (mstar_blank == FB_BLANK_POWERDOWN)
		MsDrvInterfaceTouchDeviceSuspend();
}
//add by shihuijun for modifing resume and suspend after TP 20150403 end 
int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
//add by shihuijun for modifing resume and suspend after TP 20150403 start 
	unsigned long irqflags;
	DBG("*** %s() ***\n", __func__);

	if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		spin_lock_irqsave(&pm_lock, irqflags);
		mstar_blank = *blank;

		queue_delayed_work(pm_wq, &pm_work, msecs_to_jiffies(120));
		spin_unlock_irqrestore(&pm_lock, irqflags);
#if 0
		if (*blank == FB_BLANK_UNBLANK)
			MsDrvInterfaceTouchDeviceResume();
		else if (*blank == FB_BLANK_POWERDOWN)
			MsDrvInterfaceTouchDeviceSuspend();
#endif
//add by shihuijun for modifing resume and suspend after TP 20150403 end
	}

	return 0;
}
#endif
//added by litao for fb 2014-11-17 end

/* probe function is used for matching and initializing input device */
s32 /*__devinit*/ MsDrvInterfaceTouchDeviceProbe(struct i2c_client *pClient, const struct i2c_device_id *pDeviceId)
{
    s32 nRetVal = 0;

    DBG("*** %s() ***\n", __func__);

    DrvPlatformLyrInputDeviceInitialize(pClient);
  
    DrvPlatformLyrTouchDeviceRequestGPIO();
	
    DrvPlatformLyrTouchDeviceRegisterFingerTouchInterruptHandler();
	
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
    DrvPlatformLyrTouchDeviceRegulatorPowerOn();
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON

    DrvPlatformLyrTouchDevicePowerOn();

    nRetVal = DrvMainTouchDeviceInitialize();
    if (nRetVal == -ENODEV)
    {
        DrvPlatformLyrTouchDeviceRemove(pClient);
        return nRetVal;
    }
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
    mstar_create_proc();
#endif
#endif


#if defined(CONFIG_FB)
//add by shihuijun for modifing resume and suspend after TP 20150403 start 
    spin_lock_init(&pm_lock);
    INIT_DELAYED_WORK(&pm_work, mstar_pm_worker);
    pm_wq = create_singlethread_workqueue("mstar_pm_wq");
//add by shihuijun for modifing resume and suspend after TP 20150403 end 
    call_back_functions_init();
#endif

#ifndef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
    DrvPlatformLyrEnableFingerTouchReport();
#endif	
    DBG("*** MStar touch driver registered ***\n");
//modify by pangle at 20150228 begin
#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
	 program_over = 1;
#endif
//modify by pangle at 20150228 end
    return nRetVal;
}

/* remove function is triggered when the input device is removed from input sub-system */
s32 /*__devexit*/ MsDrvInterfaceTouchDeviceRemove(struct i2c_client *pClient)
{
    DBG("*** %s() ***\n", __func__);

    return DrvPlatformLyrTouchDeviceRemove(pClient);
}

void MsDrvInterfaceTouchDeviceSetIicDataRate(struct i2c_client *pClient, u32 nIicDataRate)
{
    DBG("*** %s() ***\n", __func__);

    DrvPlatformLyrSetIicDataRate(pClient, nIicDataRate);
}    
