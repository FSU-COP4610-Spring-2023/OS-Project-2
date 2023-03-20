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
        int groupSize;
};

struct seat{
	int empty, clean;
	struct customer current_customer;
	struct list_head list;
};

struct table{
	struct seat mySeats[8];
};

struct bar{
	struct list_head queue;
	struct table myTables[4];
	enum Status status;
        int currentOccupancy;
};

struct waiter{
	int currentTableNum;
	struct table currentTable;
	struct task_struct *kthread;
        struct mutex mutex;
};

extern int (*STUB_initialize_bar) (void);
extern int (*STUB_customer_arrival) (int, int);
extern int (*STUB_close_bar) (void);
extern long (*STUB_test_call) (int);

int my_initialize_bar (void){
    printk(KERN_NOTICE "%s: initializing bar", __FUNCTION__);
    open_bar.status = IDLE;
    open_bar.currentOccupancy = 0;
    barWaiter.currentTableNum = 1;
    barWaiter.currentTable = open_bar.myTables[0];
    return 0;
}

int my_customer_arrival (int number_of_customers, int type){
    struct list_head *newList;
    struct customer *newCustomer;
    int random;
    printk(KERN_NOTICE "%s: number of customers is %d and type is %d", __FUNCTION__, number_of_customers, type);

    if (number_of_customers < 1 || number_of_customers > 8){ // number of customers in group not valid
        return 1;
    }

    if (type < 0 || type > 4){ // type (class year) not valid
        return 1;
    }

    INIT_LIST_HEAD(newList);
    for (i = 0; i < number_of_customers; i++){
        newCustomer = kmalloc(sizeof(struct customer), GFP_KERNEL); // Using kmalloc to dynamically allocate space for>
        get_random_bytes(&random, sizeof(int));
        newCustomer->group_id = random % 1000000; // Add size of group to customer struct
        newCustomer->type = type; // Add type (class year) to customer struct
        list_add_tail(&newCustomer->list, newList); // Add to end of queue (first param new entry, current queue head)
        newCustomer->groupSize = number_of_customers;
    }

    list_add_tail(newList, &open_bar.queue);
    return 0;
}

int my_close_bar (void) {
    printk(KERN_NOTICE "%s: closing bar", __FUNCTION__);
    return 0;
}

long my_test_call (int test_int){
    printk(KERN_ALERT "%s: your int is %d", __FUNCTION__, test_int);
    return test_int;
}

// Waiter functions START

void waiter(void);

void waiterInit(void){
    barWaiter.currentTableNum = 0;
    barWaiter.currentTable = open_bar.myTables[0];
    mutex_init(&barWaiter.mutex);
    barWaiter.kthread = kthread_run(waiter_thread, NULL, "waiter");
}

void waiterMoveToNext(void){ // Sets up circular scan motion
    open_bar.status = MOVING;
    mdelay(2000); // 2 second delay
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
    // error checking if queue is empty
    
    if (list_empty(&open_bar.queue) == 1) // if waiting queue is not empty
    {
	struct list_head *pos, *q; // For deletion at the end

        struct list_head *chosenList;
        struct customer *currentCustomer;
//        firstList = list_first_entry(&open_bar.queue, struct list_head, list); // Grabs first group
//        currentCustomer = list_first_entry(firstList, struct customer, list); // Grabs first customer in group

        struct list_head *currentGroup; // Used to traverse all groups
        struct list_head *currentCustomerList; // Used to traverse all customers in list
        struct list_head *dummy;
        int availableSeats; // # of seats that are clean and empty
        availableSeats = 0;
        for(i = 0; i < 8; i++){
            if (barWaiter.currentTable.mySeats[i].empty == 1 &&
                     barWaiter.currentTable.mySeats[i].clean == 1)
                availableSeats++;
        }
        list_for_each_safe(currentGroup, dummy, &(open_bar.queue)){
            currentCustomer = list_first_entry(currentGroup, struct customer, list);
            if (currentCustomer->groupSize > availableSeats){
                continue; // If not enough seats available, next group
            }else{
                list_for_each_safe(currentCustomerList, dummy, currentGroup){
                    if (barWaiter.currentTable.mySeats[i].empty == 1 &&
                          barWaiter.currentTable.mySeats[i].clean == 1)
                    { // if current seat is empty and clean
                        msleep(1000); // 1 second delay
                        barWaiter.currentTable.mySeats[i].empty = 0; // seat no longer empty
                        barWaiter.currentTable.mySeats[i].clean = 0; // seat no longer clean
                        barWaiter.currentTable.mySeats[i].current_customer = *currentCustomer; // sets new customer to seat
                        ktime_get_real_ts64(&currentTime); // gets current time
                        currentCustomer->time_entered = currentTime; // updates time_entered with that time
                        currentCustomer = list_next_entry(currentCustomer, list); // moves on to next customer in group
                    }
                    i++;
                }
               chosenList = currentGroup;
            }
        }
	// Deletes all elements in first spot of waiting queue
	list_for_each_safe(pos, q, chosenList){
	    struct customer *entry = list_entry(pos, struct customer, list);
	    if (entry == NULL)
		break;
	    list_del(pos); // Remove from linked list but don't delete
        }

    }

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

    msg=kmalloc(sizeof(char) * 50, __GFP_RECLAIM);
    snprintf(msg, 50, "Testing that the proc is working correctly\n");

    len=strlen(msg);
    temp=len;
}

// main waiter thread control function
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

    STUB_initialize_bar = my_initialize_bar;
    STUB_customer_arrival = my_customer_arrival;
    STUB_close_bar = my_close_bar;
    STUB_test_call = my_test_call;

//    create_new_proc_entry();
    return 0;
}

static void bar_exit(void){
    kthread_stop(barWaiter.kthread); 
    printk(KERN_ALERT "Exiting");
    kfree(msg);
    STUB_initialize_bar = NULL;
    STUB_customer_arrival = NULL;
    STUB_close_bar = NULL;
    STUB_test_call = NULL;
//    remove_proc_entry("majorsbar", NULL);
}

module_init(bar_init);
module_exit(bar_exit);