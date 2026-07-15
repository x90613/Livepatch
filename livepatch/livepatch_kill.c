#include <linux/module.h>
#include <linux/livepatch.h>
#include <linux/syscalls.h>
#include <linux/signal.h>

#define OURMODNAME "livepatch_kill"
MODULE_AUTHOR("Harry Hsu x90613@gmail.com");
MODULE_DESCRIPTION("Syscall hook via livepatch: deny SIGKILL");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_INFO(livepatch, "Y");

// Livepatch replacement for __arm64_sys_kill.
// sig lives in regs->regs[1] (x1) on arm64 — same register layout as the
// function_pointer approach.  For non-SIGKILL signals we forward to ksys_kill,
// which is the in-kernel callable form of the kill syscall that takes plain
// (pid_t, int) arguments instead of pt_regs.
static asmlinkage long livepatch_sys_kill(const struct pt_regs *regs)
{
	pid_t pid = (pid_t)regs->regs[0]; // x0
	int   sig = (int) regs->regs[1];  // x1

	pr_info("%s: enter kill section.\n", OURMODNAME);

	if (sig == SIGKILL) {
		pr_info("%s: SIGKILL intercepted and denied.\n", OURMODNAME);
		return 0;
	}

	return ksys_kill(pid, sig);
}

static struct klp_func funcs[] = {
	{
		.old_name = "__arm64_sys_kill",
		.new_func = livepatch_sys_kill,
	},
	{ } // sentinel
};

static struct klp_object objs[] = {
	{
		.name  = NULL, // NULL means vmlinux
		.funcs = funcs,
	},
	{ } // sentinel
};

static struct klp_patch patch = {
	.mod  = THIS_MODULE,
	.objs = objs,
};

static int __init livepatch_kill_init(void)
{
	pr_info("%s: module init\n", OURMODNAME);
	return klp_enable_patch(&patch);
}

static void __exit livepatch_kill_exit(void)
{
	pr_info("%s: module exit\n", OURMODNAME);
}

module_init(livepatch_kill_init);
module_exit(livepatch_kill_exit);
