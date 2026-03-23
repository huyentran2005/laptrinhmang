#include<stdio.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<stdlib.h>


int main(int argc, char *argv[]){
    if(argc != 3){
        printf("Sử dụng: %s <Địa chỉ IP> <Cổng>\n", argv[0]);
        return 1;
    }

    char *ip_address = argv[1];
    int port = atoi(argv[2]);

    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(client == -1){
        perror("socket() failed");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip_address);
    addr.sin_port = htons(port);

    int ret = connect(client, (struct sockaddr*)&addr, sizeof(addr));
    if(ret ==-1){
        perror("connect() failed");
        exit(1);
    }

    while(1){
        char buf[256];
        printf("Nhap du lieu:\n");
        fgets(buf, sizeof(buf),stdin);
        send(client, buf, strlen(buf),0);
        if(strncmp(buf, "exit",4) == 0){
            printf("Thoat khoi chuong trinh\n");
            break;
        }
    }
    close(client);
    return 0;
}