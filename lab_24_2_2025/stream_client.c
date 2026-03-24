#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>

int main(int argc, char * argv[]){
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(client ==-1){
        perror("sock() failed");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    addr.sin_port = htons(atoi(argv[2]));

    int ret = connect(client, (struct sockaddr*)&addr, sizeof(addr));
    if(ret == -1){
        perror("connect() failed");
        exit(1);
    }

    while(1){
        char buf[256];
        fgets(buf, sizeof(buf), stdin);
        buf[strcspn(buf,"\n")]= 0;
        if(strncmp(buf, "exit",4) == 0){
            printf("Ket thuc chuong trinh!\n");
            break;
        }
        send(client, buf, strlen(buf), 0);
    }
    close(client);
    return 0;
}