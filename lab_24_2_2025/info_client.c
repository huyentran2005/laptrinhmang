#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<dirent.h>
#include<sys/stat.h>
#include <limits.h>

int main(int argc, char * argv[]){
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if( client == -1){
        perror("socket() failed");
        exit(1);
    }
    char *ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in  addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port= htons(port);

    int ret = connect(client, (struct sockaddr*)&addr, sizeof(addr));
    if(ret ==-1){
        perror("connect() failed");
        exit(1);
    }

    char cwd[PATH_MAX];
    struct dirent *entry;
    struct stat fileStat;
    DIR *dir;
    if(getcwd(cwd, sizeof(cwd))!= NULL){
        printf("Thu muc hien tai: %s\n", cwd);
    } else{
        perror("getcwd() failed");
        exit(1);
    }
    dir = opendir(".");
    if(dir == NULL){
        perror("Khong the mo thu muc");
        return 1;
    }
    while((entry = readdir(dir))!= NULL){
        if(stat(entry->d_name, &fileStat)==0){
            if(S_ISREG(fileStat.st_mode)){
                char buf[256];
                snprintf(buf, sizeof(buf), "%-30s %ld bytes\n", entry->d_name, (long)fileStat.st_size);
                send(client, buf, strlen(buf),0);
            }
        }
    }
    close(client);
    
    return 0;
}