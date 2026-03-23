#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<time.h>

struct student{
        char mssv[50];
        char name[100];
        char birth[50];
        float tb;
};
int main(int argc, char *argv[]){
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listener ==-1){
        perror("socket() failed");
        exit(1);
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(argv[1]));
    if(bind(listener, (struct sockaddr*)&addr, sizeof(addr))){
        perror("bind() failed");
        exit(1);
    }
    if(listen(listener, 5)){
        perror("listen() failed");
        exit(1);
    }
    while(1){
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client = accept(listener, (struct sockaddr*)&client_addr, &client_addr_len);
        if(client == -1){
            perror("accept() failed");
            continue;
        }
        char *client_ip = inet_ntoa(client_addr.sin_addr);
        while(1){
            struct student sv;
            int ret = recv(client, &sv, sizeof(sv),0);
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

            if(ret <= 0){
                perror("received() failed");
                break;
            }
            printf("data received: %d\n",ret);
            printf("Client IP: %s\n",client_ip);
            printf("Time: %s\n",time_str);
            printf("MSSV: %s\n", sv.mssv);
            printf("Name: %s\n", sv.name);
            printf("Birth: %s\n", sv.birth);
            printf("TB: %f\n", sv.tb);
            FILE *f = fopen(argv[2], "a");
            if(f != NULL){
                fprintf(f, "%s %s %s %s %s %.2f\n", client_ip, time_str, sv.mssv, sv.name,sv.birth, sv.tb);
                fclose(f);
            }
        }
        close(client);
    }
    
    close(listener);
    return 0;
}