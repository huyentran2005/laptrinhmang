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
#include <time.h>


void signal_handler(int sig) {
    int pid = wait(NULL);
    printf("Child process terminated: %d\n", pid);
}

void process_cmd(char *buf, char *ans){
    char *cmd = strtok(buf, " ");
    if(cmd == NULL){
        strcpy(ans,"Vui lòng nhập lệnh!");
        return;
    }
    if(strcmp(cmd, "GET_TIME") != 0){
        strcpy(ans,"Nhập sai lệnh!");
        return;
    }
    char *format = strtok(NULL, " ");
    if(format == NULL){
        strcpy(ans,"Vui lòng nhập format!");
        return;
    }
    time_t now;
    struct tm *local;
    time(&now);
    local = localtime(&now);
    if(strcmp(format,"dd/mm/yyyy") == 0){
        sprintf(ans,"%02d/%02d/%04d",
            local->tm_mday,
            local->tm_mon+1,
            local->tm_year+1900
        );
    }else if(strcmp(format,"dd/mm/yy") == 0){
        sprintf(ans,"%02d/%02d/%02d",
            local->tm_mday,
            local->tm_mon+1,
            (local->tm_year+1900)%100
        );
    } else if(strcmp(format,"mm/dd/yy") == 0){
        sprintf(ans,"%02d/%02d/%02d",
            local->tm_mon+1,
            local->tm_mday,
            (local->tm_year+1900)%100
        );
    } else if(strcmp(format,"mm/dd/yyyy") == 0){
        sprintf(ans,"%02d/%02d/%04d",
            local->tm_mon+1,
            local->tm_mday,
            local->tm_year+1900
        );
    } else{
        strcpy(ans ,"Sai format thời gian");
    }
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
        printf("New client connected: %d\n", client);
        if (fork() == 0) {
            // Xu ly trong tien trinh con
            close(listener);

            char buf[256];
            char msg[50];
            int flag;
            strcpy(msg, "Nhập lệnh: ");
            send(client, msg,strlen(msg),0);
            while (1) {
                int len = recv(client, buf, sizeof(buf), 0);
                if (len <= 0)
                    break;
                buf[strcspn(buf,"\n")] = 0;
                process_cmd(buf, msg);
                send(client, msg,strlen(msg),0);
                strcpy(msg, "\nNhập lệnh: ");
                send(client, msg,strlen(msg),0);
            }
            exit(0);
        }
        // Xu ly o tien trinh cha
        close(client);
    }

    close(listener);
    return 0;
}