#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<pthread.h>
#include <unistd.h>
#include<time.h>

struct User{
    int client_id;
    char id[30];
    char name[50];
    int open;
};
int numClient;
struct User users[100];
void *thread_proc(void *arg);
int check_and_get_info(char *buf, char*id, char*name){
    char *token = strtok(buf, ":");
    if(token == NULL || strchr(token, ' '))
        return 0;
    strcpy(id, token);
    token = strtok(NULL, ":");
    if(token == NULL)
        return 0;

    while(*token == ' ')
        token++;
    strcpy(name, token);
    name[strcspn(name, "\n")] = 0;
    if(name[0] == '\0' || strchr(name, ' '))
        return 0;
    return 1;

}
int is_exist(char *id){
    for(int i=0; i< numClient;i++){
        if(users[i].open && strcmp(id, users[i].id) == 0) return 1;
    }
    return 0;
}
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int main(){
    setenv("TZ", "Asia/Ho_Chi_Minh", 1);
    tzset();
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listener == -1){
        perror("listener() failed");
        exit(1);
    }
    if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))){
        perror("setsockopt() failed");
        exit(1);
    }

    struct sockaddr_in addr = {0};
    addr.sin_port = htons(9000);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(listener, (struct sockaddr *)&addr, sizeof(addr))){
        perror("bind() failed");
        exit(1);
    }

    if(listen(listener, 5)){
        perror("listen() failed");
        exit(1);
    }
    printf("Server is listening on port 9000...\n");
    pthread_t thread_id;

    while(1){
        int client = accept(listener, NULL, NULL);
        users[numClient].client_id = client;
        strcpy(users[numClient].id, "\0");
        strcpy(users[numClient].name,"\0");
        users[numClient].open = 0;
        numClient++;
        printf("New client connected: %d\n", client);
        pthread_create(&thread_id, NULL, thread_proc, &client);

    }
    return 0;
}

void *thread_proc(void *arg){
    int client = *(int *)arg;
    char buf[256];
    char msg[1000];
    char id[50];
    char name[50];
    int idx_user;
    strcpy(msg,"Đăng nhập theo cú pháp \"client_id: client_name\": ");
    send(client, msg, strlen(msg), 0);
    while(1){
        int ret = recv(client, buf, sizeof(buf),0);
        if(ret <= 0){
            perror("recv() failed");
            break;
        }
        buf[ret] =0;
        int flag = 0;
        for(int i=0 ; i< numClient; i++){
            if(users[i].client_id == client){
                if(users[i].open == 0) flag = 1;
                idx_user = i;
            }
        }
        if(flag){
            pthread_mutex_lock(&lock);
            int check = check_and_get_info(buf, id, name);
            pthread_mutex_unlock(&lock);
            if(check){
                if(is_exist(id)){
                    strcpy(msg,"ID đã tồn tại!\n");
                    send(client, msg, strlen(msg), 0);
                    strcpy(msg,"Nhập theo cú pháp \"client_id: client_name\": ");
                    send(client, msg, strlen(msg), 0);
                } else{
                    strcpy(users[idx_user].id,id);
                    strcpy(users[idx_user].name,name);
                    users[idx_user].open =1;
                    printf("Client có id %d đã tham gia đoạn chat\n",client);
                    strcpy(msg,"---------Chatting--------\n");
                    send(client, msg, strlen(msg), 0);
                }
            } else{
                strcpy(msg,"Đăng nhập lỗi!\nNhập theo cú pháp \"client_id: client_name\": ");
                send(client, msg, strlen(msg), 0);
            }
            continue;
        }
        for(int i=0; i< numClient; i++){
            if(i != idx_user){
                if(users[i].open){
                    time_t now;
                    time(&now);
                    struct tm *info = localtime(&now);
                    sprintf(msg, "%04d/%02d/%02d %02d:%02d:%02d %s: %s",info->tm_year + 1900, info->tm_mon +1, info->tm_mday, info->tm_hour,info->tm_min, info->tm_sec,users[idx_user].id, buf); 
                    send(users[i].client_id, msg, strlen(msg), 0);
                }
            }
        }
    }
    close(client);
    users[idx_user] = users[numClient-1];
    numClient--;
}
