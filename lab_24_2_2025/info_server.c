#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>

int main(int argc, char* argv[]){
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listener == -1){
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
            exit;
        }
        while(1){
            char buf[256];
            int ret = recv(client, buf, sizeof(buf), 0);
            if(ret <=0){
                perror("recv() failed");
                break;
            }
            buf[ret] = '\0';
            printf("%s\n",buf);
        }
        close(client);
    }
    close(listener);
    return 0;
}