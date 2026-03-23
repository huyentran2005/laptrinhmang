#include<stdio.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<stdlib.h>


int main(int argc, char *argv[]){
    if(argc != 3){
        printf("nhap sai lenh");
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
    
    struct student{
        char mssv[50];
        char name[100];
        char birth[50];
        float tb;
    };

    while(1){
        struct student sv;
        printf("Thong tin sinh vien:\n");

        printf("MSSV:\n");
        fgets(sv.mssv, sizeof(sv.mssv),stdin);
        sv.mssv[strcspn(sv.mssv, "\n")] = 0;

        printf("Name:\n");
        fgets(sv.name, sizeof(sv.name),stdin);
        sv.name[strcspn(sv.name, "\n")] = 0;

        printf("Birth:\n");
        fgets(sv.birth, sizeof(sv.birth),stdin);
        sv.birth[strcspn(sv.birth, "\n")] = 0;

        printf("TB:\n");
        char tmp[50];
        fgets(tmp, sizeof(tmp),stdin);
        sv.tb = atof(tmp);

        send(client, &sv, sizeof(sv),0);
    }
    close(client);
    return 0;
}