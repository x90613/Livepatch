# Kill Syscall Hook — Livepatch Approach

Hooks the `kill(2)` syscall using the kernel livepatch mechanism
(`CONFIG_LIVEPATCH`).  The hook blocks `SIGKILL` and forwards all other signals
to the original syscall via `kill_pid_info()`.  The patch is removed atomically
when the module is unloaded — no manual syscall table restoration needed.

**How it works:**
1. `klp_enable_patch()` resolves the symbol `__arm64_sys_kill` via kallsyms and
   registers an ftrace handler that redirects calls to `livepatch_sys_kill`.
2. The replacement function reads `pid` and `sig` from `pt_regs` (x0/x1 on
   arm64) and blocks `SIGKILL`.  For all other signals it replicates what
   `__arm64_sys_kill` does internally: builds a `kernel_siginfo` and calls
   `kill_pid_info()`.  Calling `__arm64_sys_kill` directly would cause infinite
   recursion because ftrace would redirect it back into the replacement.
3. On `rmmod`, the livepatch core unregisters the ftrace handler and restores
   the original function automatically.

> **Note:** Requires `CONFIG_LIVEPATCH=y`, `CONFIG_FUNCTION_TRACER=y`, and
> `CONFIG_HAVE_FENTRY=y` in the kernel config.  No memory remapping is needed.

## Prerequisites

- arm64 Linux kernel 6.18 with matching headers and livepatch support
- Cross-compiler if building off-target: set `CROSS=aarch64-linux-gnu-`
- Verify livepatch support: `zcat /proc/config.gz | grep CONFIG_LIVEPATCH`

## Build and Load

```sh
# Build kill_demo from hook_kill_demo/ first (shared by both modules)
cd hook_kill_demo
make demo

cd livepatch
make                          # build livepatch kernel module
sudo insmod ./livepatch_kill.ko
lsmod | grep livepatch_kill
```

Expected kernel log (`sudo dmesg | tail`):

```
livepatch_kill: module init
```

The livepatch entry is also visible in sysfs:

```sh
ls /sys/kernel/livepatch/livepatch_kill/
# enabled  transition  ...
cat /sys/kernel/livepatch/livepatch_kill/enabled   # 1
```

## Demo

```sh
# Terminal 1 — start the demo process (binary is in hook_kill_demo/)
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
livepatch_kill: enter kill section.
livepatch_kill: enter kill section.
livepatch_kill: SIGKILL intercepted and denied.
```

## Unload and Restore

The livepatch can be disabled either via sysfs or by unloading the module directly.

**Option 1 — disable via sysfs, then rmmod:**

```sh
echo 0 | sudo tee /sys/kernel/livepatch/livepatch_kill/enabled
```

This triggers the livepatch transition back to the original function. Once the
transition completes the kernel removes the sysfs entry automatically — the
directory `/sys/kernel/livepatch/livepatch_kill/` will no longer exist. Then
unload the module:

```sh
sudo rmmod livepatch_kill
```

**Option 2 — rmmod directly:**

```sh
sudo rmmod livepatch_kill
```

The livepatch core handles the transition and cleanup automatically.

Expected kernel log after either path:

```
livepatch_kill: module exit
```

After unload, SIGKILL terminates the process normally:

```sh
kill -9 <PID>
ps -p <PID>        # process gone
```

## Make Targets

```
make               Build the livepatch kernel module
make clean         Remove kernel build artifacts
make load          Build and load the module
make unload        Remove the module
make logs          Show matching kernel log messages
```

Run `make demo` from `hook_kill_demo/` to build `out/kill_demo`.
