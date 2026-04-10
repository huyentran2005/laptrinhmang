#include<stdio.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<sys/select.h>
#include<unistd.h>
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

    fd_set fdread;
    int clients[MAXCLIENTS];
    int nClients = 0;
    char buf[1024];
    struct timeval tv;
    struct guest guests[MAXCLIENTS];
    
    
    
    while(1){
        FD_ZERO(&fdread);
        FD_SET(listener,&fdread);
        int maxdp = listener +1;
        for(int i=0; i< nClients; i++){
            FD_SET(clients[i], &fdread);
            if(clients[i] +1 > maxdp) maxdp =clients[i] +1;
        }
        tv.tv_sec= 5;
        tv.tv_usec =0;
        int ret = select(maxdp, &fdread, NULL,NULL, &tv);
        if(ret < 0){
            perror("select() failed");
            exit(1);
        } else if(ret == 0){
            printf("Time out.\n");
            continue;
        }
       
        if(FD_ISSET(listener, &fdread)){
            int client = accept(listener,NULL, NULL);
            if(nClients < MAXCLIENTS){
                clients[nClients] = client;
                guests[nClients].mark =0;
                nClients++;
                printf("New client connected: %d\n", client);
                char* msg ="Nhập user và pass theo cú pháp \"user pass\":\n";
                send(client, msg, strlen(msg),0);
            } else{
                 printf("Too many connections!\n");
                char* msg ="Sorry. Out of slots.\n";
                send(client, msg, strlen(msg),0);
                close(client);
            }
        }
        for(int i=0; i<nClients; i++){
            if(FD_ISSET(clients[i],&fdread)){
                int ret = recv(clients[i],buf, sizeof(buf),0);
                if( ret <0){
                    printf("Client %d disconnected.\n", clients[i]);
                    FD_CLR(clients[i], &fdread);
                    close(clients[i]);
                    clients[i] = clients[nClients -1];
                    guests[i] = guests[nClients -1];
                    nClients--;
                    i--;
                    continue;
                }
                buf[ret] =0;
                if(guests[i].mark ==0){
                    if(check(buf)){
                        guests[i].mark = 1;
                        char* msg ="\n----------Command---------\n  Nhập lệnh: ";
                        send(clients[i], msg, strlen(msg),0);
                    } else{
                        char* msg ="Lỗi đăng nhập!\nNhập user và pass theo cú pháp \"user pass\":\n";
                        send(clients[i], msg, strlen(msg),0);
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
                        send(clients[i], send_buf, strlen(send_buf),0);
                    }
                    fclose(f);
                    char* msg ="\n  Nhập lệnh: ";
                    send(clients[i], msg, strlen(msg),0);
                } else {
                    char* msg ="\n Sai lệnh!";
                    send(clients[i], msg, strlen(msg),0);
                }

            }
        }
    }
    close(listener);
    return 0;
}