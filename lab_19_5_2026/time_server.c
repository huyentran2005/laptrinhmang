#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<pthread.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<time.h>


void process(char *buf, char *msg){
    if(!strstr(buf, "GET_TIME")){
        strcpy(msg, "Sai cú pháp lệnh\nNhập lệnh: ");
        return;
    }
    char *cmd = strtok(buf, " ");
    if(buf == NULL || cmd == NULL){
        strcpy(msg, "Yêu cầu nhập lệnh!\nNhập lệnh: ");
        return;
    }
    if(strcmp(cmd, "GET_TIME") != 0){
        strcpy(msg, "Sai cú pháp lệnh\nNhập lệnh: ");
        return;
    }
    char *format = strtok(NULL, " ");
    if(format == NULL || strstr(format," ")){
        strcpy(msg,"Yêu cầu format thời gian!\nNhập lệnh: ");
        return;
    }
    char *extra = strtok(NULL, " ");

    if(extra != NULL){
        strcpy(msg, "Sai cú pháp lệnh\nNhập lệnh: ");
        return;
    }
    time_t now;
    time(&now);
    struct tm *raw;
    raw = localtime(&now);
    int year1 = raw->tm_year + 1900;
    int year2 = year1 % 100;
    int month = raw->tm_mon + 1;
    int day = raw->tm_mday;

    if(strcmp(format,"dd/mm/yyyy")==0){
        sprintf(msg, "%02d/%02d/%04d\nNhập lệnh: ", day, month, year1);
    } else if(strcmp(format,"dd/mm/yy")==0){
        sprintf(msg, "%02d/%02d/%02d\nNhập lệnh: ", day, month, year2);
    } else if(strcmp(format,"mm/dd/yyyy")==0){
        sprintf(msg, "%02d/%02d/%04d\nNhập lệnh: ", month,day, year1);
    } else if(strcmp(format,"mm/dd/yy")==0){
        sprintf(msg, "%02d/%02d/%02d\nNhập lệnh: ", month,day, year2);
    } else {
        strcpy(msg, "Không đúng định dạng format\nNhập lệnh: ");
    }
}

void* thread_proc(void *arg);
int main(){
    setenv("TZ", "Asia/Ho_Chi_Minh", 1);
    tzset();
    int listener = socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
    if(listener == -1){
        perror("listener() failed");
        exit(1);
    }
    if(setsockopt(listener,SOL_SOCKET,SO_REUSEADDR, &(int){1},sizeof(int))){
        perror("setsockopt() failed");
        close(listener);
        exit(1);
    }
    struct sockaddr_in addr ;
    addr.sin_port = htons(9000);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(listener,(struct sockaddr *)&addr, sizeof(addr))){
        perror("bind() failed");
        exit(1);
    }
    if(listen(listener,5)){
        perror("listen() failed");
        exit(1);
    }
    printf("Server is listening on port 9000...\n");
    pthread_t thread_id;
    while(1){
        int client = accept(listener, NULL, NULL);
        printf("New client connected: %d\n", client);
        
        pthread_create(&thread_id, NULL, thread_proc, &client);
        pthread_detach(thread_id);
    }
    return 0;
}

void *thread_proc(void *arg){
    int client = *(int *)arg;
    char buf[256];
    char msg[256];
    strcpy(msg,"Nhập lệnh: ");
    send(client,msg, strlen(msg),0);
    while(1){
        int ret = recv(client, buf, sizeof(buf), 0);
        if(ret <= 0) break;
        buf[ret]=0;
        buf[strcspn(buf,"\n")] =0;
        process(buf,msg);
        send(client,msg, strlen(msg),0);
    }
    close(client);
}
