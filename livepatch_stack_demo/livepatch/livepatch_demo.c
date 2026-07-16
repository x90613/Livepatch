#include <linux/module.h>
#include <linux/livepatch.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched/signal.h>
#include <linux/stacktrace.h>
#include <linux/kallsyms.h>

#define OURMODNAME   "livepatch_demo"
#define MAX_STACK    32

MODULE_AUTHOR("Harry Hsu x90613@gmail.com");
MODULE_DESCRIPTION("Livepatch stack demo: show per-task stack check during transition");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_INFO(livepatch, "Y");

// Replacement for slow_func() in the target module.
// Prints which task is now running the new version.
noinline void new_slow_func(void)
{
	pr_info("%s: [NEW] slow_func enter (task: %s)\n", OURMODNAME, current->comm);
	msleep(15000);
	pr_info("%s: [NEW] slow_func exit  (task: %s)\n", OURMODNAME, current->comm);
}

static struct klp_func funcs[] = {
	{
		.old_name = "slow_func",
		.new_func = new_slow_func,
	},
	{ } // sentinel
};

static struct klp_object objs[] = {
	{
		.name  = "target", // patch the target module, not vmlinux
		.funcs = funcs,
	},
	{ } // sentinel
};

static struct klp_patch patch = {
	.mod  = THIS_MODULE,
	.objs = objs,
};

// Dump the stack of every task and note whether slow_func is on it.
// Called once from module_init after klp_enable_patch() to show which tasks
// are blocked from transitioning.
static void dump_task_stacks(void)
{
	struct task_struct *task;
	unsigned long entries[MAX_STACK];
	char sym[KSYM_SYMBOL_LEN];
	int i, nr;

	pr_info("%s: --- per-task stack snapshot ---\n", OURMODNAME);

	rcu_read_lock();
	for_each_process(task) {
		bool has_slow_func = false;

		get_task_struct(task);
		rcu_read_unlock();

		nr = stack_trace_save_tsk(task, entries, MAX_STACK, 0);

		for (i = 0; i < nr; i++) {
			sprint_symbol(sym, entries[i]);
			if (strstr(sym, "slow_func")) {
				has_slow_func = true;
				break;
			}
		}

		if (has_slow_func) {
			pr_info("%s: [BLOCKED]  %s (pid %d) — slow_func on stack, cannot switch yet\n",
				OURMODNAME, task->comm, task->pid);
			for (i = 0; i < nr; i++) {
				sprint_symbol(sym, entries[i]);
				pr_info("%s:   [%d] %s\n", OURMODNAME, i, sym);
			}
		} else if (strncmp(task->comm, "demo_kthread", 12) == 0) {
			pr_info("%s: [SWITCHED] %s (pid %d) — slow_func not on stack, switched immediately\n",
				OURMODNAME, task->comm, task->pid);
		}

		rcu_read_lock();
		put_task_struct(task);
	}
	rcu_read_unlock();

	pr_info("%s: --- end of stack snapshot ---\n", OURMODNAME);
}

static int __init livepatch_demo_init(void)
{
	int ret;

	pr_info("%s: module init\n", OURMODNAME);

	ret = klp_enable_patch(&patch);
	if (ret)
		return ret;

	pr_info("%s: patch enabled, transition started\n", OURMODNAME);
	dump_task_stacks();

	return 0;
}

static void __exit livepatch_demo_exit(void)
{
	pr_info("%s: module exit\n", OURMODNAME);
}

module_init(livepatch_demo_init);
module_exit(livepatch_demo_exit);
