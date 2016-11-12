
/* Copyright (c) 2012-2012, YEP Telecom. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef HARDWARE_INFO_H
#define HARDWARE_INFO_H

typedef enum {
	LCM = 0,	
	CTP,		
	MAIN_CAM,		
	SUB_CAM,
	FLASH,
	GSENSOR,/*ACCELEROMETER*/
	MSENSOR,/*MAGNETIC_FIELD*/
	ALS_PS,/*LIGHT and PROXIMITY*/
	GYRO,/*GYROSCOPE*/
    EMCP,
    ROM, /* ROM INFO */
    RAM, /* RAM INFO */
	HARDWARE_ID_MAX,
	/* add by shihuijun for "wifi bluetooth"hardware  info start20150125*/
	WIFI,/*WIFI INFO*/    
	BLUETOOTH,/*BLUETOOTH INFO*/
	/* add by shihuijun for "wifi bluetooth"hardware info  end 20150125 */
} HARDWARE_ID;
// modified by yangze for sensor hardware info (x825) 2013-10-12 begin
struct hardware_info_pdata{	
	char lcm[125];
	char ctp[125];
	char main_cam[125];
	char sub_cam[125];
	char flash[125];
	char gsensor[125];
	char msensor[125];
	char als_ps[125];
	char gyro[125];
    char emcp[125];
    char rom[125];
    char ram[125];
	/* add by shihuijun for "wifi bluetooth"hardware info  end 20150125 */
	char wifi[125];
	char bluetooth[125];
	/* add by shihuijun for "wifi bluetooth"hardware info  end 20150125 */
};
// modified by yangze for sensor hardware info (x825) 2013-10-12 end
int register_hardware_info(HARDWARE_ID id, char* name);

#endif /* HARDWARE_INFO_H */

