# OS-Project-2
Group members: Tristan Hale, Paolo Torres, Jonathan Guzman

Division of Labor:
Part 1: Jonathan Guzman

Part 2: Jonathan Guzman

Part 3:
    Step 1 - Jonathan Guzman, Paolo Torres, Tristan Hale
    Step 2 - Jonathan Guzman, Paolo Torres
    Step 3 - Jonathan Guzman
    Step 4 - Jonathan Guzman
    
Files:
  OS-Project-2:
      Part 1:
          empty.rs - an empty rust source code file containing an empty main function
          empty.trace - a file generated using strace showing all system calls made by the empty.rs program
          Makefile - a makefile containing rules to compile the empty.rs file into an executable, and a clean function to remove the executable
          README - a file containing detailed instructions for how to build the rest of part1 using the makefile, strace, and rust commands
          part1 - project folder that will be used by the cargo command to build a binary out of part1/src/main.c, a source code file that is empty except for 4 system                     calls
      Part 2:
          my_timer.c - a source code file used to build the kernel module my_timer, which will create the proc entry /proc/timer on loading
          Makefile:
	              Use the make command to compile the my_timer.c file and create the kernel object needed for you to install the module
                Use make clean to remove all files except for my_timer.c and Makefile
      Part 3:
          barstool.c - source code file containing all code used to build the barstool kernel module and the /proc/majorsbar proc entry
          sys_call.c - file that creates system call wrappers for the custom system calls initialize_bar, customer_arrival, close_bar, and test_call. This file also                          creates pointers for those wrappers and exports them for use in barstool.c
          Makefile:
              Use the make command to compile the barstool.c and the sys_call.c file, creating the kernel object needed for you to install the module
              Use make clean to remove all files meeting the following pattern:' *.o *.ko *.mod.* Module.* modules.* *.mod .*.cmd '

Bugs:
    Minor issue. The barstool.c file features a mutex_lock_interruptible() around all waiter_thread activity as it is always handling either the queue or the bar  itself, which the proc must read from. The waiter activity is also set to not stop until all customers are served, tables are cleaned, and the bar has gone offline, so when the waiter has many things left to do it will hold the lock until the CPU sends the interrupt signal when you try to force it to shut down using 
sudo rmmod barstool.

Special Considerations:
    Scheduler was implemented using a CSCAN policy of going through the tables. When adding customers to the tables, the barWaiter looks through the queue in FIFO order to find any groups that could fit into the table it is currently on. This means that if the immediate next group is too large for a table with 6 spots occupied, but there is a group of 2 right behind it that could fit perfectly, the scheduler will take that group from the queue out of order and place it into the table.
Extra credit:
    Part 1 was done in rust for extra credit
