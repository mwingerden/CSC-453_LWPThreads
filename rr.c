#include "lwp.h"

thread first = NULL;
thread last = NULL;

void rr_admit(thread new) {
    if (!first && !last) {
        first = new;
        last = new;
        return;
    }

    last->next = new;
    new->prev = last;
    last = new;
}

void rr_remove(thread victim) {
    thread tmp = last;
    while (tmp && tmp->tid != victim->tid) {
        tmp = tmp->prev;
    }

    if (tmp == NULL || tmp->tid != victim->tid) {
        return;
    }

    if (tmp->prev) {
        tmp->prev->next = tmp->next;
    } else {
        first = tmp->next;
    }

    if (tmp->next) {
        tmp->next->prev = tmp->prev;
    } else {
        last = tmp->prev;
    }
}

thread rr_next(void) {
    thread tmp;

    if (!first) {
        return NULL;
    }

    tmp = first;
    first = first->next;
    if (first) {
        first->prev = NULL;
    } else {
        last = NULL;
    }

    tmp->next = NULL;
    tmp->prev = NULL;
    rr_admit(tmp);

    return tmp;
}