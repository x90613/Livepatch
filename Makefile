DEMO_BIN := out/kill_demo

all: demo
	$(MAKE) -C function_pointer
	$(MAKE) -C livepatch
	$(MAKE) -C livepatch_stack_demo

demo:
	mkdir -p out
	gcc -Wall -Wextra -O2 kill_demo.c -o $(DEMO_BIN)

clean:
	$(MAKE) -C function_pointer clean
	$(MAKE) -C livepatch clean
	$(MAKE) -C livepatch_stack_demo clean
	rm -rf out

.PHONY: all demo clean
