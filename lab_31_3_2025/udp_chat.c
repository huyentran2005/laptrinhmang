#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/ioctl.h>
#include <errno.h>
#include <ctype.h>
#define MAX 1024

int main(int argc, char *argv[]){
    if(argc != 4){
        printf("Nhap sai cu phap!\n");
        return 0;
    }
    int listener =socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(listener == -1){
        perror("socket() failed");
        exit(1);
    }
    int port_s = atoi(argv[1]);
    char * ip_d = argv[2];
    int port_d = atoi(argv[3]);

    // Thiet lap non-blocking cho socket
    unsigned long ul =1;
    ioctl(listener, FIONBIO, &ul);


    // Thiet lap non-blocking cho ban phim de doc input khong bi chan
    unsigned long ul_stdin =1;
    ioctl(0, FIONBIO, &ul_stdin);

    struct sockaddr_in addr;
    addr.sin_family= AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_s);

    if(bind(listener, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        perror("bind() failed");
        exit(1);
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port_d);
    dest_addr.sin_addr.s_addr = inet_addr(ip_d);

    char buf[MAX];
    printf("[Bạn]: "); 
    fflush(stdout);
    while(1){
        struct sockaddr_in from_addr;
        socklen_t  from_len = sizeof(from_addr);
        int ret = recvfrom(listener, buf, sizeof(buf)-1, 0, (struct sockaddr*)&from_addr, &from_len);
        if(ret >0){
            buf[ret]='\0';
            printf("\r [Nhận từ %s]: %s\n",inet_ntoa(from_addr.sin_addr),buf);
            printf("[Bạn]:");
            fflush(stdout);
        }

        if(fgets(buf,sizeof(buf),stdin)!= NULL){
            buf[strcspn(buf, "\n")] = 0;
            sendto(listener, buf, strlen(buf),0,(struct sockaddr*)&dest_addr, sizeof(dest_addr));
            printf("[Bạn]: ");
            fflush(stdout);
        }
        usleep(10000);

    }
    close(listener);
    return 0;
}