#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

#define OBJ_NUM 3

#define FOOD_INDEX 0
#define CONCERT_INDEX 1
#define ELECS_INDEX 2
#define RECORD_PATH "./bookingRecord"

//self-defined
#define MIN_ID 902001
#define MAX_ID 902020
#define MAX_CONN 20

static char* obj_names[OBJ_NUM] = {"Food", "Concert", "Electronics"};
//self defined
int r_cnt[MAX_CONN]; //count clients reading
int w_cnt[MAX_CONN]; //count clients writing 


typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    int id;
    int exit_handler;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";
const unsigned char IAC_IP[3] = "\xff\xf4";

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

//self-defined
int check_input(char a[], int size);

typedef struct {
    int id;          // 902001-902020
    int bookingState[OBJ_NUM]; // 1 means booked, 0 means not.
}record;

int handle_read(request* reqP) {
    /*  Return value:
     *      1: read successfully
     *      0: read EOF (client down)
     *     -1: read failed
     */
    int r;
    char buf[512];

    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
    char* p1 = strstr(buf, "\015\012");
    int newline_len = 2;
    if (p1 == NULL) {
       p1 = strstr(buf, "\012");
        if (p1 == NULL) {
            if (!strncmp(buf, IAC_IP, 2)) {
                // Client presses ctrl+C, regard as disconnection
                fprintf(stderr, "Client presses ctrl+C....\n");
            }
            return 0;
        }
    }
    size_t len = p1 - buf + 1;
    memmove(reqP->buf, buf, len);
    reqP->buf[len - 1] = '\0';
    reqP->buf_len = len-1;
    return 1;
}

int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512];
    int buf_len;
    // self defined 
    int sel_ret, set_ret;
    fd_set clientfds, readyfds;
    struct timeval timeout;
    struct flock lock;

    //open record
    file_fd = open("bookingRecord", O_RDWR);
    if (file_fd < 0) {
        fprintf(stderr, "open bookingRecord error");
        ERR_EXIT("open");
    }


    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);
    //set maxfd size
    maxfd = svr.listen_fd;
    //fd set setup
    FD_ZERO(&clientfds);
    FD_SET(svr.listen_fd, &clientfds);

    record booking_record;

    //wait for client
    while (1) {
        // TODO: Add IO multiplexing
        timeout.tv_sec = 0;
        timeout.tv_usec = 100;
        readyfds = clientfds;
        sel_ret = select(maxfd+1, &readyfds, NULL, NULL, &timeout);
        if (sel_ret == -1){ //error
            ERR_EXIT("select error");
        } else if (sel_ret == 0){ //no fd ready
            continue;
        }
        // Check new connection
        for (int i = 0; i <= maxfd; i++){
            set_ret = FD_ISSET(i, &readyfds);
            if (set_ret && i == svr.listen_fd){
                clilen = sizeof(cliaddr);
                conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
                if (conn_fd < 0) {
                    if (errno == EINTR || errno == EAGAIN) continue;  // try again
                    if (errno == ENFILE) {
                        (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                        continue;
                    }
                    ERR_EXIT("accept");
                }
                if (conn_fd > maxfd) maxfd = conn_fd;
                FD_SET(conn_fd, &clientfds);
                requestP[conn_fd].conn_fd = conn_fd;
                strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
                fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
                char *greeting = "Please enter your id (to check your booking state):\n";
                write(requestP[conn_fd].conn_fd, greeting, strlen(greeting));
                // break;
            } else if (set_ret && i != svr.listen_fd){
                conn_fd = i;
                int ret = handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
                fprintf(stderr, "ret = %d\n", ret);
                if (ret < 0) {
                    fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
                    continue;
                }
                else if (ret == 0){
                    //fprintf(stderr, "socket %d closed\n", requestP[conn_fd].conn_fd);
                    //wait for write = 1
                    if (requestP[conn_fd].wait_for_write == 1){
                        w_cnt[requestP[conn_fd].id - MIN_ID] = 0;
                        requestP[conn_fd].wait_for_write = 0;
                        lock.l_type = F_UNLCK; 
                        lock.l_start = sizeof(record) * requestP[conn_fd].id; //size of the offset = size of regRecord* id-> set up space for each id
                        lock.l_len = sizeof(record); //the range being locked
                        fcntl(file_fd, F_SETLK, &lock);

                    } else if(requestP[conn_fd].exit_handler == 1){
                        r_cnt[conn_fd]--;
                        requestP[conn_fd].exit_handler = 0;
                        if (r_cnt[requestP[conn_fd].id-MIN_ID] == 0){
                            lock.l_type = F_UNLCK; 
                            lock.l_start = sizeof(record) * requestP[conn_fd].id; //size of the offset = size of regRecord* id-> set up space for each id
                            lock.l_len = sizeof(record); //the range being locked
                            fcntl(file_fd, F_SETLK, &lock);
                        }
                    }
                    //exit_handler = 0
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                    FD_CLR(conn_fd, &clientfds);
                    continue;
                }

    // TODO: handle requests from clients

    char *errormsg = "[Error] Operation failed. Please try again.\n";
    char *right = "right\n";
    char *locked = "Locked.\n";
    char *over = "[Error] Sorry, but you cannot book more than 15 items in total.\n";
    char *under = "[Error] Sorry, but you cannot book less than 0 items.\n";
    //id range = 902001 - 902020
#ifdef READ_SERVER
        // I JUST WANT TO EXIT RIGHT AT HERE

        // fprintf(stderr, "%d", requestP[conn_fd].id);
            if (requestP[conn_fd].exit_handler == 1 ){ //if exit_handler = 1
                if (!strcmp(requestP[conn_fd].buf, "Exit")){
                    r_cnt[requestP[conn_fd].id-MIN_ID]--;
                    //unlock only if the read count == 0
                    if (r_cnt[requestP[conn_fd].id-MIN_ID] == 0){
                        lock.l_type = F_UNLCK; 
                        lock.l_start = sizeof(record) * requestP[conn_fd].id; //size of the offset = size of regRecord* id-> set up space for each id
                        lock.l_len = sizeof(record); //the range being locked
                        fcntl(file_fd, F_SETLK, &lock);

                    }
                    FD_CLR(conn_fd, &clientfds);
                    requestP[conn_fd].exit_handler = 0;
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                    continue;
                }
                
            } else{
                requestP[conn_fd].id = atoi(requestP[conn_fd].buf);

                if (check_input(requestP[conn_fd].buf, strlen(requestP[conn_fd].buf)) ||requestP[conn_fd].id < MIN_ID || requestP[conn_fd].id > MAX_ID){ //invalid id
                    write(requestP[conn_fd].conn_fd, errormsg, strlen(errormsg));
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                    FD_CLR(conn_fd, &clientfds);
                } else { //valid id
                    // write(requestP[conn_fd].conn_fd, right, strlen(right));
                    lock.l_type = F_RDLCK; //read lock
                    lock.l_whence = SEEK_SET; //move file pointer position to the beginning
                    lock.l_start = sizeof(record) * requestP[conn_fd].id; //size of the offset = size of regRecord* id-> set up space for each id
                    lock.l_len = sizeof(record); //the range being locked
                    //check if the file is being written by someone else
                    if(w_cnt[requestP[conn_fd].id-MIN_ID] > 0 || fcntl(file_fd, F_SETLK, &lock) == -1){
                        write(requestP[conn_fd].conn_fd, locked, strlen(locked));
                        close(requestP[conn_fd].conn_fd);
                        free_request(&requestP[conn_fd]);
                        FD_CLR(conn_fd, &clientfds);
                    } else { //the record is readable
                        // write(requestP[conn_fd].conn_fd, right, strlen(right));
                        r_cnt[requestP[conn_fd].id-MIN_ID]++; //reading
                        requestP[conn_fd].exit_handler = 1;
                        pread(file_fd, &booking_record, sizeof(record), sizeof(record)*(requestP[conn_fd].id-MIN_ID));
                        //idk how to print out the result in client
                        sprintf(requestP[conn_fd].buf, "Food: %d booked\nConcert: %d booked\nElectronics: %d booked\n\n(Type Exit to leave...)\n", booking_record.bookingState[FOOD_INDEX], booking_record.bookingState[CONCERT_INDEX], booking_record.bookingState[ELECS_INDEX]);
                        write(requestP[conn_fd].conn_fd, requestP[conn_fd].buf, strlen(requestP[conn_fd].buf));
                    }
                }
            }

        // fprintf(stderr, "%s", requestP[conn_fd].buf);
        // sprintf(buf,"%s : %s",accept_read_header,requestP[conn_fd].buf);
        // write(requestP[conn_fd].conn_fd, buf, strlen(buf));    
#elif defined WRITE_SERVER
        if (!requestP[conn_fd].wait_for_write){
            requestP[conn_fd].id = atoi(requestP[conn_fd].buf);
            if (requestP[conn_fd].id < MIN_ID || requestP[conn_fd].id > MAX_ID){
                write(requestP[conn_fd].conn_fd, errormsg, strlen(errormsg));
                close(requestP[conn_fd].conn_fd);
                free_request(&requestP[conn_fd]);
                FD_CLR(conn_fd, &clientfds);
            } else {
                lock.l_type = F_WRLCK; //write lock
                lock.l_whence = SEEK_SET; //move file pointer position to the beginning
                lock.l_start = sizeof(record) * requestP[conn_fd].id; //size of the offset = size of regRecord* id-> set up space for each id
                lock.l_len = sizeof(record); //the range being locked

                if ((w_cnt[requestP[conn_fd].id-MIN_ID] > 0 || r_cnt[requestP[conn_fd].id-MIN_ID] > 0 || fcntl(file_fd, F_SETLK, &lock) == -1)){ //locked
                    write(requestP[conn_fd].conn_fd, locked, strlen(locked));
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                    FD_CLR(conn_fd, &clientfds);
                }else {
                    w_cnt[requestP[conn_fd].id-MIN_ID]++; //reading
                    requestP[conn_fd].wait_for_write++; //update writing status
                    pread(file_fd, &booking_record, sizeof(record), sizeof(record)*(requestP[conn_fd].id-MIN_ID));
                    //idk how to print out the result in client
                    sprintf(requestP[conn_fd].buf, "Food: %d booked\nConcert: %d booked\nElectronics: %d booked\n\nPlease input your booking command. (Food, Concert, Electronics. Positive/negative value increases/decreases the booking amount.):\n", booking_record.bookingState[FOOD_INDEX], booking_record.bookingState[CONCERT_INDEX], booking_record.bookingState[ELECS_INDEX]);
                    write(requestP[conn_fd].conn_fd, requestP[conn_fd].buf, strlen(requestP[conn_fd].buf));
                    // continue;
                }
            }
        } else {
            char fod[32], cn[32], el[32];
            int cm_fd, cm_cn, cm_el;
            sscanf(requestP[conn_fd].buf, "%s %s %s", fod, cn, el);
            if (check_input(fod, strlen(fod)) || check_input(cn, strlen(cn)) || check_input(el, strlen(el))){
                write(requestP[conn_fd].conn_fd, errormsg, strlen(errormsg));
                memset(fod, 0, sizeof(fod));
                memset(cn, 0, sizeof(cn));
                memset(el, 0, sizeof(el));
                w_cnt[requestP[conn_fd].id-MIN_ID]--;
                requestP[conn_fd].wait_for_write--;
                lock.l_type = F_UNLCK; 
                lock.l_start = sizeof(record) * requestP[conn_fd].id; //size of the offset = size of regRecord* id-> set up space for each id
                lock.l_len = sizeof(record); //the range being locked
                fcntl(file_fd, F_SETLK, &lock);
                FD_CLR(conn_fd, &clientfds);
                close(requestP[conn_fd].conn_fd);
                free_request(&requestP[conn_fd]);
                //something wrong when closing here, so the next input cannot be accepted
            }else{
                cm_fd = atoi(fod);
                cm_cn = atoi(cn);
                cm_el = atoi(el);

                //sprintf(buf,"%d %d %d\n", cm_fd, cm_cn, cm_el);
                //write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                //update all
                record updates = booking_record;
                updates.bookingState[FOOD_INDEX] += cm_fd;
                updates.bookingState[CONCERT_INDEX] += cm_cn;
                updates.bookingState[ELECS_INDEX] += cm_el;
                if (updates.bookingState[FOOD_INDEX] < 0 ||  updates.bookingState[CONCERT_INDEX] < 0 || updates.bookingState[ELECS_INDEX] < 0){
                    write(requestP[conn_fd].conn_fd, under, strlen(under));
                    w_cnt[requestP[conn_fd].id-MIN_ID]--;
                    
                } else if (updates.bookingState[FOOD_INDEX] + updates.bookingState[CONCERT_INDEX] + updates.bookingState[ELECS_INDEX] > 15){
                    write(requestP[conn_fd].conn_fd, over, strlen(over));
                    w_cnt[requestP[conn_fd].id-MIN_ID]--;
                }else{
                    //update the booking record
                    pwrite(file_fd, &updates, sizeof(record), sizeof(record)*(requestP[conn_fd].id-MIN_ID));
                    // if there is a space after colon, fail
                    sprintf(requestP[conn_fd].buf, "Bookings for user %d are updated, the new booking state is:\nFood: %d booked\nConcert: %d booked\nElectronics: %d booked\n", requestP[conn_fd].id, updates.bookingState[FOOD_INDEX], updates.bookingState[CONCERT_INDEX], updates.bookingState[ELECS_INDEX]);
                    write(requestP[conn_fd].conn_fd, requestP[conn_fd].buf, strlen(requestP[conn_fd].buf));
                }
                //UNLOCK
                w_cnt[requestP[conn_fd].id-MIN_ID]--;
                requestP[conn_fd].wait_for_write--;
                lock.l_type = F_UNLCK; 
                lock.l_start = sizeof(record) * requestP[conn_fd].id; //size of the offset = size of regRecord* id-> set up space for each id
                lock.l_len = sizeof(record); //the range being locked
                fcntl(file_fd, F_SETLK, &lock);
                FD_CLR(conn_fd, &clientfds);
                close(requestP[conn_fd].conn_fd);
                free_request(&requestP[conn_fd]);
            }
        }
        // fprintf(stderr, "%s", requestP[conn_fd].buf);
        // sprintf(buf,"%s : %s",accept_write_header,requestP[conn_fd].buf);
        // write(requestP[conn_fd].conn_fd, buf, strlen(buf));
        // FD_CLR(conn_fd, &clientfds);    
#endif
        // FD_CLR(conn_fd, &clientfds);
        // close(requestP[conn_fd].conn_fd);
        // free_request(&requestP[conn_fd]);
        }
        }
    }

    free(requestP);
    return 0;
}


int check_input(char a[], int size){
    for (int i = 0; i < size; i++){
        if (isalpha(a[i])){
            return 1;
        }
        if (a[i] == '-' && i != 0){
            return 1;
        }
    }
    return 0;
}
// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}
