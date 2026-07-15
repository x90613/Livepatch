#include <linux/module.h>
#include <linux/syscalls.h>
#include <asm/syscall.h>

// kprobe for kallsyms_lookup_name
#include <linux/kprobes.h>
static struct kprobe kp = {
	    .symbol_name = "kallsyms_lookup_name"
};

#define OURMODNAME "function_pointer_kill"
MODULE_AUTHOR("Harry Hsu x90613@gmail.com");
MODULE_DESCRIPTION("Syscall hook via function pointer: deny SIGKILL");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");

// Addresses and permissions needed to replace syscall table entries.
static unsigned long *__sys_call_table;
static void (*update_mapping_prot)(phys_addr_t phys, unsigned long virt,
					   phys_addr_t size, pgprot_t prot);
static unsigned long start_rodata;
static unsigned long init_begin;
#define section_size (init_begin - start_rodata)

typedef asmlinkage long (*t_syscall)(const struct pt_regs *);
static t_syscall orig_kill;

// Syscall hook: deny SIGKILL and pass all other signals to the original syscall.
static asmlinkage long hacked_kill(const struct pt_regs *regs)
{
	// arm64: kill(pid, sig) — pid in x0 (regs[0]), sig in x1 (regs[1]).
	int sig = (int)regs->regs[1];

	pr_info("%s: enter kill section.\n", OURMODNAME);

	if (sig == SIGKILL) {
		pr_info("%s: SIGKILL intercepted and denied.\n", OURMODNAME);
		return 0;
	}

	return orig_kill(regs);
}

static unsigned long *get_syscall_table(void)
{
	unsigned long *syscall_table;
	typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
	kallsyms_lookup_name_t kallsyms_lookup_name;
	// kallsyms_lookup_name is unexported since kernel 5.7. We recover its address by registering
	// a kprobe on its symbol, reading kp.addr, then immediately unregistering.
	if (register_kprobe(&kp) < 0)
		return NULL;
	kallsyms_lookup_name = (kallsyms_lookup_name_t) kp.addr;
	unregister_kprobe(&kp);

	// The parameters to be obtained from kallsyms_lookup_name are generated here.
	syscall_table = (unsigned long*)kallsyms_lookup_name("sys_call_table");
	update_mapping_prot = (void *)kallsyms_lookup_name("update_mapping_prot");
	start_rodata = (unsigned long)kallsyms_lookup_name("__start_rodata");
	init_begin = (unsigned long)kallsyms_lookup_name("__init_begin");

	pr_info("%s: sys_call_table address: %p\n", OURMODNAME, syscall_table);
	return syscall_table;
}

static inline void protect_memory(void)
{
	update_mapping_prot(__pa_symbol(start_rodata), (unsigned long)start_rodata, section_size, PAGE_KERNEL_RO);
	pr_info("%s: memory protected (RO)\n", OURMODNAME);
}

static inline void unprotect_memory(void)
{
	update_mapping_prot(__pa_symbol(start_rodata), (unsigned long)start_rodata, section_size, PAGE_KERNEL);
	pr_info("%s: memory unprotected (RW)\n", OURMODNAME);
}

static void install_hooks(void)
{
	unprotect_memory();

	__sys_call_table[__NR_kill] = (unsigned long)&hacked_kill;

	protect_memory();
}

static int __init lkm_init(void)
{
	pr_info("%s: module init\n", OURMODNAME);

	__sys_call_table = get_syscall_table();
	if (!__sys_call_table || !update_mapping_prot || !start_rodata || !init_begin) {
		pr_err("%s: failed to find syscall hook symbols\n", OURMODNAME);
		return -ENOENT;
	}

	pr_info("%s: __NR_kill = %d\n", OURMODNAME, __NR_kill);

	// To store original syscall table addresses
	orig_kill = (t_syscall)__sys_call_table[__NR_kill];

	install_hooks();
	pr_info("%s: kill syscall hook installed\n", OURMODNAME);

	return 0;
}

static void __exit lkm_exit(void)
{
	pr_info("%s: module exit\n", OURMODNAME);

	unprotect_memory();
	__sys_call_table[__NR_kill] = (unsigned long)orig_kill;
	protect_memory();

	pr_info("%s: kill syscall hook restored\n", OURMODNAME);
}

module_init(lkm_init);
module_exit(lkm_exit);
