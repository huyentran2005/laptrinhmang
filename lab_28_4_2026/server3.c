#include<stdio.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<unistd.h>
#include<poll.h>
#define MAXCLIENTS 1024
#define MAXTOPIC 100

struct Client{
    char topic[MAXTOPIC][256];
    int numTopic;
};

int main(int argv, char *argc[]){
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listener == -1){
        perror("socket() failed");
        exit(1);
    }

    if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1},sizeof(int))){
        perror("setsockopt() failed");
        exit(1);
    };

    struct sockaddr_in addr; 
    addr.sin_family= AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);
    
    if(bind(listener, (struct sockaddr*)&addr, sizeof(addr))){
        perror("bind() failed");
        exit(1);
    }
    if(listen(listener, 5)){
        perror("listen() failed");
        exit(1);
    }

    int nfds = 1;
    struct pollfd fds[MAXCLIENTS];
    fds[0].fd = listener;
    fds[0].events = POLLIN;
    char msg[256];
    char buf[1024];
    struct Client *clients = calloc(MAXCLIENTS, sizeof(struct Client));
    clients[0].numTopic =0;
    while(1){
       
        int ret = poll(fds,nfds, -1);
        if(ret < 0){
            perror("poll() failed");
            exit(1);
        } 
       
        if(fds[0].revents & POLLIN){
            int client = accept(listener,NULL, NULL);
            fds[nfds].fd = client;
            clients[nfds].numTopic = 0;
            memset(clients[nfds].topic, 0, sizeof(clients[nfds].topic));
            fds[nfds].events = POLLIN;
            nfds++;
            printf("New client connected: %d\n", client);
            sprintf(msg, "Xin chào. Hiện đang có %d clients đang kết nối.\n", nfds - 1);
            send(client, msg, strlen(msg),0);
        }
        
        for(int i=1; i<nfds; i++){
            if(fds[i].revents & (POLLIN | POLLERR)){
                int ret = recv(fds[i].fd,buf, sizeof(buf),0);
                if( ret <= 0){
                    printf("Client %d disconnected.\n", fds[i].fd);
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    clients[i] = clients[nfds-1];
                    nfds--;
                    i--;
                    continue;
                }
                if(ret > 0) buf[ret] =0;
                if (strncmp(buf,"exit",4) == 0){
                    printf("Client %d disconnected.\n", fds[i].fd);
                    sprintf(msg, "Tạm biệt client %d\n", fds[i].fd);
                    send(fds[i].fd, msg, strlen(msg),0);
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    clients[i] = clients[nfds-1];
                    nfds--;
                    i--;
                    continue;
                }
                char cmd[10];
                char topic[100];
                int n = sscanf(buf, "%s %s %[^\n]", cmd, topic, msg);

                if (strcmp(cmd, "PUB") == 0) {
                    if (n == 3) {
                        int existed = 0;
                        for (int k = 0; k < clients[i].numTopic; k++) {
                            if (strcmp(clients[i].topic[k], topic) == 0){
                                existed =1;
                                break;
                            }
                        }
                        if( existed == 0) continue;
                        for(int x =1 ; x < nfds;x++){
                            if(clients[x].numTopic == 0) continue;
                            for(int y=0; y <clients[x].numTopic; y++ ){
                                if(fds[x].fd > 0 && i != x && strcmp(clients[x].topic[y], topic) == 0){
                                    char output[512];
                                    snprintf(output, sizeof(output), "[Topic %s] Client %d: %s\n",topic, fds[i].fd, msg);
                                    send(fds[x].fd, output, strlen(output), 0);
                                }
                            }
                        }
                    } else {
                        printf("Sai format PUB\n");
                    }
                } else if (strcmp(cmd, "SUB") == 0) {
                    if (n >= 2) {
                        int existed = 0;
                        for (int k = 0; k < clients[i].numTopic; k++) {
                            if (strcmp(clients[i].topic[k], topic) == 0){
                                existed =1;
                                break;
                            }
                        }
                        if( existed) continue;
                        printf("Client %d >> SUB -> topic: %s\n",fds[i].fd, topic);
                        snprintf(msg, sizeof(msg),"Client %d đăng ký topic %s thành công!\n",fds[i].fd, topic);
                        send(fds[i].fd, msg,strlen(msg),0);
                        int num = clients[i].numTopic;
                        clients[i].numTopic++;
                        strcpy(clients[i].topic[num], topic);
                    } else {
                        printf("Sai format SUB\n");
                    }
                } else if (strcmp(cmd, "UNSUB") == 0) {
                    if (n >= 2) {
                        if(clients[i].numTopic == 0) continue;
                        for(int y=0; y <clients[i].numTopic; y++ ){
                            if(strcmp(clients[i].topic[y], topic) == 0){
                                int num = clients[i].numTopic;
                                strcpy(clients[i].topic[y], clients[i].topic[num-1]);
                                clients[i].numTopic--;
                                printf("Client %d >> UNSUB -> topic: %s\n",fds[i].fd, topic);
                                snprintf(msg, sizeof(msg),"Client %d đã hủy đăng ký topic %s!\n",fds[i].fd, topic);
                                send(fds[i].fd, msg,strlen(msg),0);
                                break;
                            }
                        }
                    } else {
                        printf("Sai format UNSUB\n");
                    }
                }
            }      
        }
    }
    close(listener);
    return 0;
}