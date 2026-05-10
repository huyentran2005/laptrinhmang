#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>

void signal_handler(int sig) {
    int pid = wait(NULL);
    printf("Child process terminated: %d\n", pid);
}

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

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }
    
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);
    
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }
    
    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }
    
    printf("Server is listening on port 8080...\n");
    
    signal(SIGCHLD, signal_handler);

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (fork() == 0) {
            // Xu ly trong tien trinh con
            close(listener);

            char buf[256];
            int flag=0;
            char* msg_header ="Nhập user và pass theo cú pháp \"user pass\":\n";
            send(client, msg_header, strlen(msg_header),0);
            while (1) {
                int len = recv(client, buf, sizeof(buf), 0);
                if (len <= 0)
                    break;
                buf[len] = 0;
                if(!flag){
                    if(check(buf)){
                        flag =1;
                        char* msg ="\n----------Command---------\n  Nhập lệnh: ";
                        send(client, msg, strlen(msg),0);
                    } else{
                        char* msg ="Lỗi đăng nhập!\nNhập user và pass theo cú pháp \"user pass\":\n";
                        send(client, msg, strlen(msg),0);
                    } 
                }else{
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
            }
            exit(0);
        }
        // Xu ly o tien trinh cha
        close(client);
    }

    close(listener);
    return 0;
}