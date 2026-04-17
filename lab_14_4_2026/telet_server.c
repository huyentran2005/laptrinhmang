#include<stdio.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<unistd.h>
#include<poll.h>
#define MAXCLIENTS 1024

struct guest{
    int mark;
    char user[100];
    char password[100];
};
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

    char buf[1024];
    struct guest guests[MAXCLIENTS];
    

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
            guests[nfds].mark =0;
            nfds++;
            printf("New client connected: %d\n", client);
            char* msg ="Nhập user và pass theo cú pháp \"user pass\":\n";
            send(client, msg, strlen(msg),0);
        }
        
        for(int i=1; i<nfds; i++){
            if(fds[i].revents & (POLLIN | POLLERR)){
                int ret = recv(fds[i].fd,buf, sizeof(buf),0);
                if( ret <= 0){
                    printf("Client %d disconnected.\n", fds[i].fd);
                    close(fds[i].fd);
                    guests[i] = guests[nfds -1];
                    nfds--;
                    i--;
                    continue;
                }
                buf[ret] =0;
                if(guests[i].mark ==0){
                    if(check(buf)){
                        guests[i].mark = 1;
                        char* msg ="\n----------Command---------\n  Nhập lệnh: ";
                        send(fds[i].fd, msg, strlen(msg),0);
                    } else{
                        char* msg ="Lỗi đăng nhập!\nNhập user và pass theo cú pháp \"user pass\":\n";
                        send(fds[i].fd, msg, strlen(msg),0);
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
                        send(fds[i].fd, send_buf, strlen(send_buf),0);
                    }
                    fclose(f);
                    char* msg ="\n  Nhập lệnh: ";
                    send(fds[i].fd, msg, strlen(msg),0);
                } else {
                    char* msg ="\n Sai lệnh!";
                    send(fds[i].fd, msg, strlen(msg),0);
                }

            }
        }
    }
    close(listener);
    return 0;
}