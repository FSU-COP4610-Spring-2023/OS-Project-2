#include <linux/init.h>
#include <linux/module.h>
#include <linux/timekeeping.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>


char* msg;
int len, temp; // for read_proc usage
int i, j; // for-loop counters
int currentCustomers;
int currentTable;
struct timespec64 currentTime;
static const struct proc_ops po;
struct bar open_bar;
enum Status { OFFLINE, IDLE, LOADING, CLEANING, MOVING };


struct customer{
	struct timespec64 time_entered;
	int g_id;
	int type;
};

struct seat{
	int empty, clean;
	struct customer current_customer;
	struct list_head list;
};

struct table{
	struct seat myseats[8];
	int emptyseats;
};

struct bar{
        int full;
	struct list_head queue;
	struct table mytables[4];
	enum Status s;
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

void waiter(void){
    while(open_bar.s == OFFLINE){
    }
    ktime_get_real_ts64(&currentTime);
    while(open_bar.s != OFFLINE){
        for(i = 0; i < 4; i++){
            for(j = 0; j < 8; j++){
                // Here is where the waiter logic will be implemented
            } 
        }
    }
}

static int bar_init(void){
	printk(KERN_ALERT "Starting");
	// init bar
	open_bar.full = 0; // changed from false to 0 and added int member full since bools do not exist in c
        INIT_LIST_HEAD(&open_bar.queue);
        open_bar.s = OFFLINE;
	for (i = 0; i < 4; i++){
	        for (j = 0; j < 8; j++){
		                open_bar.mytables[i].myseats[j].empty = 1; // Changed both from true to 1 to represent true
		                open_bar.mytables[i].myseats[j].clean = 1;
	        }
	}
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
