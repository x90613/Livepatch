DEMO_BIN := out/kill_demo

all: demo
	$(MAKE) -C function_pointer
	$(MAKE) -C livepatch

demo:
	mkdir -p out
	gcc -Wall -Wextra -O2 kill_demo.c -o $(DEMO_BIN)

clean:
	$(MAKE) -C function_pointer clean
	$(MAKE) -C livepatch clean
	rm -rf out

.PHONY: all demo clean
