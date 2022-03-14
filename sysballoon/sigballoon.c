#include <linux/kernel.h>
#include <linux/signal_types.h>
#include <linux/sched/signal.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
// #include <linux/user.h>
#include <linux/namei.h>

#define SIGBALLOON 42

struct swap_header_v1 {
	uint32_t version;
	uint32_t last_page;
	uint32_t nr_badpages;
	char sws_uuid[16];
	char sws_volume[16];
	uint32_t padding[117];
	uint32_t badpages[1];
} FIX_ALIASING;
#define NWORDS 129
char bb_common_bufsiz1[];
#define hdr ((struct swap_header_v1*)bb_common_bufsiz1)

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
	len = 512 * 1024 * 1024 ; // 512 MB

	struct file *swapFile;

	// Not working
	// struct file* directory = filp_open("/ballooning", O_RDWR | O_DIRECTORY, 0);
	// filp_close(directory, current->files);
	// New approach
	struct dentry *dentry;
	struct path path1, path;

	const char *pathname = "/ballooning";
	int dir_not_exists = kern_path(pathname, LOOKUP_FOLLOW, &path1);

	if (dir_not_exists) {
		dentry = kern_path_create(AT_FDCWD, pathname, &path, LOOKUP_DIRECTORY);
		vfs_mkdir(path.dentry->d_inode, dentry, 0);
		done_path_create(&path, dentry);
	}

	swapFile = filp_open(swapfile, flags, mode);
	vfs_fallocate(swapFile, 0, offset, len);
	filp_close(swapFile, current->files);

	return 0L;
}
