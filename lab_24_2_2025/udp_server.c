#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>

int main(int argc, char *argv[]){
    int server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(server == -1){
        perror("sock() failed");
        exit(1);
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(argv[1]));

    if(bind(server, (struct sockaddr*)&addr, sizeof(addr))){
        perror("bind() failed");
        exit(1);
    }

    char buf[256];
    struct sockaddr_in s_addr;
    socklen_t s_addr_len = sizeof(s_addr);

    while(1){
        int ret = recvfrom(server, buf, sizeof(buf), 0,(struct sockaddr *)&s_addr, &s_addr_len);
        if(ret<=0){
            break;
        }
        buf[ret] = '\0';
        printf("You: %s\n", buf);

        printf("Me: ");
        fgets(buf, sizeof(buf), stdin);
        sendto(server, buf, strlen(buf), 0, (struct sockaddr *)&s_addr, s_addr_len);
        printf("\n");
    }
    close(server);
    return 0;
}