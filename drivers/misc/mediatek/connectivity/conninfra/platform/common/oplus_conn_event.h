/******************************************************************************
 ** Copyright 2019-2029 OPLUS Mobile Comm Corp., Ltd.
 ** OPLUS_EDIT, All rights reserved.
 **
 ** File: - oplus_lpm_event.h
 ** Description: mtk lpm uevent.
 ** Version: 1.0
 ** Date : 2022-06-30
 ** Author: CONNECTIVITY.WIFI.HARDWARE.POWER
 ** TAG: OPLUS_FEATURE_WIFI_HARDWARE_POWER
 ** -----------------------------Revision History: ----------------------------
 ** CONNECTIVITY.WIFI.HARDWARE.POWER 2021-06-30 1.0 OPLUS_FEATURE_WIFI_HARDWARE_POWER
********************************************************************************/

#ifndef __OPLUS_CONN_EVENT_H__
#define __OPLUS_CONN_EVENT_H__

u_int8_t oplusConnUeventInit(void);
u_int8_t oplusConnSendUevent(char *value);
void oplusConnUeventDeinit(void);

#endif /* __OPLUS_CONN_EVENT_H__ */
