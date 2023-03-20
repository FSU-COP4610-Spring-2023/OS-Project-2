#include <linux/init.h>
#include <linux/module.h>
#include <linux/timekeeping.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>


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
	int group_id;
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
	struct task_struct *kthread; // kthread declaration
        struct mutex mutex; // lock declaration
};

// Waiter functions START

int waiter_thread(void *data);

void waiterInit(void){
    barWaiter.currentTableNum = 0;
    barWaiter.currentTable = open_bar.myTables[0];
    mutex_init(&barWaiter.mutex);
    barWaiter.kthread = kthread_run(waiter_thread, NULL, "waiter");
}

void waiterMoveToNext(void){ // Sets up circular scan motion
    open_bar.status = MOVING;
    //mdelay(2000); // use kernal timer instead
    barWaiter.currentTableNum = (barWaiter.currentTableNum + 1) % 4;
    barWaiter.currentTable = open_bar.myTables[barWaiter.currentTableNum];
}

void waiterClean(void){ // Cleans current table
    // locking might need to be added
    open_bar.status = CLEANING;
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
    // free data allocated to each custoemr add check if seat is empty before removing customer
    // locking also needs to be added
    int i;
    long long int timeElapsed;
    struct customer currentCustomer;
    for(i = 0; i < 8; i++){
        currentCustomer = barWaiter.currentTable.mySeats[i].current_customer;
        ktime_get_real_ts64(&currentTime);
        timeElapsed = currentTime.tv_sec - currentCustomer.time_entered.tv_sec;
        switch (currentCustomer.type){
          case FRESHMAN:
              if (timeElapsed >= 5){
                  mdelay(1000);
                  barWaiter.currentTable.mySeats[i].empty = 1;
          }
              break;
          case SOPHOMORE:
              if (timeElapsed >= 10) {
                  mdelay(1000);
                  barWaiter.currentTable.mySeats[i].empty = 1;
          }
              break;
          case JUNIOR:
              if (timeElapsed >= 15){
                  mdelay(1000);
                  barWaiter.currentTable.mySeats[i].empty = 1;
          }
              break;
          case SENIOR:
              if (timeElapsed >= 20){
                  mdelay(1000);
                  barWaiter.currentTable.mySeats[i].empty = 1;
          }
              break;
          case PROFESSOR:
              if (timeElapsed >= 25){
                 mdelay(1000);
                 barWaiter.currentTable.mySeats[i].empty = 1;
          }
          default:
            // Do nothing
        }
    }
}

void waiterAdd(void){
    mutex_lock(&barWaiter.mutex); // only one thread can access at a time

    if (list_empty(&open_bar.queue)) { // if waiting queue empty, remove lock and return
        mutex_unlock(&barWaiter.mutex);
        return;
    }
  
    int i, groupSize;
    struct list_head *firstList;
    struct customer *currentCustomer;
    firstList = list_first_entry(&open_bar.queue, struct list_head, list); // Grabs first group
    currentCustomer = list_first_entry(firstList, struct customer, list); // Grabs first customer in group
    groupSize = currentCustomer->group_id;

    if (barWaiter.currentTable.cleanSeats < groupSize){ // if not enough clean seats for group, free lock and return
        mutex_unlock(&barWaiter.mutex);
	// free data 
        return;
    }

    for(i = 0; i < groupSize; i++)
    {
        if (barWaiter.currentTable.mySeats[i].empty == 1 && barWaiter.currentTable.mySeats[i].clean == 1) // if current seat is empty and clean
        {
            msleep(1000); // 1 second delay
            barWaiter.currentTable.mySeats[i].empty = 0; // seat no longer empty
            barWaiter.currentTable.mySeats[i].clean = 0; // seat no longer clean
            barWaiter.currentTable.mySeats[i].current_customer = *currentCustomer; // sets new customer to seat
            barWaiter.currentTable.emptySeats--; // removes empty seat from table
            barWaiter.currentTable.cleanSeats--; // removes clean seat from table
            ktime_get_real_ts64(&currentTime); // gets current time
            currentCustomer->time_entered = currentTime; // updates time_entered with that time
            currentCustomer = list_next_entry(currentCustomer, list);
        }
    }

    struct list_head *pos, *q;
    list_for_each_safe(pos, q, &firstList->list){
    	struct customer *entry = list_entry(pos, struct customer, list);
	if (entry == NULL)
	    break;
	list_del(pos);
	kfree(entry);
    }

    mutex_lock(&barWaiter.mutex); // free lock
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

// function that controls waiter kthread 
int waiter_thread(void *data)
{
    while (!kthread_should_stop()) {
	if (strcmp(open_bar.status, "IDLE") == 0){
	    waiterRemove(); // 1. Remove customers who are done drinking at current table.

            waiterAdd(); // 2. Add customers if possible (if clean and empty space).

            waiterClean(); // 3. Clean table if possible (if table empty).

            waiterMoveToNext(); // Move to next table
	}
    }

    return 0;
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

int customer_arrival(int number_of_customers, int type){
    int i;
    struct list_head *newList;

    if (number_of_customers < 1 || number_of_customers > 8){ // number of customers in group not valid
        return 1;
    }

    if (type < 0 || type > 4){ // type (class year) not valid
        return 1;
    }

    INIT_LIST_HEAD(newList);
    for (i = 0; i < number_of_customers; i++){
        struct customer *newCustomer = kmalloc(sizeof(struct customer), GFP_KERNEL); // Using kmalloc to dynamically allocate space for customers
        newCustomer->group_id = number_of_customers; // Add size of group to customer struct
        newCustomer->type = type; // Add type (class year) to customer struct
        list_add_tail(&newCustomer->list, newList); // Add to end of queue (first param new entry, current queue head)
    }

    list_add_tail(newList, &open_bar.queue);
    return 0;
}


static void bar_exit(void){
    kthread_stop(barWaiter.kthread); // stops the kthread
    mutex_destroy(&barWaiter.mutex); // destroy mutex lock
    printk(KERN_ALERT "Exiting");
    kfree(msg);
    remove_proc_entry("majorsbar", NULL);
}

module_init(bar_init);
module_exit(bar_exit);
