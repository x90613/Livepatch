# Kill Syscall Hook — Function Pointer Approach

Hooks the `kill(2)` syscall by directly overwriting `sys_call_table[__NR_kill]`
with a pointer to a replacement function.  The hook blocks `SIGKILL` and
forwards all other signals to the original syscall.  The original pointer is
restored when the module is removed.

**How it works:**
1. Locate `sys_call_table`, `update_mapping_prot`, and section boundaries via
   `kallsyms_lookup_name` (recovered through a kprobe because it is unexported
   since kernel 5.7).
2. Temporarily remap the read-only kernel text section as read-write.
3. Overwrite `sys_call_table[__NR_kill]` with the address of `hacked_kill`.
4. Restore the original memory protection.

> **Warning:** This module directly modifies kernel memory.  Use only in a
> disposable VM or test machine with a snapshot and console access.

## Prerequisites

- arm64 Linux kernel with matching headers and `CONFIG_KPROBES=y`
- Cross-compiler if building off-target: set `CROSS=aarch64-linux-gnu-`

## Build and Load

```sh
# Build kill_demo from repo root first (shared by both modules)
make demo

cd function_pointer
make                          # build kernel module
sudo insmod ./function_pointer_kill.ko
lsmod | grep function_pointer_kill
```

Expected kernel log (`sudo dmesg | tail`):

```
function_pointer_kill: module init
function_pointer_kill: sys_call_table address: 0xffff...
function_pointer_kill: __NR_kill = 129
function_pointer_kill: memory unprotected (RW)
function_pointer_kill: memory protected (RO)
function_pointer_kill: kill syscall hook installed
```

## Demo

```sh
# Terminal 1 — start the demo process (binary is at repo root)
../out/kill_demo
# prints: kill demo is running. PID: <PID>

# Terminal 2 — test while the hook is active

# SIGUSR1 (10) passes through — process receives it
kill -10 <PID>
# kill_demo prints: SIGUSR1 received

# SIGKILL (9) is intercepted — process survives
kill -9 <PID>
ps -p <PID>        # process still listed
```

Expected kernel log after the two kill commands:

```
function_pointer_kill: enter kill section.
function_pointer_kill: enter kill section.
function_pointer_kill: SIGKILL intercepted and denied.
```

## Unload and Restore

```sh
sudo rmmod function_pointer_kill
```

Expected kernel log:

```
function_pointer_kill: module exit
function_pointer_kill: memory unprotected (RW)
function_pointer_kill: memory protected (RO)
function_pointer_kill: kill syscall hook restored
```

After unload, SIGKILL terminates the process normally:

```sh
kill -9 <PID>
ps -p <PID>        # process gone
```

## Make Targets

```
make               Build the kernel module
make clean         Remove kernel build artifacts
make load          Build and load the module
make unload        Remove the module
make logs          Show matching kernel log messages
```

Run `make demo` from the repo root to build `out/kill_demo`.
