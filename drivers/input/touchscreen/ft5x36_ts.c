/* drivers/input/touchscreen/focaltech_ts.c
 *
 * FocalTech Serils IC TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
//#include <linux/earlysuspend.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/timer.h>
//added by pangle for gesture 20140917
#include <linux/wakelock.h>
#include <linux/sensors.h>
#include "ft5x36_ts.h"
#include "ft5x36_info.h"
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>

/* Added by zengguang to add head file. 2013.5.5 start*/
#ifdef CONFIG_GET_HARDWARE_INFO
#include <mach/hardware_info.h>
static char tmp_tp_name[100];
#endif
/*  Added by zengguang to add head file. 2013.5.5 end*/

#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
/* Early-suspend level */
#define FT_SUSPEND_LEVEL 1
#endif
#define dev_info(dev, fmt, arg...) _dev_info(dev, fmt, ##arg)
#ifdef FTS_CTL_IIC
#include "ft5x36_ctl.h"
#endif
#ifdef FTS_SYSFS_DEBUG
#include "ft5x36_ex.h"
#endif
//static struct mutex ft5x36_mutex;/*Added by liumx for tp no effect 2013.10.9 start*/

//modified for up last point by chenchen 2014.8.16 begin
static u8 last_touchpoint=0;
static u8 now_touchpoint=0;
//modified for up last point by chenchen 2014.8.16 end

#define FTS_POINT_UP		0x01
#define FTS_POINT_DOWN		0x00
#define FTS_POINT_CONTACT	0x02

//added by pangle for gesture 20140917 begin 
#include "ft_gesture_lib.h"
u8 gesture_flag=0;
extern bool gesture_enable;
bool gesture_enable_dtsi;
int gesture_id = 0;
//added by chenchen for gesture display 20140820 begin
short pointnum = 0;
unsigned short coordinate_x[150] = {0};
unsigned short coordinate_y[150] = {0};
unsigned char buf_gesture[FTS_GESTRUE_POINTS * 2] = { 0 };
//added by chenchen for gesture display 20140820 end
//added by pangle for gesture 20140917 end

/*
*fts_i2c_Read-read data and write data by i2c
*@client: handle of i2c
*@writebuf: Data that will be written to the slave
*@writelen: How many bytes to write
*@readbuf: Where to store data read from slave
*@readlen: How many bytes to read
*
*Returns negative errno, else the number of messages executed
*
*
*/

struct kobject *ft5x36_virtual_key_properties_kobj;
static ssize_t

/*add by pangle for virtual key 2014.09.10 begin*/
ft5x36_virtual_keys_register(struct kobject *kobj,
                            struct kobj_attribute *attr,
                            char *buf)
{
       return snprintf(buf, 200,
       __stringify(EV_KEY) ":" __stringify(KEY_MENU)  ":80:900:170:40"
       ":" __stringify(EV_KEY) ":" __stringify(KEY_HOMEPAGE)   ":240:900:170:40"
       ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK)   ":400:900:170:40"
       "\n");
}
/*add by pangle for virtual key 2014.09.10 end*/


static struct kobj_attribute ft5x36_virtual_keys_attr = {
       .attr = {
               .name = "virtualkeys.FT5x36",
               .mode = S_IRUGO,
       },
       .show = &ft5x36_virtual_keys_register,
};

static struct attribute *ft5x36_virtual_key_properties_attrs[] = {
       &ft5x36_virtual_keys_attr.attr,
       NULL,
};

static struct attribute_group ft5x36_virtual_key_properties_attr_group = {
       .attrs = ft5x36_virtual_key_properties_attrs,
};

int fts_i2c_Read(struct i2c_client *client, char *writebuf,
		    int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 },
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "f%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}
/*write data by i2c*/
int fts_i2c_Write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = writelen,
		 .buf = writebuf,
		 },
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s i2c write error.\n", __func__);

	return ret;
}

//added by litao for irq wake function begin 
void fts_disable_irq(struct fts_ts_data *data)
{
	unsigned long irqflags;
//	pr_err("%s : irq_is_disable = %d irq = %d\n",__func__,data->irq_is_disable, data->client->irq);
	spin_lock_irqsave(&data->irq_lock, irqflags);
	if(!data->irq_is_disable){
		data->irq_is_disable = 1;
		disable_irq_nosync(data->client->irq);		
	}
	spin_unlock_irqrestore(&data->irq_lock, irqflags);
}

void fts_enable_irq(struct fts_ts_data *data)
{
	unsigned long irqflags;
//	pr_err("%s : irq_is_disable = %d irq = %d\n",__func__,data->irq_is_disable, data->client->irq);
	spin_lock_irqsave(&data->irq_lock, irqflags);
	if(data->irq_is_disable){
		data->irq_is_disable = 0;
		enable_irq(data->client->irq);		
	}
	spin_unlock_irqrestore(&data->irq_lock, irqflags);
}

void fts_disable_irq_wake(struct fts_ts_data *data)
{
	unsigned long irqflags;
	spin_lock_irqsave(&data->irq_lock, irqflags);
	if(!data->irq_wake_is_disable){
		data->irq_wake_is_disable = 1;
		disable_irq_wake(data->client->irq);		
	}
	spin_unlock_irqrestore(&data->irq_lock, irqflags);
}

void fts_enable_irq_wake(struct fts_ts_data *data)
{
	unsigned long irqflags;
	spin_lock_irqsave(&data->irq_lock, irqflags);
	if(data->irq_wake_is_disable){
		data->irq_wake_is_disable = 0;
		enable_irq_wake(data->client->irq);		
	}
	spin_unlock_irqrestore(&data->irq_lock, irqflags);
}
//added by litao for irq wake function end 

#if 0
/*release the point*/
static void fts_ts_release(struct fts_ts_data *data)
{
	input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
	input_sync(data->input_dev);
}
#endif
/*Read touch point information when the interrupt  is asserted.*/
static int fts_read_Touchdata(struct fts_ts_data *data)
{
	struct ts_event *event = &data->event;
	u8 buf[POINT_READ_BUF] = { 0 };
	int ret = -1;
	int i = 0;
//add for error point begin by chenchen 2014.8.16	
	int bufnum=0;
	u8 pointid = FT_MAX_ID;

	ret = fts_i2c_Read(data->client, buf, 1, buf, POINT_READ_BUF);
//add for error point begin by zengguang 2014.08.16 begin
	 for(i=0;i<5;i++)
       {
           if(buf[i]==0)
           {
		   	bufnum++;
           }
	 }
	 if(bufnum>=5)
	 	return -1;
	 
//add for error point end by chenchen 2014.08.16 end
	if (ret < 0) {
		dev_err(&data->client->dev, "%s read touchdata failed.\n",
			__func__);
		return ret;
	}
	memset(event, 0, sizeof(struct ts_event));

	//event->touch_point = buf[2] & 0x0F;
//modified for up last point by chenchen 2014.8.16
    last_touchpoint = now_touchpoint;
    now_touchpoint = buf[2] & 0x0F;
	event->touch_point = 0;
	for (i = 0; i < data->pdata->max_touch_point; i++) {
	//for (i = 0; i < event->touch_point; i++) {
		pointid = (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
		if (pointid >= FT_MAX_ID)
			break;
		else
			event->touch_point++;
//modified for up last point by chenchen 2014.8.16		
		 	//slast_touchpoint=event->touch_point; 
		event->au16_x[i] =
		    (s16) (buf[FT_TOUCH_X_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_X_L_POS + FT_TOUCH_STEP * i];
		event->au16_y[i] =
		    (s16) (buf[FT_TOUCH_Y_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_Y_L_POS + FT_TOUCH_STEP * i];
		event->au8_touch_event[i] =
		    buf[FT_TOUCH_EVENT_POS + FT_TOUCH_STEP * i] >> 6;
		event->au8_finger_id[i] =
		    (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
	}
	
	event->pressure = FT_PRESS;

	return 0;
}

//added by pangle for gesture 20140917 begin
static int fts_read_Gesture(struct fts_ts_data *data)
{
	//unsigned char buf[FTS_GESTRUE_POINTS * 2] = { 0 };   
	int ret = -1;    
//modify by chenchen for gesture display 20140820 begin
	int i = 0;  
//	u8 auc_i2c_write_buf[2] = {0};
//modify by chenchen for gesture display 20140820 end
	unsigned short Drawdata[15] = {0};
	buf_gesture[0] = 0xd3; 
	mdelay(15);
	
	//auc_i2c_write_buf[0] = 0xd0;	
	//fts_i2c_Read(data->client, auc_i2c_write_buf, 1, auc_i2c_write_buf, 2);
	//printk("<GTP>0xd0:%x\n",auc_i2c_write_buf[0]);
	//auc_i2c_write_buf[0] = 0xd1;	
	//fts_i2c_Read(data->client, auc_i2c_write_buf, 1, auc_i2c_write_buf, 2);
	//printk("<GTP>0xd1:%x\n",auc_i2c_write_buf[0]);
//add by pangle at 20141031 begin	
	if(data->client==NULL)
	{
		printk(KERN_ERR"data->client is null pointer \n");
		return 0;
	}
	ret = fts_i2c_Read(data->client, buf_gesture, 1, buf_gesture, 8);    
	if (ret < 0) 
	{        
		dev_err(&data->client->dev, "%s read touchdata failed.\n",            __func__);        
		return ret;    
	} 

	if (0x24 == buf_gesture[0])
	{        
		gesture_id = 0x24;
		return -1;   
	}

	pointnum = (short)(buf_gesture[1]) & 0xff;
	printk(KERN_ERR"Array  pointnum=%d\n",pointnum);
	if(pointnum>125)
	{
		printk(KERN_ERR"Array bounds \n");
		pointnum=125;
	}
	buf_gesture[0] = 0xd3;
	ret = fts_i2c_Read(data->client, buf_gesture, 1, buf_gesture, ((int)pointnum * 4 + 8));
	printk(KERN_ERR" TP %d \n",__LINE__);
	/* if limit everytime only read 8 bytes*/
	//ret = ft5x0x_i2c_Read(data->client, buf, 1, buf, (8));    
	//ret = ft5x0x_i2c_Read(data->client, buf, 0, (buf+8), (8));    

	if (ret < 0) 
	{        
		dev_err(&data->client->dev, "%s read touchdata failed.\n",            __func__);        
		return ret;    
	}
	
	gesture_id = fetch_object_sample(buf_gesture,pointnum);
	// add debug info
	//printk("Debug gestrue_id = %x \n",gesture_id);
	for (i = 0; i < 14; i++)
		{
		Drawdata[i] = (unsigned short)(((buf_gesture[0 + i * 2] & 0xFF) << 8) | (buf_gesture[1 + i * 2] & 0xFF));
	    }
		for (i = 0; i < 14; i++)
		{
		printk(KERN_ERR"HW TP %d\n  i:%d\n", Drawdata[i],i);
		}
		printk("Debug end \n");
	
//modify by chenchen for gesture display 20140820 begin
/*	for(i = 0;i < pointnum;i++)
	{
		coordinate_x[i] =  (((s16) buf[0 + (4 * i)]) & 0x0F) <<
		    8 | (((s16) buf[1 + (4 * i)])& 0xFF);
		coordinate_y[i] = (((s16) buf[2 + (4 * i)]) & 0x0F) <<
		    8 | (((s16) buf[3 + (4 * i)]) & 0xFF);
	}*/
//modify by chenchen for gesture display 20140820 end

	return -1;
}

static void fts_report_Gesture(struct fts_ts_data *data)
{	
		switch(gesture_id)
               	{
	              	
               	case 0x24:
					printk("<GTP>double click\n");
					input_report_key(data->input_dev, KEY_POWER, 1);
                                   input_sync(data->input_dev);
                                   input_report_key(data->input_dev, KEY_POWER, 0);
                                   input_sync(data->input_dev);		   
				break;

				case 0x22:
					printk("<GTP>line up\n");
					input_report_key(data->input_dev, KEY_UP, 1);
                                   input_sync(data->input_dev);
                                   input_report_key(data->input_dev, KEY_UP, 0);
                                   input_sync(data->input_dev);
				break;

				case 0x23:
					printk("<GTP>line down\n");
					input_report_key(data->input_dev, KEY_DOWN, 1);
                                   input_sync(data->input_dev);
                                   input_report_key(data->input_dev, KEY_DOWN, 0);
                                   input_sync(data->input_dev);
				break;

				case 0x20:
					printk("<GTP>line left\n");
					input_report_key(data->input_dev, KEY_LEFT, 1);
                                   input_sync(data->input_dev);
                                   input_report_key(data->input_dev, KEY_LEFT, 0);
                                   input_sync(data->input_dev);
				break;

				case 0x21:
					printk("<GTP>line right\n");
					input_report_key(data->input_dev, KEY_RIGHT, 1);
                                   input_sync(data->input_dev);
                                   input_report_key(data->input_dev, KEY_RIGHT, 0);
                                   input_sync(data->input_dev);		   
				break;
				//comment by pangle at 20150303 end

				/*case 0x34:
					printk("<GTP>letter:C\n");
					input_report_key(data->input_dev, KEY_C, 1);
                                   input_sync(data->input_dev);
                                   input_report_key(data->input_dev, KEY_C, 0);
                                   input_sync(data->input_dev);		   
				break;

				case 0x31:
					printk("<GTP>letter:W\n");
					input_report_key(data->input_dev, KEY_W, 1);
                                   input_sync(data->input_dev);
                                   input_report_key(data->input_dev, KEY_W, 0);
                                   input_sync(data->input_dev);		   
				break;

				case 0x33:
					printk("<GTP>letter:E\n");
					input_report_key(data->input_dev, KEY_E, 1);
                                   input_sync(data->input_dev);
                                   input_report_key(data->input_dev, KEY_E, 0);
                                   input_sync(data->input_dev);			   
				break;

				case 0x54:
					printk("<GTP>letter:V\n");
					input_report_key(data->input_dev, KEY_V, 1);
                                   input_sync(data->input_dev);
                                   input_report_key(data->input_dev, KEY_V, 0);
                                   input_sync(data->input_dev);		   
				break;

				case 0x32:
					printk("<GTP>letter:M\n");
					input_report_key(data->input_dev, KEY_M, 1);
                                   input_sync(data->input_dev);
                                   input_report_key(data->input_dev, KEY_M, 0);
                                   input_sync(data->input_dev);		   
				break;

				case 0x46:
					printk("<GTP>letter:S\n");
					input_report_key(data->input_dev, KEY_S, 1);
                                   input_sync(data->input_dev);
                                   input_report_key(data->input_dev, KEY_S, 0);
                                   input_sync(data->input_dev);		   
				break;

				case 0x41:
					printk("<GTP>letter:Z\n");
					input_report_key(data->input_dev, KEY_Z, 1);
                                   input_sync(data->input_dev);
                                   input_report_key(data->input_dev, KEY_Z, 0);
                                   input_sync(data->input_dev);		   
				break;
				*/
				//comment by pangle at 20150303 end
		//added by shihuijun for ql701 gesture 2010115 end		
				default:
                    printk("<GTP>no this feature\n");
                break;   							 
                     }
		//shihuijun added timeout wakelock at 20141229
		wake_lock_timeout(&data->tp_wake_lock, 5*HZ);
}
//added by pangle for gesture 20140917 end
/*
*report the point information
*/
static void fts_report_value(struct fts_ts_data *data)
{
	struct ts_event *event = &data->event;
	//struct fts_platform_data *pdata = &data->pdata;
	int i = 0;
	//int up_point = 0;
	//int touch_point = 0;
	int fingerdown = 0;

	for (i = 0; i < event->touch_point; i++) {
		if (event->au8_touch_event[i] == 0 || event->au8_touch_event[i] == 2) {
			event->pressure = FT_PRESS;
			fingerdown++;
		} else {
			event->pressure = 0;
		}
		input_mt_slot(data->input_dev, event->au8_finger_id[i]);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER,
						!!event->pressure);
		if (event->pressure == FT_PRESS) {
			input_report_abs(data->input_dev, ABS_MT_POSITION_X,
					event->au16_x[i]);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
					event->au16_y[i]);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
					event->pressure);
		}
	//printk(KERN_INFO "----------------X=%d.Y=%d\n",event->au16_x[i],event->au16_y[i]);
	}
	input_report_key(data->input_dev, BTN_TOUCH, !!fingerdown);
	input_sync(data->input_dev);

//modified for up last point by chenchen 20140816 begin	
	if((last_touchpoint>0)&&(now_touchpoint==0))    
    {        /* release all touches */ 
              i=0;
         	for (i = 0; i < CFG_MAX_TOUCH_POINTS; i++) 
	 		{
                   input_mt_slot(data->input_dev, i);
                   input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
        	}
         	last_touchpoint=0;
		input_report_key(data->input_dev, BTN_TOUCH, !!fingerdown);
		input_sync(data->input_dev);
	}
//modified for up last point by chenchen 20140816 end	
}

//added by pangle for gesture 20140917 begin
static void fts_ts_worker(struct work_struct *work)
{
	int ret = 0;
	struct fts_ts_data *fts_ts = container_of(work, struct fts_ts_data, fts_work);
	if (!fts_ts) {
		ret = -ENOMEM;
		printk(KERN_ERR"fts_ts is null!\n");
		fts_enable_irq(fts_ts);
		return ;
	}

//modify by shihuijun for charger interference to tp 20141224	begin	
 /*      #if  defined(CONFIG_QL600_UOOGOU_512)    
	   if(!gesture_flag){
	if(chg_insert_for_tp)
		fts_write_reg(fts_ts->client, 0x8b, 1);
	else
		fts_write_reg(fts_ts->client, 0x8b, 0);
	   	}
	#endif*/
//modify by chenchen for charger interference to tp 20140813	end
//modify by chenchen for adding log 20140828 begin
//modify by pangle for pocket mode at 20141117 begin
	if(gesture_enable == 1 && gesture_flag == 1){ 
		 //  if(!proximity_open_flag )
		    if(1)
		   {
			printk(KERN_ERR"%s --- read gesture!\n",__func__);  	
			ret = fts_read_Gesture(fts_ts);	
			if(ret == -1)                             
				fts_report_Gesture(fts_ts);   
		  	}
		  	else
		  	{
		   		printk(KERN_ERR"%s --- psensor sheltered, not read gesture!\n",__func__);
		   	}
//modify by chenchen for adding log 20140828 end		   
		}
//modify by pangle for pocket mode at 20141117 end	
	else{             
		ret = fts_read_Touchdata(fts_ts);	      
		if (ret == 0)		  
			fts_report_value(fts_ts);		
	}
	fts_enable_irq(fts_ts);
}
//added by pangle for gesture 20140917 end
/*
//yuquan added begin for suspend and resume
#ifdef CONFIG_PM
static int ft5x36_ts_suspend(struct device *dev);
static int ft5x36_ts_resume(struct device *dev);
#endif
static void fts_ts_pm_worker(struct work_struct *work)
{
	int ret = -ENOMEM;
	struct fts_ts_data *fts_ts = container_of(work, struct fts_ts_data, fts_pm_work.work);

	if (!fts_ts) {
		ret = -ENOMEM;
		printk(KERN_ERR"fts_ts is null!\n");
		return ;
	}

	printk("%s():lcd status \%s\n",__func__,(fts_ts->pm_status == FB_BLANK_UNBLANK)?"FB_BLANK_UNBLANK":"FB_BLANK_POWERDOWN");

	if (fts_ts->pm_status == FB_BLANK_UNBLANK)
		ft5x36_ts_resume(&fts_ts->client->dev);
	else if (fts_ts->pm_status == FB_BLANK_POWERDOWN)
		ft5x36_ts_suspend(&fts_ts->client->dev);

}
//yuquan added end for suspend and resume
*/
/*The FocalTech device will signal the host about TRIGGER_FALLING.
*Processed when the interrupt is asserted.
*/
static irqreturn_t fts_ts_interrupt(int irq, void *dev_id)
{
	struct fts_ts_data *fts_ts = dev_id;
//modify by shihuijun for gesture 20141223 begin
	wake_lock_timeout(&fts_ts->tp_wake_lock, 3*HZ);
	fts_disable_irq(fts_ts);
	      queue_work(fts_ts->fts_wq,
				&fts_ts->fts_work);
//modify by shihuijun for gesture 20141223 end


	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static int ft5x36_get_dt_coords(struct device *dev, char *name,
				struct fts_platform_data *pdata)
{
	u32 coords[FT_COORDS_ARR_SIZE];
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size, rc;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != FT_COORDS_ARR_SIZE) {
		dev_err(dev, "invalid %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(np, name, coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read %s\n", name);
		return rc;
	}

	if (!strcmp(name, "focaltech,panel-coords")) {
		pdata->panel_minx = coords[0];
		pdata->panel_miny = coords[1];
		pdata->panel_maxx = coords[2];
		pdata->panel_maxy = coords[3];
	} else if (!strcmp(name, "focaltech,display-coords")) {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
	} else {
		dev_err(dev, "unsupported property %s\n", name);
		return -EINVAL;
	}

	return 0;
}

static int ft5x36_parse_dt(struct device *dev,
			struct fts_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	struct property *prop;
	u32 temp_val, num_buttons;
	u32 button_map[MAX_BUTTONS];
	bool virthual_key_support;
	bool auto_clb;

	rc = ft5x36_get_dt_coords(dev, "focaltech,panel-coords", pdata);
	if (rc && (rc != -EINVAL))
		return rc;

	rc = ft5x36_get_dt_coords(dev, "focaltech,display-coords", pdata);
	if (rc)
		return rc;

	pdata->i2c_pull_up = of_property_read_bool(np,
						"focaltech,i2c-pull-up");

	pdata->no_force_update = of_property_read_bool(np,
						"focaltech,no-force-update");
//added by pangle for gesture	20140917
	gesture_enable_dtsi= of_property_read_bool(np,
						"focaltech,gesture-enable");
	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "focaltech,reset-gpio",
				0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
		return pdata->reset_gpio;

	pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio",
				0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0)
		return pdata->irq_gpio;

	rc = of_property_read_u32(np, "focaltech,family-id", &temp_val);
	if (!rc)
		pdata->family_id = temp_val;
	else{
			dev_err(dev, "Unable to read family id\n");
			return rc;
	}

	rc = of_property_read_u32(np, "focaltech,max_touch_point", &temp_val);
	if (!rc)
		pdata->max_touch_point = temp_val;
	else{
			dev_err(dev, "Unable to read max touch point\n");
		       pdata->max_touch_point = 2;
	}

	auto_clb = of_property_read_bool(np,
                        "focaltech,auto_clb");
	if(auto_clb)
           pdata->auto_clb=true;
	else
           pdata->auto_clb=false;

	prop = of_find_property(np, "focaltech,button-map", NULL);
	if (prop) {
		num_buttons = prop->length / sizeof(temp_val);
		if (num_buttons > MAX_BUTTONS)
			return -EINVAL;

		rc = of_property_read_u32_array(np,
			"focaltech,button-map", button_map,
			num_buttons);
		if (rc) {
			dev_err(dev, "Unable to read key codes\n");
			return rc;
		}
	}

	virthual_key_support = of_property_read_bool(np,
                        "focaltech,virtual_key");
	if(virthual_key_support){
	      printk(KERN_INFO "%s,virtual key support...\n",__func__);
	      ft5x36_virtual_key_properties_kobj =
                       kobject_create_and_add("board_properties", NULL);
	      if(ft5x36_virtual_key_properties_kobj){
	      rc = sysfs_create_group(ft5x36_virtual_key_properties_kobj,
                               &ft5x36_virtual_key_properties_attr_group);
	      if(rc)
		    return rc;
	       }
	}	
	return 0;
}
#else
static int ft5x36_parse_dt(struct device *dev,
			struct fts_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int ft5x36_power_on(struct fts_ts_data *data, bool on)
{
	int rc;

	if (!on)
		goto power_off;

	rc = regulator_enable(data->vdd);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vdd enable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(data->vcc_i2c);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vcc_i2c enable failed rc=%d\n", rc);
		regulator_disable(data->vdd);
	}

	return rc;

power_off:
	rc = regulator_disable(data->vdd);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vdd disable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_disable(data->vcc_i2c);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vcc_i2c disable failed rc=%d\n", rc);
		rc=regulator_enable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
		}
	}

	return rc;
}

static int ft5x36_power_init(struct fts_ts_data *data, bool on)
{
	int rc;

	if (!on)
		goto pwr_deinit;

	data->vdd = regulator_get(&data->client->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		rc = PTR_ERR(data->vdd);
		dev_err(&data->client->dev,
			"Regulator get failed vdd rc=%d\n", rc);
		return rc;
	}

	if (regulator_count_voltages(data->vdd) > 0) {
		rc = regulator_set_voltage(data->vdd, FT_VTG_MIN_UV,
					   FT_VTG_MAX_UV);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator set_vtg failed vdd rc=%d\n", rc);
			goto reg_vdd_put;
		}
	}

	data->vcc_i2c = regulator_get(&data->client->dev, "vcc_i2c");
	if (IS_ERR(data->vcc_i2c)) {
		rc = PTR_ERR(data->vcc_i2c);
		dev_err(&data->client->dev,
			"Regulator get failed vcc_i2c rc=%d\n", rc);
		goto reg_vdd_set_vtg;
	}

	if (regulator_count_voltages(data->vcc_i2c) > 0) {
		rc = regulator_set_voltage(data->vcc_i2c, FT_I2C_VTG_MIN_UV,
					   FT_I2C_VTG_MAX_UV);
		if (rc) {
			dev_err(&data->client->dev,
			"Regulator set_vtg failed vcc_i2c rc=%d\n", rc);
			goto reg_vcc_i2c_put;
		}
	}
	return 0;

reg_vcc_i2c_put:
	regulator_put(data->vcc_i2c);
reg_vdd_set_vtg:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, FT_VTG_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return rc;

pwr_deinit:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, FT_VTG_MAX_UV);

	regulator_put(data->vdd);

	if (regulator_count_voltages(data->vcc_i2c) > 0)
		regulator_set_voltage(data->vcc_i2c, 0, FT_I2C_VTG_MAX_UV);

	regulator_put(data->vcc_i2c);
	return 0;
}
/*modified for pin used touch start.zengguang 2014.08.22*/
/***********************************************
Name: 		<ft5x06_ts_pinctrl_init>
Author:		<zengguang>
Date:   		<2014.07.17>
Purpose:		<modified for pin used touch>
Declaration:	YEP Telecom Technology Co., LTD
***********************************************/
static int ft5x36_ts_pinctrl_init(struct fts_ts_data *data)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	data->ts_pinctrl = devm_pinctrl_get(&(data->client->dev));
	if (IS_ERR_OR_NULL(data->ts_pinctrl)) {
		dev_dbg(&data->client->dev,
			"Target does not use pinctrl\n");
		retval = PTR_ERR(data->ts_pinctrl);
		data->ts_pinctrl = NULL;
		return retval;
	}

	data->gpio_state_active
		= pinctrl_lookup_state(data->ts_pinctrl,
			"pmx_ts_active");
	if (IS_ERR_OR_NULL(data->gpio_state_active)) {
		dev_dbg(&data->client->dev,
			"Can not get ts default pinstate\n");
		retval = PTR_ERR(data->gpio_state_active);
		data->ts_pinctrl = NULL;
		return retval;
	}

	data->gpio_state_suspend
		= pinctrl_lookup_state(data->ts_pinctrl,
			"pmx_ts_suspend");
	if (IS_ERR_OR_NULL(data->gpio_state_suspend)) {
		dev_err(&data->client->dev,
			"Can not get ts sleep pinstate\n");
		retval = PTR_ERR(data->gpio_state_suspend);
		data->ts_pinctrl = NULL;
		return retval;
	}

	return 0;
}

/***********************************************
Name: 		<ft5x06_ts_pinctrl_select>
Author:		<zengguang>
Date:   		<2014.07.17>
Purpose:		<Set ctp pin state to handle hardware>
Declaration:	YEP Telecom Technology Co., LTD
***********************************************/
static int ft5x36_ts_pinctrl_select(struct fts_ts_data *data,
						bool on)
{
	struct pinctrl_state *pins_state;
	int ret;

	pins_state = on ? data->gpio_state_active
		: data->gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(data->ts_pinctrl, pins_state);
		if (ret) {
			dev_err(&data->client->dev,
				"can not set %s pins\n",
				on ? "pmx_ts_active" : "pmx_ts_suspend");
			return ret;
		}
	} else {
		dev_err(&data->client->dev,
			"not a valid '%s' pinstate\n",
				on ? "pmx_ts_active" : "pmx_ts_suspend");
	}

	return 0;
}

static int ft5x36_ts_pinctrl_put(struct fts_ts_data *data)
{
	if(NULL != data->ts_pinctrl)
           pinctrl_put(data->ts_pinctrl);
	
	return 0;
}

/*modified for pin used touch start.zengguang 2014.08.22*/
#ifdef CONFIG_PM
static int ft5x36_ts_suspend(struct device *dev)
{
	struct fts_ts_data *ft5x36_ts = dev_get_drvdata(dev);
	char txbuf[2];
	int err;
//added by pangle for gesture 20140917 begin

   		u8 auc_i2c_write_buf[2] = {0};
//modify by chenchen for tp crash (SW00074483) 20140827 begin
	pr_info("TP:---------TP suspend begin\n");

	if (ft5x36_ts->loading_fw) {
		dev_info(dev, "Firmware loading in process...\n");
		return 0;
	}

	if (ft5x36_ts->suspended) {
		dev_info(dev, "Already in suspend state\n");
		return 0;
	}

		fts_disable_irq(ft5x36_ts);
   if(gesture_enable == 1 && gesture_flag == 0){		
	  	      auc_i2c_write_buf[0] = 0xd0;
              auc_i2c_write_buf[1] = 0x01;
              fts_i2c_Write(ft5x36_ts->client,auc_i2c_write_buf,2);
 
              auc_i2c_write_buf[0] = 0xd1;
         auc_i2c_write_buf[1] = 0xff;
              fts_i2c_Write(ft5x36_ts->client,auc_i2c_write_buf,2);

			  auc_i2c_write_buf[0] = 0xd2;
              auc_i2c_write_buf[1] = 0x3f;
              fts_i2c_Write(ft5x36_ts->client,auc_i2c_write_buf,2);
		memset(coordinate_x,0,150);
		memset(coordinate_y,0,150);
//modify by chenchen for tp crash (SW00074483) 20140827 end		
   	}
//added by pangle for gesture 20140917 end

//added by pangle for gesture 20140917 begin	
if(gesture_enable){
	ft5x36_ts->suspended = true;
	
	fts_enable_irq_wake(ft5x36_ts);
       gesture_flag = 1;
       printk("enter doze mode\n");
	fts_enable_irq(ft5x36_ts);
	return 0;	
}
//added by pangle for gesture 20140917 end

else{
//modify by chenchen for tp crash (SW00074483) 20140827	
	//disable_irq(ft5x36_ts->client->irq);
	if (gpio_is_valid(ft5x36_ts->pdata->reset_gpio)) {
		txbuf[0] = FTS_REG_PMODE;
		txbuf[1] = FTS_PMODE_HIBERNATE;
		fts_i2c_Write(ft5x36_ts->client, txbuf, sizeof(txbuf));
	}

	if (ft5x36_ts->pdata->power_on) {
		err = ft5x36_ts->pdata->power_on(false);
		if (err) {
			dev_err(dev, "power off failed");
			goto pwr_off_fail;
		}
	} else {
		err = ft5x36_power_on(ft5x36_ts, false);
		if (err) {
			dev_err(dev, "power off failed");
			goto pwr_off_fail;
		}
	}

	ft5x36_ts->suspended = true;
	pr_info("TP:---------TP suspend end\n");
	return 0;

pwr_off_fail:
	if (gpio_is_valid(ft5x36_ts->pdata->reset_gpio)) {
		gpio_set_value_cansleep(ft5x36_ts->pdata->reset_gpio, 0);
		msleep(FT_RESET_DLY);
		gpio_set_value_cansleep(ft5x36_ts->pdata->reset_gpio, 1);
	}
	fts_enable_irq(ft5x36_ts);
	return err;
}
}

static int ft5x36_ts_resume(struct device *dev)
{
	struct fts_ts_data *ft5x36_ts = dev_get_drvdata(dev);
	int i;
	
//added by pangle for gesture 20140917 begin
        int err;
//add tp resume begin log by xingbin 20141021
	pr_info("TP:---------TP resume begin\n");

	if (ft5x36_ts->loading_fw) {
		dev_info(dev, "Firmware loading in process...\n");
		return 0;
	}

	if (!ft5x36_ts->suspended) {
		dev_info(dev, "Already in awake state\n");
		return 0;
	}

    if(gesture_flag == 1){
         u8 auc_i2c_write_buf[2] = {0};   
//modify by chenchen for tp crash (SW00074483) 20140827 begin		 
	fts_disable_irq_wake(ft5x36_ts);
	fts_disable_irq(ft5x36_ts);
//modify by chenchen for tp crash (SW00074483) 20140827 end	
         auc_i2c_write_buf[0] = 0xd0;
         auc_i2c_write_buf[1] = 0x00;
         fts_i2c_Write(ft5x36_ts->client,auc_i2c_write_buf,2);
    	}
//added by pangle for gesture 20140917 end

	//wangkai_delete report in suspend_start
//yuquan open this for touchkey crash

//change max_touch_point by pangle 2014.09.10
	for (i = 0; i < ft5x36_ts->pdata->max_touch_point; i++) {
		input_mt_slot(ft5x36_ts->input_dev, i);
		input_mt_report_slot_state(ft5x36_ts->input_dev, MT_TOOL_FINGER, 0);
	}
	input_report_key(ft5x36_ts->input_dev, BTN_TOUCH, 0);
	input_sync(ft5x36_ts->input_dev);

//wangkai_delete report in suspend_end

//added by pangle for geture 20141027 begin	
   if(!gesture_flag){	
	if (ft5x36_ts->pdata->power_on) {
		err = ft5x36_ts->pdata->power_on(true);
		if (err) {
			dev_err(dev, "power on failed");
			return err;
		}
	} else {
		err = ft5x36_power_on(ft5x36_ts, true);
		if (err) {
			dev_err(dev, "power on failed");
			return err;
		}
	}	
   	}
   
   if (gpio_is_valid(ft5x36_ts->pdata->reset_gpio)) {
		gpio_set_value_cansleep(ft5x36_ts->pdata->reset_gpio, 0);
		msleep(FT_RESET_DLY);
		gpio_set_value_cansleep(ft5x36_ts->pdata->reset_gpio, 1);
	}
    msleep(FT_STARTUP_DLY);
//added by pangle for geture 20141027 end	
//modify by chenchen for tp crash (SW00074483) 20140827
	fts_enable_irq(ft5x36_ts);

	ft5x36_ts->suspended = false;
//added by pangle for gesture 20140917
       gesture_flag = 0;
	//add tp resume begin log by xingbin 20141021
	pr_info("TP:---------TP resume end\n");
	return 0;
}

static const struct dev_pm_ops ft5x36_ts_pm_ops = {
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
	.suspend = ft5x36_ts_suspend,
	.resume = ft5x36_ts_resume,
#endif
};
#endif

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct fts_ts_data *ft5x36_data =
		container_of(self, struct fts_ts_data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK &&
			ft5x36_data && ft5x36_data->client) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK)
			ft5x36_ts_resume(&ft5x36_data->client->dev);
		else if (*blank == FB_BLANK_POWERDOWN)
			ft5x36_ts_suspend(&ft5x36_data->client->dev);
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void ft5x36_ts_early_suspend(struct early_suspend *handler)
{
	struct ft5x06_ts_data *data = container_of(handler,
						   struct fts_ts_data,
						   early_suspend);

	ft5x36_ts_suspend(&data->client->dev);
}

static void ft5x36_ts_late_resume(struct early_suspend *handler)
{
	struct ft5x06_ts_data *data = container_of(handler,
						   struct fts_ts_data,
						   early_suspend);

	ft5x36_ts_resume(&data->client->dev);
}
#endif
/*Added by liumx for tp no effect 2013.10.9 start*/
/*
static void fts_work_func(struct work_struct *work)
{
	//struct fts_ts_data *fts_ts = dev_id;
	int ret = 0;
	struct fts_ts_data *fts_ts = NULL;
	mutex_lock(&ft5x36_mutex);
	fts_ts = container_of(work, struct fts_ts_data, work);
	ret = fts_read_Touchdata(fts_ts);
	if (ret == 0)
		fts_report_value(fts_ts);
	//spin_lock_irqsave(&fts_ts->irq_lock, flag);
	enable_irq(fts_ts->irq);
	//spin_unlock_irqrestore(&fts_ts->irq_lock, flag);
	mutex_unlock(&ft5x36_mutex);
}*/
/*Added by liumx for tp no effect 2013.10.9 end*/

//yuquan added begin hardware info
//modify by pangle for hardware info 20140928 begin
static void ft5x36_hardware_info_reg(u8 family_id,u8 vendod_id,u8 fw_ver)
{
    int i=0;
    if(family_id==IC_FT5x36)
	 strcpy(tmp_tp_name, "IC-FT5x36:Module-");
    else if(family_id==IC_FT6x06)
	 strcpy(tmp_tp_name, "IC-FT6x06:Module-");
	else if(family_id==IC_FT6x36)
	 strcpy(tmp_tp_name, "IC-FT6x36:Module-");
    else{
	 printk(KERN_ERR "invalid family id = 0x%x\n",family_id);
	 return;
    }
    for(i=0;i<sizeof(ft5x36_name)/sizeof(ft5x36_ven_info);i++){
        if(vendod_id==ft5x36_name[i].vendor_id){
	     strcat(tmp_tp_name,ft5x36_name[i].ven_name);
		 strcat(tmp_tp_name,"  Ver0x");
		 sprintf(tmp_tp_name+strlen(tmp_tp_name), "%x",fw_ver);
//modify by pangle for hardware info 20140928 end		
	     break;
        }else{
            continue;
        }
    }
    if(i==sizeof(ft5x36_name)/sizeof(ft5x36_ven_info))
        strcat(tmp_tp_name,"unknow");
    register_hardware_info(CTP, tmp_tp_name);

}
//yuquan added end hardware info

static int fts_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct fts_platform_data *pdata =
	    (struct fts_platform_data *)client->dev.platform_data;
	struct fts_ts_data *fts_ts;
	struct input_dev *input_dev;
	int err = 0;
	unsigned char ven_id_val;
	unsigned char fw_ver_val;   //add by pangle for hardware info at 20140928
	unsigned char fw_ver_addr;  //add by pangle for hardware info at 20140928
	unsigned char ven_id_addr;
	//char projectcode[32]; 

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct fts_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		err = ft5x36_parse_dt(&client->dev, pdata);
		if (err)
			return err;
	} else
		pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "Invalid pdata\n");
		return -EINVAL;
	}
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		return err;
	}
	fts_ts = kzalloc(sizeof(struct fts_ts_data), GFP_KERNEL);

	if (!fts_ts) {
		err = -ENOMEM;
		return err;
	}
	
	i2c_set_clientdata(client, fts_ts);
	fts_ts->irq = client->irq;
	fts_ts->client = client;
	fts_ts->pdata = pdata;
	fts_ts->x_max = pdata->x_max - 1;
	fts_ts->y_max = pdata->y_max - 1;
//added by pangle for gesture 20140917
	if(gesture_enable_dtsi)
		init_para(pdata->x_max,pdata->y_max ,100,0,0);
	#if 0
	err = request_threaded_irq(client->irq, NULL, fts_ts_interrupt,
				   pdata->irqflags, client->dev.driver->name,
				   fts_ts);
	if (err < 0) {
		dev_err(&client->dev, "fts_probe: request irq failed\n");
		goto exit_irq_request_failed;
	}
	disable_irq(client->irq);
	#endif
	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto free_mem;
	}
	fts_ts->input_dev = input_dev;
	
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_mt_init_slots(input_dev,pdata->max_touch_point,0);
       input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min, pdata->x_max, 0, 0);
       input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min, pdata->y_max, 0, 0);
       input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
       input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
       input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

       input_set_capability(input_dev, EV_KEY, KEY_MENU); 
       input_set_capability(input_dev, EV_KEY, KEY_HOMEPAGE); 
       input_set_capability(input_dev, EV_KEY, KEY_BACK); 
//added by pangle for gesture 20140917 begin
       input_set_capability(input_dev, EV_KEY, KEY_POWER); 
       input_set_capability(input_dev, EV_KEY, KEY_UP); 
       input_set_capability(input_dev, EV_KEY, KEY_DOWN);
	input_set_capability(input_dev, EV_KEY, KEY_LEFT); 
       input_set_capability(input_dev, EV_KEY, KEY_RIGHT); 
	   input_set_capability(input_dev, EV_KEY, KEY_C);
	   input_set_capability(input_dev, EV_KEY, KEY_W);
	   input_set_capability(input_dev, EV_KEY, KEY_E);
	   input_set_capability(input_dev, EV_KEY, KEY_V);
       input_set_capability(input_dev, EV_KEY, KEY_M);
       input_set_capability(input_dev, EV_KEY, KEY_S); 
	   input_set_capability(input_dev, EV_KEY, KEY_Z);
       
//added by shihuijun for gesture 20150115 end	

	input_dev->name = FTS_NAME;
	input_dev->id.bustype = BUS_I2C;
	input_dev->id.product = 0x0101;	
	input_dev->id.vendor = 0x1010;	
	input_dev->id.version = 0x000a;	
	
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
			"fts_ts_probe: failed to register input device: %s\n",
			dev_name(&client->dev));
		goto free_inputdev;
	}
		
	if (pdata->power_init) {
		err = pdata->power_init(true);
		if (err) {
			dev_err(&client->dev, "power init failed");
			goto unreg_inputdev;
		}
	} else {
		err = ft5x36_power_init(fts_ts, true);
		if (err) {
			dev_err(&client->dev, "power init failed");
			goto unreg_inputdev;
		}
	}

	if (pdata->power_on) {
		err = pdata->power_on(true);
		if (err) {
			dev_err(&client->dev, "power on failed");
			goto pwr_deinit;
		}
	} else {
		err = ft5x36_power_on(fts_ts, true);
		if (err) {
			dev_err(&client->dev, "power on failed");
			goto pwr_deinit;
		}
	}

	if (gpio_is_valid(pdata->irq_gpio)) {
		
		err = gpio_request(pdata->irq_gpio, "ft5x36_irq_gpio");
		if (err) {
			dev_err(&client->dev, "irq gpio request failed");
			goto pwr_off;
		}
		err = gpio_direction_input(pdata->irq_gpio);
		if (err) {
			dev_err(&client->dev,
				"set_direction for irq gpio failed\n");
			goto free_irq_gpio;
		}
	}

	if (gpio_is_valid(pdata->reset_gpio)) {
		
		err = gpio_request(pdata->reset_gpio, "ft5x36_reset_gpio");
		if (err) {
			dev_err(&client->dev, "reset gpio request failed");
			goto free_irq_gpio;
		}

		err = gpio_direction_output(pdata->reset_gpio, 0);
		if (err) {
			dev_err(&client->dev,
				"set_direction for reset gpio failed\n");
			goto free_reset_gpio;
		}
		msleep(FT_RESET_DLY);
		gpio_set_value_cansleep(fts_ts->pdata->reset_gpio, 1);
	}

/*modified for pin used touch start.zengguang 2014.08.22*/
	err = ft5x36_ts_pinctrl_init(fts_ts);
	if (!err && fts_ts->ts_pinctrl) {
		err = ft5x36_ts_pinctrl_select(fts_ts, true);
		if (err < 0){
			dev_err(&client->dev,
				"fts pinctrl select failed\n");
		}
	}
/*modified for pin used touch end.zengguang 2014.08.22*/
	/*make sure CTP already finish startup process */
	msleep(200);

	/*get some register information */
	ven_id_addr = FTS_REG_VENDOR_ID;
	err = fts_i2c_Read(client, &ven_id_addr, 1, &ven_id_val, 1);
	printk(KERN_INFO"[FTS]FTS_REG_VENDOR_ID =0x%X\n ",ven_id_val);
	if (err < 0) {
		dev_err(&client->dev, "vendor id read failed");
		goto free_reset_gpio;
	}
	/*Added by liumx for tp no effect 2013.10.9 start*/
	/*
	fts_ts->fts_wq = create_workqueue("fts_wq");
	INIT_WORK(&fts_ts->work, fts_work_func);*/
	/*Added by liumx for tp no effect 2013.10.9 end*/
//added by pangle for gesture 20140917 begin
	INIT_WORK(&fts_ts->fts_work, fts_ts_worker);
	mutex_init(&fts_ts->fts_mutex);
	spin_lock_init(&fts_ts->irq_lock);
	//yuquan added timeout wakelock at 20141129
	wake_lock_init(&fts_ts->tp_wake_lock, WAKE_LOCK_SUSPEND, "tp_wake_lock");


	fts_ts->fts_wq = create_singlethread_workqueue("fts_wq");
	if (!fts_ts->fts_wq) {
		dev_err(&client->dev, "failed to create fts_wq");
		goto free_queue;
	}
//added by pangle for gesture 20140917 end
/*
//yuquan added begin for suspend and resume
	INIT_DELAYED_WORK(&fts_ts->fts_pm_work, fts_ts_pm_worker);
	fts_ts->fts_pm_wq = create_singlethread_workqueue("fts_pm_wq");
	if (!fts_ts->fts_pm_wq) {
		dev_err(&client->dev, "failed to create fts_pm_wq");
		goto free_queue;
	}
//yuquan added end for suspend and resume
*/
//modify interrupt trigger by chenchen 20140825 
	err = request_threaded_irq(client->irq, NULL, fts_ts_interrupt,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT, client->dev.driver->name,
				   fts_ts);	 //modified for  tp cpu freq  low IRQF_TRIGGER_FALLING by zengguang 2014.7.10
	if (err < 0) {
		dev_err(&client->dev, "fts_probe: request irq failed\n");
		goto free_reset_gpio;
	}
	fts_disable_irq(fts_ts);

#if defined(CONFIG_FB)
	fts_ts->fb_notif.notifier_call = fb_notifier_callback;

	err = fb_register_client(&fts_ts->fb_notif);

	if (err)
		dev_err(&client->dev, "Unable to register fb_notifier: %d\n",
			err);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	fts_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN +
						    FT_SUSPEND_LEVEL;
	fts_ts->early_suspend.suspend = ft5x36_ts_early_suspend;
	fts_ts->early_suspend.resume = ft5x36_ts_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

#ifdef FTS_SYSFS_DEBUG
	fts_create_sysfs(client);
  #ifdef FTS_APK_DEBUG
	fts_create_apk_debug_channel(client);
  #endif
#endif
//modify by pangle for hardware info 20140928 begin	
	fw_ver_addr = FTS_REG_FW_VER;
	err = fts_i2c_Read(client, &fw_ver_addr, 1, &fw_ver_val, 1);
	if (err < 0) {
		dev_err(&client->dev, "FW version read failed");
		goto free_reset_gpio;
	}	
//modify by pangle for hardware info 20140928 end
#ifdef CONFIG_GET_HARDWARE_INFO
ft5x36_hardware_info_reg(pdata->family_id,ven_id_val,fw_ver_val); //modify by pangle for hardware info 20140928
#endif
//modify by chenchen for hardware info 20140909 end

#ifdef FTS_CTL_IIC
	if (ft_rw_iic_drv_init(client) < 0)
		dev_err(&client->dev, "%s:[FTS] create fts control iic driver failed\n",
				__func__);
#endif
	fts_enable_irq(fts_ts);
	return 0;
//added by pangle for gesture 20140917 begin	
free_queue:
	kfree(fts_ts->fts_wq);
//added by pangle for gesture 20140917 end	
free_reset_gpio:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
free_irq_gpio:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
/*modified for pin used touch start.zengguang 2014.08.22*/	
	if (fts_ts->ts_pinctrl) {
		err = ft5x36_ts_pinctrl_select(fts_ts, false);
		if (err < 0)
			pr_err("Cannot get idle pinctrl state\n");
		ft5x36_ts_pinctrl_put(fts_ts);
	}
/*modified for pin used touch end.zengguang 2014.08.22*/
pwr_off:
	if (pdata->power_on)
		pdata->power_on(false);
	else
		ft5x36_power_on(fts_ts, false);
pwr_deinit:
	if (pdata->power_init)
		pdata->power_init(false);
	else
		ft5x36_power_init(fts_ts, false);
unreg_inputdev:
	input_unregister_device(input_dev);
	input_dev = NULL;

free_inputdev:
	input_free_device(input_dev);
	
free_mem:
	kfree(fts_ts);
	return err;
}
static int  fts_ts_remove(struct i2c_client *client)
{
	struct fts_ts_data *fts_ts;
	fts_ts = i2c_get_clientdata(client);
	input_unregister_device(fts_ts->input_dev);
	#ifdef CONFIG_PM
	gpio_free(fts_ts->pdata->reset_gpio);
	#endif
/*modified for pin used touch start.zengguang 2014.08.22*/	
	if (fts_ts->ts_pinctrl) {
		ft5x36_ts_pinctrl_put(fts_ts);
	}
/*modified for pin used touch end.zengguang 2014.08.22*/
#ifdef FTS_SYSFS_DEBUG
  #ifdef FTS_APK_DEBUG
	fts_release_apk_debug_channel();
  #endif
	fts_release_sysfs(client);
#endif
	#ifdef FTS_CTL_IIC
	ft_rw_iic_drv_exit();
	#endif
	free_irq(client->irq, fts_ts);
	kfree(fts_ts);
//added by pangle for gesture 20140917 begin
	destroy_workqueue(fts_ts->fts_wq);
	mutex_destroy(&fts_ts->fts_mutex);
//added by pangle for gesture 20140917 end
//	destroy_workqueue(fts_ts->fts_pm_wq);

	//yuquan added timeout wakelock at 20141129
	wake_lock_destroy(&fts_ts->tp_wake_lock);


	i2c_set_clientdata(client, NULL);
	return 0;
}

static const struct i2c_device_id fts_ts_id[] = {
	{FTS_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, fts_ts_id);

#ifdef CONFIG_OF
static struct of_device_id ft5x36_match_table[] = {
	{ .compatible = "focaltech,FT5x36",},
	{ },
};
#else
#define ft5x36_match_table NULL
#endif


static struct i2c_driver fts_ts_driver = {
	.probe 		= fts_ts_probe,
	.remove 	=  fts_ts_remove,
	.id_table 	= fts_ts_id,
	.driver = {
		   .name = FTS_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = ft5x36_match_table,
#ifdef CONFIG_PM
		   .pm = &ft5x36_ts_pm_ops,
#endif
		   },
};

static int __init fts_ts_init(void)
{
	int ret;
	ret = i2c_add_driver(&fts_ts_driver);
	if (ret) {
		printk(KERN_WARNING "Adding FTS driver failed " "(errno = %d)\n", ret);
	} else {
		pr_info("Successfully added driver %s\n",fts_ts_driver.driver.name);
	}
	return ret;
}

static void __exit fts_ts_exit(void)
{
	i2c_del_driver(&fts_ts_driver);
}

module_init(fts_ts_init);
module_exit(fts_ts_exit);

MODULE_AUTHOR("<luowj>");
MODULE_DESCRIPTION("FocalTech TouchScreen driver");
MODULE_LICENSE("GPL");
