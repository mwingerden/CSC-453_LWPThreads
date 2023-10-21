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
static tid_t current_thread = NO_THREAD;  // Initialize to an invalid value
static scheduler current_scheduler = NULL;

static void lwp_schedule();

extern tid_t lwp_create(lwpfun fun, void *arg) {
    int i;
    tid_t new_thread_id = NO_THREAD;

    for (i = 1; i < MAX_THREADS; i++) {
        if (threads[i].status == LWP_TERMINATED) {
            new_thread_id = i;
            break;
        }
    }

    if (new_thread_id == NO_THREAD) {
        fprintf(stderr, "Maximum number of threads reached.\n");
        return NO_THREAD;
    }

    unsigned long *stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap");
        return NO_THREAD;
    }

    struct threadinfo_st *new_thread = &threads[new_thread_id];
    new_thread->tid = new_thread_id;
    new_thread->stack = stack;
    new_thread->status = LWP_READY;
    new_thread->exited = 0;
    new_thread->lib_one = NULL;
    new_thread->lib_two = NULL;
    new_thread->sched_one = NULL;
    new_thread->sched_two = NULL;

    tid_t old_thread = current_thread;

    current_thread = new_thread_id;
    lwp_schedule();

    if (new_thread->status == LWP_READY) {
        current_scheduler->admit(new_thread);
    }

    new_thread->state.rdi = (unsigned long)arg;
    new_thread->state.rbp = (unsigned long)fun;

    return new_thread_id;
}

extern void lwp_exit(int status) {
    struct threadinfo_st *current = &threads[current_thread];
    current->exited = (thread)(unsigned long)(int)status;

    current->status = LWP_TERMINATED;

    tid_t old_thread = current_thread;

    current_thread = current_scheduler->next()->tid;
    lwp_schedule();

    if (threads[old_thread].status == LWP_READY) {
        current_scheduler->admit(&threads[old_thread]);
    }
}

extern tid_t lwp_gettid(void) {
    return current_thread;
}

extern void lwp_yield(void) {
    lwp_schedule();
}

static void lwp_schedule() {
    tid_t old_thread = current_thread;
    current_thread = current_scheduler->next()->tid;

    swap_rfiles(&threads[old_thread].state, &threads[current_thread].state);

    if (threads[old_thread].status == LWP_READY) {
        current_scheduler->admit(&threads[old_thread]);
    }
}

extern void lwp_start(void) {
    if (current_scheduler == NULL) {
        fprintf(stderr, "No scheduler set. Use lwp_set_scheduler() to set a scheduler.\n");
        return;
    }

    struct threadinfo_st *initial_thread = &threads[0];
    current_thread = initial_thread->tid;
    current_scheduler->admit(initial_thread);
    lwp_schedule();
}

extern tid_t lwp_wait(int *status) {
    int i;
    tid_t thread_id = NO_THREAD;

    for (i = 1; i < MAX_THREADS; i++) {
        if (threads[i].status == LWP_TERMINATED) {
            thread_id = i;
            if (status != NULL) {
                *status = (int)(unsigned long)threads[i].exited;
            }
            munmap(threads[i].stack, STACK_SIZE); // Deallocate the stack
            break;
        }
    }

    if (thread_id == NO_THREAD && current_scheduler->qlen() > 1) {
        struct threadinfo_st *current = &threads[current_thread];
        current->status = LWP_BLOCKED;
        lwp_schedule();
        thread_id = current_thread;

        if (status != NULL) {
            *status = (int)(unsigned long)threads[thread_id].exited;
        }
    }

    return thread_id;
}

extern void lwp_set_scheduler(scheduler fun) {
    current_scheduler = fun;
}

extern scheduler lwp_get_scheduler(void) {
    return current_scheduler;
}

extern thread tid2thread(tid_t tid) {
    if (tid >= 0 && tid < MAX_THREADS) {
        return &threads[tid];
    }
    return NULL;
}
