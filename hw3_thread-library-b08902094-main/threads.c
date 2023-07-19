#include "threadtools.h"

void fibonacci(int id, int arg) {
    // sprintf(functionname, "fibonacci");
    thread_setup(id, arg);
    // RUNNING = Temp;
        for (RUNNING->i = 1; ; RUNNING->i++) {
            if (RUNNING->i <= 2)
                RUNNING->x = RUNNING->y = 1;
            else {
                //We don't need to save tmp, so it's safe to declare it here. 
                int tmp = RUNNING->y;  
                RUNNING->y = RUNNING->x + RUNNING->y;
                RUNNING->x = tmp;
            }
            printf("%d %d\n", RUNNING->id, RUNNING->y);
            sleep(1);

            if (RUNNING->i == RUNNING->arg) {
                // printf("Before Exittt Fibo%s\n", RUNNING->fifonm);
                thread_exit();
            }
            else {
                thread_yield();
            }
        }
}

void collatz(int id, int arg) {
    // TODO
    // sprintf(functionname, "collatz");
    thread_setup(id, arg);
        // RUNNING = Temp;
        RUNNING->x = RUNNING->arg;
        for (RUNNING->i = 1; ;RUNNING->i++){
            if (RUNNING->x % 2 == 0){ //if N is even divide it by 2
                RUNNING->x = RUNNING->x/2;
            }else if (RUNNING->x%2 == 1){
                RUNNING->x = RUNNING->x*3+1;
            }
            printf("%d %d\n", RUNNING->id, RUNNING->x);
            sleep(1);
            if (RUNNING->x == 1){
                thread_exit();
            } else if(RUNNING->x > 1){
                thread_yield();
            }
        }
}

void max_subarray(int id, int arg) {
    // TODO
    // char sub_arg[5];
    // sprintf(functionname, "max_subarray");
    thread_setup(id, arg);
    RUNNING-> y = 0; //max
    int temp = 0;
    for (RUNNING->i = 1; ; RUNNING->i++){
        async_read(5);
        temp = atoi(RUNNING->buf);
        RUNNING->x += temp;
        if (RUNNING->y < RUNNING->x){
            RUNNING->y = RUNNING -> x;
        }
        if (RUNNING -> x < 0){
            RUNNING -> x = 0;
        }
        printf("%d %d\n", RUNNING->id, RUNNING->y);
        sleep(1);
        if (RUNNING->i == RUNNING->arg){
            thread_exit();
        } else {
            thread_yield();
        }
    }
}
