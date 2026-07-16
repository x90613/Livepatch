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
	pr_info("%s: slow_func enter (task: %s)\n", OURMODNAME, current->comm);
	msleep(15000);
	pr_info("%s: slow_func exit  (task: %s)\n", OURMODNAME, current->comm);
}
EXPORT_SYMBOL(slow_func);

// kthread1: calls slow_func() in a loop with no gap — almost always inside it.
// This ensures slow_func is on its stack when the livepatch loads.
static int kthread1_fn(void *unused)
{
	pr_info("%s: kthread1 started\n", OURMODNAME);
	while (!kthread_should_stop())
		slow_func();
	pr_info("%s: kthread1 stopped\n", OURMODNAME);
	return 0;
}

// kthread2: also calls slow_func() but with a short 3s sleep — so it spends
// most of its time outside slow_func() and is switched immediately when the
// livepatch loads. After switching it calls the new version each iteration.
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
