#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#define OURMODNAME "target"
MODULE_AUTHOR("Harry Hsu x90613@gmail.com");
MODULE_DESCRIPTION("Target module: exports slow_func() for livepatch stack demo");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

static struct task_struct *kthread1;
static struct task_struct *kthread2;

// noinline ensures slow_func has its own stack frame and is visible to stack_trace_save_tsk.
noinline void slow_func(void)
{
	pr_info("%s: [OLD] slow_func enter (task: %s)\n", OURMODNAME, current->comm);
	msleep(15000);
	pr_info("%s: [OLD] slow_func exit  (task: %s)\n", OURMODNAME, current->comm);
}
EXPORT_SYMBOL(slow_func);

// kthread1: calls slow_func() in a tight loop with no gap -- always inside it,
// so the livepatch checker can never switch it. Demonstrates a task permanently
// stuck on the old version.
static int kthread1_fn(void *unused)
{
	pr_info("%s: kthread1 started\n", OURMODNAME);
	while (!kthread_should_stop())
		slow_func();
	pr_info("%s: kthread1 stopped\n", OURMODNAME);
	return 0;
}

// kthread2: calls slow_func() then sleeps 3s outside it -- stack is clear most
// of the time, so the livepatch checker switches it almost immediately.
static int kthread2_fn(void *unused)
{
	pr_info("%s: kthread2 started\n", OURMODNAME);
	while (!kthread_should_stop()) {
		pr_info("%s: kthread2 about to call slow_func\n", OURMODNAME);
		slow_func();
		pr_info("%s: kthread2 outside slow_func, sleeping 3s\n", OURMODNAME);
		msleep(3000);
	}
	pr_info("%s: kthread2 stopped\n", OURMODNAME);
	return 0;
}

static int __init target_init(void)
{
	pr_info("%s: module init\n", OURMODNAME);

	kthread1 = kthread_run(kthread1_fn, NULL, "demo_kthread1");
	if (IS_ERR(kthread1))
		return PTR_ERR(kthread1);

	kthread2 = kthread_run(kthread2_fn, NULL, "demo_kthread2");
	if (IS_ERR(kthread2)) {
		kthread_stop(kthread1);
		return PTR_ERR(kthread2);
	}

	return 0;
}

static void __exit target_exit(void)
{
	kthread_stop(kthread2);
	kthread_stop(kthread1);
	pr_info("%s: module exit\n", OURMODNAME);
}

module_init(target_init);
module_exit(target_exit);
