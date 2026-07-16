# Livepatch Stack Demo

Demonstrates how the Linux kernel livepatch mechanism uses **per-task stack
checking** to safely transition tasks to a patched function.  Two kthreads show
the two outcomes side by side:

- **kthread1** calls `slow_func()` in a tight loop with no gap -- its stack
  almost always contains the old function, so the kernel can never switch it.
  It is permanently stuck on the old version for as long as it runs.
- **kthread2** calls `slow_func()` then sleeps 3 s outside it -- its stack is
  clear most of the time, so the kernel switches it almost immediately after the
  patch loads and it calls the new version on every subsequent iteration.

The livepatch module dumps both stacks to the kernel log right after enabling
the patch, so you can see exactly what is blocking the transition.

**Why stack checking is necessary:**

Livepatch works by inserting an ftrace hook at the **entry point** of the old
function.  Once the hook is in place, any *new* call to that function is
redirected to the replacement.  But a task that is already *inside* the old
function -- mid-execution, with the old function's stack frame still live -- is
a different story.  If the kernel switched that task's view of the function
while it was still running the old body, the task would eventually execute a
`ret` back into code that no longer matches its state, causing undefined
behaviour.  The kernel therefore inspects every task's call stack before
switching it: if the old function appears anywhere on the stack, that task stays
on the old version until it returns naturally.  Only then is it safe to redirect
it.

**How it works:**
1. `target.ko` exports `slow_func()` and spawns two kthreads.  kthread1 calls
   `slow_func()` (which sleeps for 15 s) in a tight loop with no gap.  kthread2
   calls `slow_func()` then sleeps 3 s outside it.
2. `livepatch_demo.ko` calls `klp_enable_patch()` to redirect `slow_func()` to
   `new_slow_func()`.  The kernel enters transition and marks
   `/sys/kernel/livepatch/livepatch_demo/transition` as `1`.
3. In `module_init`, the livepatch module iterates every task with
   `for_each_process`, captures its stack via `stack_trace_save_tsk()`, and
   prints whether `slow_func` is present.  Tasks with `slow_func` on the stack
   are marked `[BLOCKED]`; demo kthreads without it are marked `[SWITCHED]`.
4. kthread2 is switched almost immediately (it is in the 3 s gap outside
   `slow_func` most of the time) and begins printing `[NEW]`.  kthread1 is
   permanently blocked -- `transition` stays `1` indefinitely.

> **Note:** Requires `CONFIG_LIVEPATCH=y`, `CONFIG_FUNCTION_TRACER=y`,
> `CONFIG_HAVE_RELIABLE_STACKTRACE=y`, and `CONFIG_KPROBES=y`.

## Prerequisites

- arm64 Linux kernel 6.18 with matching headers and livepatch support
- `target.ko` must be loaded before `livepatch_demo.ko`
- Cross-compiler if building off-target: set `CROSS=aarch64-linux-gnu-`

## Build and Load

```sh
cd livepatch_stack_demo

# Build both modules
make

# Step 1: load the target module
cd target
sudo insmod ./target.ko
```

Expected kernel log:

```
target: module init
target: kthread1 started
target: [OLD] slow_func enter (task: demo_kthread1)
target: kthread2 started
target: kthread2 about to call slow_func
target: [OLD] slow_func enter (task: demo_kthread2)
```

```sh
# Step 2: load the livepatch module (while kthread1 is still sleeping)
cd ../livepatch
sudo insmod ./livepatch_demo.ko
```

Expected kernel log:

```
livepatch_demo: module init
livepatch_demo: patch enabled, transition started
livepatch_demo: --- per-task stack snapshot ---
livepatch_demo: [BLOCKED]  demo_kthread1 (pid 123) -- slow_func on stack, cannot switch yet
livepatch_demo:   [0] slow_func+0x.../target [target]
livepatch_demo:   [1] kthread1_fn+0x.../target [target]
livepatch_demo:   [2] kthread+0x...
livepatch_demo:   [3] ret_from_fork+0x...
livepatch_demo: [SWITCHED] demo_kthread2 (pid 124) -- slow_func not on stack, switched immediately
livepatch_demo: --- end of stack snapshot ---
```

Check that the transition is permanently stuck:

```sh
cat /sys/kernel/livepatch/livepatch_demo/transition   # 1 (stays 1 indefinitely)
```

## Demo

Use two terminals so you can watch the kernel log live while running commands.

**Terminal 1** -- follow the kernel log continuously:

```sh
sudo dmesg -w
```

**Terminal 2** -- load modules and observe the transition:

```sh
# Load the target module
cd target && sudo insmod ./target.ko

# Load the livepatch while kthread1 is sleeping inside slow_func
cd ../livepatch && sudo insmod ./livepatch_demo.ko
```

Terminal 1 will immediately show the stack snapshot -- kthread1 blocked,
kthread2 switched -- and then kthread2 will start printing `[NEW] slow_func`
while kthread1 continues to print `[OLD] slow_func` forever.

Check transition status in Terminal 2:

```sh
cat /sys/kernel/livepatch/livepatch_demo/transition   # 1 -- stuck, kthread1 never clears
```

## Unload and Restore

```sh
# Disable the patch via sysfs
echo 0 | sudo tee /sys/kernel/livepatch/livepatch_demo/enabled

# Then unload both modules (livepatch first, target last)
sudo rmmod livepatch_demo
sudo rmmod target
```

Expected kernel log:

```
livepatch_demo: module exit
target: kthread1 stopped
target: kthread2 stopped
target: module exit
```

## Make Targets

```
make               Build both target and livepatch modules
make clean         Remove kernel build artifacts from both subdirs
```

Per-subdir targets (run inside `target/` or `livepatch/`):

```
make               Build the module
make clean         Remove kernel build artifacts
make load          Build and load the module
make unload        Remove the module
make logs          Show matching kernel log messages
```
