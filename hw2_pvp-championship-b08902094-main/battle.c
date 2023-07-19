#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include "status.h"
#include <sys/wait.h>
#include <ctype.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

int check_input(char a[], int size){
    for (int i = 0; i < size; i++){
        if (isalpha(a[i])){
            return 1;
        }
    }
    return 0;
}
int checklog(Status *stanew, Status *staold){
	if (stanew->HP != staold->HP || (stanew->HP == staold->HP && stanew->battle_ended_flag != staold->battle_ended_flag))
		return 1;
	else return 0;
}

void signal_handler(int signum){
	return;
}

char *tournament[14][3]={
	{"B", "C"}, //A
	{"D", "E"}, //B
	{"F","15"}, //C
	{"G","H"}, //D
	{"I","J"}, //E
	{"K","L"}, //F
	{"1","2"}, //G
	{"3","4"}, //H
	{"5","6"}, //I
	{"7","8"}, //J
	{"M","11"}, //K
	{"N","14"}, //L
	{"9","10"}, //M
	{"12","13"}, //N
};
char *logpurpose[14][3]={
	{"B", "C"}, //A
	{"D", "E"}, //B
	{"F","14"}, //C
	{"G","H"}, //D
	{"I","J"}, //E
	{"K","L"}, //F
	{"0","1"}, //G
	{"2","3"}, //H
	{"4","5"}, //I
	{"6","7"}, //J
	{"M","10"}, //K
	{"N","13"}, //L
	{"8","9"}, //M
	{"11","12"}, //N
};

Attribute attr_game[14] = {
	FIRE, //A
	GRASS, //B
	WATER, //C
	WATER, //D
	FIRE, //E
	FIRE, //F
	FIRE, //G
	GRASS, //H
	WATER, //I
	GRASS, //J
	GRASS, //K
	GRASS, //L
	FIRE, //M
	WATER, //N
};
// ---------------------A B C D E F G H I J K L M N
char parentmap[14] = {'O', 'A', 'A', 'B', 'B', 'C', 'D', 'D', 'E', 'E', 'F', 'F', 'K', 'L'};

void child_func(char id, int ppid){
	char logName[20];
	sprintf(logName, "log_battle%c.txt", id);
	int ffd = open(logName, O_WRONLY | O_CREAT | O_TRUNC, 0777);
	FILE *battle_log = fdopen(ffd, "w");
	if (!battle_log) {
		ERR_EXIT("fopen error ");
    }
	int index;
	int pid;
	int pfd1[2][2];
	int cpid[2];
	int winner = -1;
	char *temp;
	Attribute at_attr = attr_game[id-'A']; //current attribute
	int ATK_new[2];
	int target_pid[2];
	char *target_id[2];
	int flag;
	Status * stat[2];
	char buf[2][1024];
	//:::::::::::::::init mode:::::::::::::::
	for (int i = 0 ; i < 2 ; i++){
		pipe(pfd1[i]);
		pid = fork();
		if (pid == 0){ // child
			dup2(pfd1[i][0], 0);
			dup2(pfd1[i][1], 1);
			close(pfd1[i][0]);
			close(pfd1[i][1]);
			int int_ppid = getppid();
			char ppid[10];
			sprintf(ppid, "%d", int_ppid);
			index = id - 'A';
			if (atoi(tournament[index][i]) > 0){ //player
				char tmp[4];
				sprintf(tmp, "%d", atoi(tournament[index][i])-1);
				execl("./player", "./player", tmp, ppid, NULL);
			}
			else{ //game
				execl("./battle", "./battle", tournament[index][i], ppid, NULL);
			}
			exit(1);
		}
		else{
			//obtain PSSM from child to start battle
			cpid[i] = pid;

			// fprintf(battle_log, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", id, getpid(), tournament[index][i], cpid[i], stat[i]->real_player_id, stat[i]->HP, stat[i]->current_battle_id, stat[i]->battle_ended_flag);

			// fprintf(battle_log, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", id, ppid, tournament[index][i], cpid[i], stat[i]->real_player_id, stat[i]->HP, stat[i]->current_battle_id, stat[i]->battle_ended_flag);
			// fflush(battle_log);
		}
	}
	// ::::::::::::::::::battle::::::::::::::::::::::
	while(winner == -1){
		index = id - 'A';
		//read PSSM from the child
		read(pfd1[0][0], buf[0], sizeof(Status));
		stat[0] = (Status *) &(buf[0]);

		read(pfd1[1][0], buf[1], sizeof(Status));
		stat[1] = (Status *) &(buf[1]);

		//attr calc
		for (int i = 0; i < 2; i++){
			if (at_attr == stat[i]->attr){ //attr buffer
				ATK_new[i] = (stat[i]->ATK)*2;
			} else {
				ATK_new[i] = stat[i]->ATK;
			}
		}
		//the battle is NOT over
		if(stat[0]->battle_ended_flag == 0 && stat[1]->battle_ended_flag == 0){
			fprintf(battle_log, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", id, getpid(),logpurpose[index][0], cpid[0], stat[0]->real_player_id, stat[0]->HP, stat[0]->current_battle_id, stat[0]->battle_ended_flag);
			//fprintf(battle_log, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", id, ppid, tournament[index][0], cpid[0], stat[0]->real_player_id, stat[0]->HP, stat[0]->current_battle_id, stat[0]->battle_ended_flag);
			fflush(battle_log);
			fprintf(battle_log, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", id, getpid(),logpurpose[index][1], cpid[1], stat[1]->real_player_id, stat[1]->HP, stat[1]->current_battle_id, stat[1]->battle_ended_flag);
			//fprintf(battle_log, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", id, ppid, tournament[index][1], cpid[1], stat[1]->real_player_id, stat[1]->HP, stat[1]->current_battle_id, stat[1]->battle_ended_flag);
			fflush(battle_log);
			stat[0]->current_battle_id = id;
			stat[1]->current_battle_id = id;

			if (stat[0]->HP < stat[1]->HP) { // 0 attacks first
				flag = 0;
			}
			else if (stat[1]->HP < stat[0]->HP) { //1 attacks first
				flag = 1; 
			}
			else{
				if (stat[0]->real_player_id < stat[1]->real_player_id) { //0 attacks firsts
					flag = 0;
				}
				else if (stat[1]->real_player_id < stat[0]->real_player_id){ //1 attacks first
					flag = 1;
				}
			}
			if((stat[1-flag]->HP = stat[1-flag]->HP - (ATK_new[flag])) > 0){ //2nd player still alive
				if((stat[flag]->HP = stat[flag]->HP - (ATK_new[1-flag])) <= 0){ //first player dies
					stat[0]->battle_ended_flag = 1;
					stat[1]->battle_ended_flag = 1;
					winner = 1-flag;	
				}
			}
			else{ 
				stat[0]->battle_ended_flag = 1;
				stat[1]->battle_ended_flag = 1;
				winner = flag;
			}
			for (int i = 0 ; i < 2 ; i++){
				fprintf(battle_log, "%c,%d pipe to %s,%d %d,%d,%c,%d\n", id, getpid(), logpurpose[index][i], cpid[i], stat[i]->real_player_id, stat[i]->HP, stat[i]->current_battle_id, stat[i]->battle_ended_flag);
				fflush(battle_log);
			}
		}
		//send PSSM to child
		for (int i = 0 ; i < 2 ; i++){
			temp = (char * ) stat[i];
			write(pfd1[i][1], temp, sizeof(Status));
			// fprintf(battle_log, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", id, getpid(), tournament[index][i], cpid[i], stat[i]->real_player_id, stat[i]->HP, stat[i]->current_battle_id, stat[i]->battle_ended_flag);
			// fflush(battle_log);
		}
		usleep(500);
	}
	//:::::::::::::::::::passing::::::::::::::::

	char psbuf[100];
	Status * pstmp;
	if (ppid != 0){
		while(1){
			//Champion has been decided
			if(stat[winner]->HP <= 0 || (stat[winner]->current_battle_id == 'A' && stat[winner]->battle_ended_flag == 1)){
				printf("Champion is P%d\n",stat[winner]->real_player_id);
				break;
			}

			//receive from child
			read(pfd1[winner][0], psbuf, sizeof(Status));
			pstmp = (Status *) &psbuf;


			//send to parent
			write(1, psbuf, sizeof(Status));

			//receive from parent
			read(0, buf[winner], sizeof(Status));
			stat[winner] = (Status *) &(buf[winner]);
			if (checklog(pstmp, stat[winner])){
				fprintf(battle_log, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", id, getpid(), logpurpose[id-'A'][winner], cpid[winner], pstmp->real_player_id, pstmp->HP, pstmp->current_battle_id, pstmp->battle_ended_flag);
				fflush(battle_log);

				fprintf(battle_log, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", id, getpid(), parentmap[id-'A'], ppid, pstmp->real_player_id, pstmp->HP, pstmp->current_battle_id, pstmp->battle_ended_flag);
				fflush(battle_log);

				fprintf(battle_log, "%c,%d pipe from %c,%d %d,%d,%c,%d\n", id, getpid(), parentmap[id-'A'], ppid, stat[winner]->real_player_id, stat[winner]->HP, stat[winner]->current_battle_id, stat[winner]->battle_ended_flag);
				fflush(battle_log);
			}
			
			//send to child
			write(pfd1[winner][1], buf[winner], sizeof(Status));
			if (checklog(pstmp, stat[winner])){
				fprintf(battle_log, "%c,%d pipe to %s,%d %d,%d,%c,%d\n", id, getpid(), logpurpose[id-'A'][winner], cpid[winner], stat[winner]->real_player_id, stat[winner]->HP, stat[winner]->current_battle_id, stat[winner]->battle_ended_flag);
				fflush(battle_log);
			}
			usleep(500);
		}
	}
	fclose(battle_log);
	for (int i = 0 ; i < 2 ; i++){
		close(pfd1[i][0]);
		close(pfd1[i][1]);
		wait(NULL);
	}

}

int main (int argc, char *argv[]) {
	//TODO
	if (argc != 3){
		ERR_EXIT("input error");
	}
	// TODO: validity check later (A-N)
	char *id = argv[1];
	char *ppid = argv[2];
	child_func(*id, *ppid);
	return 0;
}
