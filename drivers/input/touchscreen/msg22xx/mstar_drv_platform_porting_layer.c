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
 * @file    mstar_drv_platform_porting_layer.c
 *
 * @brief   This file defines the interface of touch screen
 *
 * @version v2.2.0.0
 *
 */
 
/*=============================================================*/
// INCLUDE FILE
/*=============================================================*/

#include "mstar_drv_platform_porting_layer.h"
#include "mstar_drv_ic_fw_porting_layer.h"
#include "mstar_drv_platform_interface.h"
#include "mstar_drv_utility_adaption.h"   //add by pangle for suspend function at 20150401
#include <linux/input/mt.h>   //modify by pangle for report typed B at 20150417



/*=============================================================*/
// EXTREN VARIABLE DECLARATION
/*=============================================================*/

#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
extern struct kset *g_TouchKSet;
extern struct kobject *g_TouchKObj;
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG
extern u16 g_GestureWakeupMode;
extern int tp_suspend_flag;
#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
extern struct tpd_device *tpd;
#endif //CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM

extern u8 g_ChipType;  //add by pangle for suspend function at 20150401
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
extern struct regulator *g_ReguVdd;
extern struct regulator *g_ReguVcc_i2c;

extern int mstar_reset_gpio;
extern int mstar_irq_gpio;
#endif

/*=============================================================*/
// LOCAL VARIABLE DEFINITION
/*=============================================================*/

struct mutex g_Mutex;
static struct work_struct _gFingerTouchWork;
static struct wake_lock mstar_wake_lock;
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
#if defined(CONFIG_HAS_EARLYSUSPEND)
static struct early_suspend _gEarlySuspend;
#endif
#endif

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifndef CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM
static DECLARE_WAIT_QUEUE_HEAD(_gWaiter);
static struct task_struct *_gThread = NULL;
static int _gTpdFlag = 0;
#endif //CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM
#endif //CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM

/*=============================================================*/
// GLOBAL VARIABLE DEFINITION
/*=============================================================*/

//modify by pangle for back_menu key at 20150512
#ifdef CONFIG_TP_HAVE_KEY
const int g_TpVirtualKey[] = {TOUCH_KEY_MENU, TOUCH_KEY_HOME, TOUCH_KEY_BACK, TOUCH_KEY_BACK_MENU};

#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
#define BUTTON_W (100)
#define BUTTON_H (100)

const int g_TpVirtualKeyDimLocal[MAX_KEY_NUM][4] = {{BUTTON_W/2*1,TOUCH_SCREEN_Y_MAX+BUTTON_H/2,BUTTON_W,BUTTON_H},{BUTTON_W/2*3,TOUCH_SCREEN_Y_MAX+BUTTON_H/2,BUTTON_W,BUTTON_H},{BUTTON_W/2*5,TOUCH_SCREEN_Y_MAX+BUTTON_H/2,BUTTON_W,BUTTON_H},{BUTTON_W/2*7,TOUCH_SCREEN_Y_MAX+BUTTON_H/2,BUTTON_W,BUTTON_H}};
#endif //CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
#endif //CONFIG_TP_HAVE_KEY

struct input_dev *g_InputDevice = NULL;
int _gIrq = -1;

/*
Added by Litao for enable irq or disable irq protect,avoid to make logic error,the irq was always disabled.
*/
static spinlock_t irq_lock;
static s32 irq_is_disable = 0;
static s32 irq_wake_is_disable = 0;

//added by litao for fb callback 2014-11-17 begin
#if defined(CONFIG_FB)
struct notifier_block fb_notif;
#endif
//added by litao for fb callback 2014-11-17 end

/*=============================================================*/
// LOCAL FUNCTION DEFINITION
/*=============================================================*/

/* read data through I2C then report data to input sub-system when interrupt occurred */
static void _DrvPlatformLyrFingerTouchDoWork(struct work_struct *pWork)
{
//    DBG("*** %s() ***\n", __func__);
//    printk(KERN_ERR"Litao Enter irq work %s begin \n",__func__);
    mutex_lock(&g_Mutex);

    DrvIcFwLyrHandleFingerTouch(NULL, 0);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
//    enable_irq(mstar_irq_gpio);
    DrvPlatformLyrEnableFingerTouchReport();
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
#endif

    mutex_unlock(&g_Mutex);
//    printk(KERN_ERR"Litao Exit irq work %s end \n",__func__);
}

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
/* The interrupt service routine will be triggered when interrupt occurred */
static irqreturn_t _DrvPlatformLyrFingerTouchInterruptHandler(s32 nIrq, void *pDeviceId)
{
    DrvPlatformLyrDisableFingerTouchReport();
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    if((tp_suspend_flag == 1)&&(g_GestureWakeupMode != 0))
    	wake_lock_timeout(&mstar_wake_lock, 5*HZ);
#endif
    schedule_work(&_gFingerTouchWork);

    return IRQ_HANDLED;
}
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
static void _DrvPlatformLyrFingerTouchInterruptHandler(void)
{
#ifdef CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
    schedule_work(&_gFingerTouchWork);
#else    
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

    _gTpdFlag = 1;
    wake_up_interruptible(&_gWaiter); 
#endif //CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM
}

static int _DrvPlatformLyrFingerTouchHandler(void *pUnUsed)
{
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
    sched_setscheduler(current, SCHED_RR, &param);
	
    do
    {
        set_current_state(TASK_INTERRUPTIBLE);
        wait_event_interruptible(_gWaiter, _gTpdFlag != 0);
        _gTpdFlag = 0;
        
        set_current_state(TASK_RUNNING);

        mutex_lock(&g_Mutex);

        DrvIcFwLyrHandleFingerTouch(NULL, 0);

        mutex_unlock(&g_Mutex);

        mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
		
    } while (!kthread_should_stop());
	
    return 0;
}
#endif

/*=============================================================*/
// GLOBAL FUNCTION DEFINITION
/*=============================================================*/

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
void DrvPlatformLyrTouchDeviceRegulatorPowerOn(void)
{
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    s32 nRetVal = 0;

    DBG("*** %s() ***\n", __func__);
    
    nRetVal = regulator_set_voltage(g_ReguVdd, 2850000, 2850000); // For specific SPRD BB chip(ex. SC7715) or QCOM BB chip, need to enable this function call for correctly power on Touch IC.

    if (nRetVal)
    {
        DBG("Could not set to 2800mv.\n");
    }

    nRetVal = regulator_set_voltage(g_ReguVcc_i2c, 1800000, 1800000); // For specific SPRD BB chip(ex. SC7715) or QCOM BB chip, need to enable this function call for correctly power on Touch IC.

    if (nRetVal)
    {
        DBG("Could not set to 1800mv.\n");
    }

	nRetVal = regulator_enable(g_ReguVdd);
	if (nRetVal) {
//		dev_err(&msg21xx_i2c_client->dev,
//			"Regulator vdd enable failed nRetVal=%d\n", nRetVal);
		return;
	}

	nRetVal = regulator_enable(g_ReguVcc_i2c);
	if (nRetVal) {
//		dev_err(&msg21xx_i2c_client->dev,
//			"Regulator vcc_i2c enable failed nRetVal=%d\n", nRetVal);
		//regulator_disable(msg->vdd);
		return;
	}

//    regulator_enable(g_ReguVdd);
//    regulator_enable(g_ReguVcc_i2c);

    mdelay(20); //mdelay(100);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    hwPowerOn(MT6323_POWER_LDO_VGP1, VOL_2800, "TP"); // For specific MTK BB chip(ex. MT6582), need to enable this function call for correctly power on Touch IC.
#endif
}
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON
//add by pangle for suspend function at 20150401 begin
void DrvFwCtrlOptimizeCurrentConsumption(void)
{
    u32 i;
    u8 szDbBusTxData[27] = {0};

    DBG("g_ChipType = 0x%x\n", g_ChipType);
    

    if (g_ChipType == CHIP_TYPE_MSG22XX)
    {
        DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
        DmaAlloc();
#endif //CONFIG_ENABLE_DMA_IIC
#endif //CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM

        DrvPlatformLyrTouchDeviceResetHw(); 

        DbBusEnterSerialDebugMode();
        DbBusStopMCU();
        DbBusIICUseBus();
        DbBusIICReshape();

        RegSet16BitValue(0x1618, (RegGet16BitValue(0x1618) | 0x80));

        // Enable burst mode
        RegSet16BitValue(0x160C, (RegGet16BitValue(0x160C) | 0x01));

        szDbBusTxData[0] = 0x10;
        szDbBusTxData[1] = 0x11;
        szDbBusTxData[2] = 0xA0; //bank:0x11, addr:h0050
    
        for (i = 0; i < 24; i ++)
        {
            szDbBusTxData[i+3] = 0x11;
        }
		
        IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3+24);  // write 0x1111 for reg 0x1150~0x115B

        szDbBusTxData[0] = 0x10;
        szDbBusTxData[1] = 0x11;
        szDbBusTxData[2] = 0xB8; //bank:0x11, addr:h005C
    
        for (i = 0; i < 6; i ++)
        {
            szDbBusTxData[i+3] = 0xFF;
        }
		
        IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3+6);   // Write 0xFF for reg 0x115C~0x115E 
    
        // Clear burst mode
        RegSet16BitValue(0x160C, RegGet16BitValue(0x160C) & (~0x01)); 
    
        DbBusIICNotUseBus();
        DbBusNotStopMCU();
        DbBusExitSerialDebugMode();

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
        DmaFree();
#endif //CONFIG_ENABLE_DMA_IIC
#endif //CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
    }
}
//add by pangle for suspend function at 20150401 end

void DrvPlatformLyrTouchDevicePowerOn(void)
{
    DBG("*** %s() ***\n", __func__);
    
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    gpio_direction_output(mstar_reset_gpio, 1);
//    gpio_set_value(MS_TS_MSG_IC_GPIO_RST, 1); 
//    mdelay(100);
    gpio_set_value(mstar_reset_gpio, 0);
    mdelay(10);
    gpio_set_value(mstar_reset_gpio, 1);
    mdelay(300);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    mt_set_gpio_mode(mstar_reset_gpio, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(mstar_reset_gpio, GPIO_DIR_OUT);
    mt_set_gpio_out(mstar_reset_gpio, GPIO_OUT_ZERO);  

#ifdef TPD_CLOSE_POWER_IN_SLEEP
    hwPowerDown(TPD_POWER_SOURCE, "TP"); 
    mdelay(100);
    hwPowerOn(TPD_POWER_SOURCE, VOL_2800, "TP"); 
    mdelay(10);  // reset pulse
#endif //TPD_CLOSE_POWER_IN_SLEEP

    mt_set_gpio_out(mstar_reset_gpio, GPIO_OUT_ONE);
    mdelay(180); // wait stable
#endif
}

void DrvPlatformLyrTouchDevicePowerOff(void)
{
    DBG("*** %s() ***\n", __func__);
	
	DrvFwCtrlOptimizeCurrentConsumption ();  //add by pangle for suspend function at 20150401 
    
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
//    gpio_direction_output(MS_TS_MSG_IC_GPIO_RST, 0);
    gpio_set_value(mstar_reset_gpio, 0);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    mt_set_gpio_mode(mstar_reset_gpio, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(mstar_reset_gpio, GPIO_DIR_OUT);
    mt_set_gpio_out(mstar_reset_gpio, GPIO_OUT_ZERO);  
#ifdef TPD_CLOSE_POWER_IN_SLEEP
    hwPowerDown(TPD_POWER_SOURCE, "TP");
#endif //TPD_CLOSE_POWER_IN_SLEEP
#endif    
}

void DrvPlatformLyrTouchDeviceResetHw(void)
{
    DBG("*** %s() ***\n", __func__);
    
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    gpio_direction_output(mstar_reset_gpio, 1);
//    gpio_set_value(MS_TS_MSG_IC_GPIO_RST, 1); 
    gpio_set_value(mstar_reset_gpio, 0);
    mdelay(50); //100
    gpio_set_value(mstar_reset_gpio, 1);
    mdelay(200); //100
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    mt_set_gpio_mode(mstar_reset_gpio, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(mstar_reset_gpio, GPIO_DIR_OUT);
    mt_set_gpio_out(mstar_reset_gpio, GPIO_OUT_ONE);
    mdelay(10);
    mt_set_gpio_mode(mstar_reset_gpio, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(mstar_reset_gpio, GPIO_DIR_OUT);
    mt_set_gpio_out(mstar_reset_gpio, GPIO_OUT_ZERO);  
    mdelay(50);
    mt_set_gpio_mode(mstar_reset_gpio, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(mstar_reset_gpio, GPIO_DIR_OUT);
    mt_set_gpio_out(mstar_reset_gpio, GPIO_OUT_ONE);
    mdelay(50); 
#endif
}

void DrvPlatformLyrDisableFingerTouchReport(void)
{
    unsigned long irqflags;
//    DBG("*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    spin_lock_irqsave(&irq_lock, irqflags);
    if(!irq_is_disable){
        irq_is_disable = 1;
        disable_irq_nosync(_gIrq);		
    }
    spin_unlock_irqrestore(&irq_lock, irqflags);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif
}

void DrvPlatformLyrEnableFingerTouchReport(void)
{
    	unsigned long irqflags;
//    DBG("*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    spin_lock_irqsave(&irq_lock, irqflags);
    if(irq_is_disable){
        irq_is_disable = 0;
        enable_irq(_gIrq);		
    }
    spin_unlock_irqrestore(&irq_lock, irqflags);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif
}

//added by litao for irq wake function begin 
void DrvPlatformLyrDisableFingerTouchIrqWake(void)
{
    unsigned long irqflags;
    DBG("*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    spin_lock_irqsave(&irq_lock, irqflags);
    if(!irq_wake_is_disable){
        irq_wake_is_disable = 1;
        disable_irq_wake(_gIrq);		
    }
    spin_unlock_irqrestore(&irq_lock, irqflags);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif
}

void DrvPlatformLyrEnableFingerTouchIrqWake(void)
{
    	unsigned long irqflags;
    DBG("*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    spin_lock_irqsave(&irq_lock, irqflags);
    if(irq_wake_is_disable){
        irq_wake_is_disable = 0;
        enable_irq_wake(_gIrq);		
    }
    spin_unlock_irqrestore(&irq_lock, irqflags);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif
}
//added by litao for irq wake function end 

void DrvPlatformLyrFingerTouchPressed(s32 nX, s32 nY, s32 nPressure, s32 nId)
{
//    DBG("*** %s() ***\n", __func__);
//    DBG("point touch pressed\n");

    input_report_key(g_InputDevice, BTN_TOUCH, 1);
#if defined(CONFIG_ENABLE_CHIP_MSG26XXM)
    input_report_abs(g_InputDevice, ABS_MT_TRACKING_ID, nId);
#endif //CONFIG_ENABLE_CHIP_MSG26XXM
    input_report_abs(g_InputDevice, ABS_MT_TOUCH_MAJOR, 1);
    input_report_abs(g_InputDevice, ABS_MT_WIDTH_MAJOR, 1);
    input_report_abs(g_InputDevice, ABS_MT_POSITION_X, nX);
    input_report_abs(g_InputDevice, ABS_MT_POSITION_Y, nY);

    //input_mt_sync(g_InputDevice);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_TP_HAVE_KEY    
    if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
    {   
        tpd_button(nX, nY, 1);  
    }
#endif //CONFIG_TP_HAVE_KEY

    TPD_EM_PRINT(nX, nY, nX, nY, nPressure-1, 1);
#endif //CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
}

void DrvPlatformLyrFingerTouchReleased(s32 nX, s32 nY)
{
//    DBG("*** %s() ***\n", __func__);
//    DBG("point touch released\n");
//modify by pangle for report typed B at 20150417 begin
#ifdef CHANGE_REPORT_TYPE_TO_B   // add by byron 
	u8 i;
	for(i=0;i<2;i++)
	{
		input_mt_slot(g_InputDevice, i);
		input_report_abs(g_InputDevice, ABS_MT_TRACKING_ID, -1);	
	}
#endif
    input_report_key(g_InputDevice, BTN_TOUCH, 0);
    //input_mt_sync(g_InputDevice);
 //modify by pangle for report typed B at 20150417 end

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_TP_HAVE_KEY 
    if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode())
    {   
       tpd_button(nX, nY, 0); 
//       tpd_button(0, 0, 0); 
    }            
#endif //CONFIG_TP_HAVE_KEY    

    TPD_EM_PRINT(nX, nY, nX, nY, 0, 0);
#endif //CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
}

//added by litao for fb callback 2014-11-17 begin
#if defined(CONFIG_FB)
void call_back_functions_init(void)
{
	s32 ret = 0;

	fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&fb_notif);
	if (ret)
    		DBG("*** %s() Unable to register fb_notifier***\n", __func__);
}
#endif
//added by litao for fb callback 2014-11-17 end

s32 DrvPlatformLyrInputDeviceInitialize(struct i2c_client *pClient)
{
    s32 nRetVal = 0;

    DBG("*** %s() ***\n", __func__);

    mutex_init(&g_Mutex);
    spin_lock_init(&irq_lock);
    wake_lock_init(&mstar_wake_lock, WAKE_LOCK_SUSPEND, "mstar_wake_lock");
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    /* allocate an input device */
    g_InputDevice = input_allocate_device();
    if (g_InputDevice == NULL)
    {
        DBG("*** input device allocation failed ***\n");
        return -ENOMEM;
    }

    g_InputDevice->name = pClient->name;
    g_InputDevice->phys = "I2C";
    g_InputDevice->dev.parent = &pClient->dev;
    g_InputDevice->id.bustype = BUS_I2C;
    
    /* set the supported event type for input device */
    set_bit(EV_ABS, g_InputDevice->evbit);
    set_bit(EV_SYN, g_InputDevice->evbit);
    set_bit(EV_KEY, g_InputDevice->evbit);
    set_bit(BTN_TOUCH, g_InputDevice->keybit);
    set_bit(INPUT_PROP_DIRECT, g_InputDevice->propbit);

#ifdef CONFIG_TP_HAVE_KEY
    {
        u32 i;
        for (i = 0; i < MAX_KEY_NUM; i ++)
        {
            input_set_capability(g_InputDevice, EV_KEY, g_TpVirtualKey[i]);
        }
		input_set_capability(g_InputDevice, EV_KEY, KEY_F10);	//modify by pangle for back_menu key 20150512
    }
#endif

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    input_set_capability(g_InputDevice, EV_KEY, KEY_ENTER);  //modify by pangle for double click event flow
    input_set_capability(g_InputDevice, EV_KEY, KEY_UP);
    input_set_capability(g_InputDevice, EV_KEY, KEY_DOWN);
    input_set_capability(g_InputDevice, EV_KEY, KEY_LEFT);
    input_set_capability(g_InputDevice, EV_KEY, KEY_RIGHT);
//modify  by pangle for gesture display 20150304 begin
#ifdef GESTURE_ALL_SWITCH
    input_set_capability(g_InputDevice, EV_KEY, KEY_W);
    input_set_capability(g_InputDevice, EV_KEY, KEY_Z);
    input_set_capability(g_InputDevice, EV_KEY, KEY_V);
    input_set_capability(g_InputDevice, EV_KEY, KEY_O);
    input_set_capability(g_InputDevice, EV_KEY, KEY_M);
    input_set_capability(g_InputDevice, EV_KEY, KEY_C);
    input_set_capability(g_InputDevice, EV_KEY, KEY_E);
    input_set_capability(g_InputDevice, EV_KEY, KEY_S);
#endif
//modify  by pangle for gesture display 20150304 end
#endif //CONFIG_ENABLE_GESTURE_WAKEUP

/*
#ifdef CONFIG_TP_HAVE_KEY
    set_bit(TOUCH_KEY_MENU, g_InputDevice->keybit); //Menu
    set_bit(TOUCH_KEY_HOME, g_InputDevice->keybit); //Home
    set_bit(TOUCH_KEY_BACK, g_InputDevice->keybit); //Back
    set_bit(TOUCH_KEY_SEARCH, g_InputDevice->keybit); //Search
#endif
*/

#if defined(CONFIG_ENABLE_CHIP_MSG26XXM)
    input_set_abs_params(g_InputDevice, ABS_MT_TRACKING_ID, 0, 255, 0, 0);
#endif //CONFIG_ENABLE_CHIP_MSG26XXM
//modify by pangle for report typed B at 20150417 begin
#ifdef CHANGE_REPORT_TYPE_TO_B
			input_set_abs_params(g_InputDevice, ABS_MT_TRACKING_ID, 0, 255, 0, 0);
			input_mt_init_slots(g_InputDevice, 5,0);
#endif
//modify by pangle for report typed B at 20150417 end
    input_set_abs_params(g_InputDevice, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(g_InputDevice, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
    input_set_abs_params(g_InputDevice, ABS_MT_POSITION_X, TOUCH_SCREEN_X_MIN, TOUCH_SCREEN_X_MAX, 0, 0);
    input_set_abs_params(g_InputDevice, ABS_MT_POSITION_Y, TOUCH_SCREEN_Y_MIN, TOUCH_SCREEN_Y_MAX, 0, 0);

    /* register the input device to input sub-system */
    nRetVal = input_register_device(g_InputDevice);
    if (nRetVal < 0)
    {
        DBG("*** Unable to register touch input device ***\n");
    }
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    g_InputDevice = tpd->dev;

//    g_InputDevice->name = pClient->name;
    g_InputDevice->phys = "I2C";
    g_InputDevice->dev.parent = &pClient->dev;
    g_InputDevice->id.bustype = BUS_I2C;
    
    /* set the supported event type for input device */
    set_bit(EV_ABS, g_InputDevice->evbit);
    set_bit(EV_SYN, g_InputDevice->evbit);
    set_bit(EV_KEY, g_InputDevice->evbit);
    set_bit(BTN_TOUCH, g_InputDevice->keybit);
    set_bit(INPUT_PROP_DIRECT, g_InputDevice->propbit);

#ifdef CONFIG_TP_HAVE_KEY
    {
        u32 i;
        for (i = 0; i < MAX_KEY_NUM; i ++)
        {
            input_set_capability(g_InputDevice, EV_KEY, g_TpVirtualKey[i]);
        }
    }
#endif
//modify by pangle at 20150409 begin
/*
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    input_set_capability(g_InputDevice, EV_KEY, KEY_POWER);
    input_set_capability(g_InputDevice, EV_KEY, KEY_UP);
    input_set_capability(g_InputDevice, EV_KEY, KEY_DOWN);
    input_set_capability(g_InputDevice, EV_KEY, KEY_LEFT);
    input_set_capability(g_InputDevice, EV_KEY, KEY_RIGHT);
    input_set_capability(g_InputDevice, EV_KEY, KEY_W);
    input_set_capability(g_InputDevice, EV_KEY, KEY_Z);
    input_set_capability(g_InputDevice, EV_KEY, KEY_V);
    input_set_capability(g_InputDevice, EV_KEY, KEY_O);
    input_set_capability(g_InputDevice, EV_KEY, KEY_M);
    input_set_capability(g_InputDevice, EV_KEY, KEY_C);
    input_set_capability(g_InputDevice, EV_KEY, KEY_E);
    input_set_capability(g_InputDevice, EV_KEY, KEY_S);
#endif //CONFIG_ENABLE_GESTURE_WAKEUP
*/
//modify by pangle at 20150409 end
/*
#ifdef CONFIG_TP_HAVE_KEY
    set_bit(TOUCH_KEY_MENU, g_InputDevice->keybit); //Menu
    set_bit(TOUCH_KEY_HOME, g_InputDevice->keybit); //Home
    set_bit(TOUCH_KEY_BACK, g_InputDevice->keybit); //Back
    set_bit(TOUCH_KEY_SEARCH, g_InputDevice->keybit); //Search
#endif
*/
//modify by pangle for report typed B at 20150417 begin
#ifdef CHANGE_REPORT_TYPE_TO_B
		input_set_abs_params(g_InputDevice, ABS_MT_TRACKING_ID, 0, 255, 0, 0);
		input_mt_init_slots(g_InputDevice, 5,0);
#endif
//modify by pangle for report typed B at 20150417 end
#if defined(CONFIG_ENABLE_CHIP_MSG26XXM)
    input_set_abs_params(g_InputDevice, ABS_MT_TRACKING_ID, 0, 255, 0, 0);
#endif //CONFIG_ENABLE_CHIP_MSG26XXM
    input_set_abs_params(g_InputDevice, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(g_InputDevice, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
    input_set_abs_params(g_InputDevice, ABS_MT_POSITION_X, TOUCH_SCREEN_X_MIN, TOUCH_SCREEN_X_MAX, 0, 0);
    input_set_abs_params(g_InputDevice, ABS_MT_POSITION_Y, TOUCH_SCREEN_Y_MIN, TOUCH_SCREEN_Y_MAX, 0, 0);
#endif

    return nRetVal;    
}

s32 DrvPlatformLyrTouchDeviceRequestGPIO(void)
{
    s32 nRetVal = 0;

    DBG("*** %s() ***\n", __func__);
    
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    nRetVal = gpio_request(mstar_reset_gpio, "C_TP_RST");     
    if (nRetVal < 0)
    {
        DBG("*** Failed to request GPIO %d, error %d ***\n", mstar_reset_gpio, nRetVal);
    }

    nRetVal = gpio_request(mstar_irq_gpio, "C_TP_INT");    
    if (nRetVal < 0)
    {
        DBG("*** Failed to request GPIO %d, error %d ***\n", mstar_irq_gpio, nRetVal);
    }
#endif

    return nRetVal;    
}

s32 DrvPlatformLyrTouchDeviceRegisterFingerTouchInterruptHandler(void)
{
    s32 nRetVal = 0;

    DBG("*** %s() ***\n", __func__);

    if (DrvIcFwLyrIsRegisterFingerTouchInterruptHandler())
    {    	
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
        /* initialize the finger touch work queue */ 
        INIT_WORK(&_gFingerTouchWork, _DrvPlatformLyrFingerTouchDoWork);

        _gIrq = gpio_to_irq(mstar_irq_gpio);

        /* request an irq and register the isr */
        nRetVal = request_irq(_gIrq/*mstar_irq_gpio*/, _DrvPlatformLyrFingerTouchInterruptHandler,
                      IRQF_TRIGGER_RISING /* | IRQF_NO_SUSPEND *//* IRQF_TRIGGER_FALLING */,
                      "msg2xxx", NULL);
        if (nRetVal != 0)
        {
            DBG("*** Unable to claim irq %d; error %d ***\n", mstar_irq_gpio, nRetVal);
        }
	else
	{
		DBG("*** %s() :request irq success ! disable irq !***\n", __func__);
		DrvPlatformLyrDisableFingerTouchReport();
	}
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
        mt_set_gpio_mode(mstar_irq_gpio, GPIO_CTP_EINT_PIN_M_EINT);
        mt_set_gpio_dir(mstar_irq_gpio, GPIO_DIR_IN);
        mt_set_gpio_pull_enable(mstar_irq_gpio, GPIO_PULL_ENABLE);
        mt_set_gpio_pull_select(mstar_irq_gpio, GPIO_PULL_UP);

        mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
        mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, EINTF_TRIGGER_RISING, _DrvPlatformLyrFingerTouchInterruptHandler, 1);

        mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

#ifdef CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM
        /* initialize the finger touch work queue */ 
        INIT_WORK(&_gFingerTouchWork, _DrvPlatformLyrFingerTouchDoWork);
#else
        _gThread = kthread_run(_DrvPlatformLyrFingerTouchHandler, 0, TPD_DEVICE);
        if (IS_ERR(_gThread))
        { 
            nRetVal = PTR_ERR(_gThread);
            DBG("Failed to create kernel thread: %d\n", nRetVal);
        }
#endif //CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM
#endif
    }
    
    return nRetVal;    
}	

void DrvPlatformLyrTouchDeviceRegisterEarlySuspend(void)
{
    DBG("*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
#if defined(CONFIG_HAS_EARLYSUSPEND)
    _gEarlySuspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
    _gEarlySuspend.suspend = MsDrvInterfaceTouchDeviceSuspend;
    _gEarlySuspend.resume = MsDrvInterfaceTouchDeviceResume;
    register_early_suspend(&_gEarlySuspend);
#endif
#endif    
}

/* remove function is triggered when the input device is removed from input sub-system */
s32 DrvPlatformLyrTouchDeviceRemove(struct i2c_client *pClient)
{
    DBG("*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
//    free_irq(mstar_irq_gpio, g_InputDevice);
    free_irq(_gIrq, NULL);
    gpio_free(mstar_irq_gpio);
    gpio_free(mstar_reset_gpio);
    input_unregister_device(g_InputDevice);
#endif    
#ifdef CONFIG_ENABLE_FIRMWARE_DATA_LOG
    kset_unregister(g_TouchKSet);
    kobject_put(g_TouchKObj);
#endif //CONFIG_ENABLE_FIRMWARE_DATA_LOG

    return 0;
}

void DrvPlatformLyrSetIicDataRate(struct i2c_client *pClient, u32 nIicDataRate)
{
    DBG("*** %s() nIicDataRate = %d ***\n", __func__, nIicDataRate);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM)
    // TODO : Please FAE colleague to confirm with customer device driver engineer for how to set i2c data rate on SPRD platform
    //sprd_i2c_ctl_chg_clk(pClient->adapter->nr, nIicDataRate); 
    //mdelay(100);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
    // TODO : Please FAE colleague to confirm with customer device driver engineer for how to set i2c data rate on QCOM platform
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
    pClient->timing = nIicDataRate/1000;
#endif
}
