#include "threadtools.h"

/*
 * Print out the signal you received.
 * If SIGALRM is received, reset the alarm here.
 * This function should not return. Instead, call longjmp(sched_buf, 1).
 */
struct tcb *Temp;
struct tcb a;
struct tcb *tmp_obj = &a;

void sighandler(int signo) {
    // TODO
    if(signo == SIGALRM){
        printf("caught SIGALRM\n");
        alarm(timeslice);
    }else if(signo == SIGTSTP){
        printf("caught SIGTSTP\n");
    }
    sigprocmask(SIG_BLOCK, &base_mask, NULL);
    longjmp(sched_buf, 1);
}

/*
 * Prior to calling this function, both SIGTSTP and SIGALRM should be blocked.
 */
void scheduler() {
    int tmp = setjmp(sched_buf);
    int bytesav = 0;
    int forsize = wq_size;
    for (int i = 0; i<forsize; i++){
        ioctl(waiting_queue[i]->fifofd, FIONREAD, &bytesav);
        if(bytesav > 0){
            ready_queue[rq_size] = waiting_queue[i];
            rq_size+=1;
            wq_size-=1;
            waiting_queue[i] = tmp_obj;
        }
    }
    int updated_size = 0;
    for (int i = 0; i < forsize; i++){
        if (waiting_queue[i] != tmp_obj){
            waiting_queue[updated_size] = waiting_queue[i];
            updated_size+=1;
        }
    }
    if (tmp == 0){
        //fprintf(stderr, "%d", ready_queue[0]->fifofd);
        rq_current = 0;
        //fprintf(stderr, "tempppp %d", Temp->arg);
        longjmp(RUNNING->environment, 1);
    }
    if (tmp == 1){
        //execute the current thread
        //if it is already the last thread
        if (rq_current == rq_size - 1){
            rq_current = 0;
        }
        else{
            rq_current += 1;
        }
        longjmp(RUNNING->environment, 1);
    }
    if (tmp == 2){ //async_read
        //remove thread to the end of the waiting queue
        waiting_queue[wq_size]=ready_queue[rq_current];
        wq_size+=1;
        if (rq_current != rq_size-1){
            ready_queue[rq_current]= ready_queue[rq_size-1];
            rq_size-=1;
            longjmp(RUNNING->environment, 1);
            // printf("idk1\n");
        }else{
            rq_size-=1;
            if (rq_size > 0){
                rq_current = 0;
                longjmp(RUNNING->environment,1);
            }
            // printf("idk1\n");
        }

    }
    if (tmp == 3){
        free(RUNNING);
        if (rq_current != rq_size - 1){
            ready_queue[rq_current] = ready_queue[rq_size - 1];
            rq_size-=1;
            longjmp(RUNNING->environment, 1);
        }else{
            rq_size-=1;
            if (rq_size > 0){
                rq_current = 0;
                longjmp(RUNNING->environment,1);
            }
        }
    }
    if (rq_size == 0){
        if (wq_size != 0){
            forsize = wq_size;
            while(1){
                for (int i = 0; i<forsize; i++){
                    ioctl(waiting_queue[i]->fifofd, FIONREAD, &bytesav);
                    if(bytesav > 0){
                        ready_queue[rq_size] = waiting_queue[i];
                        rq_size+=1;
                        wq_size-=1;
                        waiting_queue[i] = tmp_obj;
                    }
                }
                int updated_size = 0;
                for (int i = 0; i < forsize; i++){
                    if (waiting_queue[i] != tmp_obj){
                        waiting_queue[updated_size] = waiting_queue[i];
                        updated_size+=1;
                    }
                }
                if (rq_size>0){
                    break;
                }
            }
            rq_current = 0;
            longjmp(RUNNING->environment, 1);
        }
        else{
            return;
        }
    }
}
