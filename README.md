# linux-kernel-hooking-demos

Two independent Linux kernel module demos, each exploring a different aspect
of syscall hooking and livepatching on arm64.

| Demo | What it shows |
|---|---|
| [`hook_kill_demo/`](hook_kill_demo/README.md) | Two ways to hook `kill(2)` and block `SIGKILL`: direct syscall table manipulation vs. the kernel livepatch mechanism. |
| [`livepatch_stack_demo/`](livepatch_stack_demo/README.md) | How livepatch uses per-task stack checking to decide when it's safe to switch an in-flight task to a patched function. |

Each demo is self-contained with its own `Makefile` and build instructions.
See the READMEs linked above for details.

> **Warning:** These modules modify kernel behaviour.  Use only in a
> disposable VM or test machine with a snapshot and console access.
