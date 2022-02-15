#include <linux/kernel.h>
#include <linux/signal_types.h>
#include <linux/sched/signal.h>
#include <linux/sched.h>

extern int vm_swappiness;

#define SIGBALLOON 42
#define SWAP_OFF 0

int isProcessRegisteredForBallooning = 0;
struct task_struct *processRegisteredForBallooning;

asmlinkage long __x64_sys_balloon(void) {
	// Register the process with its task_struct
	isProcessRegisteredForBallooning = 1;
	processRegisteredForBallooning = current;
	// Disable paging
	vm_swappiness = SWAP_OFF;
	printk("Received system call from process (pid: %d)\n", processRegisteredForBallooning->pid);

	return 0L;
}

