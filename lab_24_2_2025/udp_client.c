#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>

int main(int argc, char * argv[]){
    int client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(client ==-1){
        perror("sock() failed");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    addr.sin_port = htons(atoi(argv[2]));
    char buf[256];
    while(1){
        printf("Me: ");
        fgets(buf, sizeof(buf), stdin);
        buf[strcspn(buf,"\n")]= 0;
        sendto(client, buf, strlen(buf), 0,(struct sockaddr*)&addr, sizeof(addr));

        int ret = recvfrom(client, buf, sizeof(buf),0, NULL, NULL);
        if(ret<=0){
            break;
        }
        buf[ret] = '\0';
        printf("You: %s\n", buf);
    }
    close(client);
    return 0;
}