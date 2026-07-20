#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static void handle_usr1(int signal_number)
{
	(void)signal_number;
	write(STDOUT_FILENO, "SIGUSR1 received\n", 17);
}

int main(void)
{
	if (signal(SIGUSR1, handle_usr1) == SIG_ERR)
		return 1;

	printf("kill demo is running. PID: %ld\n", (long)getpid());
	printf("Send SIGTERM or SIGKILL from another terminal.\n");
	fflush(stdout);

	for (;;) {
		pause();
	}

	return 0;
}
