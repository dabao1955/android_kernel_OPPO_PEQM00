/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 Oplus. All rights reserved.
 */

#ifndef _OPLUS_KLOCKOPT_H_
#define _OPLUS_KLOCKOPT_H_

struct klock_monitor_para {
	u64 mutex_time_ms;
	u64 mutex_warning;
	u64 rwsem_read_time_ms;
	u64 rwsem_write_time_ms;
	u64 rwsem_read_times;
	u64 rwsem_write_times;
	u64 rwsem_write_opt_ux_times;
	u64 rwsem_write_opt_unux_times;
	u64 rwsem_waiting_ux_sad_times;
	u64 rwsem_read_warning;
	u64 rwsem_write_warning;
};
struct mutex;
struct mutex_waiter;
extern struct klock_monitor_para ux_lock_monitor;
extern int sysctl_monitor_enable;
extern void do_lock_task_set_ux(struct task_struct *p,
				struct task_struct *donor_task);
extern void add_owner_ilocked(struct task_struct *task,
			      struct rw_semaphore *sem);
extern void add_owner(struct task_struct *task, struct rw_semaphore *sem);
extern void delete_owner_ilocked(struct rw_semaphore *sem);
extern void delete_owner(struct rw_semaphore *sem);
extern void mutex_owner_unboost(bool next_is_ux);
extern void mutex_owner_boost(struct mutex *lock,
				    struct mutex_waiter *waiter,
				    struct task_struct *task);
extern void adjust_task_block_on(struct task_struct *task, struct mutex *mutex);
extern void boost_owner(struct rw_semaphore *sem, int type);
extern bool check_rwsem_waiting_list(struct rw_semaphore *sem);
extern void deboost_owner(struct rw_semaphore *sem, int type);
extern bool test_task_lock_ux(struct task_struct *task);
extern bool lock_ux_target(struct task_struct *task);
extern void lock_ux_reset(struct task_struct *tsk);
extern void unset_mark_systrace(void);
extern void unset_mark_systrace_end(void);
extern bool rwsem_wait_list_has_ux(struct rw_semaphore *sem);
extern void record_rwsem_optimistic_spin(struct rw_semaphore *sem);
extern void exit_rwsem(void);
#endif /* _OPLUS_KLOCKOPT_H_ */
