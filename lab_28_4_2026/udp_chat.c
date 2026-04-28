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
#include<poll.h>
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

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO; 
    fds[0].events = POLLIN;
    fds[1].fd = listener;   
    fds[1].events = POLLIN;

    char buf[MAX];
    printf("---CHAT(Port: %d) ---\n", port_s);
    printf("[Bạn]: ");
    fflush(stdout);

    while(1){
        int ret = poll(fds, 2, -1); 
        if (ret < 0) {
            perror("poll() failed");
            exit(1);
        }
        if (fds[1].revents & POLLIN) {
            struct sockaddr_in from_addr;
            socklen_t from_len = sizeof(from_addr);
            int ret = recvfrom(listener, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from_addr, &from_len);
            if (ret > 0) {
                buf[ret] = '\0';
                printf("\r[Nhận từ %s:%d]: %s\n", inet_ntoa(from_addr.sin_addr),ntohs(from_addr.sin_port), buf);
                printf("[Bạn]: ");
                fflush(stdout);
            }
        }
        if (fds[0].revents & POLLIN) {
            if (fgets(buf, sizeof(buf), stdin) != NULL) {
                buf[strcspn(buf, "\n")] = 0;
                sendto(listener, buf, strlen(buf), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                if (strcmp(buf, "exit") == 0) break;
                printf("[Bạn]: ");
                fflush(stdout);
            }
        }
    }
    close(listener);
    return 0;
}