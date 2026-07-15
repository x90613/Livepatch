# syscall_demo

Two Linux kernel modules that hook the `kill(2)` syscall to block `SIGKILL`,
implemented two different ways.  Side by side, they show how the same behaviour
can be achieved with direct syscall table manipulation versus the kernel
livepatch mechanism.

| | `function_pointer` | `livepatch` |
|---|---|---|
| Hook mechanism | Overwrites `sys_call_table[__NR_kill]` | Redirects `__arm64_sys_kill` via ftrace |
| Memory remapping | Required (`update_mapping_prot`) | Not needed |
| Symbol lookup | kprobe → `kallsyms_lookup_name` | `klp_enable_patch()` |
| Forward to original | Saved function pointer (`orig_kill`) | `ksys_kill(pid, sig)` |
| Restore on unload | Manual (`lkm_exit`) | Automatic (livepatch core) |

Both modules target **arm64, kernel 6.18**.

## Repo layout

```
kill_demo.c              user-space demo process (shared)
function_pointer/        syscall table hook via function pointer
livepatch/               syscall hook via kernel livepatch
```

## Build

```sh
# Build the shared user-space demo binary (out/kill_demo)
make demo

# Build a specific kernel module
cd function_pointer && make
cd livepatch && make

# Or build everything at once
make
```

## Demo

The demo flow is the same for both modules.

**Terminal 1** — start the demo process:

```sh
./out/kill_demo
# kill demo is running. PID: <PID>
```

**Terminal 2** — load a module and test:

```sh
# Load either module
sudo insmod function_pointer/function_pointer_kill.ko
# or
sudo insmod livepatch/livepatch_kill.ko

# SIGUSR1 passes through — process receives it
kill -10 <PID>

# SIGKILL is intercepted — process survives
kill -9 <PID>
ps -p <PID>    # still listed

# Unload restores normal behaviour
sudo rmmod function_pointer_kill
# For livepatch, you can also disable via sysfs first (the sysfs entry
# disappears once the transition completes), then rmmod:
#   echo 0 | sudo tee /sys/kernel/livepatch/livepatch_kill/enabled
#   sudo rmmod livepatch_kill

# Now SIGKILL terminates the process
kill -9 <PID>
ps -p <PID>    # gone
```

Expected kernel log during the hook (same format for both modules, prefix differs):

```
function_pointer_kill: enter kill section.
function_pointer_kill: enter kill section.
function_pointer_kill: SIGKILL intercepted and denied.
```

See each subdirectory's README for full build instructions, expected kernel
logs, and module-specific notes.

> **Warning:** These modules modify kernel behaviour.  Use only in a disposable
> VM or test machine with a snapshot and console access.
