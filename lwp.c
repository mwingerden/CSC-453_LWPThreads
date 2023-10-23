#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "lwp.h"

#define MAX_THREADS 32
#define STACK_SIZE (8 * 1024 * 1024)

#define LWP_READY       1
#define LWP_BLOCKED     2
#define LWP_TERMINATED  3

static struct threadinfo_st threads[MAX_THREADS];
static tid_t currentThread = NO_THREAD;  // Initialize to an invalid value
static scheduler currentScheduler = NULL;

static void lwp_schedule();

extern tid_t lwp_create(lwpfun fun, void *arg) {
    int i;
    tid_t newThreadId = NO_THREAD;

    for (i = 1; i < MAX_THREADS; i++) {
        if (threads[i].status == LWP_TERMINATED) {
            newThreadId = i;
            break;
        }
    }

    if (newThreadId == NO_THREAD) {
        fprintf(stderr, "Maximum number of threads reached.\n");
        return NO_THREAD;
    }

    unsigned long *stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap");
        return NO_THREAD;
    }

    struct threadinfo_st *newThread = &threads[newThreadId];
    newThread->tid = newThreadId;
    newThread->stack = stack;
    newThread->status = LWP_READY;
    newThread->exited = 0;
    newThread->lib_one = NULL;
    newThread->lib_two = NULL;
    newThread->sched_one = NULL;
    newThread->sched_two = NULL;

    tid_t old_thread = currentThread;

    currentThread = newThreadId;
    lwp_schedule();

    if (newThread->status == LWP_READY) {
        currentScheduler->admit(newThread);
    }

    newThread->state.rdi = (unsigned long)arg;
    newThread->state.rbp = (unsigned long)fun;

    return newThreadId;
}

extern void lwp_exit(int status) {
    struct threadinfo_st *current = &threads[currentThread];
    current->exited = (thread)(unsigned long)(int)status;

    current->status = LWP_TERMINATED;

    tid_t oldThread = currentThread;

    currentThread = currentScheduler->next()->tid;
    lwp_schedule();

    if (threads[oldThread].status == LWP_READY) {
        currentScheduler->admit(&threads[oldThread]);
    }

    // Deallocate the thread's stack
    munmap(threads[oldThread].stack, STACK_SIZE);
}

extern tid_t lwp_gettid(void) {
    return currentThread;
}

extern void lwp_yield(void) {
    lwp_schedule();
}

static void lwp_schedule() {
    tid_t oldThread = currentThread;
    currentThread = currentScheduler->next()->tid;

    swap_rfiles(&threads[oldThread].state, &threads[currentThread].state);

    if (threads[oldThread].status == LWP_READY) {
        currentScheduler->admit(&threads[oldThread]);
    }
}

extern void lwp_start(void) {
    if (currentScheduler == NULL) {
        fprintf(stderr, "No scheduler set. Use lwp_set_scheduler() to set a scheduler.\n");
        return;
    }

    struct threadinfo_st *initialThread = &threads[0];
    currentThread = initialThread->tid;
    currentScheduler->admit(initialThread);
    lwp_schedule();
}

extern tid_t lwp_wait(int *status) {
    int i;
    tid_t threadId = NO_THREAD;

    for (i = 1; i < MAX_THREADS; i++) {
        if (threads[i].status == LWP_TERMINATED) {
            threadId = i;
            if (status != NULL) {
                *status = (int)(unsigned long)threads[i].exited;
            }
            munmap(threads[i].stack, STACK_SIZE); // Deallocate the stack
            break;
        }
    }

    if (threadId == NO_THREAD && currentScheduler->qlen() > 1) {
        struct threadinfo_st *current = &threads[currentThread];
        current->status = LWP_BLOCKED;
        lwp_schedule();
        threadId = currentThread;

        if (status != NULL) {
            *status = (int)(unsigned long)threads[threadId].exited;
        }
    }

    return threadId;
}

extern void lwp_set_scheduler(scheduler fun) {
    currentScheduler = fun;
}

extern scheduler lwp_get_scheduler(void) {
    return currentScheduler;
}

extern thread tid2thread(tid_t tid) {
    if (tid >= 0 && tid < MAX_THREADS) {
        return &threads[tid];
    }
    return NULL;
}
