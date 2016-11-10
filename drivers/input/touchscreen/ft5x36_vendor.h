/*
 focaltelech tp firmware update infomations
 
 Date           Author       Module    vendor id    Old_ver    New_ver
 2015.01.19    pangle     toptouch     0xA0         null         0x0C
 2015.04.09    pangle     toptouch     0xA0         0x0C         0x0D
 2015.04.09    pangle     toptouch     0xA0         0x0D         0x0F
 2015.05.08    pangle     toptouch     0xA0         0x0F          0x10
 2015.05.08    pangle     toptouch     0xA0         0x10          0x11
 2015.06.26    pangle     toptouch     0xA0         0x11          0x14
 2016.04.05    shihuijun  toptouch     0xA0         0x14          0x15
 */
#ifndef __FOCALTECH_VENDOR_H__
#define __FOCALTECH_VENDOR_H__

#include "ft5x36_vendor_id.h"

//added by pangle for 702 fw upgrade at 20150303 begin
#if defined(CONFIG_Y560_U23_BASE) || defined(CONFIG_Y560_L01_BASE)|| defined(CONFIG_Y560_L02_BASE)
static unsigned char FT6X36_FIRMWARE0xA0_TOPTOUCH[] = {
#if defined(CONFIG_Y560_U23_BASE) || defined(CONFIG_Y560_L01_BASE)|| defined(CONFIG_Y560_L02_BASE)
	//#include "ft5x36_firmware/QW702_QL703_huawei_y560_FT6336_0xA0_Ver0x0B_20150309.h"
	//#include "ft5x36_firmware/QW702_QL703_huawei_y560_FT6336_0xA0_Ver0x0D_20150409.h"
	//#include "ft5x36_firmware/QW702_QL703_huawei_y560_FT6336_0xA0_Ver0x0F_20150413.h"
	//#include "ft5x36_firmware/QW702_QL703_huawei_y560_FT6336_0xA0_Ver0x10_20150508.h"
	//#include "ft5x36_firmware/QW702_QL703_huawei_y560_FT6336_0xA0_Ver0x11_20150512.h"
	//#include "ft5x36_firmware/QW702_QL703_huawei_y560_FT6336_0xA0_Ver0x12_20150519.h"
	//#include "ft5x36_firmware/QW702_QL703_huawei_y560_FT6336_0xA0_Ver0x14_20150608.h"
	#include "ft5x36_firmware/QW702_QL703_huawei_y560_FT6336_0xA0_Ver0x15_20160405.h"
#endif
}; 
#endif
//added by pangle for 702 fw upgrade at 20150303 begin



#endif
