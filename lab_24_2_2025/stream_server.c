#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>

int main(int argc, char *argv[]){
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listener == -1){
        perror("sock() failed");
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

    if(listen(listener,5)){
        perror("listen() failed");
        exit(1);
    }

    int client = accept(listener, NULL, NULL);
    if(client ==-1){
        perror("accept() failed");
        exit(1);
    }

    char endbuf[10]="";
    long count =0;
    char *s ="0123456789";
    while(1){
        char buf[256];
        char tmp[300];
        int ret = recv(client, buf, sizeof(buf),0);
        if(ret<=0){
            perror("recv() failed");
            break;
        }
        buf[ret] = '\0';
        if(strlen(endbuf) > 0){
            strcpy(tmp, endbuf);
            strcat(tmp, buf);
        } else {
            strcpy(tmp, buf);
        }

        char *p = tmp;
        while((p= strstr(p, s)) != NULL){
            count++;
            p ++;
        }
        
        printf("%s\n", buf);
        printf("So lan xuat hien xau ky tu \"0123456789\": %ld\n", count);
        if(strlen(buf) >=9){
            strcpy(endbuf, buf +(strlen(buf) - 9));
        }
        endbuf[9]='\0';
    }
    printf("So lan xuat hien xau ky tu \"0123456789\": %ld\n", count);
    close(listener);
    return 0;
}