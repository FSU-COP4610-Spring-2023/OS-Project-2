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
struct waiter barWaiter;
enum Status { OFFLINE, IDLE, LOADING, CLEANING, MOVING };
enum Type { FRESHMAN, SOPHOMORE, JUNIOR, SENIOR, PROFESSOR };

struct customer{
	struct timespec64 time_entered;
	int g_id;
	enum Type type;
        struct list_head list;
};

struct seat{
	int empty, clean;
	struct customer current_customer;
	struct list_head list;
};

struct table{
	struct seat mySeats[8];
	int emptySeats;
	int cleanSeats;
};

struct bar{
	struct list_head queue;
	struct table myTables[4];
	enum Status status;
};

struct waiter{
	int currentTableNum;
	struct table currentTable;
};

// Waiter functions START

void waiterInit(void){
    barWaiter.currentTableNum = 0;
    barWaiter.currentTable = open_bar.myTables[0];
}

void waiterMoveToNext(void){ // Sets up circular scan motion
    barWaiter.currentTableNum = (barWaiter.currentTableNum + 1) % 4;
    barWaiter.currentTable = open_bar.myTables[barWaiter.currentTableNum];
}

void waiterClean(void){ // Cleans current table
    struct timespec64 initialTime;
    int i;
    ktime_get_real_ts64(&initialTime);
    for(i = 0; i < 8; i++){
        barWaiter.currentTable.mySeats[i].clean = 1;
    }
    ktime_get_real_ts64(&currentTime);
    while(currentTime.tv_sec - initialTime.tv_sec < 10){
        ktime_get_real_ts64(&currentTime);
    }
}

void waiterRemove(void){ // Removes finished customers from current table
    int i;
    long long int timeElapsed;
    struct customer currentCustomer;
    for(i = 0; i < 8; i++){
        currentCustomer = barWaiter.currentTable.mySeats[i].current_customer;
        ktime_get_real_ts64(&currentTime);
        timeElapsed = currentTime.tv_sec - currentCustomer.time_entered.tv_sec;
        switch (currentCustomer.type){
          case FRESHMAN:
              if (timeElapsed >= 5)
                  barWaiter.currentTable.mySeats[i].empty = 1;
              break;
          case SOPHOMORE:
              if (timeElapsed >= 10)
                  barWaiter.currentTable.mySeats[i].empty = 1;
              break;
          case JUNIOR:
              if (timeElapsed >= 15)
                  barWaiter.currentTable.mySeats[i].empty = 1;
              break;
          case SENIOR:
              if (timeElapsed >= 20)
                  barWaiter.currentTable.mySeats[i].empty = 1;
              break;
          case PROFESSOR:
              if (timeElapsed >= 25)
                 barWaiter.currentTable.mySeats[i].empty = 1;
              break;
          default:
            // Do nothing
        }
    }
}

void waiterAdd(void){
    // Introduce use of linked list here since
    // customers will need to be grabbed from the queue
}

// Waiter functions END

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
    while(open_bar.status == OFFLINE){
    }
    while(open_bar.status != OFFLINE){
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
    INIT_LIST_HEAD(&open_bar.queue);
    open_bar.status = OFFLINE;
    waiterInit();
    for (i = 0; i < 4; i++){
        for (j = 0; j < 8; j++){
	    open_bar.myTables[i].mySeats[j].empty = 1;
	    open_bar.myTables[i].mySeats[j].clean = 1;
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
