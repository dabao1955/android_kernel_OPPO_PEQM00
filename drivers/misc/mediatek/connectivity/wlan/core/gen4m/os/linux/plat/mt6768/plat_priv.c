/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include "gl_os.h"

#if KERNEL_VERSION(5, 10, 0) <= CFG80211_VERSION_CODE
#include <uapi/linux/sched/types.h>
#include <linux/sched/task.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include "wmt_exp.h"
#else
#include <cpu_ctrl.h>
#include <topo_ctrl.h>
#include <helio-dvfsrc-opp.h>
#if KERNEL_VERSION(4, 19, 0) <= CFG80211_VERSION_CODE
#include <linux/soc/mediatek/mtk-pm-qos.h>
#define pm_qos_add_request(_req, _class, _value) \
		mtk_pm_qos_add_request(_req, _class, _value)
#define pm_qos_update_request(_req, _value) \
		mtk_pm_qos_update_request(_req, _value)
#define pm_qos_remove_request(_req) \
		mtk_pm_qos_remove_request(_req)
#define pm_qos_request mtk_pm_qos_request
#define PM_QOS_DDR_OPP MTK_PM_QOS_DDR_OPP
#define ppm_limit_data cpu_ctrl_data
#else
#include <linux/pm_qos.h>
#include <helio-dvfsrc-opp.h>
#endif
#endif

#include "precomp.h"

#ifdef CONFIG_WLAN_MTK_EMI
#if KERNEL_VERSION(5, 10, 0) <= CFG80211_VERSION_CODE
#include <soc/mediatek/emi.h>
#else
#include <mt_emi_api.h>
#endif
#define WIFI_EMI_MEM_OFFSET    0x140000
#define WIFI_EMI_MEM_SIZE      0x130000
#endif

#define MAX_CPU_FREQ (3 * 1024 * 1024) /* in kHZ */
#define MAX_CLUSTER_NUM  3

uint32_t kalGetCpuBoostThreshold(void)
{
	DBGLOG(SW4, TRACE, "enter kalGetCpuBoostThreshold\n");
	/*  3, stands for 100Mbps */
	return 3;
}

#if KERNEL_VERSION(5, 10, 0) <= CFG80211_VERSION_CODE
#define DOMAIN_AP	0
#define DOMAIN_CONN	2
#define CPU_ALL_CORE (0xff)
#define CPU_BIG_CORE (0xc0)
#define CPU_LITTLE_CORE (CPU_ALL_CORE - CPU_BIG_CORE)

enum ENUM_CPU_BOOST_STATUS {
	ENUM_CPU_BOOST_STATUS_INIT = 0,
	ENUM_CPU_BOOST_STATUS_START,
	ENUM_CPU_BOOST_STATUS_STOP,
	ENUM_CPU_BOOST_STATUS_NUM
};


int32_t kalCheckTputLoad(IN struct ADAPTER *prAdapter,
			 IN uint32_t u4CurrPerfLevel,
			 IN uint32_t u4TarPerfLevel,
			 IN int32_t i4Pending,
			 IN uint32_t u4Used)
{
	uint32_t pendingTh =
		CFG_TX_STOP_NETIF_PER_QUEUE_THRESHOLD *
		prAdapter->rWifiVar.u4PerfMonPendingTh / 100;
	uint32_t usedTh = (HIF_TX_MSDU_TOKEN_NUM / 2) *
		prAdapter->rWifiVar.u4PerfMonUsedTh / 100;

	if (cnmIsMccMode(prAdapter)
		&& prAdapter->rWifiVar.u4PerfMonTpTh[2]
			== PERF_MON_MCC_TP_THRESHOLD
		&& u4TarPerfLevel >= 3) {
		return TRUE;
	}

	return u4TarPerfLevel >= 3 &&
	       u4TarPerfLevel < prAdapter->rWifiVar.u4BoostCpuTh &&
	       i4Pending >= pendingTh &&
	       u4Used >= usedTh ?
	       TRUE : FALSE;
}

void kalSetTaskUtilMinPct(IN int pid, IN unsigned int min)
{
	int ret = 0;
	unsigned int blc_1024;
	struct task_struct *p;
	struct sched_attr attr = {};

	if (pid < 0)
		return;

	/* Fill in sched_attr */
	attr.sched_policy = -1;
	attr.sched_flags =
		SCHED_FLAG_KEEP_ALL |
		SCHED_FLAG_UTIL_CLAMP |
		SCHED_FLAG_RESET_ON_FORK;

	if (min == 0) {
		attr.sched_util_min = -1;
		attr.sched_util_max = -1;
	} else {
		blc_1024 = (min << 10) / 100U;
		blc_1024 = clamp(blc_1024, 1U, 1024U);
		attr.sched_util_min = (blc_1024 << 10) / 1280;
		attr.sched_util_max = (blc_1024 << 10) / 1280;
	}

	/* get task_struct */
	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (likely(p))
		get_task_struct(p);
	rcu_read_unlock();

	/* sched_setattr */
	if (likely(p)) {
		ret = sched_setattr(p, &attr);
		put_task_struct(p);
	}
}

static LIST_HEAD(wlan_policy_list);
struct wlan_policy {
	struct freq_qos_request	qos_req;
	struct list_head	list;
};

void kalSetCpuFreq(IN int32_t freq)
{
	int cpu, ret;
	struct cpufreq_policy *policy;
	struct wlan_policy *wReq;

	if (list_empty(&wlan_policy_list)) {
		for_each_possible_cpu(cpu) {
			policy = cpufreq_cpu_get(cpu);
			if (!policy)
				continue;

			wReq = kzalloc(sizeof(struct wlan_policy), GFP_KERNEL);
			if (!wReq)
				break;

			ret = freq_qos_add_request(&policy->constraints,
				&wReq->qos_req, FREQ_QOS_MIN, 0);
			if (ret < 0) {
				pr_info("%s: freq_qos_add_request fail cpu%d\n",
					__func__, cpu);
				kfree(wReq);
				break;
			}

			list_add_tail(&wReq->list, &wlan_policy_list);
			cpufreq_cpu_put(policy);
		}
	}

	list_for_each_entry(wReq, &wlan_policy_list, list) {
		freq_qos_update_request(&wReq->qos_req, freq);
	}
}

void kalSetDramBoost(IN struct ADAPTER *prAdapter, IN u_int8_t onoff)
{
	/* TODO */
}
#endif

int32_t kalBoostCpu(IN struct ADAPTER *prAdapter,
		    IN uint32_t u4TarPerfLevel,
		    IN uint32_t u4BoostCpuTh)
{
#if KERNEL_VERSION(5, 10, 0) <= CFG80211_VERSION_CODE
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Freq = -1;
	static u_int8_t fgRequested = ENUM_CPU_BOOST_STATUS_INIT;

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	i4Freq = (u4TarPerfLevel >= u4BoostCpuTh) ? MAX_CPU_FREQ : -1;

	if (fgRequested == ENUM_CPU_BOOST_STATUS_INIT) {
		/* initially enable rps working at small cores */
		kalSetRpsMap(prGlueInfo, CPU_LITTLE_CORE);
		fgRequested = ENUM_CPU_BOOST_STATUS_STOP;
	}

	if (u4TarPerfLevel >= u4BoostCpuTh) {
		if (fgRequested == ENUM_CPU_BOOST_STATUS_STOP) {
			pr_info("kalBoostCpu start (%d>=%d)\n",
				u4TarPerfLevel, u4BoostCpuTh);
			fgRequested = ENUM_CPU_BOOST_STATUS_START;

			kalSetTaskUtilMinPct(prGlueInfo->u4TxThreadPid, 100);
			kalSetTaskUtilMinPct(prGlueInfo->u4RxThreadPid, 100);
			kalSetTaskUtilMinPct(prGlueInfo->u4HifThreadPid, 100);
			kalSetRpsMap(prGlueInfo, CPU_BIG_CORE);
			kalSetCpuFreq(i4Freq);
			kalSetDramBoost(prAdapter, TRUE);
		}
	} else {
		if (fgRequested == ENUM_CPU_BOOST_STATUS_START) {
			pr_info("kalBoostCpu stop (%d<%d)\n",
				u4TarPerfLevel, u4BoostCpuTh);
			fgRequested = ENUM_CPU_BOOST_STATUS_STOP;

			kalSetTaskUtilMinPct(prGlueInfo->u4TxThreadPid, 0);
			kalSetTaskUtilMinPct(prGlueInfo->u4RxThreadPid, 0);
			kalSetTaskUtilMinPct(prGlueInfo->u4HifThreadPid, 0);
			kalSetRpsMap(prGlueInfo, CPU_LITTLE_CORE);
			kalSetCpuFreq(i4Freq);
			kalSetDramBoost(prAdapter, FALSE);
		}
	}
	kalTraceInt(fgRequested == ENUM_CPU_BOOST_STATUS_START, "kalBoostCpu");
#else
	struct ppm_limit_data freq_to_set[MAX_CLUSTER_NUM];
	int32_t i = 0, i4Freq = -1;
#ifdef WLAN_FORCE_DDR_OPP
	static struct pm_qos_request wifi_qos_request;
	static u_int8_t fgRequested;
#endif
	uint32_t u4ClusterNum = topo_ctrl_get_nr_clusters();

	ASSERT(u4ClusterNum <= MAX_CLUSTER_NUM);
	/* ACAO, we dont have to set core number */
	i4Freq = (u4TarPerfLevel >= u4BoostCpuTh) ? MAX_CPU_FREQ : -1;
	for (i = 0; i < u4ClusterNum; i++) {
		freq_to_set[i].min = i4Freq;
		freq_to_set[i].max = i4Freq;
	}

	update_userlimit_cpu_freq(CPU_KIR_WIFI, u4ClusterNum, freq_to_set);

#ifdef WLAN_FORCE_DDR_OPP
	if (u4TarPerfLevel >= u4BoostCpuTh) {
		if (!fgRequested) {
			fgRequested = 1;
			pm_qos_add_request(&wifi_qos_request,
					   PM_QOS_DDR_OPP,
					   DDR_OPP_0);
		}
		pm_qos_update_request(&wifi_qos_request, DDR_OPP_0);
	} else if (fgRequested) {
		pm_qos_update_request(&wifi_qos_request, DDR_OPP_UNREQ);
		pm_qos_remove_request(&wifi_qos_request);
		fgRequested = 0;
	}
#endif
#endif
	return 0;
}

#ifdef CONFIG_WLAN_MTK_EMI
void kalSetEmiMpuProtection(phys_addr_t emiPhyBase, bool enable)
{
}

void kalSetDrvEmiMpuProtection(phys_addr_t emiPhyBase, uint32_t offset,
			       uint32_t size)
{

#if KERNEL_VERSION(5, 10, 0) <= CFG80211_VERSION_CODE
	struct emimpu_region_t region;
	unsigned long long start = emiPhyBase + offset;
	unsigned long long end = emiPhyBase + offset + size - 1;
	int ret;

	DBGLOG(INIT, INFO, "emiPhyBase: 0x%p, offset: %d, size: %d\n",
				emiPhyBase, offset, size);

	ret = mtk_emimpu_init_region(&region, 18);
	if (ret) {
		DBGLOG(INIT, ERROR, "mtk_emimpu_init_region failed, ret: %d\n",
				ret);
		return;
	}
	mtk_emimpu_set_addr(&region, start, end);
	mtk_emimpu_set_apc(&region, DOMAIN_AP, MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&region, DOMAIN_CONN, MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_lock_region(&region, MTK_EMIMPU_LOCK);
	ret = mtk_emimpu_set_protection(&region);
	if (ret)
		DBGLOG(INIT, ERROR,
			"mtk_emimpu_set_protection failed, ret: %d\n",
			ret);
	mtk_emimpu_free_region(&region);
#else
	struct emi_region_info_t region_info;

	DBGLOG(INIT, INFO, "emiPhyBase: 0x%x, offset: %u, size: %u\n",
			emiPhyBase, offset, size);

	/*set MPU for EMI share Memory */
	region_info.start = emiPhyBase + offset;
	region_info.end = emiPhyBase + offset + size - 1;
	region_info.region = 29;
	SET_ACCESS_PERMISSION(region_info.apc, LOCK, FORBIDDEN, FORBIDDEN,
			      FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			      FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
			      FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION,
			      FORBIDDEN, NO_PROTECTION);
	emi_mpu_set_protection(&region_info);
#endif
}
#endif

int32_t kalGetFwFlavorByPlat(uint8_t *flavor)
{
	*flavor = 'a';
	return 1;
}
