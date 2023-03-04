#include <linux/init.h>
#include <linux/module.h>
#include <linux/timekeeping.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

char* msg;
int accesses;

struct customer{
	time_t time_entered;
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

static int bar_init(void){
	printk(KERN_ALERT "Starting");
	accesses=0;
	// create new proc entry
	return 0;
}



static void bar_exit(void){
	printk(KERN_ALERT "Exiting");
	kfree(msg);
	// remove proc entry
}

module_init(bar_init);
module_exit(bar_exit);
