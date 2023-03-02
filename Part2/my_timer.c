#include <linux/init.h>
#include <linux/module.h>
#include <linux/timekeeping.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

MODULE_LICENSE("Dual BSD/GPL");

char* msg;
char* nanoseconds;
int len, temp, accesses;
struct timespec64 timeElapsed;
struct timespec64 ts;
struct timespec64 initialTime;

void nanosecondsToChar(long int nsecs){
    const int MAX_DIGITS = 9;
    int digits = 0;
    int zeroes;
    long int origSecs = nsecs;
    long int tempSecs = nsecs;
    int i, j; // counters for two for loops
    kfree(nanoseconds);
    nanoseconds = kmalloc(sizeof(char) * 10, FL_RECLAIM);
    while(tempSecs != 0){
        tempSecs /= 10;
        digits++;
    }
    zeroes = MAX_DIGITS - digits;
    for(i = 0; i < zeroes; i++){
        nanoseconds[i] = '0';
        j++;
    }
    for(i = MAX_DIGITS - 1; i > zeroes - 1; i--){
        int dig = origSecs % 10;
        origSecs /= 10;
        nanoseconds[i] = dig + '0';
    }
    printk(KERN_ALERT "Final format of nanoseconds is %s from %lu", nanoseconds, nsecs);
}

void calculateElapsedTime(struct timespec64* x, struct timespec64* y){
    long long int secs = x->tv_sec - y->tv_sec;
    long int nsecs = x->tv_nsec - y->tv_nsec;
    if (nsecs < 0){
        secs--;
        nsecs += 1000000000;
    }

    timeElapsed.tv_sec = secs;
    timeElapsed.tv_nsec = nsecs;
    printk(KERN_ALERT "timeElapsed is %llu.%lu", timeElapsed.tv_sec, timeElapsed.tv_nsec);
    nanosecondsToChar(timeElapsed.tv_nsec);
}

void read_time(void){
    ktime_get_real_ts64(&ts);
    snprintf(msg, 120, "Current time: %llu.%lu                             \n", ts.tv_sec, ts.tv_nsec);
    if(accesses == 0){
        initialTime.tv_sec = ts.tv_sec;
        initialTime.tv_nsec = ts.tv_nsec;
    }else if (accesses > 1){
        calculateElapsedTime(&ts, &initialTime);
        snprintf(msg, 120, "Current time: %llu.%lu\nElapsed time: %llu.%s",
                 ts.tv_sec, ts.tv_nsec, timeElapsed.tv_sec, nanoseconds);
    }
}

static ssize_t read_proc(struct file *filp, char* buf, size_t count, loff_t *offp) 
{
    read_time();
    accesses++;
    if(count > temp){
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
    ktime_get_real_ts64(&ts);

    proc_create("timer",0,NULL, &po);

    msg=kmalloc(sizeof(char) * 120, FL_RECLAIM);

    ktime_get_real_ts64(&ts);
    snprintf(msg, 120, "Current time: %llu.%lu                             \n", ts.tv_sec, ts.tv_nsec);
    // Initialize msg buffer with an initial message here or
    // it will not print later for an unknown reason

    len=strlen(msg);
    temp=len;
}

static int timer_init(void){
    printk(KERN_ALERT "Starting");
    accesses = 0;
    create_new_proc_entry();
    return 0;
}

static void timer_exit(void){
    printk(KERN_ALERT "Exiting");
    kfree(msg);
    remove_proc_entry("timer",NULL);
}


module_init(timer_init);
module_exit(timer_exit);
