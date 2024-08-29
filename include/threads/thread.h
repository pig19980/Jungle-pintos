#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status {
	THREAD_RUNNING, /* Running thread. */
	THREAD_READY,   /* Not running but ready to run. */
	THREAD_BLOCKED, /* Waiting for an event to trigger. */
	THREAD_DYING	/* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0	  /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63	 /* Highest priority. */

/* Max depth of nesting semaphore */
#define NESTING_DEPTH 8

/* Macro for 4BSD Scheduler */
typedef int32_t myfloat;
#define DEFAULT_NICE 0 /* Default nice. */
#define FRAC 16

#define I2F(n) ((n) << FRAC)
#define F2I(f) ((f) >> FRAC)

#define ADDFF(x, y) ((x) + (y))
#define SUBFF(x, y) ((x) - (y))

#define MULFF(x, y) ((myfloat)((((int64_t)(x)) * (y)) >> FRAC))
#define DIVFF(x, y) ((myfloat)((((int64_t)(x)) << FRAC) / (y)))

#define MULFN(x, n) ((x) * (n))
#define DIVFN(x, n) ((x) / (n))

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `status_elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread {
	/* Owned by thread.c. */
	tid_t tid;				   /* Thread identifier. */
	enum thread_status status; /* Thread state. */
	char name[16];			   /* Name (for debugging purposes). */
	int priority;			   /* Original priority. */
	int real_priority;		   /* Real priority */
	int64_t wake_tick;		   /* Value for check when this thread awake. */
	/* Value for check whick lock waiting.
	 * Use this Value to donate recursive. */
	struct lock *waiting_lock;
	/* List for locked by this thread.
	 * Use this list to calculate real priority. */
	struct list locking_list;

	/* Shared between thread.c and synch.c. */
	struct list_elem status_elem; /* Status list element. */
	struct list_elem thread_elem; /* All threads in process list element. */

	/* Value for 4BSD Scheduler.
	 * Thread nice value. Default is 0 and can changed by thread_get_nice. */
	int nice;
	/* Value for 4BSD Scheduler.
	 * It become bigger when this thread use cpu much recently. */
	myfloat recent_cpu;

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4; /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf; /* Information for switching. */
	unsigned magic;		  /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void do_iret(struct intr_frame *tf);

// For sleep machanism
void thread_sleep(int64_t);
bool sort_by_tick_ascending(const struct list_elem *, const struct list_elem *,
							void *);
void thread_wakeup(int64_t);

// For priority donate
bool sort_by_priority_descending(const struct list_elem *,
								 const struct list_elem *, void *);
int thread_priority_of(struct thread *);
void thread_donate_priority_to_holder(struct thread *);
int thread_max_priority_in_waiters(struct list *);
void thread_reset_real_priority(void);

// For 4BSD Scheduler
void calculate_all_priority(void);
void calculate_load_avg_and_recent_cpu(void);

#endif /* threads/thread.h */
