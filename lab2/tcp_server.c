#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>

int main(int argc, char *argv[]){
    if(argc != 4){
        printf("Thieu tham so");
        return 1;
    }
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
        int client = accept(listener, NULL, NULL);
        if(client == -1){
            perror("accept() failed");
            continue;
        }

        FILE *f1 = fopen(argv[2],"r");
        if(f1 != NULL){
            char msg[256];
            while(fgets(msg, sizeof(msg),f1) != NULL){
                send(client, msg, strlen(msg),0);
            }
            fclose(f1);
        }
        while(1){
            char buf[256];
            int ret = recv(client, buf, sizeof(buf),0);
            if(ret <= 0){
                perror("received() failed");
                break;
            }
            buf[ret] ='\0';
            FILE *f2 = fopen(argv[3],"a");
            printf("Data received: %s\n", buf);
            fputs(buf,f2);
            fclose(f2);
        }
        close(client);
    }
    
    close(listener);
    return 0;
}