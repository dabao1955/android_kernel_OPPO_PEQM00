// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Oplus. All rights reserved.
 */
#define pr_fmt(fmt) "oplus_kernel_lockopt:" fmt

#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/ww_mutex.h>
#include <linux/sched/signal.h>
#include <linux/sched/rt.h>
#include <linux/sched/wake_q.h>
#include <linux/sched/debug.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/debug_locks.h>
#include <linux/osq_lock.h>
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#include <linux/seq_file.h>
#include <../kernel/sched/sched.h>
#ifdef OPLUS_FEATURE_SCHED_ASSIST
#include <linux/sched_assist/sched_assist_common.h>
#endif
#define UX_LOCK_WAITING_WARNING 10

int sysctl_debug_kernel_lock = 0;
int sysctl_monitor_enable = 1;
int sysctl_kernel_lock_opt = 1;
int ux_max_lock_depth = 8;
struct klock_monitor_para ux_lock_monitor;

module_param_named(sysctl_debug_kernel_lock, sysctl_debug_kernel_lock, uint,
		   0660);
module_param_named(sysctl_monitor_enable, sysctl_monitor_enable, uint, 0660);

enum rwsem_waiter_type {
	RWSEM_WAITING_FOR_WRITE,
	RWSEM_WAITING_FOR_READ
};

struct rwsem_waiter {
	struct list_head list;
	struct task_struct *task;
	enum rwsem_waiter_type type;
};

/******************
 * systrace debug
 *******************/

unsigned long __read_mostly tracing_mark_write_addr;
inline void __ux_update_tracing_mark_write_addr(void)
{
	if (!sysctl_debug_kernel_lock)
		return;
	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr =
		    kallsyms_lookup_name("tracing_mark_write");
}

void unset_mark_systrace(void)
{
	if (!sysctl_debug_kernel_lock)
		return;
	__ux_update_tracing_mark_write_addr();
	event_trace_printk(tracing_mark_write_addr,
			   "C|6666666|mark_unset_%d|%d\n", current->pid, 1);
}

void unset_mark_systrace_end(void)
{
	if (!sysctl_debug_kernel_lock)
		return;
	__ux_update_tracing_mark_write_addr();
	event_trace_printk(tracing_mark_write_addr,
			   "C|6666666|mark_unset_%d|%d\n", current->pid, 0);
}

void ux_lock_stack_start(int tgid, char *comm, struct task_struct *tsk, int i,
			 int type)
{
	if (!sysctl_debug_kernel_lock)
		return;
	__ux_update_tracing_mark_write_addr();

	event_trace_printk(tracing_mark_write_addr,
			   "B|%d|%s_%d->ux_%dspread%d_%s_%d\n", tgid, comm,
			   current->pid, type, i, tsk->comm, tsk->pid);
}

void ux_lock_stack_end(struct task_struct *tsk)
{
	if (!sysctl_debug_kernel_lock)
		return;
	__ux_update_tracing_mark_write_addr();
	event_trace_printk(tracing_mark_write_addr, "E\n");
}

/******************
 * common function
 *******************/

bool test_task_lock_ux(struct task_struct *task)
{
	if (task->ux_state & SA_TYPE_LISTPICK_LOCK)
		return true;
	else
		return false;
}

bool rwsem_wait_list_has_ux(struct rw_semaphore *sem)
{
	struct rwsem_waiter *waiter;

	raw_spin_lock_irq(&sem->wait_lock);
	waiter =
	    list_first_entry_or_null(&sem->wait_list, struct rwsem_waiter,
				     list);

	if (waiter) {
		if (waiter->task
		    && (test_task_ux(waiter->task) || is_sf(waiter->task))) {
			raw_spin_unlock_irq(&sem->wait_lock);
			return true;
		}
	}
	raw_spin_unlock_irq(&sem->wait_lock);
	return false;
}

void record_rwsem_optimistic_spin(struct rw_semaphore *sem)
{
	if (sysctl_monitor_enable) {
		if (test_task_ux(current) || is_sf(current))
			ux_lock_monitor.rwsem_write_opt_ux_times++;
		else {
			ux_lock_monitor.rwsem_write_opt_unux_times++;
			if (rwsem_wait_list_has_ux(sem))
				ux_lock_monitor.rwsem_waiting_ux_sad_times++;
		}
	}
}

void start_record_mutex(void)
{
	if (!sysctl_monitor_enable)
		return;
	if (test_task_ux(current) || is_sf(current))
		current->lock_waiting_start = sched_clock();
}

void stop_record_mutex(void)
{
	u64 delta;

	if (!sysctl_monitor_enable)
		return;

	if (current->lock_waiting_start)
		delta = sched_clock() - current->lock_waiting_start;
	delta >>= 20;
	ux_lock_monitor.mutex_time_ms += delta;
	if (delta > UX_LOCK_WAITING_WARNING)
		ux_lock_monitor.mutex_warning++;
	current->lock_waiting_start = 0;
}

void start_record_rwsem(int type)
{
	if (!sysctl_monitor_enable)
		return;

	if (test_task_ux(current) || is_sf(current)) {
		current->lock_waiting_start = sched_clock();
		if (type == RWSEM_WAITING_FOR_WRITE)
			ux_lock_monitor.rwsem_write_times++;
		if (type == RWSEM_WAITING_FOR_READ)
			ux_lock_monitor.rwsem_read_times++;
	}
}

void stop_record_rwsem(int type)
{
	u64 delta;

	if (!sysctl_monitor_enable)
		return;

	switch (type) {
	case RWSEM_WAITING_FOR_WRITE:
		if (current->lock_waiting_start) {
			delta = sched_clock() - current->lock_waiting_start;
			delta >>= 20;
			ux_lock_monitor.rwsem_write_time_ms += delta;
			if (delta > UX_LOCK_WAITING_WARNING)
				ux_lock_monitor.rwsem_write_warning++;
			current->lock_waiting_start = 0;
		}
		break;
	case RWSEM_WAITING_FOR_READ:
		if (current->lock_waiting_start) {
			delta = sched_clock() - current->lock_waiting_start;
			delta >>= 20;
			ux_lock_monitor.rwsem_read_time_ms += delta;
			if (delta > UX_LOCK_WAITING_WARNING)
				ux_lock_monitor.rwsem_read_warning++;
			current->lock_waiting_start = 0;
		}
		break;
	default:
		break;
	}
}

bool lock_ux_target(struct task_struct *task)
{
	if (!task)
		return false;
	if (test_task_ux(task) || is_sf(task))
		return true;
	return false;
	/* || (task->sched_class == &rt_sched_class)  only for test */
}

void adjust_task_block_on(struct task_struct *task, struct mutex *mutex)
{
	task->block_on_mutex = mutex;
	if (mutex)
		start_record_mutex();
	else
		stop_record_mutex();
}

void do_lock_task_set_ux(struct task_struct *p, struct task_struct *donor_task)
{
	int ux_state, queued, running, sleeping, queue_flag =
	    DEQUEUE_SAVE | DEQUEUE_NOCLOCK;
	struct rq_flags rf;
	struct rq *rq;

	if (!p)
		return;

	rq = task_rq_lock(p, &rf);
	update_rq_clock(rq);
	if (unlikely(p == rq->idle) && donor_task) {
		goto out_unlock;
	}

	queued = task_on_rq_queued(p);
	running = task_current(rq, p);

	if (queued)
		deactivate_task(rq, p, queue_flag);
	if (donor_task) {
		if (sysctl_debug_kernel_lock)
			event_trace_printk(tracing_mark_write_addr,
					   "C|999999|set_target_loop_%d|%d\n",
					   p->pid, 1);
		if (!test_task_ux(p)) {
			p->ux_state |= SA_TYPE_LISTPICK_LOCK;
		}
	} else {
		p->ux_state &= ~SA_TYPE_LISTPICK_LOCK;
		if (sysctl_debug_kernel_lock)
			event_trace_printk(tracing_mark_write_addr,
					   "C|999999|set_target_loop_%d|%d\n",
					   p->pid, 0);
	}
	if (queued) {
		activate_task(rq, p, ENQUEUE_RESTORE | ENQUEUE_NOCLOCK);
		resched_curr(rq);
	}
out_unlock:
	task_rq_unlock(rq, p, &rf);
}

void add_owner_ilocked(struct task_struct *task, struct rw_semaphore *sem)
{
	if (task && list_empty(&task->own_rwsem)) {
		list_add_tail(&task->own_rwsem, &sem->owner_list);
		current->hold_rwsem = sem;
	}
}

void add_owner(struct task_struct *task, struct rw_semaphore *sem)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&sem->wait_lock, flags);
	add_owner_ilocked(task, sem);
	raw_spin_unlock_irqrestore(&sem->wait_lock, flags);
}

void delete_owner_ilocked(struct rw_semaphore *sem)
{
	struct list_head *pos, *n;

	if (!list_empty(&current->own_rwsem)) {
		list_for_each_safe(pos, n, &sem->owner_list) {
			if (pos == &current->own_rwsem) {
				list_del_init(&current->own_rwsem);
				current->hold_rwsem = NULL;
			}
		}
	}
}

void delete_owner(struct rw_semaphore *sem)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&sem->wait_lock, flags);
	delete_owner_ilocked(sem);
	raw_spin_unlock_irqrestore(&sem->wait_lock, flags);
}

void lock_ux_reset(struct task_struct *tsk)
{
	if (tsk->ux_state & SA_TYPE_LISTPICK_LOCK) {
		do_lock_task_set_ux(tsk, NULL);
	}
}

void mutex_owner_unboost(bool next_is_ux)
{
	if (current->mt_count > 0 && next_is_ux) {
		current->mt_count--;
		if (current->mt_count == 0)
			lock_ux_reset(current);
	}

}

static inline int list_is_first(const struct list_head *list,
				const struct list_head *head)
{
	return list->prev == head;
}

static inline bool check_ux_set_well(struct list_head *entry,
				     struct list_head *head,
				     struct task_struct *task)
{
	struct list_head *pos = NULL;
	struct list_head *n = NULL;
	struct mutex_waiter *waiter = NULL;
	bool ret = true;

	list_for_each_safe(pos, n, head) {
		waiter = list_entry(pos, struct mutex_waiter, list);
		if (waiter->task == task)
			break;
		if (!lock_ux_target(waiter->task))
			ret = false;
	}
	return ret;
}

static inline struct mutex *task_blocked_on_mutex_lock(struct task_struct *p)
{
	return p->block_on_mutex ? p->block_on_mutex : NULL;
}

bool current_boost_fit(struct task_struct *owner)
{
	return (owner->sched_class == &fair_sched_class) && !test_task_ux(owner)
	    && (lock_ux_target(current));
}

void ux_mutex_ajust_waiter_list(struct task_struct *task, struct mutex *lock)
{
	struct list_head *pos, *n;
	struct mutex_waiter *mw;
	bool found = false;

	spin_lock(&lock->wait_lock);
	list_for_each_safe(pos, n, &lock->wait_list) {
		mw = container_of(pos, struct mutex_waiter, list);
		if (mw->task == task) {
			list_del_init(&mw->list);
			found = true;
			goto out;
		}
	}
out:
	if (found) {
		mutex_list_add(task, &mw->list, &lock->wait_list, lock);
	}
	spin_unlock(&lock->wait_lock);
}

void mutex_owner_boost(struct mutex *lock, struct mutex_waiter *waiter,
			     struct task_struct *task)
{
	struct task_struct *owner = NULL;
	struct mutex_waiter *top_waiter = waiter;
	struct mutex *next_lock;
	int depth = 0;
	struct task_struct *backup_owner = NULL;

	adjust_task_block_on(current, lock);

	if (!sysctl_kernel_lock_opt)
		return;

	rcu_read_lock();
	owner = __mutex_owner(lock);
	if (!owner) {
		rcu_read_unlock();
		return;
	}
	if (test_task_lock_ux(owner) && lock_ux_target(current)
	    && list_is_first(&waiter->list, &lock->wait_list)) {
		owner->mt_count++;
	}

	if (!current_boost_fit(owner)) {
		rcu_read_unlock();
		return;
	}

	next_lock = task_blocked_on_mutex_lock(owner);

	rcu_read_unlock();
	if (!next_lock) {
		owner->mt_count++;
		ux_lock_stack_start(current->tgid, current->comm, owner, 0, 0);
		do_lock_task_set_ux(owner, current);
		ux_lock_stack_end(owner);
	} else {
		spin_unlock(&lock->wait_lock);
		do {
			backup_owner = owner;
			backup_owner->mt_count++;
			ux_lock_stack_start(current->tgid, current->comm,
					    backup_owner, 1, 0);
			backup_owner->ux_state |= SA_TYPE_LISTPICK_LOCK;
			ux_lock_stack_end(backup_owner);
			depth++;
			next_lock = task_blocked_on_mutex_lock(owner);
			if (!next_lock) {
				break;
			}

			if (next_lock) {
				ux_mutex_ajust_waiter_list(backup_owner,
							   next_lock);
				rcu_read_lock();
				owner = __mutex_owner(next_lock);
				if (owner) {
					if (!test_task_ux(owner)) {
					} else {
						owner = NULL;
					}
				}
				rcu_read_unlock();
			}

		} while (owner && next_lock && depth < ux_max_lock_depth);

		if (depth < ux_max_lock_depth) {
			do_lock_task_set_ux(backup_owner, task);
		}
		spin_lock(&lock->wait_lock);
	}

}

void boost_owner(struct rw_semaphore *sem, int type)
{
	struct list_head *pos, *n;
	struct task_struct *owner;
	struct task_struct *own;
	struct task_struct *sem_owner;
	struct rwsem_waiter *waiter;
	pid_t t;
	int countx = 0;
	int flag = false;

	start_record_rwsem(type);
	if (!sysctl_kernel_lock_opt)
		return;

	if (sysctl_debug_kernel_lock) {
		rcu_read_lock();
		sem_owner = READ_ONCE(sem->owner);
		if (sem_owner) {
			event_trace_printk(tracing_mark_write_addr,
					   "B|%d|ux_spread_%d_%d_has!!!\n",
					   current->tgid,
					   lock_ux_target(current),
					   !list_empty(&sem->owner_list));
		} else {
			event_trace_printk(tracing_mark_write_addr,
					   "B|%d|ux_spread_%d_%d_null!!!\n",
					   current->tgid,
					   lock_ux_target(current),
					   !list_empty(&sem->owner_list));
		}
		rcu_read_unlock();
		ux_lock_stack_end(current);
	}
	if (!lock_ux_target(current))
		return;
	/*
	   list_for_each_safe(pos, n, &sem->owner_list) {
	   own = container_of(pos, struct task_struct, own_rwsem);
	   if (own == sem_owner)
	   flag = true;
	   countx++;
	   }

	   if (type == RWSEM_WAITING_FOR_READ && (flag == false))
	   return;
	 */
	list_for_each_safe(pos, n, &sem->owner_list) {
		owner = container_of(pos, struct task_struct, own_rwsem);
		ux_lock_stack_start(current->tgid, current->comm, owner, 1, 1);
		ux_lock_stack_end(owner);
		if (test_task_ux(owner))
			continue;
		do_lock_task_set_ux(owner, current);
		countx++;
		if (countx > 4)
			break;
		/*
		   if (type == RWSEM_WAITING_FOR_READ) {
		   return;
		   }
		 */
	}

}

bool check_rwsem_waiting_list(struct rw_semaphore *sem)
{
	struct rwsem_waiter *waiter;

	raw_spin_lock_irq(&sem->wait_lock);
	waiter =
	    list_first_entry_or_null(&sem->wait_list, struct rwsem_waiter,
				     list);
	if (waiter) {
		if (lock_ux_target(waiter->task)) {
			raw_spin_unlock_irq(&sem->wait_lock);
			return true;
		}
	}
	raw_spin_unlock_irq(&sem->wait_lock);
	return false;
}

/*
	deboost earlier when ux get sem lock
*/
void deboost_owner(struct rw_semaphore *sem, int type)
{
	struct task_struct *tsk;
	struct list_head *pos, *n;
	unsigned long flags;
	int count = 0;

	/* if sem's waiting list head is ux, we don't need deboost
	   until owner release lock or list head  don't have ux
	 */
	stop_record_rwsem(type);

	if (!lock_ux_target(current))
		return;
	if (check_rwsem_waiting_list(sem)) {
		event_trace_printk(tracing_mark_write_addr,
				   "C|1000000|has_ux_%d|%d\n", current->pid, 1);
		event_trace_printk(tracing_mark_write_addr,
				   "C|1000000|has_ux_%d|%d\n", current->pid, 0);
		return;
	}

	raw_spin_lock_irqsave(&sem->wait_lock, flags);

	/*
	   if ux get lock ,we should de boost all owner before us
	 */

	list_for_each_safe(pos, n, &sem->owner_list) {
		tsk = container_of(pos, struct task_struct, own_rwsem);
		event_trace_printk(tracing_mark_write_addr,
				   "C|7777777|mark_unset_%d|%d\n", current->pid,
				   1);
		lock_ux_reset(tsk);
		event_trace_printk(tracing_mark_write_addr,
				   "C|7777777|mark_unset_%d|%d\n", current->pid,
				   0);
		count++;
	}
	raw_spin_unlock_irqrestore(&sem->wait_lock, flags);
}


void exit_rwsem(void)
{
	struct list_head *pos, *n;

	if (current->hold_rwsem) {
		raw_spin_lock_irq(&current->hold_rwsem->wait_lock);
		if (!list_empty(&current->own_rwsem)) {
			list_for_each_safe(pos, n, &current->hold_rwsem->owner_list) {
				if (pos == &current->own_rwsem) {
					list_del_init(&current->own_rwsem);
				}
			}
		}
		raw_spin_unlock_irq(&current->hold_rwsem->wait_lock);
		current->hold_rwsem = NULL;
	}
}

static ssize_t proc_klock_monitor_read(struct file *file, char __user *buff,
				       size_t count, loff_t *off)
{
	int len = 0;
	char page[1024] = { 0 };

	if (!page)
		return -ENOMEM;

	len =
	    sprintf(page,
		    "mutex_time_ms:%llu\nmutex_warning:%llu\nrwsem_read_time_ms:%llu\n"
		    "rwsem_write_time_ms:%llu\n"
		    "rwsem_read_warning:%llu\nrwsem_write_warning:%llu\n"
		    "rwsem_read_times:%llu\nrwsem_write_times:%llu\nrwsem_write_opt_ux_times:%llu\nrwsem_write_opt_unux_times:%llu\n"
		    "rwsem_waiting_ux_sad_times:%llu\n",
		    ux_lock_monitor.mutex_time_ms,
		    ux_lock_monitor.mutex_warning,
		    ux_lock_monitor.rwsem_read_time_ms,
		    ux_lock_monitor.rwsem_write_time_ms,
		    ux_lock_monitor.rwsem_read_warning,
		    ux_lock_monitor.rwsem_write_warning,
		    ux_lock_monitor.rwsem_read_times,
		    ux_lock_monitor.rwsem_write_times,
		    ux_lock_monitor.rwsem_write_opt_ux_times,
		    ux_lock_monitor.rwsem_write_opt_unux_times,
		    ux_lock_monitor.rwsem_waiting_ux_sad_times);

	if (len > *off)
		len -= *off;
	else
		len = 0;
	if (copy_to_user(buff, page, (len < count ? len : count)))
		return -EFAULT;
	*off += len < count ? len : count;

	return (len < count ? len : count);

}

static ssize_t proc_klock_monitor_write(struct file *file,
					const char __user *buff, size_t len,
					loff_t *ppos)
{
	char buffer[128];
	int err, val;

	if (len > sizeof(buffer) - 1)
		len = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buff, len))
		return -EFAULT;

	buffer[len] = '\0';
	err = kstrtoint(strstrip(buffer), 10, &val);
	if (err)
		return err;

	sysctl_monitor_enable = val;
	if (sysctl_monitor_enable == 0) {
		ux_lock_monitor.mutex_time_ms = 0;
		ux_lock_monitor.mutex_warning = 0;
		ux_lock_monitor.rwsem_read_time_ms = 0;
		ux_lock_monitor.rwsem_write_time_ms = 0;
		ux_lock_monitor.rwsem_read_warning = 0;
		ux_lock_monitor.rwsem_write_warning = 0;
		ux_lock_monitor.rwsem_read_times = 0;
		ux_lock_monitor.rwsem_write_times = 0;
		ux_lock_monitor.rwsem_write_opt_ux_times = 0;
		ux_lock_monitor.rwsem_write_opt_unux_times = 0;
		ux_lock_monitor.rwsem_waiting_ux_sad_times = 0;
	}
	return len;

}

static const struct file_operations proc_klock_monitor_fops = {
	.read = proc_klock_monitor_read,
	.write = proc_klock_monitor_write,
};

static ssize_t proc_klock_opt_write(struct file *file,
				    const char __user *buff, size_t len,
				    loff_t *ppos)
{
	char buffer[128];
	int err, val;

	if (len > sizeof(buffer) - 1)
		len = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buff, len))
		return -EFAULT;

	buffer[len] = '\0';
	err = kstrtoint(strstrip(buffer), 10, &val);
	if (err)
		return err;

	sysctl_kernel_lock_opt = val;
	return len;
}

static ssize_t proc_klock_opt_read(struct file *file, char __user *buff,
				   size_t count, loff_t *ppos)
{
	char buffer[128];
	size_t len = 0;

	len =
	    snprintf(buffer, sizeof(buffer), "klockopt enable=%d\n",
		     sysctl_kernel_lock_opt);
	return simple_read_from_buffer(buff, count, ppos, buffer, len);
}

static const struct file_operations proc_klock_opt_fops = {
	.write = proc_klock_opt_write,
	.read = proc_klock_opt_read,
};

#define OPLUS_BINDER_PROC_DIR "oplus_lockopt"
struct proc_dir_entry *oplus_kernel_lockopt_dir;

static int oplus_kernel_lockopt_proc_init(void)
{
	struct proc_dir_entry *proc_node;

	int ret = 0;
	oplus_kernel_lockopt_dir = proc_mkdir(OPLUS_BINDER_PROC_DIR, NULL);
	if (!oplus_kernel_lockopt_dir) {
		pr_err("failed to create proc dir oplus_lockopt");
		goto err_create_oplus_lockopt;
	}

	proc_node =
	    proc_create("opt_enabled", 0660, oplus_kernel_lockopt_dir,
			&proc_klock_opt_fops);
	if (!proc_node) {
		pr_err("failed to create proc node opt_enabled!!\n");
		goto err_create_oplus_lockopt;
	}

	proc_node =
	    proc_create("lock_monitor", 0440, oplus_kernel_lockopt_dir,
			&proc_klock_monitor_fops);
	if (!proc_node) {
		pr_err("failed to create proc node klock_monitor!!\n");
		goto err_create_oplus_lockopt;
	}

	return ret;
err_create_oplus_lockopt:
	remove_proc_entry(OPLUS_BINDER_PROC_DIR, NULL);
	return -ENOENT;
}

static int __init oplus_kernel_lockopt_init(void)
{
	int ret;

	ret = oplus_kernel_lockopt_proc_init();
	return ret;
}

module_init(oplus_kernel_lockopt_init);
MODULE_DESCRIPTION("Oplus Klock Opt Driver");
MODULE_LICENSE("GPL v2");
