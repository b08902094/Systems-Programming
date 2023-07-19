#ifndef THREADTOOL
#define THREADTOOL
#include <setjmp.h>
#include <sys/signal.h>
#include <setjmp.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>

#define THREAD_MAX 16  // maximum number of threads created
#define BUF_SIZE 512
struct tcb {
    int id;  // the thread id
    jmp_buf environment;  // where the scheduler should jump to
    int arg;  // argument to the function
    int fd;  // file descriptor for the thread
    char buf[BUF_SIZE];  // buffer for the thread
    int i, x, y;  // declare the variables you wish to keep between switches
    int fifofd; //fifo
    char *fifonm;
    char bufff[BUF_SIZE];
};
extern int timeslice;
extern jmp_buf sched_buf;
extern struct tcb *ready_queue[THREAD_MAX], *waiting_queue[THREAD_MAX];
// char functionname[25];
extern struct tcb *Temp;
/*
 * rq_size: size of the ready queue
 * rq_current: current thread in the ready queue
 * wq_size: size of the waiting queue
 */
extern int rq_size, rq_current, wq_size;
/*
* base_mask: blocks both SIGTSTP and SIGALRM
* tstp_mask: blocks only SIGTSTP
* alrm_mask: blocks only SIGALRM
*/
extern sigset_t base_mask, tstp_mask, alrm_mask;
/*
 * Use this to access the running thread.
 */
#define RUNNING (ready_queue[rq_current])

void sighandler(int signo);
void scheduler();

#define thread_create(func, id, arg) {\
    func(id, arg);\
}

#define thread_setup(id, arg) {\
    Temp = (struct tcb *)malloc(sizeof(struct tcb));\
    Temp->id = id;\
    Temp->arg = arg;\
    sprintf(Temp->bufff, "%d_%s", Temp->id, __func__);\
    Temp->fifonm = Temp->bufff;\
    mkfifo(Temp->bufff, 0666);\
    Temp->fifofd = open(Temp->bufff, O_RDONLY | O_NONBLOCK);\
    ready_queue[rq_current] = Temp;\
    rq_current += 1;\
    rq_size += 1;\
    RUNNING = Temp;\
    if (setjmp(Temp->environment)==0){\
        return;\
    }\
}

#define thread_exit() {\
    remove(RUNNING->bufff);\
    longjmp(sched_buf, 3);   \
}

#define thread_yield() {\
    if(setjmp(RUNNING->environment) == 0){      \
        sigpending(&base_mask);                      \
        if(sigismember(&base_mask, SIGTSTP))         \
            sigprocmask(SIG_UNBLOCK, &tstp_mask, NULL); \
        if(sigismember(&base_mask, SIGALRM))         \
            sigprocmask(SIG_UNBLOCK, &alrm_mask, NULL); \
    }                                                       \
}\

#define async_read(count)({\
    if (setjmp(RUNNING->environment)==0)\
        longjmp(sched_buf, 2);\
    else{\
        read(RUNNING->fifofd, RUNNING->buf, count);\
    }\
})\

#endif // THREADTOOL
