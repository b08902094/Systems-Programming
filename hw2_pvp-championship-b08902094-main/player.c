#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "status.h"

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)
//0, 1, 2, 3, 4, 5, 6, 
char parentmap[15] = {'G', 'G', 'H', 'H', 'I', 'I', 'J', 'J', 'M', 'M', 'K', 'N', 'N', 'L', 'C'};
// A, B, C, D, E, F, G, H, I, J, K, L, M, N
int agentmap[15] = {-1, 14, -1, 10, 13, -1, 8, 11, 9, 12, -1, -1, -1, -1};
// input format: ./player [player_id] [parent_pid]
int checklog(Status *stanew, Status *staold){
	if (stanew->HP != staold->HP && stanew->battle_ended_flag == 0)
		return 1;
	else return 0;
}
int main (int argc, char *argv[]) {
	//TODO
	char input[100];
	FILE *f = fopen("player_status.txt", "r");
	Status st;
	Status *status = &st;
	int fd[2];
	char readbuf[1024];
	char *writebuf;
	char logName[20];
	char agent_buf[2][100];
	char pathnameFIFO[25];
	int afterHP;
	int fdFIFO;
	int id = atoi(argv[1]);
	int ppid = atoi(argv[2]);
	char *tmp;
	int wait_flag = -1;
	int initialHP;
	char logbuf[2][1024];
	int fdlog;
	int checkr = -1;
	if (argc != 3) {
		ERR_EXIT("input error");
	}
	// create FIFO
	for (int i = 8; i < 15; i++){
		sprintf(pathnameFIFO, "player%d.fifo", i);
		mkfifo(pathnameFIFO, 0777);
	}
	//:::::::::: REAL PLAYER :::::::::::
	if (id > -1 && id <= 7){
		sprintf(logName, "log_player%d.txt", id);
		fdlog = open(logName, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0777);
		FILE *logf = fdopen(fdlog, "w");
		for (int i = 0; i <= id; i++)
			fgets(input, sizeof(input), f);
		status->real_player_id = id;
		tmp = strtok(input, " ");
		status->HP = atoi(tmp);
		initialHP = status->HP;
		tmp = strtok(NULL, " ");
		status->ATK = atoi(tmp);
		tmp = strtok(NULL, " ");
		if (strcmp(tmp, "FIRE")==0){
			status->attr = 0;
		}else if (strcmp(tmp, "GRASS")==0){
			status->attr = 1;
		}else if(strcmp(tmp, "WATER")==0){
			status->attr = 2;
		}
		tmp = strtok(NULL, " ");
		status->current_battle_id = *tmp;
		tmp = strtok(NULL, " ");
		status->battle_ended_flag = atoi(tmp);
		//create PSSM from the file
		while(1){
			Status *temps = status;
			writebuf = (char*) status;
			checkr = status->HP;
			//child sends PSSM to the parent
			write(1, writebuf, sizeof(Status));
			sprintf(logbuf[0], "%d,%d pipe to %c,%d %d,%d,%c,%d\n", id, getpid(), parentmap[id], ppid, status->real_player_id, status->HP, status->current_battle_id, status->battle_ended_flag);
			int change = status ->HP;
			//read PSSM from the parent
			read(0, readbuf, sizeof(Status));
			sprintf(logbuf[1], "%d,%d pipe from %c,%d %d,%d,%c,%d\n", id, getpid(), parentmap[id], ppid, status->real_player_id, status->HP, status->current_battle_id, status->battle_ended_flag);
			status = (Status*) &readbuf;
			if (status->HP != checkr && status->battle_ended_flag == 0){
				fprintf(logf, "%s", logbuf[0]);
				fflush(logf);
				fprintf(logf, "%s", logbuf[1]);
				fflush(logf);
			}
			if (status->battle_ended_flag == 1){
				fprintf(logf, "%s", logbuf[0]);
				fflush(logf);
				fprintf(logf, "%s", logbuf[1]);
				fflush(logf);
				if(status->current_battle_id == 'A'){ 
					break;
				}
				status->battle_ended_flag = 0;
				if (status->HP > 0){
					//wait_flag = 1;
					//half blood recov
					afterHP = status->HP;
					status->HP = status->HP + ((initialHP - status->HP)/2);
				}
				else if (status->HP <= 0){ 
					//full blood recover
					sprintf(pathnameFIFO, "player%d.fifo", agentmap[(status->current_battle_id) - 'A']);
					status->HP = initialHP;
					// mkfifo(pathnameFIFO, 0777);
					fdFIFO = open(pathnameFIFO, O_WRONLY);
					writebuf = (char *) status;
					//send fifo to agent 
					fprintf(logf, "%d,%d fifo to %d %d,%d\n", id, getpid(), agentmap[(status->current_battle_id) - 'A'], status->real_player_id, status->HP);
					fflush(logf);
					write(fdFIFO, writebuf, sizeof(Status));
					close(fdFIFO);
					break;
				}
			}
		}

		//	printf("%d %d %d %d %c %d\n", id, status->HP, status->ATK, status->attr, status->current_battle_id, status->battle_ended_flag);
		//	fflush(stdout);
		// 	fprintf(stderr, "player.c: %d %d %d %d %c %d\n", status->real_player_id, status->HP, status->ATK, status->attr, status->current_battle_id, status->battle_ended_flag);
	} else if (id >= 8 && id <= 14){

		sprintf(pathnameFIFO, "player%d.fifo", id);
		// mkfifo(pathnameFIFO, 0777);
		fdFIFO = open(pathnameFIFO, O_RDONLY);
		//read PSSM from parent
		read(fdFIFO, readbuf, sizeof(Status));
		status = (Status *)readbuf;
		close(fdFIFO);
		sprintf(logName, "log_player%d.txt", status ->real_player_id);
		fdlog = open(logName, O_WRONLY | O_APPEND);
		FILE *logf = fdopen(fdlog, "w");
		if (!logf){
			ERR_EXIT("log file cannot be open");
		}
		fprintf(logf, "%d,%d fifo from %d %d,%d\n", id, getpid(), status->real_player_id, status->real_player_id, status->HP);
		initialHP = status->HP;
		while(1){
			Status *temps = status;
			writebuf = (char *) status;
			checkr = status->HP;
			write(1, writebuf, sizeof(Status));
			sprintf(logbuf[0], "%d,%d pipe to %c,%d %d,%d,%c,%d\n", id, getpid(), parentmap[id], ppid, status->real_player_id, status->HP, status->current_battle_id, status->battle_ended_flag);
			//read PSSM from the parent
			read(0, readbuf, sizeof(Status));
			sprintf(logbuf[1], "%d,%d pipe from %c,%d %d,%d,%c,%d\n", id, getpid(), parentmap[id], ppid, status->real_player_id, status->HP, status->current_battle_id, status->battle_ended_flag);
			status = (Status*) &readbuf;
			if (status->HP != checkr && status->battle_ended_flag == 0){
				fprintf(logf, "%s", logbuf[0]);
				fflush(logf);
				fprintf(logf, "%s", logbuf[1]);
				fflush(logf);
			}
			if (status->battle_ended_flag == 1){
				fprintf(logf, "%s", logbuf[0]);
				fflush(logf);
				fprintf(logf, "%s", logbuf[1]);
				fflush(logf);
				if(status->current_battle_id == 'A'){ //currently doing root battle
					break;
				}
				status->battle_ended_flag = 0;
				if (status->HP > 0){
					// wait_flag = 1;
					// half recov
					afterHP = status->HP;
					status->HP = status->HP + ((initialHP - status->HP)/2);
				}
				else {
					break;
				}
			}
		}
		fclose(logf);
	} else {
		ERR_EXIT("input 1 out of range");
	}


	return 0;
}
