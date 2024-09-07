#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"
#include <list.h>
#define PROCESS_MAGIC 0xcd6abf4b

/* Similar with ptr_thread */
#define ptr_process(ptr) ((struct process *)(pg_round_down(ptr)))

struct process {
	struct thread thread;
	struct file *(*fd_list)[64];
	int exist_status;
	struct list child_list;
	struct list_elem child_elem;
	/* Sema up when parent process wait this process */
	struct semaphore parent_waited;
	/* Sema up when this process set exist status */
	struct semaphore exist_status_setted;
	unsigned magic; /* Detects stack overflow. */
};

tid_t process_create_initd(const char *file_name);
void process_init_in_thread_init(struct process *new);
void process_init_of_initial_thread(void);
tid_t process_fork(const char *name, struct intr_frame *if_);
int process_exec(void *f_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(struct thread *next);

struct process *process_current(void);

#endif /* userprog/process.h */
