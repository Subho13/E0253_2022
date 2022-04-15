#include <linux/kernel.h>
#include <linux/signal_types.h>
#include <linux/sched/signal.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
// #include <linux/user.h>
#include <linux/namei.h>
#include <linux/swap.h>

#include "sigballoon.h"

int isProcessRegisteredForBallooning = 0;
struct task_struct *processRegisteredForBallooning;

int my_int_len(int n) {
    int len = 0;
    while( n ) {
        len++;
        n /= 10;
    }
    return len;
}

void my_itoa(int n, char *buff) {
    int len = my_int_len(n);
    int i;
	buff[len] = '\0';
    for (i = len - 1; i >= 0; i--) {
        buff[i] = '0' + (n % 10);
        n /= 10;
    }
}

void create_directory_if_not_exists(const char *dirName) {
	struct dentry *dentry;
	struct path path_check, path_create;
	int dir_not_exists;

	const char *pathname = "/ballooning";

	if (kern_path(dirName, LOOKUP_FOLLOW, &path_check)) {
		dentry = kern_path_create(AT_FDCWD, dirName, &path_create, LOOKUP_DIRECTORY);
		vfs_mkdir(path_create.dentry->d_inode, dentry, 0);
		done_path_create(&path_create, dentry);
	}
}

void my_mkswap(struct file *swapFile) {
	loff_t size = 10, offset = 1024;
	char zero[2];
	zero[0] = (char)1;
	zero[1] = (char)255;
	kernel_write(swapFile, zero, 1, &offset);
	offset = 1024 + 4;
	kernel_write(swapFile, zero + 1, 1, &offset);
	offset = PAGE_SIZE - size;
	kernel_write(swapFile, "SWAPSPACE2", 10, &offset);
}

extern int swapon_kernel(char* specialfile, int swap_flags);

asmlinkage long __x64_sys_balloon(void) {
	// Register the process with its task_struct
	isProcessRegisteredForBallooning = 1;
	processRegisteredForBallooning = current;
	printk("Received system call [sys_balloon] from process (pid: %d)\n", processRegisteredForBallooning->pid);

	// Create file to be used as swap
	// reference: fallocate.c, fallocate system call in fs/open.c
	char swapfile[64] = "/ballooning/swap_\0";
    char pidString[8];
    my_itoa((int)(processRegisteredForBallooning->pid), pidString);
    strcat(swapfile, pidString);

	int mode = 0666;
	int flags = O_RDWR | O_CREAT;
	loff_t offset, len;
	offset = 0;
	len = SWAP_FILE_SIZE;

	struct file *swapFile;


	const char *pathname = "/ballooning";

	create_directory_if_not_exists(pathname);

	swapFile = filp_open(swapfile, flags, mode);
	vfs_fallocate(swapFile, 0, offset, len);
	filp_close(swapFile, current->files);

	return 0L;
}
