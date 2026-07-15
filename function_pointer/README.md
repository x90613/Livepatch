# Kill Syscall Function Pointer Hook

This Linux kernel module replaces only `__NR_kill` in `sys_call_table`.
It blocks `SIGKILL` and forwards all other signals to the original syscall.
The original function pointer is restored when the module is removed.

> **Warning:** This module directly modifies kernel memory. Use only in a disposable
> VM or test machine with console access and a snapshot. A kernel crash is possible.

## Build and Load

```sh
# Enter the module directory and build the kernel module and demo program.
cd function_pointer
make

# Load the module. The hook is installed during insmod.
sudo insmod ./loadable_kernel_module.ko

# Confirm that the module is loaded.
lsmod | grep loadable_kernel_module

#Load the module first, then run the demo in the background. It prints its PID and waits for a signal:

# Start the user-space demo in the background.
./out/kill_demo

# Replace <PID> with the PID shown above.
# Signal 10 (SIGUSR1) is not denied and passes through to the demo.
kill -10 <PID>
ps -p <PID>

# Replace <PID> with the same PID. SIGKILL is intercepted.
kill -9 <PID>
ps -p <PID>

# Check the hook messages.
sudo dmesg | tail -n 20
```

The process should still be listed after `SIGKILL`. The kernel log should contain:

```text
enter hacked kill section.
kill signal intercepted and denied.
```

To compare the behavior without the hook:

```sh
# Remove the module and restore the original kill function pointer.
sudo rmmod loadable_kernel_module

# SIGKILL should terminate the demo normally now.
kill -KILL <PID>
ps -p <PID>
```

The target kernel needs matching headers, module-loading support, and access to
`kallsyms_lookup_name`. Stop test workloads before running `rmmod`; this demo does
not provide full synchronization for syscalls already executing inside the hook.

## Make Targets

```text
make               Build the demo and kernel module
make clean         Remove kernel build artifacts
make generateTestFile  Build the user-space demo in `out/`
make cleanTestFile Remove the `out/` directory
make load          Build and load the module
make unload        Remove the module
make logs          Show matching kernel log messages
```
