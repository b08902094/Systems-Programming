#include "header.h"

movie_profile* movies[MAX_MOVIES];
unsigned int num_of_movies = 0;
unsigned int num_of_reqs = 0;
unsigned int num_of_thread = 8;

// Global request queue and pointer to front of queue
// TODO: critical section to protect the global resources
request* reqs[MAX_REQ];
int front = -1;

/* Note that the maximum number of processes per workstation user is 512. * 
 * We recommend that using about <256 threads is enough in this homework. */
pthread_t tid[MAX_CPU][MAX_THREAD]; //tids for multithread

#ifdef PROCESS
pid_t pid[MAX_CPU][MAX_THREAD]; //pids for multiprocess
#endif

//mutex
pthread_mutex_t lock; 

void initialize(FILE* fp);
request* read_request();
int pop();
void filter();

int pop(){
	front+=1;
	return front;
}

int main(int argc, char *argv[]){

	if(argc != 1){
#ifdef PROCESS
		fprintf(stderr,"usage: ./pserver\n");
#elif defined THREAD
		fprintf(stderr,"usage: ./tserver\n");
#endif
		exit(-1);
	}

	FILE *fp;

	if ((fp = fopen("./data/movies.txt","r")) == NULL){
		ERR_EXIT("fopen");
	}

	initialize(fp);
	assert(fp != NULL);
	fclose(fp);	
	filter();
	return 0;
}

/**=======================================
 * You don't need to modify following code *
 * But feel free if needed.                *
 =========================================**/

request* read_request(){
	int id;
	char buf1[MAX_LEN],buf2[MAX_LEN];
	char delim[2] = ",";

	char *keywords;
	char *token, *ref_pts;
	char *ptr;
	double ret,sum;

	scanf("%u %254s %254s",&id,buf1,buf2);
	keywords = malloc(sizeof(char)*strlen(buf1)+1);
	if(keywords == NULL){
		ERR_EXIT("malloc");
	}

	memcpy(keywords, buf1, strlen(buf1));
	keywords[strlen(buf1)] = '\0';

	double* profile = malloc(sizeof(double)*NUM_OF_GENRE);
	if(profile == NULL){
		ERR_EXIT("malloc");
	}
	sum = 0;
	ref_pts = strtok(buf2,delim);
	for(int i = 0;i < NUM_OF_GENRE;i++){
		ret = strtod(ref_pts, &ptr);
		profile[i] = ret;
		sum += ret*ret;
		ref_pts = strtok(NULL,delim);
	}

	// normalize
	sum = sqrt(sum);
	for(int i = 0;i < NUM_OF_GENRE; i++){
		if(sum == 0)
				profile[i] = 0;
		else
				profile[i] /= sum;
	}

	request* r = malloc(sizeof(request));
	if(r == NULL){
		ERR_EXIT("malloc");
	}

	r->id = id;
	r->keywords = keywords;
	r->profile = profile;
	return r;
}

/*=================initialize the dataset=================*/
void initialize(FILE* fp){

	char chunk[MAX_LEN] = {0};
	char *token,*ptr;
	double ret,sum;
	int cnt = 0;

	assert(fp != NULL);

	// first row
	if(fgets(chunk,sizeof(chunk),fp) == NULL){
		ERR_EXIT("fgets");
	}

	memset(movies,0,sizeof(movie_profile*)*MAX_MOVIES);	

	while(fgets(chunk,sizeof(chunk),fp) != NULL){
		
		assert(cnt < MAX_MOVIES);
		chunk[MAX_LEN-1] = '\0';

		const char delim1[2] = " "; 
		const char delim2[2] = "{";
		const char delim3[2] = ",";
		unsigned int movieId;
		movieId = atoi(strtok(chunk,delim1));

		// title
		token = strtok(NULL,delim2);
		char* title = malloc(sizeof(char)*strlen(token)+1);
		if(title == NULL){
			ERR_EXIT("malloc");
		}
		
		// title.strip()
		memcpy(title, token, strlen(token)-1);
	 	title[strlen(token)-1] = '\0';

		// genres
		double* profile = malloc(sizeof(double)*NUM_OF_GENRE);
		if(profile == NULL){
			ERR_EXIT("malloc");
		}

		sum = 0;
		token = strtok(NULL,delim3);
		for(int i = 0; i < NUM_OF_GENRE; i++){
			ret = strtod(token, &ptr);
			profile[i] = ret;
			sum += ret*ret;
			token = strtok(NULL,delim3);
		}

		// normalize
		sum = sqrt(sum);
		for(int i = 0; i < NUM_OF_GENRE; i++){
			if(sum == 0)
				profile[i] = 0;
			else
				profile[i] /= sum;
		}

		movie_profile* m = malloc(sizeof(movie_profile));
		if(m == NULL){
			ERR_EXIT("malloc");
		}

		m->movieId = movieId;
		m->title = title;
		m->profile = profile;

		movies[cnt++] = m;
	}
	num_of_movies = cnt;

	// request
	scanf("%d",&num_of_reqs);
	assert(num_of_reqs <= MAX_REQ);
	for(int i = 0; i < num_of_reqs; i++){
		reqs[i] = read_request();
	}
}
/*========================================================*/
/*my functions and structures*/

struct sort_str{
	char ** movies;
	double ** profile;
	double * pts;
	int size;
};

struct pass{
	int id;
	char **argmvs;
	double *argpts;
	int size;
	int j;
	int i;
	int chunk;
	int check;
};

void merge(struct pass *str, void *t){
	int min = 0;
	int * temp_cmp = t;
	int cmp = *temp_cmp;
	int max = str->size+cmp-1;
	int cur1 = 0;
	int cur2 = str->size;
	if(str->chunk<=num_of_thread){
		char * tmpmv[max+1];
		int current=0;
		double tmparr[max+1];
		while (cur1 < str->size && cur2 <= max){
			if (str->argpts[cur1] > str->argpts[cur2]){
				tmparr[current] = str->argpts[cur1];
				tmpmv[current] = str->argmvs[cur1];
				current++;
				cur1++;
			}
			else if (str->argpts[cur1] < str->argpts[cur2]){
				tmparr[current] = str->argpts[cur2];
				tmpmv[current] = str->argmvs[cur2];
				current++;
				cur2++;
			} else {
				if (strcmp(str->argmvs[cur1], str->argmvs[cur2])>0){
					tmparr[current] = str->argpts[cur2];
					tmpmv[current] = str->argmvs[cur2];
					current++;
					cur2++;
				}else{
					tmparr[current] = str->argpts[cur1];
					tmpmv[current] = str->argmvs[cur1];
					current++;
					cur1++;
				}
			}
		}
		while(cur1<str->size){
			tmparr[current] = str->argpts[cur1];
			tmpmv[current] = str->argmvs[cur1];
			current++;
			cur1++;
		}
		while(cur2<=max){
			tmparr[current] = str->argpts[cur2];
			tmpmv[current] = str->argmvs[cur2];
			current++;
			cur2++;
		}
		str->size = max+1;
		for (int i = 0; i <= max; i++){
			str->argpts[i] = tmparr[i];
			str->argmvs[i] = tmpmv[i];
		}
	}
	if (str->chunk>num_of_thread){ //this is the last thread
		FILE* f;
		char filename[25];
		sprintf(filename, "%dt.out", str->id);
		f = fopen(filename, "w");
		while (cur1 < str->size && cur2 <= max){
			if (str->argpts[cur1] > str->argpts[cur2]){
				fprintf(f, "%s\n", str->argmvs[cur1]);
				cur1++;
			}
			else if (str->argpts[cur1] < str->argpts[cur2]){
				fprintf(f, "%s\n", str->argmvs[cur2]);
				cur2++;
			} else {
				if (strcmp(str->argmvs[cur1], str->argmvs[cur2])>0){
					fprintf(f, "%s\n", str->argmvs[cur2]);
					cur2++;
				}else{
					fprintf(f, "%s\n", str->argmvs[cur1]);
					cur1++;
				}
			}
		}
		while(cur1<str->size){
			fprintf(f, "%s\n", str->argmvs[cur1]);
			cur1++;
		}
		while(cur2<=max){
			fprintf(f, "%s\n", str->argmvs[cur2]);
			cur2++;
		}
	}
}

void* passover(void *str){
	struct pass *tmp = str;
	sort(tmp->argmvs, tmp->argpts, tmp->size);
	void *t; //get the size of exited tmp
	if (tmp->check < num_of_thread){
		char filename[25];
		FILE *file;
		sprintf(filename, "%dt.out", tmp->id);
		file = fopen(filename, "w");
		for (int i = 0; i < tmp->size; i++){
			fprintf(file, "%s\n", tmp->argmvs[i]);
		}
		pthread_exit((void*)NULL);
	}else{
		while(tmp->i%tmp->chunk==0 && tmp->chunk <= num_of_thread){
			pthread_join(tid[tmp->j][tmp->i+(tmp->chunk/2)], &t);
			int  * x = t;
			tmp->chunk *= 2;
			merge(tmp, t);
		}
		pthread_exit((void*)&tmp->size);
	}
}

void filter(){
	struct sort_str *temp = malloc(sizeof(struct sort_str) * num_of_reqs);
	for (int j = 0; j < num_of_reqs; j++){
		int k = 0;
		temp[j].movies = malloc(sizeof(char *) * num_of_movies);
		temp[j].profile = malloc(sizeof(double *) * num_of_movies);
		
		if (strcmp(reqs[j]->keywords, "*")==0){
			for (int i = 0; i < num_of_movies; i++){
				temp[j].movies[i] = movies[i]->title;
				temp[j].profile[i] = movies[i]->profile;
			}
			temp[j].size = num_of_movies;

		}else{
			for (int i = 0; i < num_of_movies; i++){
				if (strstr(movies[i]->title, reqs[j]->keywords) != NULL){
					temp[j].movies[k] = movies[i]->title;
					temp[j].profile[k]= movies[i]->profile;
					k++;
				}
			}
			temp[j].size = k;
		}
		
		temp[j].pts = malloc(sizeof(double) * temp[j].size);
		double totalpts = 0;
		for (int i = 0; i < temp[j].size; i++){
			for (int l = 0; l < NUM_OF_GENRE; l++){
				totalpts += reqs[j]->profile[l] * temp[j].profile[i][l];
			}
			temp[j].pts[i] = totalpts;
			totalpts = 0;
		}
		free(temp[j].profile);
	}
	int total_thread = num_of_thread * num_of_reqs;
	int chunk;
	int check = 0;
	struct pass **arg = malloc(sizeof(struct pass *)*num_of_reqs);
	int id = 0;
	for (int j=0; j < num_of_reqs; j++){
		int chunksize = temp[j].size/num_of_thread;
		int remainder = temp[j].size%num_of_thread;
		int start = 0;
		int end = 0;
		check = temp[j].size;
		if (temp[j].size < num_of_thread && temp[j].size > 0){
			arg[j] = malloc(sizeof(struct pass));
			arg[j][0].argmvs = &(temp[j].movies[0]);
			arg[j][0].argpts = &(temp[j].pts[0]);
			arg[j][0].size = temp[j].size;
			arg[j][0].chunk = chunk;
			arg[j][0].i = 0;
			arg[j][0].j = j;
			arg[j][0].check = 1;
			arg[j][0].id = reqs[j]->id;
			pthread_create(&tid[j][0], NULL, passover, (void*)&arg[j][0]);
		}else if(temp[j].size >= num_of_thread){
			chunk = 2;
			arg[j] = malloc(sizeof(struct pass)*num_of_thread);
			for (int i = 0; i < num_of_thread; i++){
				end += chunksize;
				if (remainder){
					end++;
					remainder--;
				}
				arg[j][i].argmvs = &(temp[j].movies[start]);
				arg[j][i].argpts = &temp[j].pts[start];
				arg[j][i].size = end - start;
				arg[j][i].chunk = chunk;
				arg[j][i].i = i;
				arg[j][i].j = j;
				arg[j][i].check = check;
				arg[j][i].id = reqs[j]->id;
				start = end;
				pthread_create(&tid[j][i], NULL, passover, (void*)&arg[j][i]);
			}
		}
	}
	for (int j = 0; j < num_of_reqs ; j++){
		if (temp[j].size >= num_of_thread)
			pthread_join(tid[j][0], NULL);
	}
	free(temp);
	free(arg);
}
