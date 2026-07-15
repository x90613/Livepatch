#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/signal.h>
#include <linux/syscalls.h>
#include <asm/syscall.h>

#include "loadable_kernel_module.h"

#define OURMODNAME "loadable_kernel_module"

MODULE_AUTHOR("Harry Hsu x90613@gmail.com");
MODULE_DESCRIPTION("Syscall hook test LKM");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");

static struct kprobe kp = {
	.symbol_name = "kallsyms_lookup_name",
};

static int major;
static struct cdev *kernel_cdev;
static unsigned long *__sys_call_table;
static void (*update_mapping_prot)(phys_addr_t phys, unsigned long virt,
					   phys_addr_t size, pgprot_t prot);
static unsigned long start_rodata;
static unsigned long init_begin;

#define section_size (init_begin - start_rodata)

typedef asmlinkage long (*t_syscall)(const struct pt_regs *);
static t_syscall orig_kill;
static t_syscall orig_reboot;
static t_syscall orig_getdents64;
static bool hooks_installed;

static asmlinkage long hacked_reboot(const struct pt_regs *regs)
{
	unsigned int cmd = (unsigned int)regs->regs[2];

	pr_info("enter hacked reboot section\n");
	if (cmd == LINUX_REBOOT_CMD_POWER_OFF) {
		pr_info("power off command intercepted and denied\n");
		return 0;
	}
	return orig_reboot(regs);
}

static asmlinkage long hacked_kill(const struct pt_regs *regs)
{
	int sig = (int)regs->regs[1];

	pr_info("enter hacked kill section\n");
	if (sig == SIGKILL) {
		pr_info("kill signal intercepted and denied\n");
		return 0;
	}
	return orig_kill(regs);
}

static asmlinkage long hacked_getdents64(const struct pt_regs *regs)
{
	pr_info("enter hacked getdents64 section\n");
	return orig_getdents64(regs);
}

static unsigned long *get_syscall_table(void)
{
	typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
	kallsyms_lookup_name_t lookup;
	unsigned long *table;

	if (register_kprobe(&kp) < 0)
		return NULL;
	lookup = (kallsyms_lookup_name_t)kp.addr;
	unregister_kprobe(&kp);

	table = (unsigned long *)lookup("sys_call_table");
	update_mapping_prot = (void *)lookup("update_mapping_prot");
	start_rodata = (unsigned long)lookup("__start_rodata");
	init_begin = (unsigned long)lookup("__init_begin");
	return table;
}

static void protect_memory(void)
{
	update_mapping_prot(__pa_symbol(start_rodata), start_rodata,
				   section_size, PAGE_KERNEL_RO);
}

static void unprotect_memory(void)
{
	update_mapping_prot(__pa_symbol(start_rodata), start_rodata,
				   section_size, PAGE_KERNEL);
}

static int install_hooks(void)
{
	if (hooks_installed)
		return 0;

	unprotect_memory();
	__sys_call_table[__NR_reboot] = (unsigned long)hacked_reboot;
	__sys_call_table[__NR_kill] = (unsigned long)hacked_kill;
	__sys_call_table[__NR_getdents64] = (unsigned long)hacked_getdents64;
	protect_memory();
	hooks_installed = true;
	return 0;
}

static int lkm_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int lkm_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static long lkm_ioctl(struct file *filp, unsigned int command,
			      unsigned long arg)
{
	if (command != IOCTL_MOD_HOOK)
		return -EINVAL;
	return install_hooks();
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = lkm_open,
	.unlocked_ioctl = lkm_ioctl,
	.release = lkm_release,
};

static int __init lkm_init(void)
{
	dev_t dev_no;
	int ret;

	kernel_cdev = cdev_alloc();
	if (!kernel_cdev)
		return -ENOMEM;

	ret = alloc_chrdev_region(&dev_no, 0, 1, OURMODNAME);
	if (ret < 0)
		goto err_cdev;
	major = MAJOR(dev_no);

	cdev_init(kernel_cdev, &fops);
	kernel_cdev->owner = THIS_MODULE;
	ret = cdev_add(kernel_cdev, dev_no, 1);
	if (ret < 0)
		goto err_region;

	__sys_call_table = get_syscall_table();
	if (!__sys_call_table || !update_mapping_prot) {
		cdev_del(kernel_cdev);
		unregister_chrdev_region(dev_no, 1);
		return -ENOENT;
	}

	orig_reboot = (t_syscall)__sys_call_table[__NR_reboot];
	orig_kill = (t_syscall)__sys_call_table[__NR_kill];
	orig_getdents64 = (t_syscall)__sys_call_table[__NR_getdents64];
	pr_info("%s loaded; use IOCTL_MOD_HOOK to install syscall hooks\n",
		OURMODNAME);
	return 0;

err_region:
	unregister_chrdev_region(dev_no, 1);
err_cdev:
	cdev_del(kernel_cdev);
	return ret < 0 ? ret : -ENOENT;
}

static void __exit lkm_exit(void)
{
	dev_t dev_no = MKDEV(major, 0);

	if (__sys_call_table && hooks_installed) {
		unprotect_memory();
		__sys_call_table[__NR_reboot] = (unsigned long)orig_reboot;
		__sys_call_table[__NR_kill] = (unsigned long)orig_kill;
		__sys_call_table[__NR_getdents64] = (unsigned long)orig_getdents64;
		protect_memory();
		hooks_installed = false;
	}
	cdev_del(kernel_cdev);
	unregister_chrdev_region(dev_no, 1);
	pr_info("%s removed\n", OURMODNAME);
}

module_init(lkm_init);
module_exit(lkm_exit);
