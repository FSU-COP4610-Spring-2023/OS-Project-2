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
#include <linux/random.h>
#include <linux/mutex.h>
#include <linux/kthread.h>

char* msg;
int len, temp; // for read_proc usage
int customersServiced;
int totalDirty;
int queueSize;
struct timespec64 openTime;
struct timespec64 currentTime;
static const struct proc_ops po;
struct bar open_bar;
struct list_head barQueue;
struct waiter barWaiter;
int offline;
enum Status { OFFLINE, IDLE, LOADING, CLEANING, MOVING };
enum Type { FRESHMAN, SOPHOMORE, JUNIOR, SENIOR, PROFESSOR };

static int read_p;
struct customer{
	struct timespec64 time_entered;
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
};

struct group{
	unsigned int group_id;
        struct customer *headCustomer;
        struct list_head groupList;
        int groupSize;
};

struct bar{
	struct table myTables[4];
        int currentOccupancy;
};

struct waiter{
	int currentTableNum;
	struct table *currentTable;
        enum Status status;
        struct task_struct *kthread;
        struct mutex mutex;
};

extern int (*STUB_initialize_bar) (void);
extern int (*STUB_customer_arrival) (int, int);
extern int (*STUB_close_bar) (void);
extern long (*STUB_test_call) (int);

void waiterInit(struct waiter *bw);

int waiter_thread(void *data);

int my_initialize_bar (void){
    if (barWaiter.status == OFFLINE){
        printk(KERN_NOTICE "%s: initializing bar", __FUNCTION__);
        barWaiter.status = IDLE;
        offline = 0;
        return 0;
    }
    return 1;
}


int my_customer_arrival (int number_of_customers, int type){
    struct group *currentGroup;
    int notUnique = 1;
    int i;
    struct group *newGroup = NULL;
    struct customer *newCustomer;
    struct customer *headCus;
    printk(KERN_NOTICE "%s: number of customers is %d and type is %d", __FUNCTION__, number_of_customers, type);

    if(offline == 1){ // If waiter is offline, take no new requests
        return 1;
    }

    if (number_of_customers < 1 || number_of_customers > 8){ // number of customers in group not valid
        return 1;
    }

    if (type < 0 || type > 4){ // type (class year) not valid
        return 1;
    }
    newGroup = kmalloc(sizeof(struct group), __GFP_RECLAIM);
    newGroup->headCustomer = kmalloc(sizeof(struct customer), __GFP_RECLAIM);
    headCus = newGroup->headCustomer;
    INIT_LIST_HEAD(&newGroup->groupList);
    INIT_LIST_HEAD(&headCus->list);

    newGroup->group_id = 0;
    newGroup->groupSize = number_of_customers;
    get_random_bytes(&newGroup->group_id, sizeof(int)-1);

    for (i = 0; i < number_of_customers; i++){
        newCustomer = kmalloc(sizeof(struct customer), __GFP_RECLAIM);
        newCustomer->type = type; // Add type (class year) to customer struct
        list_add_tail(&newCustomer->list, &headCus->list); // Add to end of queue (first param new entry, current queue head)
    }
    newGroup->headCustomer = headCus;

    list_add_tail(&newGroup->groupList, &barQueue);

    while(notUnique == 1){ // Generate new ID if it is not unique
        notUnique = 0;
        list_for_each_entry(currentGroup, &barQueue, groupList){
            if((currentGroup != newGroup) && (currentGroup->group_id == newGroup->group_id)){
                get_random_bytes(&newGroup->group_id, sizeof(int)-1);
                notUnique = 1;
            }
        }
    }
    return 0;
}

int my_close_bar (void) {
    printk(KERN_NOTICE "%s: closing bar", __FUNCTION__);
    barWaiter.status = OFFLINE;
    offline = 1;
    return 0;
}

long my_test_call (int test_int){
    printk(KERN_ALERT "%s: your int is %d", __FUNCTION__, test_int);
    return test_int;
}

// Waiter functions START

int waiter_thread(void *data);

void waiterInit(struct waiter *bw){
    int i, j;
    customersServiced = 0;
    open_bar.currentOccupancy = 0;
    barWaiter.currentTableNum = 0;
    barWaiter.currentTable = &open_bar.myTables[0];
    for (i = 0; i < 4; i++){
        for (j = 0; j < 8; j++){
            open_bar.myTables[i].mySeats[j].empty = 1;
            open_bar.myTables[i].mySeats[j].clean = 1;
        }
    }
    mutex_init(&barWaiter.mutex);
    bw->kthread = kthread_run(waiter_thread, NULL, "waiter");
}

void waiterMoveToNext(void){ // Sets up circular scan motion
    barWaiter.status = MOVING;
    mdelay(2000); // 2 second delay
    barWaiter.currentTableNum = (barWaiter.currentTableNum + 1) % 4;
    barWaiter.currentTable = &open_bar.myTables[barWaiter.currentTableNum];
}

void waiterClean(void){ // Cleans current table
    struct timespec64 initialTime;
    int i;
    int tableEmpty = 1;
    int amountDirty = 0;
    ktime_get_real_ts64(&initialTime);
    for(i = 0; i < 8; i++){
        if(barWaiter.currentTable->mySeats[i].empty == 0){
            tableEmpty = 0; // Table is NOT empty
            break;
        }
        if(barWaiter.currentTable->mySeats[i].clean == 0)
            amountDirty++; // Table is NOT clean
    }
    if(tableEmpty == 1 && (amountDirty > 3 || (offline == 1 && amountDirty > 0))){ 
        // Clean if table is empty and either # of dirty seats > 3 or bar is closing
        barWaiter.status = CLEANING;
        ktime_get_real_ts64(&currentTime);

        while(currentTime.tv_sec - initialTime.tv_sec < 10){
            ktime_get_real_ts64(&currentTime);
        }

        for(i = 0; i < 8; i++){
            if(barWaiter.currentTable->mySeats[i].clean == 0)
                totalDirty--;
            barWaiter.currentTable->mySeats[i].clean = 1;
        }
    }
}

void waiterRemove(void){ // Removes finished customers from current table
    int i;
    long long int timeElapsed;
    struct customer currentCustomer;
    for(i = 0; i < 8; i++){
        if(barWaiter.currentTable->mySeats[i].empty == 0){
            ktime_get_real_ts64(&currentTime);
            currentCustomer = barWaiter.currentTable->mySeats[i].current_customer;
            timeElapsed = currentTime.tv_sec - currentCustomer.time_entered.tv_sec;
            switch (currentCustomer.type){
                case FRESHMAN:
                    if (timeElapsed >= 5){
                        barWaiter.status = LOADING;
                        mdelay(1000);
                        barWaiter.currentTable->mySeats[i].empty = 1;
                        open_bar.currentOccupancy--;
                        customersServiced++;
                        totalDirty++;
                    }
                    break;
                case SOPHOMORE:
                    if (timeElapsed >= 10) {
                        barWaiter.status = LOADING;
                        mdelay(1000);
                        barWaiter.currentTable->mySeats[i].empty = 1;
                        open_bar.currentOccupancy--;
                        customersServiced++;
                        totalDirty++;
                    }
                     break;
                case JUNIOR:
                    if (timeElapsed >= 15){
                        barWaiter.status = LOADING;
                        mdelay(1000);
                        barWaiter.currentTable->mySeats[i].empty = 1;
                        open_bar.currentOccupancy--;
                        customersServiced++;
                        totalDirty++;
                    }
                    break;
                case SENIOR:
                     if (timeElapsed >= 20){
                         barWaiter.status = LOADING;
                         mdelay(1000);
                         barWaiter.currentTable->mySeats[i].empty = 1;
                         open_bar.currentOccupancy--;
                         customersServiced++;
                         totalDirty++;
                    }
                    break;
                case PROFESSOR:
                     if (timeElapsed >= 25){
                         barWaiter.status = LOADING;
                         mdelay(1000);
                         barWaiter.currentTable->mySeats[i].empty = 1;
                         open_bar.currentOccupancy--;
                         customersServiced++;
                         totalDirty++;
                   }
                    break;
              default:
                // Do nothing
            }
        }
    }
}

void waiterAdd(void){
    // error checking if queue is empty

    if (list_empty(&barQueue) == 0) // if waiting queue is not empty
    {
        int j;
        int i = 0;
        struct group *chosenGroup = NULL;
        struct customer *currentCustomer;

        struct list_head *queue; // Used to traverse all groups
        struct list_head *queue2;
        struct group *currentGroup; // Used to traverse all customers in list
        struct list_head *dummyGroup;
        struct list_head *dummyGroup2;
        struct list_head *dummyCustomer;
        struct list_head *dummyCustomer2;
        struct list_head *temp;
        struct list_head *temp2;
        int availableSeats; // # of seats that are clean and empty
        // Find first group that fits into the current table, if any
        if(list_empty(&barQueue) == 0){
            list_for_each_safe(queue, dummyGroup, &barQueue){
                currentGroup = list_entry(queue, struct group, groupList);
                if(queue == &barQueue)
                   break;
                availableSeats = 0;
                for(j = 0; j < 8; j++){
                    if (barWaiter.currentTable->mySeats[j].empty == 1 &&
                         barWaiter.currentTable->mySeats[j].clean == 1)
                     availableSeats++;
                }
                if (currentGroup->groupSize > availableSeats){
                    continue; // If not enough seats available, next group
                }else{
                    list_for_each_safe(temp, dummyCustomer, &currentGroup->headCustomer->list){ 
                        currentCustomer = list_entry(temp, struct customer, list);
                        if (barWaiter.currentTable->mySeats[i].empty == 1 &&
                              barWaiter.currentTable->mySeats[i].clean == 1)
                        { // if current seat is empty and clean
                            barWaiter.status = LOADING;
                            msleep(1000); // 1 second delay
                            barWaiter.currentTable->mySeats[i].empty = 0; // seat no longer empty
                            barWaiter.currentTable->mySeats[i].clean = 0; // seat no longer clean
                            ktime_get_real_ts64(&currentTime); // gets current time
                            currentCustomer->time_entered = currentTime; // updates time_entered with that time
                            barWaiter.currentTable->mySeats[i].current_customer.time_entered = currentCustomer->time_entered;
                            barWaiter.currentTable->mySeats[i].current_customer.type = currentCustomer->type;
                            open_bar.currentOccupancy++;
                            chosenGroup = currentGroup;
                        }
                        i++;
                    }
                   if (chosenGroup != NULL){
                       list_for_each_safe(queue2, dummyGroup2, &barQueue){
                           currentGroup = list_entry(queue2, struct group, groupList);
                           if (currentGroup == chosenGroup){
                               list_for_each_safe(temp2, dummyCustomer2, &chosenGroup->headCustomer->list){
                                  currentCustomer = list_entry(temp2, struct customer, list);
                                  list_del(temp2);
                                  kfree(currentCustomer);
                               }
                               list_del(queue2);
                               kfree(chosenGroup->headCustomer);
                               kfree(chosenGroup);
                               break;
                          }
                       }
                   }
                }
            }
        }
    }
}

int waiter_thread(void *data){
    while(!kthread_should_stop()){
        if(mutex_lock_interruptible(&barWaiter.mutex) == 0){
            if (offline != 1 || list_empty(&barQueue) == 0 || open_bar.currentOccupancy > 0 || totalDirty > 0){
                waiterRemove(); // 1. Remove customers who are done drinking at current table.

                waiterAdd(); // 2. Add customers if possible (if clean and empty space).

                waiterClean(); // 3. Clean table if possible (if table empty).

                waiterMoveToNext(); // Move to next table
            }
            if(offline == 1)
                barWaiter.status = OFFLINE;
            mutex_unlock(&barWaiter.mutex);
        }
    }
    return 0;
}


// Waiter functions END

MODULE_LICENSE("Dual BSD/GPL");

int print_bar(void){
    int count = 0;
    int i, j;
    int freshmen, sophomores, juniors, seniors, professors;
    int waitingGroups, waitingCustomers;
    struct customer *currentCustomer;
    struct group *currentGroup;
    struct list_head *temp;
    struct list_head *queue;
    struct seat currentSeat;
    char selected;

    char* waiterState;
    char* buf = kmalloc(sizeof(char) * 100, __GFP_RECLAIM);

    if (buf == NULL){
        printk(KERN_WARNING "print_bar");
        return -ENOMEM;
    }
    switch (barWaiter.status){
        case OFFLINE:
            waiterState = "OFFLINE";
            break;
        case IDLE:
            waiterState = "IDLE";
            break;
        case LOADING:
            waiterState = "LOADING";
            break;
        case CLEANING:
            waiterState = "CLEANING";
            break;
        case MOVING:
            waiterState = "MOVING";
            break;
    }
    ktime_get_real_ts64(&currentTime);
    strcpy(msg, "");
    sprintf(buf, "Waiter state: %s\n", waiterState);
    strcat(msg, buf);
    sprintf(buf, "Current table: %d\n", barWaiter.currentTableNum);
    strcat(msg, buf);
    sprintf(buf, "Elapsed time:  %d seconds\n", (int)(currentTime.tv_sec - openTime.tv_sec));
    strcat(msg, buf);
    sprintf(buf, "Current occupancy: %d customers\n", open_bar.currentOccupancy);
    strcat(msg, buf);
    sprintf(buf, "Bar Status: ");
    strcat(msg, buf);
    freshmen = sophomores = juniors = seniors = professors = 0;
    for(i = 0; i < 4; i++){
        for(j = 0; j < 8; j++){
            if(open_bar.myTables[i].mySeats[j].empty == 1)
                continue;
            currentCustomer = &(open_bar.myTables[i].mySeats[j].current_customer);
            switch (currentCustomer->type){
                 case FRESHMAN:
                     freshmen++;
                     break;
                 case SOPHOMORE:
                     sophomores++;
                     break;
                 case JUNIOR:
                     juniors++;
                     break;
                 case SENIOR:
                     seniors++;
                     break;
                 case PROFESSOR:
                     professors++;
                     break;
            }
        }
    }
    if(freshmen > 0) { sprintf(buf, "%d F ", freshmen); strcat(msg, buf); }
    if(sophomores > 0) { sprintf(buf, "%d O ", sophomores); strcat(msg, buf); }
    if(juniors > 0) { sprintf(buf, "%d J ", juniors); strcat(msg, buf); }
    if(seniors > 0) { sprintf(buf, "%d S ", seniors); strcat(msg, buf); }
    if(professors > 0) { sprintf(buf, "%d P", professors); strcat(msg, buf); }

    sprintf(buf, "\n"); strcat(msg, buf);

    waitingGroups = 0;
    waitingCustomers = 0;
    if(list_empty(&barQueue) == 0){
        list_for_each(queue, &barQueue){
            currentGroup = list_entry(queue, struct group, groupList);
            waitingGroups++;
            list_for_each(temp, &currentGroup->headCustomer->list){
                waitingCustomers++;
            }
        }
    }

    sprintf(buf, "Number of customers waiting: %d\nNumber of groups waiting: %d\n", waitingCustomers, waitingGroups);
    strcat(msg, buf);
    sprintf(buf, "Contents of queue:\n");
    strcat(msg, buf);
    
    if(list_empty(&barQueue) == 0){
        list_for_each(queue, &barQueue){
            currentGroup = list_entry(queue, struct group, groupList);
            if(currentGroup != NULL && list_empty(&currentGroup->headCustomer->list) == 0){
                list_for_each(temp, &currentGroup->headCustomer->list){
                    currentCustomer = list_entry(temp, struct customer, list);
                    switch (currentCustomer->type){
                         case FRESHMAN:
                             sprintf(buf, "F ");
                             break;
                         case SOPHOMORE:
                             sprintf(buf, "O ");
                             break;
                         case JUNIOR:
                             sprintf(buf, "J ");
                             break;
                         case SENIOR:
                             sprintf(buf, "S ");
                             break;
                         case PROFESSOR:
                             sprintf(buf, "P ");
                             break;
                     }
                    strcat(msg, buf);
                }
                sprintf(buf, "(group id:  %d)\n", currentGroup->group_id);
                strcat(msg, buf);
                count++;
            }
            if(count > 8){
                sprintf(buf, ".......\n\n");
                strcat(msg, buf);
                break;
            }
        }
    }
    sprintf(buf, "Number of customers serviced: %d\n\n\n", customersServiced);
    strcat(msg, buf);

    for(i = 3; i >= 0; i--){
        if(barWaiter.currentTableNum == i)
            selected = '*';
        else
            selected = ' ';
        sprintf(buf, "[%c] Table %d: ", selected, i + 1);
        strcat(msg, buf);
        for(j = 0; j < 8; j++){
            currentSeat = open_bar.myTables[i].mySeats[j];
            if(currentSeat.empty == 1){
                if(currentSeat.clean == 1)
                    sprintf(buf, "C ");
                else
                    sprintf(buf, "D ");
            }else if(currentSeat.current_customer.type == FRESHMAN){
                sprintf(buf, "F ");
            }else if(currentSeat.current_customer.type == SOPHOMORE){
                sprintf(buf, "O ");
            }else if(currentSeat.current_customer.type == JUNIOR){
                sprintf(buf, "J ");
            }else if(currentSeat.current_customer.type == SENIOR){
                sprintf(buf, "S ");
            }else if(currentSeat.current_customer.type == PROFESSOR){
                sprintf(buf, "P ");
            }
            strcat(msg, buf);
        }
        sprintf(buf, "\n");
        strcat(msg, buf);
    }
    return 0;
}

int open_proc(struct inode *sp_inode, struct file *sp_file){
    struct list_head *temp;
    queueSize = 0;
    list_for_each(temp, &barQueue){
        queueSize++;
    }
    msg = kmalloc(sizeof(char) * (500 + (queueSize * 40)), __GFP_RECLAIM);
    read_p = 1;
    if (msg == NULL) {
        printk(KERN_WARNING "open_proc");
        return -ENOMEM;
    }
    return print_bar();
}

static ssize_t read_proc(struct file *filp, char* buf, size_t count, loff_t *offp){
    int len = strlen(msg);

    read_p = !read_p;
    if (read_p){
        return 0;
    }
    copy_to_user(buf, msg, len);
    return len;
}

int release_proc(struct inode *sp_inode, struct file *sp_file) {
    kfree(msg);
    return 0;
}

static const struct proc_ops po = {
    .proc_open = open_proc,
    .proc_read = read_proc,
    .proc_release = release_proc,
};

static int bar_init(void){
    printk(KERN_ALERT "Starting");
    ktime_get_real_ts64(&openTime);
    // init bar
    barWaiter.status = OFFLINE;
    offline = 1;
    totalDirty = 0;
    INIT_LIST_HEAD(&barQueue);
    customersServiced = 0;
    waiterInit(&barWaiter);


    STUB_initialize_bar = my_initialize_bar;
    STUB_customer_arrival = my_customer_arrival;
    STUB_close_bar = my_close_bar;
    STUB_test_call = my_test_call;

    if(!proc_create("majorsbar", 0, NULL, &po)){
         printk(KERN_WARNING "bar_init\n");
         remove_proc_entry("majorsbar", NULL);
    }

    return 0;
}

static void bar_exit(void){
    printk(KERN_ALERT "Exiting");
    kthread_stop(barWaiter.kthread);
    mutex_destroy(&barWaiter.mutex);
    STUB_initialize_bar = NULL;
    STUB_customer_arrival = NULL;
    STUB_close_bar = NULL;
    STUB_test_call = NULL;
    remove_proc_entry("majorsbar", NULL);
}

module_init(bar_init);
module_exit(bar_exit);
