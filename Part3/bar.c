#include <linux/init.h>
#include <linux/module.h>
#include <linux/timekeeping.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

char* msg;
int len, temp, accesses;
struct timespec64 currentTime;

struct customer{
	struct timespec64 time_entered;
	int g_id;
	int type;
};

struct seat{
	bool empty, clean;
	struct customer current_customer;
};

struct table{
	struct seat mytable[8];
	int emptyseats;
};

struct bar{
	struct customer queue[32];
	struct table mybar[4];
};

MODULE_LICENSE("Dual BSD/GPL");

static ssize_t read_proc(struct file *filp, char* buf, size_t count, loff_t *offp){
    if (count > temp){
        count = temp;
    }
    temp -= count;
    copy_to_user(buf, msg, count);
    if (count == 0){
        temp = len;
    }
    return count;
}

static const struct proc_ops po = {
  .proc_read = read_proc,
};

void create_new_proc_entry(void){
  proc_create("majorsbar", 0, NULL, &po);

  msg=kmalloc(sizeof(char) * 50, FL_RECLAIM);
  snprintf(msg, 50, "Testing that the proc is working correctly\n");

  len=strlen(msg);
  temp=len;
}

static int bar_init(void){
	printk(KERN_ALERT "Starting");
	accesses=0;
	create_new_proc_entry();
	return 0;
}



static void bar_exit(void){
	printk(KERN_ALERT "Exiting");
	kfree(msg);
	remove_proc_entry("majorsbar", NULL);
}

module_init(bar_init);
module_exit(bar_exit);
