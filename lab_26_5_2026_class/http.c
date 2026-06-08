/*******************************************************************************
 * @file    http_client_post.c
 * @brief   Mô tả ngắn gọn về chức năng của file
 * @date    2026-05-26 07:30
 *******************************************************************************/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

int main() {
    int client = socket(AF_INET, SOCK_STREAM, 0);
    struct addrinfo *res;
    getaddrinfo("lebavui.io.vn", "80", NULL, &res);

    connect(client, res->ai_addr, res->ai_addrlen);

    char *request = "POST /post HTTP/1.1\r\n"
        "Host: lebavui.io.vn\r\n"
        "Connection: close\r\n";
    send(client, request, strlen(request), 0);

    char buf[4096];
    FILE *fp = fopen("downloaded_video.mp4","wb");
    int len = recv(client, buf, sizeof(buf) - 1, 0);
    long download =0;
    long content_length =0;
    if (len > 0) {
        buf[len] = '\0';
        printf("%s\n", buf);
        if(strstr(buf,"\r\n\r\n")){
            content_length = strtol(strstr(buf, "Content-Length:")+16, NULL,1);
            char *body = strstr(buf,"\r\n\r\n")+4;
            int header_len = body -buf;
            int body_len = len - header_len;
            fwrite(body ,1,body_len,fp);
            download += body_len;
        } else {
            fwrite(buf,1, len,fp);
            download += len;
        }
    }
    for(int i =0; i< len ;i++){
        putchar(buf[i]);
    }
    close(client);
    freeaddrinfo(res);

    return 0;
}