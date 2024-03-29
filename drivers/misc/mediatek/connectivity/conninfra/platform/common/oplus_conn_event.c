/******************************************************************************
 ** Copyright 2019-2029 OPLUS Mobile Comm Corp., Ltd.
 ** OPLUS_EDIT, All rights reserved.
 **
 ** File: - plus_conn_event.c
 ** Description: oplus lpm uevent.
 ** Version: 1.0
 ** Date : 2022-06-30
 ** Author: CONNECTIVITY.WIFI.HARDWARE.POWER
 ** TAG: OPLUS_FEATURE_WIFI_HARDWARE_POWER
 ** -----------------------------Revision History: ----------------------------
 ** CONNECTIVITY.WIFI.HARDWARE.POWER 2021-06-30 1.0 OPLUS_FEATURE_WIFI_HARDWARE_POWER
********************************************************************************/

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>


#define RET_ERR  1
#define RET_OK  0

static struct miscdevice mConnObj;
static char mUevent[256] = {'\0'};
static struct workqueue_struct *mQueue = NULL;
static struct delayed_work mWork;


static void oplusWorkHandler(struct work_struct *data)
{
	char *envp[2];

	if (mUevent[0] == '\0')
		return;

	envp[0] = mUevent;
	envp[1] = NULL;
	kobject_uevent_env(
		&mConnObj.this_device->kobj,
		KOBJ_CHANGE, envp);
}

u_int8_t oplusConnUeventInit(void)
{
	u_int8_t ret = RET_OK;

	if (mConnObj.this_device != NULL) {
		return RET_OK;
	}
	mConnObj.name = "consys";
	mConnObj.minor = MISC_DYNAMIC_MINOR;
	ret = misc_register(&mConnObj);
	if (ret == RET_OK) {
		ret = kobject_uevent(&mConnObj.this_device->kobj, KOBJ_ADD);
		if (ret != RET_OK) {
			misc_deregister(&mConnObj);
			return RET_ERR;
		}
	}
	if (mQueue == NULL) {
		mQueue = create_singlethread_workqueue("oplus_conn_uevent");
		INIT_DELAYED_WORK(&mWork, oplusWorkHandler);
	}
	return ret;
}

u_int8_t oplusConnSendUevent(char *value)
{
	if (mConnObj.this_device == NULL) {
		return RET_ERR;
	}

	if (value == NULL) {
		return RET_ERR;
	}

	strlcpy(mUevent, value, sizeof(mUevent));
	if (mQueue) {
		queue_delayed_work(mQueue, &mWork, msecs_to_jiffies(200));
	} else {
		schedule_delayed_work(&mWork, msecs_to_jiffies(200));
	}

	return RET_OK;
}

void oplusConnUeventDeinit(void)
{
	misc_deregister(&mConnObj);
	cancel_delayed_work(&mWork);
	if (mQueue) {
		flush_workqueue(mQueue);
		destroy_workqueue(mQueue);
	}
}
