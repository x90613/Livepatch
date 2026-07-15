#include <linux/module.h>
#include <linux/livepatch.h>
#include <linux/syscalls.h>
#include <linux/signal.h>
#include <linux/kprobes.h>

#define OURMODNAME "livepatch_kill"
MODULE_AUTHOR("Harry Hsu x90613@gmail.com");
MODULE_DESCRIPTION("Syscall hook via livepatch: deny SIGKILL");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_INFO(livepatch, "Y");

// Use the same kprobe trick as the function_pointer approach to recover
// kallsyms_lookup_name (unexported since kernel 5.7), then look up the
// original __arm64_sys_kill so we can forward non-SIGKILL signals.
static struct kprobe kp = {
	.symbol_name = "kallsyms_lookup_name"
};

typedef asmlinkage long (*t_syscall)(const struct pt_regs *);
static t_syscall orig_kill;

// Livepatch replacement for __arm64_sys_kill.
// arm64: kill(pid, sig) — sig in x1 (regs[1]).
// Non-SIGKILL signals are forwarded to the original function via orig_kill,
// exactly as the function_pointer approach does with orig_kill(regs).
static asmlinkage long livepatch_sys_kill(const struct pt_regs *regs)
{
	int sig = (int)regs->regs[1]; // x1

	pr_info("%s: enter kill section.\n", OURMODNAME);

	if (sig == SIGKILL) {
		pr_info("%s: SIGKILL intercepted and denied.\n", OURMODNAME);
		return 0;
	}

	return orig_kill(regs);
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
	typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
	kallsyms_lookup_name_t kallsyms_lookup_name;

	pr_info("%s: module init\n", OURMODNAME);

	if (register_kprobe(&kp) < 0) {
		pr_err("%s: failed to register kprobe\n", OURMODNAME);
		return -ENOENT;
	}
	kallsyms_lookup_name = (kallsyms_lookup_name_t)kp.addr;
	unregister_kprobe(&kp);

	orig_kill = (t_syscall)kallsyms_lookup_name("__arm64_sys_kill");
	if (!orig_kill) {
		pr_err("%s: failed to find __arm64_sys_kill\n", OURMODNAME);
		return -ENOENT;
	}

	return klp_enable_patch(&patch);
}

static void __exit livepatch_kill_exit(void)
{
	pr_info("%s: module exit\n", OURMODNAME);
}

module_init(livepatch_kill_init);
module_exit(livepatch_kill_exit);
