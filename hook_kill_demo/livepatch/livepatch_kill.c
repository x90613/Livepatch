#include <linux/module.h>
#include <linux/livepatch.h>
#include <linux/syscalls.h>
#include <linux/signal.h>
#include <linux/pid.h>
#include <linux/cred.h>
#include <linux/sched.h>
#include <linux/kprobes.h>

#define OURMODNAME "livepatch_kill"
MODULE_AUTHOR("Harry Hsu x90613@gmail.com");
MODULE_DESCRIPTION("Syscall hook via livepatch: deny SIGKILL");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_INFO(livepatch, "Y");

// kprobe trick to recover kallsyms_lookup_name (unexported since kernel 5.7),
// then use it to find kill_pid_info (also not exported in this kernel build).
static struct kprobe kp = {
	.symbol_name = "kallsyms_lookup_name"
};

typedef int (*kill_pid_info_t)(int sig, struct kernel_siginfo *info, struct pid *pid);
static kill_pid_info_t fn_kill_pid_info;

// Livepatch replacement for __arm64_sys_kill.
// arm64: kill(pid, sig) — pid in x0 (regs[0]), sig in x1 (regs[1]).
//
// We cannot call __arm64_sys_kill directly: the ftrace hook would redirect it
// back into this function causing infinite recursion.  Instead we replicate
// what __arm64_sys_kill does internally: build a kernel_siginfo and call
// kill_pid_info(), whose address is resolved at init time via kallsyms.
static asmlinkage long livepatch_sys_kill(const struct pt_regs *regs)
{
	pid_t pid = (pid_t)regs->regs[0]; // x0
	int   sig = (int) regs->regs[1];  // x1

	pr_info("%s: enter kill section.\n", OURMODNAME);

	if (sig == SIGKILL) {
		pr_info("%s: SIGKILL intercepted and denied.\n", OURMODNAME);
		return 0;
	}

	struct kernel_siginfo info;
	struct pid *pid_struct;
	int ret;

	clear_siginfo(&info);
	info.si_signo = sig;
	info.si_errno = 0;
	info.si_code  = SI_USER;
	info.si_pid   = task_tgid_vnr(current);
	info.si_uid   = from_kuid_munged(current_user_ns(), current_uid());

	pid_struct = find_get_pid(pid);
	ret = fn_kill_pid_info(sig, &info, pid_struct);
	put_pid(pid_struct);
	return ret;
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

	fn_kill_pid_info = (kill_pid_info_t)kallsyms_lookup_name("kill_pid_info");
	if (!fn_kill_pid_info) {
		pr_err("%s: failed to find kill_pid_info\n", OURMODNAME);
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
