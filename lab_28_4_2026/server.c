#include<stdio.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<unistd.h>
#include<poll.h>
#define MAXCLIENTS 1024



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
    

    while(1){
       
        int ret = poll(fds,nfds, -1);
        if(ret < 0){
            perror("poll() failed");
            exit(1);
        } 
       
        if(fds[0].revents & POLLIN){
            int client = accept(listener,NULL, NULL);
            fds[nfds].fd = client;
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
                    nfds--;
                    i--;
                    continue;
                }
                buf[ret] =0;
                if (strncmp(buf,"exit",4) == 0){
                    printf("Client %d disconnected.\n", fds[i].fd);
                    sprintf(msg, "Tạm biệt client %d\n", fds[i].fd);
                    send(fds[i].fd, msg, strlen(msg),0);
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    nfds--;
                    i--;
                    continue;
                }
                for (int j = 0; j < strlen(buf); j++) {
                    if (buf[j] >= 'a' && buf[j] <= 'z') {
                        if (buf[j] == 'z') buf[j] = 'a';
                        else buf[j] += 1;
                    } else if (buf[j] >= 'A' && buf[j] <= 'Z') {
                        if (buf[j] == 'Z') buf[j] = 'A';
                        else buf[j] += 1;
                    } else if (buf[j] >= '0' && buf[j] <= '9') {
                        buf[j] = '9' - (buf[j] - '0') ;
                    }
                }
                send(fds[i].fd, buf, strlen(buf),0);
            }      
        }
    }
    close(listener);
    return 0;
}