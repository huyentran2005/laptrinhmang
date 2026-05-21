#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<pthread.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>

struct Client{
    int id;
    int is_login;
};
int numClient;
struct Client clients[1000];
int check(char *buf){
    char recv_buf[1024];
    strcpy(recv_buf,buf);
    char *user = strtok(recv_buf," \r\n");
    char *pass = strtok(NULL, " \r\n");
    if(user == NULL || pass == NULL ) return 0;
    pass[strcspn(pass, "\n")] = 0;
    char *token1 = strtok(NULL, " \r\n");
    if(token1 != NULL) return 0;
    FILE *f = fopen("sql.txt", "r");
    if( f== NULL) return 0;
    char f_buf[1024];
    while(fgets(f_buf, sizeof(f_buf), f)){
        char *user_correct = strtok(f_buf," ");
        char *pass_correct = strtok(NULL, " ");
        pass_correct[strcspn(pass_correct, "\n")] = 0;
        char *token = strtok(NULL, " ");
        if(token != NULL) return 0;
        if( strcmp(user_correct, user) == 0 && strcmp(pass_correct,pass) == 0) 
            return 1;
    }
    fclose(f);
    return 0;
}
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
void* thread_proc(void *arg);

int main(){
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
        clients[numClient].id = client;
        clients[numClient].is_login = 0;
        numClient++;
        
        pthread_create(&thread_id, NULL, thread_proc, &client);
        pthread_detach(thread_id);
    }
    return 0;
}

void *thread_proc(void *arg){
    int client = *(int *)arg;
    char buf[256];
    int idx_client;
    char* msg1 ="Nhập user và pass theo cú pháp \"user pass\":\n";
    send(client, msg1, strlen(msg1),0);
    while(1){
        int ret = recv(client, buf, sizeof(buf), 0);
        if(ret <= 0) break;
        buf[ret]=0;
        for(int i=0; i<numClient; i++){
            if(clients[i].id == client){
                idx_client = i;
                break;
            }
        }
        if(!clients[idx_client].is_login){
            pthread_mutex_lock(&lock);
            int flag = check(buf);
            pthread_mutex_unlock(&lock);
            if(flag){
                clients[idx_client].is_login = 1;
                char* msg ="\n----------Command---------\n  Nhập lệnh: ";
                send(client, msg, strlen(msg),0);
            } else{
                char* msg ="Lỗi đăng nhập!\nNhập user và pass theo cú pháp \"user pass\":\n";
                send(client, msg, strlen(msg),0);
            } 
            continue;
        }
        buf[strcspn(buf, "\r\n")] = 0;
        char cmd[1200];
        snprintf(cmd, sizeof(cmd), "%s > out.txt", buf);
        system(cmd);
        FILE *f = fopen("out.txt","r");
        if(f != NULL){
            char send_buf[1024];
            while(fgets(send_buf, sizeof(send_buf),f)){
                send(client, send_buf, strlen(send_buf),0);
            }
            fclose(f);
            char* msg ="\n  Nhập lệnh: ";
            send(client, msg, strlen(msg),0);
        } else {
            char* msg ="\n Sai lệnh!";
            send(client, msg, strlen(msg),0);
        }
    }
    close(client);
    clients[idx_client] = clients[numClient-1];
    numClient--;
}
