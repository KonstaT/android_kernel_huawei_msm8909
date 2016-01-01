/*
 * This file is part of the AP3426, AP3212C and AP3216C sensor driver.
 * AP3426 is combined proximity and ambient light sensor.
 * AP3216C is combined proximity, ambient light sensor and IRLED.
 *
 * Contact: John Huang <john.huang@dyna-image.com>
 *	    Templeton Tsai <templeton.tsai@dyna-image.com>
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 *
 * Filename: ap3426.c
 *
 * Summary:
 *	AP3426 device driver.
 *
 * Modification History:
 * Date     By       Summary
 * -------- -------- -------------------------------------------------------
 * 02/02/12 YC       1. Modify irq function to seperate two interrupt routine. 
 *					 2. Fix the index of reg array error in em write. 
 * 02/22/12 YC       3. Merge AP3426 and AP3216C into the same driver. (ver 1.8)
 * 03/01/12 YC       Add AP3212C into the driver. (ver 1.8)
 * 07/25/14 John	  Ver.2.1 , ported for Nexus 7
 * 08/21/14 Templeton AP3426 Ver 1.0, ported for Nexus 7
 * 09/24/14 kevin    Modify for Qualcomm8x10 to support device tree
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/string.h>
#include <mach/gpio.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include "ap3426.h"
#include <linux/regulator/consumer.h>	
#include <linux/ioctl.h>
#ifdef CONFIG_GET_HARDWARE_INFO
#include <mach/hardware_info.h>
#endif

#define AP3426_DRV_NAME		"ap3426"
#define DRIVER_VERSION		"1.0"

#define PL_TIMER_DELAY 200

/* misc define */ 
#define MIN_ALS_POLL_DELAY_MS	110


#define AP3426_VDD_MIN_UV	2000000
#define AP3426_VDD_MAX_UV	3300000
#define AP3426_VIO_MIN_UV	1750000
#define AP3426_VIO_MAX_UV	1950000

#define LSC_DBG
#ifdef LSC_DBG
#define LDBG(s,args...)	{printk("LDBG: func [%s], line [%d], ",__func__,__LINE__); printk(s,## args);}
#else
#define LDBG(s,args...) {}
#endif

//added by shihuijun for pocket mode 20150127
bool proximity_open_flag;
int yep_i2c_err_flag;
//static void pl_timer_callback(unsigned long pl_data);
static int ap3426_power_ctl(struct ap3426_data *data, bool on);
static int ap3426_power_init(struct ap3426_data*data, bool on);

// AP3426 register
static u8 ap3426_reg_to_idx_array[AP3426_MAX_REG_NUM] = {
	0,	1,	2,	0xff,	0xff,	0xff,	3,	0xff,
	0xff,	0xff,	4,	5,	6,	7,	8,	9,
	10,	0xff,	0xff,	0xff,	0xff,	0xff,	0xff,	0xff,
	0xff,	0xff,	11,	12,	13,	14,	0xff,	0xff,
	15,	16,	17,	18,	19,	20,	21,	0xff,
	22,	23,	24,	25,	26,	27         //20-2f
};
static u8 ap3426_reg[AP3426_NUM_CACHABLE_REGS] = {
	0x00,0x01,0x02,0x06,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x10,0x1A,0x1B,0x1C,0x1D,0x20,0x21,0x22,0x23,0x24,
	0x25,0x26,0x28,0x29,0x2A,0x2B,0x2C,0x2D
};
// AP3426 range
static int ap3426_range[4] = {32768,8192,2048,512};
//static u16 ap3426_threshole[8] = {28,444,625,888,1778,3555,7222,0xffff};

static u8 *reg_array = ap3426_reg;
static int *range = ap3426_range;

static int cali = 100;

struct regulator *vdd;
struct regulator *vio;
bool power_enabled;
//add by yanfei for ap3426 20150107
static struct ap3426_data *pdev_data = NULL;
//static struct attribute_group ap3426_ps_attribute_group;
static int flag=0;
/*
 * register access helpers
 */

/*
fixed for msm8916 kevindang20141010
*/
static struct sensors_classdev sensors_light_cdev = { 
	.name = "ap3426-light", 
	.vendor = "DI", 
	.version = 1, 
	.handle = SENSORS_LIGHT_HANDLE, 
	.type = SENSOR_TYPE_LIGHT, 
	.max_range = "65535", 
	.resolution = "1.0",
	.sensor_power = "0.35",
	.min_delay = 0,	/* us */ 
	.fifo_reserved_event_count = 0, 
	.fifo_max_event_count = 0, 
	.flags = 0,
	.enabled = 0, 
	.delay_msec = 200, 
	.sensors_enable = NULL, 
	.sensors_poll_delay = NULL, 
	.sensors_calibrate = NULL,
	.sensors_write_cal_params = NULL,
	.params = NULL,
}; 


static struct sensors_classdev sensors_proximity_cdev = { 
	.name = "ap3426-proximity", 
	.vendor = "DI", 
	.version = 1, 
	.handle = SENSORS_PROXIMITY_HANDLE, 
	.type = SENSOR_TYPE_PROXIMITY, 
	.max_range = "5.0", 
	.resolution = "5.0",
	.sensor_power = "0.35",
	.min_delay = 0, 
	.fifo_reserved_event_count = 0, 
	.fifo_max_event_count = 0, 
	.flags = 1,
	.enabled = 0, 
	.delay_msec = 200, 
	.sensors_enable = NULL, 
	.sensors_poll_delay = NULL, 
	.sensors_calibrate = NULL,
	.sensors_write_cal_params = NULL,
	.sensors_get_crosstalk = NULL,
	
	.params = NULL,
}; 

static int __ap3426_read_reg(struct i2c_client *client,
	u32 reg, u8 mask, u8 shift)
{
    struct ap3426_data *data = i2c_get_clientdata(client);

    return (data->reg_cache[ap3426_reg_to_idx_array[reg]] & mask) >> shift;
}

static int __ap3426_write_reg(struct i2c_client *client,
	u32 reg, u8 mask, u8 shift, u8 val)
{
    struct ap3426_data *data = i2c_get_clientdata(client);
    int ret = 0;
    u8 tmp;
//add by yanfei for mutex lock 20150205 begin
 //   mutex_lock(&data->lock);
	
    tmp = data->reg_cache[ap3426_reg_to_idx_array[reg]];
    tmp &= ~mask;
    tmp |= val << shift;

    ret = i2c_smbus_write_byte_data(client, reg, tmp);
    if (!ret)
	data->reg_cache[ap3426_reg_to_idx_array[reg]] = tmp;
	
//   mutex_unlock(&data->lock);
//add by yanfei for mutex lock 20150205 end
    return ret;
}

/*
 * internally used functions
 */

/* range */
static int ap3426_get_range(struct i2c_client *client)
{
    u8 idx = __ap3426_read_reg(client, AP3426_REG_ALS_CONF,
	    AP3426_ALS_RANGE_MASK, AP3426_ALS_RANGE_SHIFT); 
    return range[idx];
}

static int ap3426_set_range(struct i2c_client *client, int range)
{
    return __ap3426_write_reg(client, AP3426_REG_ALS_CONF,
	    AP3426_ALS_RANGE_MASK, AP3426_ALS_RANGE_SHIFT, range);
}

/* mode */
static int ap3426_get_mode(struct i2c_client *client)
{
    int ret;

    ret = __ap3426_read_reg(client, AP3426_REG_SYS_CONF,
	    AP3426_REG_SYS_CONF_MASK, AP3426_REG_SYS_CONF_SHIFT);
    return ret;
}

static int ap3426_set_mode(struct i2c_client *client, int mode)
{
    int ret;



    ret = __ap3426_write_reg(client, AP3426_REG_SYS_CONF,
	    AP3426_REG_SYS_CONF_MASK, AP3426_REG_SYS_CONF_SHIFT, mode);
    return ret;
}

/* ALS low threshold */
static int ap3426_get_althres(struct i2c_client *client)
{
    int lsb, msb;
    lsb = __ap3426_read_reg(client, AP3426_REG_ALS_THDL_L,
	    AP3426_REG_ALS_THDL_L_MASK, AP3426_REG_ALS_THDL_L_SHIFT);
    msb = __ap3426_read_reg(client, AP3426_REG_ALS_THDL_H,
	    AP3426_REG_ALS_THDL_H_MASK, AP3426_REG_ALS_THDL_H_SHIFT);
    return ((msb << 8) | lsb);
}

static int ap3426_set_althres(struct i2c_client *client, int val)
{

    int lsb, msb, err;

    msb = val >> 8;
    lsb = val & AP3426_REG_ALS_THDL_L_MASK;

    err = __ap3426_write_reg(client, AP3426_REG_ALS_THDL_L,
	    AP3426_REG_ALS_THDL_L_MASK, AP3426_REG_ALS_THDL_L_SHIFT, lsb);
    if (err)
	return err;

    err = __ap3426_write_reg(client, AP3426_REG_ALS_THDL_H,
	    AP3426_REG_ALS_THDL_H_MASK, AP3426_REG_ALS_THDL_H_SHIFT, msb);

    return err;
}

/* ALS high threshold */
static int ap3426_get_ahthres(struct i2c_client *client)
{
    int lsb, msb;
    lsb = __ap3426_read_reg(client, AP3426_REG_ALS_THDH_L,
	    AP3426_REG_ALS_THDH_L_MASK, AP3426_REG_ALS_THDH_L_SHIFT);
    msb = __ap3426_read_reg(client, AP3426_REG_ALS_THDH_H,
	    AP3426_REG_ALS_THDH_H_MASK, AP3426_REG_ALS_THDH_H_SHIFT);
    return ((msb << 8) | lsb);
}

static int ap3426_set_ahthres(struct i2c_client *client, int val)
{
    int lsb, msb, err;

    msb = val >> 8;
    lsb = val & AP3426_REG_ALS_THDH_L_MASK;

    err = __ap3426_write_reg(client, AP3426_REG_ALS_THDH_L,
	    AP3426_REG_ALS_THDH_L_MASK, AP3426_REG_ALS_THDH_L_SHIFT, lsb);
    if (err)
	return err;

    err = __ap3426_write_reg(client, AP3426_REG_ALS_THDH_H,
	    AP3426_REG_ALS_THDH_H_MASK, AP3426_REG_ALS_THDH_H_SHIFT, msb);

    return err;
}

/* PX low threshold */
static int ap3426_get_plthres(struct i2c_client *client)
{
    int lsb, msb;
    lsb = __ap3426_read_reg(client, AP3426_REG_PS_THDL_L,
	    AP3426_REG_PS_THDL_L_MASK, AP3426_REG_PS_THDL_L_SHIFT);
    msb = __ap3426_read_reg(client, AP3426_REG_PS_THDL_H,
	    AP3426_REG_PS_THDL_H_MASK, AP3426_REG_PS_THDL_H_SHIFT);
    return ((msb << 8) | lsb);
}

static int ap3426_set_plthres(struct i2c_client *client, int val)
{
    int lsb, msb, err;

    msb = val >> 8;
    lsb = val & AP3426_REG_PS_THDL_L_MASK;
   // printk(KERN_ERR"yanfei %s",__func__);

    err = __ap3426_write_reg(client, AP3426_REG_PS_THDL_L,
	    AP3426_REG_PS_THDL_L_MASK, AP3426_REG_PS_THDL_L_SHIFT, lsb);
    if (err)
	return err;

    err = __ap3426_write_reg(client, AP3426_REG_PS_THDL_H,
	    AP3426_REG_PS_THDL_H_MASK, AP3426_REG_PS_THDL_H_SHIFT, msb);

    return err;
}

/* PX high threshold */
static int ap3426_get_phthres(struct i2c_client *client)
{
    int lsb, msb;
    lsb = __ap3426_read_reg(client, AP3426_REG_PS_THDH_L,
	    AP3426_REG_PS_THDH_L_MASK, AP3426_REG_PS_THDH_L_SHIFT);
    msb = __ap3426_read_reg(client, AP3426_REG_PS_THDH_H,
	    AP3426_REG_PS_THDH_H_MASK, AP3426_REG_PS_THDH_H_SHIFT);
    return ((msb << 8) | lsb);
}

static int ap3426_set_phthres(struct i2c_client *client, int val)
{
    int lsb, msb, err;

    msb = val >> 8;
    lsb = val & AP3426_REG_PS_THDH_L_MASK;
    //printk(KERN_ERR"yanfei %s",__func__);

    err = __ap3426_write_reg(client, AP3426_REG_PS_THDH_L,
	    AP3426_REG_PS_THDH_L_MASK, AP3426_REG_PS_THDH_L_SHIFT, lsb);
    if (err)
	return err;

    err = __ap3426_write_reg(client, AP3426_REG_PS_THDH_H,
	    AP3426_REG_PS_THDH_H_MASK, AP3426_REG_PS_THDH_H_SHIFT, msb);

    return err;
}

static int ap3426_get_adc_value(struct i2c_client *client)
{
    unsigned int lsb, msb, val;
#ifdef LSC_DBG
    unsigned int tmp,range;
#endif
    lsb = i2c_smbus_read_byte_data(client, AP3426_REG_ALS_DATA_LOW);

    if (lsb < 0) {
	return lsb;
    }

    msb = i2c_smbus_read_byte_data(client, AP3426_REG_ALS_DATA_HIGH);

    if (msb < 0)
	return msb;

#ifdef LSC_DBG
    range = ap3426_get_range(client);
    tmp = (((msb << 8) | lsb) * range) >> 16;
    tmp = tmp * cali / 100;
//    LDBG("ALS val=%d lux\n",tmp);
#endif
    val = msb << 8 | lsb;

    return val;
}
//add by yanfei for get ir value 20150121 begin
static int ap3426_get_ir_value(struct i2c_client *client)
{
    unsigned int lsb, msb, val;
#ifdef LSC_DBG
    unsigned int tmp,range;
#endif

    i2c_smbus_write_byte_data(client, AP3426_REG_ALS_CONF, 0x10);//range1
    i2c_smbus_write_byte_data(client, AP3426_REG_PS_LEDD, 0x02);//led driver 16.7%
    i2c_smbus_write_byte_data(client, AP3426_REG_PS_MEAN, 0x03);//ps mean time = 3
    i2c_smbus_write_byte_data(client, AP3426_REG_PS_INTEGR, 0x03);//ptime = 1t
//    i2c_smbus_write_byte_data(client, 0x00, 0x03);//als+ps mode 
    
  
    lsb = i2c_smbus_read_byte_data(client, AP3426_REG_IR_DATA_LOW);

    if (lsb < 0) {
	return lsb;
    }

    msb = i2c_smbus_read_byte_data(client, AP3426_REG_IR_DATA_HIGH);

    if (msb < 0){
	return msb;
    	}

#ifdef LSC_DBG
    range = ap3426_get_range(client);
    tmp = (((msb << 8) | lsb) * range) >> 16;
    tmp = tmp * cali / 100;
#endif
    val = (msb&0x03) << 8 | lsb;

    return val;
}
//add by yanfei for get ir value 20150121 end

static int ap3426_get_object(struct i2c_client *client)
{
//    struct ap3426_data *data = i2c_get_clientdata(client);
    int val;
//add by yanfei for mutex lock 20150205 begin
//    mutex_lock(&data->lock);
    val = i2c_smbus_read_byte_data(client, AP3426_OBJ_COMMAND);
//    mutex_unlock(&data->lock);
//add by yanfei for mutex lock 20150205 end
    //LDBG("val=%x\n", val);
    val &= AP3426_OBJ_MASK;

//    return val >> AP3426_OBJ_SHIFT;
	return !(val >> AP3426_OBJ_SHIFT);
}

static int ap3426_get_intstat(struct i2c_client *client)
{
    int val;

    val = i2c_smbus_read_byte_data(client, AP3426_REG_SYS_INTSTATUS);
    val &= AP3426_REG_SYS_INT_MASK;

    return val >> AP3426_REG_SYS_INT_SHIFT;
}

static int ap3426_get_px_value(struct i2c_client *client)
{
//    struct ap3426_data *data = i2c_get_clientdata(client);
	
    int lsb, msb;
//add by yanfei for mutex lock 20150205 begin
//    mutex_lock(&data->lock);

    lsb = i2c_smbus_read_byte_data(client, AP3426_REG_PS_DATA_LOW);
    msb = i2c_smbus_read_byte_data(client, AP3426_REG_PS_DATA_HIGH);

//    mutex_unlock(&data->lock);
//add by yanfei for mutex lock 20150205 end
  //  LDBG("%s, IR = %d\n", __func__, (u32)(msb));
    return (u32)(((msb & AL3426_REG_PS_DATA_HIGH_MASK) << 8) | (lsb & AL3426_REG_PS_DATA_LOW_MASK));
}

static int ap3426_ps_enable(struct ap3426_data *ps_data,int enable)
{
    int32_t ret;
    /** added by litao for input sync timestamp begin **/
    int value;
    int i=0;
    int tmp;
    int pxvalue=0;
    int read_count = 0;
    int sum_value = 0;
    ktime_t timestamp;
    timestamp = ktime_get_boottime();
    /** added by litao for input sync timestamp end **/
	
    printk(KERN_ERR"ap3426_ps_enable =%d \n",enable);
//add by yanfei for enable ps or als sensor 20150205 begin
//add by yanfei for sensor reg init 20150604 begin
    if(yep_i2c_err_flag == 1)
	{
		yep_i2c_err_flag = 0;
	//modify for ps-sensor config by yanfei 20150318 begin	
	    i2c_smbus_write_byte_data(ps_data->client, AP3426_REG_SYS_INTCTRL, 0x80);
	    i2c_smbus_write_byte_data(ps_data->client, AP3426_REG_ALS_CONF, 0x10);   
	    i2c_smbus_write_byte_data(ps_data->client, AP3426_REG_PS_CONF, 0x04);
	    i2c_smbus_write_byte_data(ps_data->client, AP3426_REG_PS_LEDD, 0x02);
	    i2c_smbus_write_byte_data(ps_data->client, AP3426_REG_PS_MEAN, 0x03);//ps mean time = 3
	    i2c_smbus_write_byte_data(ps_data->client, AP3426_REG_PS_INTEGR, 0x03);
	    i2c_smbus_write_byte_data(ps_data->client, AP3426_REG_PS_PERSIS, 0x02);
	    i2c_smbus_write_byte_data(ps_data->client, AP3426_REG_PS_SMARTINT, 0x00);		
	}
//add by yanfei for sensor reg init 20150604 end

    mutex_lock(&ps_data->lock);

    if(enable==1)
    {
//add by yanfei for SW00134021 20150507 begin 
	    ret = __ap3426_write_reg(ps_data->client,
			             AP3426_REG_SYS_CONF, AP3426_MODE_MASK, 0,2);
	    msleep(10);
	    for(i=0;i<4;i++)
	    {
	        tmp = ap3426_get_px_value(ps_data->client);
		 if(tmp < 0)
		 {
			printk(KERN_ERR "%s: i2c read ps data fail. \n", __func__);
			mutex_unlock(&ps_data->lock);
			return -1;
		 }
		 sum_value += tmp;
		 read_count++;
		 msleep(6);
	    }
	    ret = __ap3426_write_reg(ps_data->client,
			             AP3426_REG_SYS_CONF, AP3426_MODE_MASK, 0,0);
	    if(read_count > 0)
	    {
	    		value=sum_value/read_count;
			printk(KERN_ERR "%s:value=%d sum_value=%d read_count=%d. \n", __func__,value,sum_value,read_count);
	    }
	    else
	    {
	    	    	value=200;

	    }
	    if(value == 0)
	    {
	        value = 200;
	    }
	    pxvalue=value-ps_data->crosstalk;
	    printk(KERN_ERR" %s px_value=%d ps_data->crosstalk=%d\n",__func__,pxvalue,ps_data->crosstalk);
//modify by yanfei for sensor calibration 20150629
	    if((pxvalue < 150)&&(pxvalue > 0))
	    {
	    	ps_data->ps_thres_high=value+70;
		ps_data->ps_thres_low =value+35;
		flag =1;
		ps_data->prevalue=value;
	    }
	    else if(pxvalue <= 0)
	    {
	    	ps_data->ps_thres_high=ps_data->crosstalk+70;
		ps_data->ps_thres_low =ps_data->crosstalk+35;
		flag =1;
		ps_data->prevalue=ps_data->crosstalk;
	    }
	    else
	    {
		    if(flag == 1)
			{
				ps_data->ps_thres_high=ps_data->prevalue+70;
				ps_data->ps_thres_low =ps_data->prevalue+35;
			}
			else
			{
				ps_data->ps_thres_high=ps_data->crosstalk+70;
				ps_data->ps_thres_low =ps_data->crosstalk+35;	
			}
		}
	    msleep(10);
    	    if(ps_data->ps_enable==0)
    	    {
    	    	    ps_data->ps_enable = 1;
		    ap3426_set_phthres(ps_data->client,ps_data->ps_thres_high);
        	    ap3426_set_plthres(ps_data->client, ps_data->ps_thres_low);
	    	    if(ps_data->rels_enable==1)
	    	    	{
	    	        ret = __ap3426_write_reg(ps_data->client,
		             AP3426_REG_SYS_CONF, AP3426_MODE_MASK, 0,3);
			 printk(KERN_ERR" %s misc_ls_opened=%d enable=%d\n",__func__,ps_data->rels_enable,enable);
	    	    	}
			else
			{
			 ret = __ap3426_write_reg(ps_data->client,
		        	AP3426_REG_SYS_CONF, AP3426_MODE_MASK, 0, 2);

			 printk(KERN_ERR" %s misc_ls_opened=%d enable=%d\n",__func__,ps_data->rels_enable,enable);
			}

		        enable_irq(ps_data->client->irq);
			 enable_irq_wake(ps_data->client->irq);
    	    	}


    }
    else if(enable==0)
    {
            if(ps_data->ps_enable==1)
            {
            	     ps_data->ps_enable= 0;
		     disable_irq_wake(ps_data->client->irq); 
		     disable_irq(ps_data->client->irq);
	            if(ps_data->rels_enable==1)
	    	     	{
			   ap3426_set_phthres(ps_data->client,1023);
	        	   ap3426_set_plthres(ps_data->client, 0);
	    	          ret = __ap3426_write_reg(ps_data->client,
		        	AP3426_REG_SYS_CONF, AP3426_MODE_MASK, 0, 3);

			printk(KERN_ERR" %s misc_ls_opened=%d enable=%d\n",__func__,ps_data->rels_enable,enable);
	    	     	}
			else
			{
			ret = __ap3426_write_reg(ps_data->client,
		        	AP3426_REG_SYS_CONF, AP3426_MODE_MASK, 0, 0);
			printk(KERN_ERR" %s misc_ls_opened=%d enable=%d\n",__func__,ps_data->rels_enable,enable);
			}

		       input_report_abs(ps_data->psensor_input_dev, ABS_DISTANCE, 1);
			/** added by litao for input sync timestamp begin **/
			input_event(ps_data->psensor_input_dev, EV_SYN, SYN_TIME_SEC,
					ktime_to_timespec(timestamp).tv_sec);
			input_event(ps_data->psensor_input_dev, EV_SYN, SYN_TIME_NSEC,
					ktime_to_timespec(timestamp).tv_nsec);
			/** added by litao for input sync timestamp end **/
		       input_sync(ps_data->psensor_input_dev);
            }
    	     
		   
    }
    else
     	     ret = __ap3426_write_reg(ps_data->client,
	        AP3426_REG_SYS_CONF, AP3426_REG_SYS_INT_PMASK, 1, enable);
//modify by yanfei for ap3426  ps enable  20150212 end
//add by yanfei for enable ps or als sensor 20150205 end
    if(ret < 0){
	printk("ps enable error!!!!!!\n");
    }
	mutex_unlock(&ps_data->lock);

//	enable_irq(ps_data->client->irq);

//    ret = mod_timer(&ps_data->pl_timer, jiffies + msecs_to_jiffies(PL_TIMER_DELAY));

    return ret;
}
static int ap3426_ls_enable(struct ap3426_data *ps_data,int enable)
{
    	int32_t ret;
    LDBG("%s, misc_ls_opened = %d, enable=%d\n", __func__, ps_data->rels_enable, enable);
//add by yanfei for enable ps or als sensor 20150205 begin
//    wake_lock_timeout(&ps_data->prx_wake_lock, 5*HZ);
//modify by yanfei for ap3426  ps enable  20150212 begin

    mutex_lock(&ps_data->lock);
    if(enable==0)
    	{
    	    if(ps_data->rels_enable==1)
    	    {
    	           ps_data->rels_enable=0;
		    if(ps_data->ps_enable==1)
	        	{
	        	    printk(KERN_ERR" %s misc_ps_opened=%d\n",__func__,ps_data->ps_enable);
			    ret = __ap3426_write_reg(ps_data->client,
			        AP3426_REG_SYS_CONF, AP3426_MODE_MASK, 0, 2);
	        	}
			else
				{
				    ret = __ap3426_write_reg(ps_data->client,
		        		AP3426_REG_SYS_CONF, AP3426_MODE_MASK, 0, 0);
				printk(KERN_ERR" %s misc_ps_opened=%d\n",__func__,ps_data->ps_enable);
				}
    	    	}
		cancel_delayed_work_sync(&ps_data->work_light);

    	}
	else
	{
		  if(ps_data->rels_enable==0)
		  {
		  	  ps_data->rels_enable=1;
			  if(ps_data->ps_enable==1)
			  	{
			            ret = __ap3426_write_reg(ps_data->client,
		        		  AP3426_REG_SYS_CONF, AP3426_MODE_MASK, 0, 3);
				     printk(KERN_ERR" %s misc_ps_opened=%d enable=%d\n",__func__,ps_data->ps_enable,enable);
			  	}
				else
		        	{
			        	 ap3426_set_phthres(ps_data->client,1023);
			        	 ap3426_set_plthres(ps_data->client, 0);
			        	 ret = __ap3426_write_reg(ps_data->client,
			        		   AP3426_REG_SYS_CONF, AP3426_MODE_MASK, 0, 3);

					 printk(KERN_ERR" %s misc_ps_opened=%d enable=%d\n",__func__,ps_data->ps_enable,enable);
		        	}
				cancel_delayed_work_sync(&ps_data->work_light);
				schedule_delayed_work(&ps_data->work_light, msecs_to_jiffies(ps_data->light_poll_delay));	
		  }
	}
//modify by yanfei for ap3426  ps enable  20150212 end
//add by yanfei for enable ps or als sensor 20150205 end

    if(ret < 0){
        printk("ls enable error!!!!!!\n");
    } 
     mutex_unlock(&ps_data->lock);

    return ret;
}

/*********************************************************************
light sensor register & unregister
********************************************************************/

static int ap3426_register_lsensor_device(struct i2c_client *client, struct ap3426_data *data)
{
    struct input_dev *input_dev;
    int rc;

    LDBG("allocating input device lsensor\n");
    input_dev = input_allocate_device();
    if (!input_dev) {
	dev_err(&client->dev,"%s: could not allocate input device for lsensor\n", __FUNCTION__);
	rc = -ENOMEM;
	goto done;
    }
    data->lsensor_input_dev = input_dev;
    input_set_drvdata(input_dev, data);
    input_dev->name = "light";//"lightsensor-level";
    input_dev->dev.parent = &client->dev;
    set_bit(EV_ABS, input_dev->evbit);
    input_set_abs_params(input_dev, ABS_MISC, 0, 65535, 0, 0);

    rc = input_register_device(input_dev);
    if (rc < 0) {
	pr_err("%s: could not register input device for lsensor\n", __FUNCTION__);
	goto done;
    }
//    rc = sysfs_create_group(&data->client->dev.kobj, &ap3426_ls_attribute_group);// every devices register his own devices
done:
    return rc;
}

static void ap3426_unregister_lsensor_device(struct i2c_client *client, struct ap3426_data *data)
{
    input_unregister_device(data->lsensor_input_dev);
}


static void ap3426_unregister_heartbeat_device(struct i2c_client *client, struct ap3426_data *data)
{
    input_unregister_device(data->hsensor_input_dev);
}

/*********************************************************************
proximity sensor register & unregister
********************************************************************/

static int ap3426_register_psensor_device(struct i2c_client *client, struct ap3426_data *data)
{
    struct input_dev *input_dev;
    int rc;

    LDBG("allocating input device psensor\n");
    input_dev = input_allocate_device();
    if (!input_dev) {
	dev_err(&client->dev,"%s: could not allocate input device for psensor\n", __FUNCTION__);
	rc = -ENOMEM;
	goto done;
    }
    data->psensor_input_dev = input_dev;
    input_set_drvdata(input_dev, data);
    input_dev->name = "proximity";
    input_dev->dev.parent = &client->dev;
    set_bit(EV_ABS, input_dev->evbit);
    input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);

    rc = input_register_device(input_dev);
    if (rc < 0) {
	pr_err("%s: could not register input device for psensor\n", __FUNCTION__);
	goto done;
    }

//    rc = sysfs_create_group(&data->client->dev.kobj, &ap3426_ps_attribute_group);// every devices register his own devices

done:
    return rc;
}

static void ap3426_unregister_psensor_device(struct i2c_client *client, struct ap3426_data *data)
{
    input_unregister_device(data->psensor_input_dev);
}


/* range */
static ssize_t ap3426_show_range(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);
    return sprintf(buf, "%i\n", ap3426_get_range(data->client));
}

static ssize_t ap3426_store_range(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);
    unsigned long val;
    int ret;

    if ((strict_strtoul(buf, 10, &val) < 0) || (val > 3))
	return -EINVAL;

    ret = ap3426_set_range(data->client, val);

    return (ret < 0)? ret:count;
}


//kevindang for msm8916 20141010
static ssize_t ap3426_als_enable_set(struct sensors_classdev *sensors_cdev, 
						unsigned int enabled) 
{ 
	struct ap3426_data *als_data = container_of(sensors_cdev, 
						struct ap3426_data, als_cdev); 
	int err; 

	err = ap3426_ls_enable(als_data,enabled);


	if (err < 0) 
		return err; 
	return 0; 
} 

static ssize_t ap3426_show_ps_all_reg(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	struct i2c_client *client=data->client;
       int idx=0,count=0;
	int reg_value[1];	
	#define AP3xx6_NUM_CACHABLE_REGS	29 
	u8 AP3xx6_reg[AP3xx6_NUM_CACHABLE_REGS] = 
		{0x00,0x01,0x02,0x06,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
	   0x10,0x14,0x1a,0x1b,0x1c,0x1d,
	   0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x28,0x29,0x2a,0x2b,0x2c,0x2d};

    for(idx=0;idx<AP3xx6_NUM_CACHABLE_REGS;idx++){
    reg_value[0] = i2c_smbus_read_byte_data(client,AP3xx6_reg[idx]);
    if(reg_value[0] < 0)
	{
    		count+=sprintf(buf+count, "i2c read_reg err \n");
    		return count;
	}
	 count+=sprintf(buf+count, "[%x]=0x%x \n",AP3xx6_reg[idx],reg_value[0]);
    }

    return count;
	
}
static ssize_t ap3426_show_ps_calibration(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct ap3426_data *data = input_get_drvdata(input);
	struct i2c_client *client=data->client;
	int tmp;
	int i;
	int read_count = 0;
	int sum_value = 0;
//modify by yanfei for ps-sensor threshold 20150318 begin
	int ps_th_limit = 700;
	data->ps_high=70;
	data->ps_low =35;
	msleep(200);
	for(i=0;i<40;i++){
		tmp = ap3426_get_px_value(client);
		if(tmp < 0)
		{
			printk(KERN_ERR "%s: i2c read ps data fail. \n", __func__);
			return -1;
		}else if(tmp < ps_th_limit){
			sum_value += tmp;
			read_count ++;
		}
		printk(KERN_ERR" test tmp==%d read_count=%d\n",tmp,read_count);
		msleep(50);
	}
//add by yanfei for calibration fail
	if(read_count == 0){
//modify by yanfei for SW00144500  20150528
		data->crosstalk =-1;
		return sprintf(buf, "%d\n",  data->crosstalk);
	}else{
		 data->crosstalk = sum_value/read_count;
		 if(data->crosstalk == 0)
		 {
		 	data->crosstalk = 5;
		 }
		 printk(KERN_ERR"%s data->crosstalk=%d,sum_value=%d,read_count=%d\n",__func__,data->crosstalk,sum_value,read_count);
		 tmp=ap3426_set_phthres(client, data->ps_high+data->crosstalk);
		 if(tmp<0)
		 	{
		 		data->ps_thres_high = data->ps_high+data->crosstalk;
				data->ps_thres_low = data->ps_low+data->crosstalk;
		 		printk(KERN_ERR"%s err ap3426_set_phthres\n ",__func__);
				return sprintf(buf, "%d\n", data->crosstalk);

		 	}
		 data->ps_thres_high=data->ps_high+data->crosstalk;
		 tmp=ap3426_set_plthres(client, data->ps_low+data->crosstalk);
		 if(tmp<0)
		 {
		 		data->ps_thres_high = data->ps_high+data->crosstalk;
				data->ps_thres_low = data->ps_low+data->crosstalk;
		 		printk(KERN_ERR"%s err ap3p3426_set_plthres\n ",__func__);
				return sprintf(buf, "%d\n", data->crosstalk);

		 }		 
		 data->ps_thres_low=data->ps_low+data->crosstalk;
	}
	printk(KERN_ERR"%s data->crosstalk=%d data->ps_thres_high=%d data->ps_thres_low=%d\n",__func__,data->crosstalk,data->ps_thres_high,data->ps_thres_low);
	return sprintf(buf, "%d\n", data->crosstalk);

}
static DEVICE_ATTR(ps_cal, 0666, ap3426_show_ps_calibration, NULL);
static DEVICE_ATTR(all_reg, 0660, ap3426_show_ps_all_reg, NULL);


//modify for ap3426 crosstalk 20150112
static ssize_t ap3426_get_ps_crosstalk(struct sensors_classdev *sensors_cdev,
		unsigned int prox_data)
{
	struct ap3426_data *data = container_of(sensors_cdev,
			struct ap3426_data, ps_cdev);
	struct i2c_client *client=data->client;
	int ret;
	data->crosstalk= prox_data;
//modify for ps-sensor threshold by yanfei 20150318 begin
	data->ps_high=70;
	data->ps_low =35;
	mutex_lock(&data->lock);
//add by yanfei for set threshold 20150214 beign
	//printk(KERN_ERR"%s %d %d %d %d \n",__func__,data->ps_thres_high,data->ps_thres_low,data->ps_high,data->ps_low);
	if(data->crosstalk==0)
		{
			data->ps_thres_high = data->ps_high+data->crosstalk;
			data->ps_thres_low = data->ps_low+data->crosstalk;
			mutex_unlock(&data->lock);
			printk(KERN_ERR"%s %d %d %d %d \n",__func__,data->ps_thres_high,data->ps_thres_low,data->ps_high,data->ps_low);
			return 0;
		}

	ret=ap3426_set_phthres(client, data->ps_high+data->crosstalk);
	if(ret<0)
		{
			data->ps_thres_high = data->ps_high+data->crosstalk;
			data->ps_thres_low = data->ps_low+data->crosstalk;
			mutex_unlock(&data->lock);
		 	printk(KERN_ERR"%s err ap3426_set_phthres\n ",__func__);
			return 0;
		}
	else
		{
			data->ps_thres_high=data->ps_high+data->crosstalk;
		}
	ret=ap3426_set_plthres(client, data->ps_low+data->crosstalk);
	if(ret<0)
		{
			data->ps_thres_high = data->ps_high+data->crosstalk;
			data->ps_thres_low = data->ps_low+data->crosstalk;
			mutex_unlock(&data->lock);
			printk(KERN_ERR"%s err ap3426_set_plthres\n ",__func__);
		        return 0;
		 }
	else
		{
			data->ps_thres_low=data->ps_low+data->crosstalk;
		}
//add by yanfei for set threshold 20150214 end
	mutex_unlock(&data->lock);
	printk(KERN_ERR"%s data->ps_thres_high=%d data->ps_thres_low=%d  data->ps_high= %d data->ps_low=%d \n",__func__,data->ps_thres_high,data->ps_thres_low,data->ps_high,data->ps_low);
	return 0;
}
//static struct device_attribute ps_cal_attribute = __ATTR(ps_cal, 0666, ap3426_show_ps_calibration, NULL);
//static struct attribute *ap3426_ps_attrs [] =
//{
//    &ps_enable_attribute.attr,
//    &ps_cal_attribute.attr,	
//    NULL
//};

//static struct attribute_group ap3426_ps_attribute_group = {
//        .attrs = ap3426_ps_attrs,
	 
//};
//modified by yanfei for calibration 2014/07/28  end
/**************************************************************************/
static ssize_t ap3426_als_poll_delay_set(struct sensors_classdev *sensors_cdev, 
					   unsigned int delay_msec) 
{ 
   struct ap3426_data *als_data = container_of(sensors_cdev, 
					   struct ap3426_data, als_cdev); 	
//modify by yanfei for ap3426  ps enable  20150212 begin

   	if (delay_msec < ALS_SET_MIN_DELAY_TIME * 1000000)
		delay_msec = ALS_SET_MIN_DELAY_TIME * 1000000;	
	
	als_data->light_poll_delay = delay_msec /1000000;	// convert us => ms

	
	/* we need this polling timer routine for sunlight canellation */
	if(als_data->rels_enable == 1){ 
		/*
		 * If work is already scheduled then subsequent schedules will not
		 * change the scheduled time that's why we have to cancel it first.
		 */
		 //modify by yanfei for work 
		cancel_delayed_work_sync(&als_data->work_light);
		schedule_delayed_work(&als_data->work_light, msecs_to_jiffies(als_data->light_poll_delay));
				
	}
//modify by yanfei for ap3426  ps enable  20150212 end
   return 0; 
} 

static ssize_t ap3426_ps_enable_set(struct sensors_classdev *sensors_cdev, 
					   unsigned int enabled) 
{ 
   struct ap3426_data *ps_data = container_of(sensors_cdev, 
					   struct ap3426_data, ps_cdev); 
   int err; 

   err = ap3426_ps_enable(ps_data,enabled);


   if (err < 0) 
	   return err; 
   return 0; 
}

static int ap3426_ps_flush(struct sensors_classdev *sensors_cdev)
{
	struct ap3426_data *data = container_of(sensors_cdev,
			struct ap3426_data, ps_cdev);

	input_event(data->psensor_input_dev, EV_SYN, SYN_CONFIG,
			data->flush_count++);
	input_sync(data->psensor_input_dev);

	return 0;
}

static int ap3426_als_flush(struct sensors_classdev *sensors_cdev)
{
	struct ap3426_data *data = container_of(sensors_cdev,
			struct ap3426_data, als_cdev);

	input_event(data->lsensor_input_dev, EV_SYN, SYN_CONFIG, data->flush_count++);
	input_sync(data->lsensor_input_dev);

	return 0;
}

//end
static int ap3426_power_ctl(struct ap3426_data *data, bool on)
{
	int ret = 0;

	if (!on && data->power_enabled)
	{
		ret = regulator_disable(data->vdd);
		if (ret) 
		{
			dev_err(&data->client->dev,
				"Regulator vdd disable failed ret=%d\n", ret);
			return ret;
		}

		ret = regulator_disable(data->vio);
		if (ret) 
		{
			dev_err(&data->client->dev,
				"Regulator vio disable failed ret=%d\n", ret);
			ret = regulator_enable(data->vdd);
			if (ret) 
			{
				dev_err(&data->client->dev,
					"Regulator vdd enable failed ret=%d\n",
					ret);
			}			
			return ret;
		}

		data->power_enabled = on;
		printk(KERN_INFO "%s: disable ap3426 power", __func__);
		dev_dbg(&data->client->dev, "ap3426_power_ctl on=%d\n",
				on);
	} 
	else if (on && !data->power_enabled) 
	{
		ret = regulator_enable(data->vdd);
		if (ret) 
		{
			dev_err(&data->client->dev,
				"Regulator vdd enable failed ret=%d\n", ret);
			return ret;
		}

		ret = regulator_enable(data->vio);
		if (ret) 
		{
			dev_err(&data->client->dev,
				"Regulator vio enable failed ret=%d\n", ret);
			regulator_disable(data->vdd);
			return ret;
		}

		data->power_enabled = on;
		printk(KERN_INFO "%s: enable ap3426 power", __func__);
	} 
	else
	{
		dev_warn(&data->client->dev,
				"Power on=%d. enabled=%d\n",
				on, data->power_enabled);
	}

	return ret;
}

static int ap3426_power_init(struct ap3426_data*data, bool on)
{
	int ret;

	if (!on)
	{
		if (regulator_count_voltages(data->vdd) > 0)
			regulator_set_voltage(data->vdd,
					0, AP3426_VDD_MAX_UV);

		regulator_put(data->vdd);

		if (regulator_count_voltages(data->vio) > 0)
			regulator_set_voltage(data->vio,
					0, AP3426_VIO_MAX_UV);

		regulator_put(data->vio);
	} 
	else 
	{
		data->vdd = regulator_get(&data->client->dev, "vdd");
		if (IS_ERR(data->vdd)) 
		{
			ret = PTR_ERR(data->vdd);
			dev_err(&data->client->dev,
				"Regulator get failed vdd ret=%d\n", ret);
			return ret;
		}

		if (regulator_count_voltages(data->vdd) > 0)
		{
			ret = regulator_set_voltage(data->vdd,
					AP3426_VDD_MIN_UV,
					AP3426_VDD_MAX_UV);
			if (ret) 
			{
				dev_err(&data->client->dev,
					"Regulator set failed vdd ret=%d\n",
					ret);
				goto reg_vdd_put;
			}
		}

		data->vio = regulator_get(&data->client->dev, "vio");
		if (IS_ERR(data->vio)) 
		{
			ret = PTR_ERR(data->vio);
			dev_err(&data->client->dev,
				"Regulator get failed vio ret=%d\n", ret);
			goto reg_vdd_set;
		}

		if (regulator_count_voltages(data->vio) > 0)
		{
			ret = regulator_set_voltage(data->vio,
					AP3426_VIO_MIN_UV,
					AP3426_VIO_MAX_UV);
			if (ret) 
			{
				dev_err(&data->client->dev,
				"Regulator set failed vio ret=%d\n", ret);
				goto reg_vio_put;
			}
		}
	}

	return 0;

reg_vio_put:
	regulator_put(data->vio);
reg_vdd_set:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, AP3426_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return ret;
}


static DEVICE_ATTR(range, S_IWUSR | S_IRUGO,
	ap3426_show_range, ap3426_store_range);


/* mode */
static ssize_t ap3426_show_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);
    return sprintf(buf, "%d\n", ap3426_get_mode(data->client));
}

static ssize_t ap3426_store_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);
    unsigned long val;
    int ret;

    if ((strict_strtoul(buf, 10, &val) < 0) || (val > 7))
	return -EINVAL;

    ret = ap3426_set_mode(data->client, val);

    if (ret < 0)
	return ret;
    LDBG("Starting timer to fire in 200ms (%ld)\n", jiffies );
    ret = mod_timer(&data->pl_timer, jiffies + msecs_to_jiffies(PL_TIMER_DELAY));

    if(ret) 
	LDBG("Timer Error\n");
    return count;
}
//modify by yanfei for mode 
static DEVICE_ATTR(mode,0664,
	ap3426_show_mode, ap3426_store_mode);
/* lux */
static ssize_t ap3426_show_lux(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);

    /* No LUX data if power down */
    if (ap3426_get_mode(data->client) == AP3426_SYS_DEV_DOWN)
	return sprintf((char*) buf, "%s\n", "Please power up first!");

    return sprintf(buf, "%d\n", ap3426_get_adc_value(data->client));
}

static DEVICE_ATTR(lux, S_IRUGO, ap3426_show_lux, NULL);


/* Px data */
static ssize_t ap3426_show_pxvalue(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);

    /* No Px data if power down */
    if (ap3426_get_mode(data->client) == AP3426_SYS_DEV_DOWN)
	return -EBUSY;

    return sprintf(buf, "%d\n", ap3426_get_px_value(data->client));
}

static DEVICE_ATTR(pxvalue, S_IRUGO, ap3426_show_pxvalue, NULL);


/* proximity object detect */
static ssize_t ap3426_show_object(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);
    return sprintf(buf, "%d\n", ap3426_get_object(data->client));
}

static DEVICE_ATTR(object, S_IRUGO, ap3426_show_object, NULL);


/* ALS low threshold */
static ssize_t ap3426_show_althres(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);
    return sprintf(buf, "%d\n", ap3426_get_althres(data->client));
}

static ssize_t ap3426_store_althres(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);
    unsigned long val;
    int ret;

    if (strict_strtoul(buf, 10, &val) < 0)
	return -EINVAL;

    ret = ap3426_set_althres(data->client, val);
    if (ret < 0)
	return ret;

    return count;
}

static DEVICE_ATTR(althres, S_IWUSR | S_IRUGO,
	ap3426_show_althres, ap3426_store_althres);


/* ALS high threshold */
static ssize_t ap3426_show_ahthres(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);
    return sprintf(buf, "%d\n", ap3426_get_ahthres(data->client));
}

static ssize_t ap3426_store_ahthres(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);
    unsigned long val;
    int ret;

    if (strict_strtoul(buf, 10, &val) < 0)
	return -EINVAL;

    ret = ap3426_set_ahthres(data->client, val);
    if (ret < 0)
	return ret;

    return count;
}

static DEVICE_ATTR(ahthres, S_IWUSR | S_IRUGO,
	ap3426_show_ahthres, ap3426_store_ahthres);

/* Px low threshold */
static ssize_t ap3426_show_plthres(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);
    return sprintf(buf, "%d\n", ap3426_get_plthres(data->client));
}

static ssize_t ap3426_store_plthres(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);
    unsigned long val;
    int ret;

    if (strict_strtoul(buf, 10, &val) < 0)
	return -EINVAL;

    ret = ap3426_set_plthres(data->client, val);
    if (ret < 0)
	return ret;

    return count;
}

static DEVICE_ATTR(plthres, S_IWUSR | S_IRUGO,
	ap3426_show_plthres, ap3426_store_plthres);

/* Px high threshold */
static ssize_t ap3426_show_phthres(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);
    return sprintf(buf, "%d\n", ap3426_get_phthres(data->client));
}

static ssize_t ap3426_store_phthres(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);
    unsigned long val;
    int ret;

    if (strict_strtoul(buf, 10, &val) < 0)
	return -EINVAL;

    ret = ap3426_set_phthres(data->client, val);
    if (ret < 0)
	return ret;

    return count;
}

static DEVICE_ATTR(phthres, S_IWUSR | S_IRUGO,
	ap3426_show_phthres, ap3426_store_phthres);


/* calibration */
static ssize_t ap3426_show_calibration_state(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
    return sprintf(buf, "%d\n", cali);
}

static ssize_t ap3426_store_calibration_state(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
    struct input_dev *input = to_input_dev(dev);
    struct ap3426_data *data = input_get_drvdata(input);
    int stdls, lux; 
    char tmp[10];

    LDBG("DEBUG ap3426_store_calibration_state..\n");

    /* No LUX data if not operational */
    if (ap3426_get_mode(data->client) == AP3426_SYS_DEV_DOWN)
    {
	printk("Please power up first!");
	return -EINVAL;
    }

    cali = 100;
    sscanf(buf, "%d %s", &stdls, tmp);

    if (!strncmp(tmp, "-setcv", 6))
    {
	cali = stdls;
	return -EBUSY;
    }

    if (stdls < 0)
    {
	printk("Std light source: [%d] < 0 !!!\nCheck again, please.\n\
		Set calibration factor to 100.\n", stdls);
	return -EBUSY;
    }

    lux = ap3426_get_adc_value(data->client);
    cali = stdls * 100 / lux;

    return -EBUSY;
}

static DEVICE_ATTR(calibration, S_IWUSR | S_IRUGO,
	ap3426_show_calibration_state, ap3426_store_calibration_state);

#ifdef LSC_DBG
/* engineer mode */
static ssize_t ap3426_em_read(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct ap3426_data *data = i2c_get_clientdata(client);
    int i;
    u8 tmp;

    LDBG("DEBUG ap3426_em_read..\n");

    for (i = 0; i < AP3426_NUM_CACHABLE_REGS; i++)
    {
//add by yanfei for mutex lock 20150112 begin
//    mutex_lock(&data->lock);
	tmp = i2c_smbus_read_byte_data(data->client, reg_array[i]);
//	mutex_unlock(&data->lock);
//add by yanfei for mutex lock 20150112 end

	printk("Reg[0x%x] Val[0x%x]\n", reg_array[i], tmp);
    }

    return 0;
}

static ssize_t ap3426_em_write(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct ap3426_data *data = i2c_get_clientdata(client);
    u32 addr,val;
    int ret = 0;

    LDBG("DEBUG ap3426_em_write..\n");

    sscanf(buf, "%x%x", &addr, &val);

    printk("Write [%x] to Reg[%x]...\n",val,addr);
//add by yanfei for mutex lock 20150112 begin
//    mutex_lock(&data->lock);

    ret = i2c_smbus_write_byte_data(data->client, addr, val);
//    mutex_unlock(&data->lock);
//add by yanfei for mutex lock 20150112 end

    if (!ret)
	    data->reg_cache[ap3426_reg_to_idx_array[addr]] = val;

    return count;
}
static DEVICE_ATTR(em, S_IWUSR |S_IRUGO,
	ap3426_em_read, ap3426_em_write);
#endif

static struct attribute *ap3426_attributes[] = {
    &dev_attr_range.attr,
    &dev_attr_mode.attr,
    &dev_attr_lux.attr,
    &dev_attr_object.attr,
    &dev_attr_pxvalue.attr,
    &dev_attr_althres.attr,
    &dev_attr_ahthres.attr,
    &dev_attr_plthres.attr,
    &dev_attr_phthres.attr,
    &dev_attr_calibration.attr,
    &dev_attr_ps_cal.attr,
    &dev_attr_all_reg.attr,
#ifdef LSC_DBG
    &dev_attr_em.attr,
#endif
    NULL
};

static const struct attribute_group ap3426_attr_group = {
    .attrs = ap3426_attributes,
};

static int ap3426_init_client(struct i2c_client *client)
{
    struct ap3426_data *data = i2c_get_clientdata(client);
    int i;
    int ret;
//add by yanfei for ap3426 reg init config 20150121 begin
    LDBG("DEBUG ap3426_init_client..\n");
		/*lsensor high low thread*/
    i2c_smbus_write_byte_data(client, AP3426_REG_ALS_THDL_L, 0);
    i2c_smbus_write_byte_data(client, AP3426_REG_ALS_THDL_H, 0);
    i2c_smbus_write_byte_data(client, AP3426_REG_ALS_THDH_L, 0xFF);
    i2c_smbus_write_byte_data(client, AP3426_REG_ALS_THDH_H, 0xFF);
		/*psensor high low thread*/
    i2c_smbus_write_byte_data(client, AP3426_REG_PS_THDL_L, 0x20);
    i2c_smbus_write_byte_data(client, AP3426_REG_PS_THDL_H, 0x03);
    i2c_smbus_write_byte_data(client, AP3426_REG_PS_THDH_L, 0x60);
    i2c_smbus_write_byte_data(client, AP3426_REG_PS_THDH_H, 0x03);
//modify for ps-sensor config by yanfei 20150318 begin	
    i2c_smbus_write_byte_data(client, AP3426_REG_SYS_INTCTRL, 0x80);
    i2c_smbus_write_byte_data(client, AP3426_REG_ALS_CONF, 0x10);   
    i2c_smbus_write_byte_data(client, AP3426_REG_PS_CONF, 0x04);
    i2c_smbus_write_byte_data(client, AP3426_REG_PS_LEDD, 0x02);
    i2c_smbus_write_byte_data(client, AP3426_REG_PS_MEAN, 0x03);//ps mean time = 3
    i2c_smbus_write_byte_data(client, AP3426_REG_PS_INTEGR, 0x03);
    i2c_smbus_write_byte_data(client, AP3426_REG_PS_PERSIS, 0x02);
//modify for ps-sensor config by yanfei 20150318 end
//add by yanfei for set threshold 20150214
    data->ps_thres_high = 1023;
    data->ps_thres_low = 0;
    ret=ap3426_set_phthres(client, data->ps_thres_high);
    if(ret<0)
	{
		 	printk(KERN_ERR"%s err ap3426_set_phthres\n ",__func__);
	}

     ret=ap3426_set_plthres(client, data->ps_thres_low);
     if(ret<0)
     {
			 printk(KERN_ERR"%s err ap3426_set_plthres\n ",__func__);
     }
    i2c_smbus_write_byte_data(client, AP3426_REG_PS_IFORM, 0);
    /* read all the registers once to fill the cache.
     * if one of the reads fails, we consider the init failed */
    for (i = 0; i < AP3426_NUM_CACHABLE_REGS; i++) {
	int v = i2c_smbus_read_byte_data(client, reg_array[i]);
	if (v < 0)
	    return -ENODEV;
	data->reg_cache[i] = v;
    }
    /* set defaults */
//    ap3426_set_range(client, AP3426_ALS_RANGE_1);
    ap3426_set_mode(client, AP3426_SYS_DEV_DOWN);
//add by yanfei for ap3426 reg init config 20150121 end

    return 0;
}

static int ap3426_check_id(struct ap3426_data *data)
{
		return 0;	
}

static void lsensor_work_handler(struct work_struct *w)
{

    struct ap3426_data *data =
	container_of(w, struct ap3426_data, work_light.work);
       int value,value_ir;
	ktime_t timestamp;
	timestamp = ktime_get_boottime();
	   
    schedule_delayed_work(&data->work_light, msecs_to_jiffies(data->light_poll_delay));		  
//add by yanfei for mutex lock 20150112 begin
    mutex_lock(&data->update_lock);
    value = ap3426_get_adc_value(data->client);

    value_ir =ap3426_get_ir_value(data->client);
    mutex_unlock(&data->update_lock);
//add by yanfei for lcd name 20150430 begin
//    printk(KERN_ERR"yanfei valur_ir=%d \n",value_ir);
//modify by yanfei for "CW" light value 20150601 begin
 	if(value_ir <15)
	{
	value = value * 24906/10000;

	}
	else
	value = value * 12241/10000;
//modify for distinguish "C" light and "D" "A" light 20150204 end
	input_report_abs(data->lsensor_input_dev, ABS_MISC, value);
	input_event(data->lsensor_input_dev, EV_SYN, SYN_TIME_SEC,
			ktime_to_timespec(timestamp).tv_sec);
	input_event(data->lsensor_input_dev, EV_SYN, SYN_TIME_NSEC,
			ktime_to_timespec(timestamp).tv_nsec);
       input_sync(data->lsensor_input_dev);

}

static void ap3426_work_handler(struct work_struct *w)
{

	struct ap3426_data *data = container_of(w, struct ap3426_data, work_proximity.work);
	u8 int_stat;
	int pxvalue;
	int distance;
	ktime_t timestamp;
	timestamp = ktime_get_boottime();
//    int value;
    msleep(10);
    i2c_smbus_write_byte_data(data->client, AP3426_REG_PS_PERSIS, 0x02);
    i2c_smbus_write_byte_data(data->client, AP3426_REG_PS_SMARTINT, 0x00);
    mutex_lock(&data->irq_lock);
    int_stat = ap3426_get_intstat(data->client);
//delete by yanfei for mutex 20150206 begin
//modify by yanfei for lock 20150605
    if(int_stat & AP3426_REG_SYS_INT_PMASK)

    {
	pxvalue = ap3426_get_px_value(data->client); //test
	if(pxvalue < 0)
	{
		printk(KERN_ERR "%s: i2c read ps data fail. \n", __func__);
		mutex_unlock(&data->irq_lock);
		return;
	}

	if(pxvalue==1023)
	{
		pxvalue = 800;
	}

	if(pxvalue > data->ps_thres_high)
	{
		ap3426_set_phthres(data->client,1023);
		ap3426_set_plthres(data->client,data->ps_thres_low);

		distance = 0;

		input_report_abs(data->psensor_input_dev, ABS_DISTANCE, distance);
		input_event(data->psensor_input_dev, EV_SYN, SYN_TIME_SEC,
				ktime_to_timespec(timestamp).tv_sec);
		input_event(data->psensor_input_dev, EV_SYN, SYN_TIME_NSEC,
				ktime_to_timespec(timestamp).tv_nsec);
		input_sync(data->psensor_input_dev);

		printk(KERN_ERR"distance=%d pxvalue=%d  data->ps_thres_low=%d \n",distance,pxvalue,data->ps_thres_low);
	}
	else if(pxvalue < data->ps_thres_low)
	{
		ap3426_set_phthres(data->client,data->ps_thres_high);
		ap3426_set_plthres(data->client,0);

		distance = 1;

		input_report_abs(data->psensor_input_dev, ABS_DISTANCE, distance);
		input_event(data->psensor_input_dev, EV_SYN, SYN_TIME_SEC,
				ktime_to_timespec(timestamp).tv_sec);
		input_event(data->psensor_input_dev, EV_SYN, SYN_TIME_NSEC,
				ktime_to_timespec(timestamp).tv_nsec);
		input_sync(data->psensor_input_dev);

		printk(KERN_ERR"distance=%d pxvalue=%d data->ps_thres_high =%d\n",distance,pxvalue,data->ps_thres_high);
	}
	else
	{
		distance = 1;
		input_report_abs(data->psensor_input_dev, ABS_DISTANCE, distance);
		input_event(data->psensor_input_dev, EV_SYN, SYN_TIME_SEC,
				ktime_to_timespec(timestamp).tv_sec);
		input_event(data->psensor_input_dev, EV_SYN, SYN_TIME_NSEC,
				ktime_to_timespec(timestamp).tv_nsec);
		input_sync(data->psensor_input_dev);

		printk(KERN_ERR"distance=%d pxvalue=%d\n",distance,pxvalue);
	}

	/*add by shihuijun for tp charge interface updatedate 20150127 start*/
	if (distance==0)
		 proximity_open_flag = true;
	else
	     proximity_open_flag = false;
	/*add by shihuijun for tp charge interface updatedate 20150127 start*/
    }

    enable_irq(data->client->irq);
    enable_irq_wake(data->client->irq);
    mutex_unlock(&data->irq_lock);
//delete by yanfei for mutex 20150206 end

}
 

static irqreturn_t ap3426_irq(int irq, void *data_)
{
    struct ap3426_data *data = data_;

    printk("!!!!!!!!!!!!!!!!!!!!!ap3426_irq !!!!!!!!!!!!!!!!  %d ,  %d\n",data->client->irq,gpio_to_irq(data->int_pin));
    disable_irq_nosync(data->client->irq);
    disable_irq_wake(data->client->irq);
    wake_lock_timeout(&data->prx_wake_lock, 5*HZ);
    schedule_delayed_work(&data->work_proximity, 0);
    return IRQ_HANDLED;
}
static int sensor_platform_hw_power_on(bool on)
{
	int err;

	if (pdev_data == NULL)
		return -ENODEV;

	if (!IS_ERR_OR_NULL(pdev_data->pinctrl)) {
		if (on)
			err = pinctrl_select_state(pdev_data->pinctrl,
				pdev_data->pin_default);
		else
			err = pinctrl_select_state(pdev_data->pinctrl,
				pdev_data->pin_sleep);
		if (err)
			dev_err(&pdev_data->client->dev,
				"Can't select pinctrl state\n");
	}

	ap3426_power_ctl(pdev_data, on);


	return 0;
}
#ifdef CONFIG_OF
static int ap3426_parse_dt(struct device *dev, struct ap3426_data *pdata)
{
    struct device_node *dt = dev->of_node;

    if (pdata == NULL) 
    {
	LDBG("%s: pdata is NULL\n", __func__);
	return -EINVAL;
    }

/*********************************************/
	pdata->power_on = sensor_platform_hw_power_on;

/*********************************************/
    pdata->int_pin = of_get_named_gpio_flags(dt, "ap3426,irq-gpio",
                                0, &pdata->irq_flags);

    if (pdata->int_pin < 0)
    {
	dev_err(dev, "Unable to read irq-gpio\n");
	return pdata->int_pin;
    }
    return 0;
}
#endif
/************************************************************/

static int ap3426_pinctrl_init(struct ap3426_data *data)
{
	struct i2c_client *client = data->client;

	data->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(data->pinctrl)) {
		dev_err(&client->dev, "Failed to get pinctrl\n");
		return PTR_ERR(data->pinctrl);
	}

	data->pin_default =
		pinctrl_lookup_state(data->pinctrl, "default");
	if (IS_ERR_OR_NULL(data->pin_default)) {
		dev_err(&client->dev, "Failed to look up default state\n");
		return PTR_ERR(data->pin_default);
	}
	data->pin_sleep =
		pinctrl_lookup_state(data->pinctrl, "sleep");
	if (IS_ERR_OR_NULL(data->pin_sleep)) {
		dev_err(&client->dev, "Failed to look up sleep state\n");
		return PTR_ERR(data->pin_sleep);
	}
	return 0;
}
/************************************************************/
static struct of_device_id ap3426_match_table[];
static int ap3426_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
    struct ap3426_data *data;
    int err = 0;

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
    {
	err = -EIO;
	goto exit_free_gpio;
    }

    data = kzalloc(sizeof(struct ap3426_data), GFP_KERNEL);
    if (!data)
    {
	err = -ENOMEM;
	goto exit_free_gpio;
    }
	pdev_data = data;

#ifdef CONFIG_OF
    if (client->dev.of_node) 
    {
	LDBG("Device Tree parsing.");

	err = ap3426_parse_dt(&client->dev, data);
	if (err) 
	{
	    dev_err(&client->dev, "%s: ap3426_parse_dt "
		    "for pdata failed. err = %d",
		    __func__, err);
	    goto exit_parse_dt_fail;
	}
    }

#else
    data->irq = client->irq;
#endif
    data->client = client;
    i2c_set_clientdata(client, data);
//add by yanfei for ap3426 wake lock 20150107 beign
    mutex_init(&data->lock);
    mutex_init(&data->irq_lock);
    mutex_init(&data->update_lock);
    wake_lock_init(&data->prx_wake_lock, WAKE_LOCK_SUSPEND, "prx_wake_lock");
//add by yanfei for ap3426 wake lock 20150107 end
    err = ap3426_power_init(data, true);
    if (err)
	goto err_power_on;
	
    err = ap3426_power_ctl(data, true);
    if (err)
    	{
    	err = ap3426_power_init(data, false);
	goto exit_kfree;
    	}

    /* initialize the AP3426 chip */
    err = ap3426_init_client(client);

    if (err)
	goto exit_kfree;
    if(ap3426_check_id(data) !=0 )
    {
	dev_err(&client->dev, "failed to check ap3426 id\n");
        goto err_power_on;
    }
    err = ap3426_pinctrl_init(data);
    if (err) {
	dev_err(&client->dev, "Can't initialize pinctrl\n");
	goto exit_kfree;
    }

    err = pinctrl_select_state(data->pinctrl, data->pin_default);
    if (err) {
		dev_err(&client->dev,
			"Can't select pinctrl default state\n");
		goto exit_kfree;
    }
    err = ap3426_register_lsensor_device(client,data);
    if (err)
    {
	dev_err(&client->dev, "failed to register_lsensor_device\n");
	goto exit_kfree;
    }

    err = ap3426_register_psensor_device(client, data);
    if (err) 
    {
	dev_err(&client->dev, "failed to register_psensor_device\n");
	goto exit_free_ls_device;
    }

    /* register sysfs hooks */
    if (err)
	goto exit_free_ps_device;

    err = gpio_request(data->int_pin,"ap3426-int");
    if(err < 0)
    {
	printk(KERN_ERR "%s: gpio_request, err=%d", __func__, err);
        return err;
    }
    err = gpio_direction_input(data->int_pin);
    if(err < 0)
    {
        printk(KERN_ERR "%s: gpio_direction_input, err=%d", __func__, err);
        return err;
    }
    data->client->irq = gpio_to_irq(data->int_pin);

   if (data->power_on)
		err = data->power_on(true);
#if 0
	input_report_abs(data->psensor_input_dev, ABS_DISTANCE, 1);
	input_event(data->psensor_input_dev, EV_SYN, SYN_TIME_SEC,
		ktime_to_timespec(timestamp).tv_sec);
	input_event(data->psensor_input_dev, EV_SYN, SYN_TIME_NSEC,
		ktime_to_timespec(timestamp).tv_nsec);
	input_sync(data->psensor_input_dev);
#endif
//modify by yanfei for request irq API 20150227
    err = request_irq(data->client->irq,ap3426_irq, IRQF_TRIGGER_FALLING,
			 "ap3426",(void *)data );
#if 0
    err = request_threaded_irq(gpio_to_irq(data->int_pin), NULL, ap3426_irq,
	    IRQF_TRIGGER_FALLING  | IRQF_ONESHOT,
	    "ap3426", data);
#endif
    if (err) 
    {
	dev_err(&client->dev, "ret: %d, could not get IRQ %d\n",err,gpio_to_irq(data->int_pin));
	goto exit_free_ps_device;
    }

//modify by yanfei for  ap3426 20150212 begin
//add by yanfei for irq 201501012 begin
    disable_irq(data->client->irq);
//    enable_irq_wake(data->client->irq);
//add by yanfei for irq 201501012 end

    data->light_poll_delay=ALS_SET_DEFAULT_DELAY_TIME;
    INIT_DELAYED_WORK(&data->work_proximity, ap3426_work_handler);
    INIT_DELAYED_WORK(&data->work_light, lsensor_work_handler);  
    err = sysfs_create_group(&data->client->dev.kobj, &ap3426_attr_group);
//modify by yanfei for  ap3426 20150212 end

   if (err)
	goto exit_kfree;

	data->als_cdev = sensors_light_cdev; 
 	data->als_cdev.sensors_enable = ap3426_als_enable_set; 
 	data->als_cdev.sensors_poll_delay = ap3426_als_poll_delay_set; 
	data->als_cdev.sensors_flush = ap3426_als_flush;
	err = sensors_classdev_register(&data->lsensor_input_dev->dev, &data->als_cdev);
 	if (err) 
 		goto exit_pwoer_ctl; 
 
//modify for ap3426 crosstalk 20150112
 	data->ps_cdev = sensors_proximity_cdev; 
 	data->ps_cdev.sensors_enable = ap3426_ps_enable_set;
	data->ps_cdev.sensors_flush = ap3426_ps_flush;
	data->ps_cdev.sensors_get_crosstalk = ap3426_get_ps_crosstalk;
	err = sensors_classdev_register(&data->psensor_input_dev->dev, &data->ps_cdev);
	if(err)
		goto err_power_on;

 //add by yanfei for sensor hardware information 20150107 begin
#ifdef CONFIG_GET_HARDWARE_INFO
	register_hardware_info(ALS_PS, ap3426_match_table[0].compatible);
#endif
//add by yanfei for sensor hardware information 20150107 end
    err = ap3426_power_ctl(data, true); //kevindnag for msm8916 20141010
    if (err)
	goto err_power_on;                     //end
	
    dev_info(&client->dev, "Driver version %s enabled\n", DRIVER_VERSION);
    return 0;

exit_free_ps_device:
    ap3426_unregister_psensor_device(client,data);

exit_free_ls_device:
    ap3426_unregister_lsensor_device(client,data);
exit_pwoer_ctl:	
	ap3426_power_ctl(data, false);
	sensors_classdev_unregister(&data->ps_cdev);
	
err_power_on:	
	ap3426_power_init(data, false);
	sensors_classdev_unregister(&data->als_cdev);
	
exit_kfree:
//added by yanfei  for ap3426  2015-01-07 
    wake_lock_destroy(&data->prx_wake_lock);
    mutex_destroy(&data->lock);
    if (data->power_on)
	data->power_on(false);
    pdev_data = NULL;
    kfree(data);
#ifdef CONFIG_OF
exit_parse_dt_fail:

LDBG("dts initialize failed.");
return err;
/*
    if (client->dev.of_node && data->client->dev.platform_data)
	kfree(data->client->dev.platform_data);
*/
#endif
exit_free_gpio:

    return err;
}

static int ap3426_remove(struct i2c_client *client)
{
    struct ap3426_data *data = i2c_get_clientdata(client);
    free_irq(gpio_to_irq(data->int_pin), data);

    ap3426_power_ctl(data, false);
    wake_lock_destroy(&data->prx_wake_lock);

    sysfs_remove_group(&data->client->dev.kobj, &ap3426_attr_group);
//kevindang20140924
//    sysfs_remove_group(&data->client->dev.kobj, &ap3426_ps_attribute_group);// every devices register his own devices
//    sysfs_remove_group(&data->client->dev.kobj, &ap3426_ls_attribute_group);// every devices register his own devices
//end    
    ap3426_unregister_psensor_device(client,data);
    ap3426_unregister_lsensor_device(client,data);
    ap3426_unregister_heartbeat_device(client,data);

    ap3426_power_init(data, false);


    ap3426_set_mode(client, 0);
    kfree(i2c_get_clientdata(client));


    if(&data->pl_timer)
	del_timer(&data->pl_timer);
//add by yanfei for destory resource 20150107 beign
    pdev_data = NULL;
    if (data->power_on)
		data->power_on(false);
//add by yanfei for destory resource 20150107 end
    return 0;
}

static const struct i2c_device_id ap3426_id[] = 
{
    { AP3426_DRV_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, ap3426_id);

#ifdef CONFIG_OF
static struct of_device_id ap3426_match_table[] = 
{
        {.compatible = "di_ap3426" },
        {},
};
#else
#define ap3426_match_table NULL
#endif

static int ap3426_suspend(struct device *dev)
{
//modify by yanfei for suspend and resume 20150212 begin
	struct ap3426_data *ps_data = dev_get_drvdata(dev);
	if (ps_data->rels_enable==1)
		{
			cancel_delayed_work_sync(&ps_data->work_light);
		}
//modify by yanfei for suspend and resume 20150212 end

        return 0;
}

static int ap3426_resume(struct device *dev)
{
//modify by yanfei for suspend and resume 20150212 begin

	struct ap3426_data *ps_data = dev_get_drvdata(dev);
	if(ps_data ->rels_enable == 1)
	{
		schedule_delayed_work(&ps_data->work_light, msecs_to_jiffies(ps_data->light_poll_delay));
	}
//modify by yanfei for suspend and resume 20150212 end

        return 0;
}
//modify by yanfei for ap3426 suspend and resume 20150212
//static SIMPLE_DEV_PM_OPS(ap3426_pm_ops, ap3426_suspend, ap3426_resume);
static const struct dev_pm_ops ap3426_pm_ops = {
	.suspend	= ap3426_suspend,
	.resume 	= ap3426_resume,
};
static struct i2c_driver ap3426_driver = {
    .driver = {
	.name	= AP3426_DRV_NAME,
	.owner	= THIS_MODULE,
	.of_match_table = ap3426_match_table,
	.pm     = &ap3426_pm_ops,
    },
    .probe	= ap3426_probe,
    .remove	= ap3426_remove,
    .id_table = ap3426_id,
};

static int __init ap3426_init(void)
{
    int ret;

    ret = i2c_add_driver(&ap3426_driver);
    return ret;	

}

static void __exit ap3426_exit(void)
{
    i2c_del_driver(&ap3426_driver);
}

module_init(ap3426_init);
module_exit(ap3426_exit);
MODULE_AUTHOR("Kevin.dang, <kevin.dang@dyna-image.com>");
MODULE_DESCRIPTION("AP3426 driver.");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
