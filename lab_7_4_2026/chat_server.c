#include<stdio.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<sys/select.h>
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

    fd_set fdread;
    struct user users[MAX_CLIENTS];
    int clients[MAX_CLIENTS];
    int nClients =0;
    struct timeval tv;
    char buf[2048];

    while(1){
        FD_ZERO(&fdread);
        FD_SET(listener, &fdread);
        int maxdp = listener +1;
        for(int i = 0; i < nClients; i++){
            FD_SET(clients[i], &fdread);
            if(clients[i]+1 > maxdp) maxdp = clients[i] + 1; 
        }
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        int ret = select(maxdp, &fdread, NULL, NULL, &tv);
        if(ret < 0){
            printf("select() failed.\n"); return 1;
        }

        if( ret == 0){
            printf("Time out.\n"); continue;
        }

        if(FD_ISSET(listener, &fdread)){
            int client = accept(listener,NULL, NULL);
            if(nClients < MAX_CLIENTS){
                clients[nClients] = client;
                users[nClients].mark = 0;
                nClients++;
                printf("New client connected: %d\n",client);
                char *msg = "Nhập tên theo cú pháp \"client_id: client_name\":\n";
                send(client, msg, strlen(msg), 0);
            } else{
                printf("Too many connections\n");
                char *msg ="Sorry. Out of slots.\n";
                send(client, msg, strlen(msg),0);
                close(client);
            }
        }

        for(int i=0; i< nClients; i++){
            if(FD_ISSET(clients[i], &fdread)){
                ret = recv(clients[i], buf, sizeof(buf) - 1,0);
                if( ret <=0){
                    printf("Client %d disconnected\n", clients[i]);
                    FD_CLR(clients[i], &fdread);
                    close(clients[i]);
                    clients[i] = clients[nClients - 1];
                    users[i] = users[nClients -1];
                    nClients--;
                    i--;
                    continue;
                }
                if(users[i].mark == 0){
                    if(check_and_get_info(buf, users[i].name, users[i].id)){
                        users[i].mark = 1;
                        printf("Người dùng có id: %s, tên: %s đã vào phòng chat \n",users[i].id, users[i].name);
                        char *msg = "\n---------Chatting-----------\n";
                        send(clients[i], msg, strlen(msg), 0);
                    } else{
                        char *msg = "Nhập tên theo cú pháp \"client_id: client_name\":\n";
                        send(clients[i], msg, strlen(msg), 0);
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
                for(int j=0; j<nClients; j++ ){
                    if(i == j) continue;;
                    if(users[j].mark ==1 ) send(clients[j],log_msg, strlen(log_msg),0);
                } 
            }
        }
    }
    close(listener);
    return 0;
}