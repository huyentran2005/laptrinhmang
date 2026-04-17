#include<stdio.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<poll.h>
#include<stdlib.h>
#include<time.h>
#define MAX_CLIENTS 1020

int check_and_get_info(char*buf, char*name, char*id){
    char* ch= ":";
    strcpy(id,strtok(buf, ch));
    char *token = strtok(NULL, ch);
    if(token == NULL || id == NULL) return 0;
    while(*token == ' ') token++;
    strcpy(name, token);

    name[strcspn(name,"\n")] = 0;
    if(strchr(name,' ')) return 0;
    return 1;
}

struct user{
    int mark;
    char id[100];
    char name[100];
};

int main(int argv, char *argc[]){
    setenv("TZ", "Asia/Ho_Chi_Minh", 1);
    tzset();
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listener == -1){
        perror("socket() failed");
        exit(0);
    }

    if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))){
        perror("setsockopt() failed");
        return 1;
    }

    struct sockaddr_in addr ={0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    if(bind(listener, (struct sockaddr*)&addr, sizeof(addr))){
        perror("bind() failed");
        close(listener);
        exit(1);
    }

    if(listen(listener, 5)){
        perror("listen() failed");
        close(listener);
        exit(1);
    }

    struct pollfd fds[64];
    int nfds = 1;

    fds[0].fd = listener;
    fds[0].events = POLLIN;

    struct user users[MAX_CLIENTS];
    char buf[2048];

    while(1){
      
        int ret = poll(fds, nfds, -1);
        
        if(fds[0].revents & POLLIN){
            int client = accept(listener,NULL, NULL);
            fds[nfds].fd = client;
            fds[nfds].events = POLLIN;
            nfds++;
            printf("New client connected: %d\n",client);
            char *msg = "Nhập tên theo cú pháp \"client_id: client_name\":\n";
            send(client, msg, strlen(msg), 0);
        }

        for(int i=1 ; i< nfds; i++){
            if(fds[i].revents & (POLLIN | POLLERR)){
                ret = recv(fds[i].fd,buf, sizeof(buf),0);
                if( ret <=0){
                    printf("Client %d disconnected\n", fds[i].fd);
                    close(fds[i].fd);
                    users[i] = users[nfds -1];
                    nfds--;
                    i--;
                    continue;
                }
                if(users[i].mark == 0){
                    if(check_and_get_info(buf, users[i].name, users[i].id)){
                        users[i].mark = 1;
                        printf("Người dùng có id: %s, tên: %s đã vào phòng chat \n",users[i].id, users[i].name);
                        char *msg = "\n---------Chatting-----------\n";
                        send(fds[i].fd, msg, strlen(msg), 0);
                    } else{
                        char *msg = "Nhập tên theo cú pháp \"client_id: client_name\":\n";
                        send(fds[i].fd, msg, strlen(msg), 0);
                    }
                    continue;
                } 
                buf[ret] =0;
                char log_msg[2500];
                char time_buf[30];
                time_t now = time(NULL);
                strftime(time_buf, sizeof(time_buf),"%d/%m/%Y %H:%M:%S", localtime(&now));
                snprintf(log_msg, sizeof(log_msg), "  %s %s: %s", time_buf, users[i].id, buf);
                printf("%s",log_msg);
                for(int j=1; j<nfds; j++ ){
                    if(i == j) continue;;
                    if(users[j].mark ==1 ) send(fds[j].fd,log_msg, strlen(log_msg),0);
                } 
            }
        }
    }
    close(listener);
    return 0;
}