// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/freezer.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched_assist/sched_assist_common.h>
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#include <linux/seq_file.h>

static struct mutex mutexA;
static struct mutex mutexB;
static struct mutex mutexC;
static struct rw_semaphore semA;

u64 ux_block_time;
extern unsigned long __read_mostly tracing_mark_write_addr;

struct kthread_param {
	int policy;
	int nice;
	int rt_priority;
	int ux_state;
	struct cpumask mask;
	int busy_tick;
	int idle_tick;
};
u64 ux_wait_time_rw_read;
u64 ux_block_time_rw_read;
u64 ux_wait_time_rw_write;
u64 ux_block_time_rw_write;
u64 ux_wait_time_mutex;
u64 ux_block_time_mutex;
static ssize_t proc_klock_test_read_rwsem(struct file *file, char __user *buff,
					  size_t count, loff_t *ppos)
{
	char buffer[128];
	size_t len = 0;

	len =
	    snprintf(buffer, sizeof(buffer), "ux_wait_rwsem_read_time=%llu\n",
		     ux_wait_time_rw_read);
	return simple_read_from_buffer(buff, count, ppos, buffer, len);
}

static const struct file_operations proc_klock_test_read_rw_fops = {
	.read = proc_klock_test_read_rwsem,
};

static ssize_t proc_klock_test_write_rwsem(struct file *file,
					   char __user *buff, size_t count,
					   loff_t *ppos)
{
	char buffer[128];
	size_t len = 0;

	len =
	    snprintf(buffer, sizeof(buffer), "ux_wait_rwsem_write_time=%llu\n",
		     ux_wait_time_rw_write);
	return simple_read_from_buffer(buff, count, ppos, buffer, len);
}

static const struct file_operations proc_klock_test_write_rw_fops = {
	.read = proc_klock_test_write_rwsem,
};

static ssize_t proc_klock_test_read_mutex(struct file *file, char __user *buff,
					  size_t count, loff_t *ppos)
{
	char buffer[128];
	size_t len = 0;

	len =
	    snprintf(buffer, sizeof(buffer), "ux_wait_mutex_time=%llu\n",
		     ux_wait_time_mutex);
	return simple_read_from_buffer(buff, count, ppos, buffer, len);
}

static const struct file_operations proc_klock_test_mutex_fops = {
	.read = proc_klock_test_read_mutex,
};

static int rw_read_wait_A(void *data)
{
	struct kthread_param *param = data;
	bool flag = false;

	if (param && param->ux_state == 8) {
		current->ux_state |= SA_TYPE_LISTPICK;
		flag = true;
	} else if (param && (param->policy == SCHED_FIFO)) {
		struct sched_param sp;
		sp.sched_priority = param->rt_priority;
		sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
	}

	if (param->nice == 19) {
		set_user_nice(current, param->nice);
	}
	while (true) {
		event_trace_printk(tracing_mark_write_addr,
				   "B|%d|read_acquire_semA\n", current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		if (flag)
			ux_block_time_rw_read = sched_clock();
		down_read(&semA);
		if (flag)
			ux_wait_time_rw_read =
			    (ux_wait_time_rw_read +
			     (sched_clock() - ux_block_time_rw_read)) >> 2;
		mdelay(20);
		up_read(&semA);
		event_trace_printk(tracing_mark_write_addr,
				   "B|%d|read_release_semA\n", current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		mdelay(10);
	}
}

static int rw_write_wait_A(void *data)
{
	struct kthread_param *param = data;
	bool flag = false;

	if (param && param->ux_state == 8) {
		current->ux_state |= SA_TYPE_LISTPICK;
		flag = true;
	} else if (param && (param->policy == SCHED_FIFO)) {
		struct sched_param sp;
		sp.sched_priority = param->rt_priority;
		sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
		printk("rt_set good");
	}

	if (param->nice == 19) {
		set_user_nice(current, param->nice);
	}

	while (true) {
		event_trace_printk(tracing_mark_write_addr,
				   "B|%d|write_acquire_semA\n", current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		if (flag)
			ux_block_time_rw_write = sched_clock();
		down_write(&semA);
		if (flag)
			ux_wait_time_rw_write =
			    (ux_wait_time_rw_write +
			     (sched_clock() - ux_block_time_rw_write)) >> 2;
		mdelay(20);
		up_write(&semA);
		event_trace_printk(tracing_mark_write_addr,
				   "B|%d|write_release_semA\n", current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		mdelay(10);
	}
}

static int busy_wait_A(void *data)
{
	struct kthread_param *param = data;
	u64 before;
	bool flag = false;

	if (param && param->ux_state == 8) {
		current->ux_state |= SA_TYPE_LISTPICK;
		flag = true;
	} else if (param && (param->policy == SCHED_FIFO)) {
		struct sched_param sp;
		sp.sched_priority = param->rt_priority;
		sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
	}

	while (true) {
		event_trace_printk(tracing_mark_write_addr, "B|%d|acquire_A\n",
				   current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		if (flag)
			ux_block_time_mutex = sched_clock();
		mutex_lock(&mutexA);
		if (flag) {
			ux_wait_time_mutex =
			    (ux_wait_time_mutex +
			     (sched_clock() - ux_block_time_mutex)) >> 2;
		}

		mdelay(20);
		mutex_unlock(&mutexA);
		event_trace_printk(tracing_mark_write_addr, "B|%d|release_A\n",
				   current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		mdelay(10);
	}
	return 0;
}

static int busy_wait_B(void *data)
{
	while (true) {
		event_trace_printk(tracing_mark_write_addr, "B|%d|acquire_B\n",
				   current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		mutex_lock(&mutexB);
		mdelay(20);
		mutex_unlock(&mutexB);
		event_trace_printk(tracing_mark_write_addr, "B|%d|release_B\n",
				   current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		mdelay(10);
	}
	return 0;
}

static busy_wait_C(void *data)
{
	while (true) {
		event_trace_printk(tracing_mark_write_addr, "B|%d|acquire_C\n",
				   current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		mutex_lock(&mutexC);
		mdelay(20);
		mutex_unlock(&mutexC);
		event_trace_printk(tracing_mark_write_addr, "B|%d|release_C\n",
				   current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		mdelay(10);
	}
	return 0;
}

static int busy_wait_ABC(void *data)
{
	while (true) {
		event_trace_printk(tracing_mark_write_addr, "B|%d|acquire_A\n",
				   current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		mutex_lock(&mutexA);
		event_trace_printk(tracing_mark_write_addr, "B|%d|acquire_B\n",
				   current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		mutex_lock(&mutexB);
		event_trace_printk(tracing_mark_write_addr, "B|%d|acquire_C\n",
				   current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		mutex_lock(&mutexC);
		mdelay(20);
		mutex_unlock(&mutexC);
		event_trace_printk(tracing_mark_write_addr, "B|%d|release_C\n",
				   current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		mutex_unlock(&mutexB);
		event_trace_printk(tracing_mark_write_addr, "B|%d|release_B\n",
				   current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		mutex_unlock(&mutexA);
		event_trace_printk(tracing_mark_write_addr, "B|%d|release_A\n",
				   current->pid);
		event_trace_printk(tracing_mark_write_addr, "E\n");
		mdelay(10);
	}
	return 0;
}

void test_ux(void)
{
	int i = 0;
	struct kthread_param param;
	/*
	for (i = 0 ;i < 2; i++) {
		kthread_run(busy_wait_A, NULL,"test_thread_A");
	}
	mdelay(100);

	for (i = 0 ;i < 2; i++) {
		kthread_run(busy_wait_B, NULL,"test_thread_B");
	}
	mdelay(100);

	for (i = 0 ;i < 2; i++) {
		kthread_run(busy_wait_C, NULL,"test_thread_C");
	}
	mdelay(100);

	for (i = 0 ;i < 2; i++) {
			kthread_run(busy_wait_ABC, NULL,"test_thread_ABC");
		}
	mdelay(100);

	param.policy = SCHED_FIFO;
	param.rt_priority = 1;
	kthread_run(busy_wait_A, (void *)&param, "test_thread_rtA");

	mdelay(1000);

	param.policy = 0;
	param.rt_priority = 0;

	param.ux_state = 8;
	kthread_run(busy_wait_A, (void *)&param, "test_thread_uxA");

	mdelay(100);
	param.ux_state = 0;
	param.policy = SCHED_FIFO;
	param.rt_priority = 1;
	kthread_run(rw_read_wait_A, (void *)&param, "test_rw_rt_read");	//create a rt thread to read
	mdelay(100);
	param.policy = 0;
	param.rt_priority = 0;

	param.nice = 19;
	*/
	for (i = 0; i < 200; i++) {
		kthread_run(rw_read_wait_A, (void *)&param, "test_rw_read");
	}
	mdelay(100);

	/*
	for (i = 0; i < 10; i++) {
		kthread_run(rw_write_wait_A, (void *)&param, "test_rw_write");
	}
	mdelay(100);
	   param.ux_state = 8;
	   kthread_run(rw_read_wait_A, (void *)&param, "test_rw_ux_read");
	   mdelay(100);
	param.nice = 0;
	param.ux_state = 8;
	kthread_run(rw_read_wait_A, (void *)&param, "test_rw_ux_read");

	mdelay(100);
	*/
}

static ssize_t test_write(struct file *file,
			  const char *ubuf, size_t count, loff_t *ppos)
{
	char buf[32];

	if (count >= sizeof(buf) || count == 0)
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, count))
		return -EFAULT;

	buf[count] = '\0';

	if (!strncmp(buf, "go", count - 1))
		test_ux();

	return count;
}

static const struct file_operations proc_ux_fops = {
	.open = simple_open,
	.write = test_write,
};

static int __init uxlockdebug_init(void)
{
	mutex_init(&mutexA);
	mutex_init(&mutexB);
	mutex_init(&mutexC);
	init_rwsem(&semA);
	proc_create("uxtest", 0660, NULL, &proc_ux_fops);
	proc_create("uxrwsemreadresult", 0660, NULL,
		    &proc_klock_test_read_rw_fops);
	proc_create("uxrwsemwriteresult", 0660, NULL,
		    &proc_klock_test_write_rw_fops);
	proc_create("uxmutexresult", 0660, NULL, &proc_klock_test_mutex_fops);
	return 0;
}

fs_initcall(uxlockdebug_init);
