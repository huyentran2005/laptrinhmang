#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<pthread.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<time.h>
#define THREAD 10;

void* thread_proc(void *arg);
int main(){
    
    int listener = socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
    if(listener == -1){
        perror("listener() failed");
        exit(1);
    }
    if(setsockopt(listener,SOL_SOCKET,SO_REUSEADDR, &(int){1},sizeof(int))){
        perror("setsockopt() failed");
        close(listener);
        exit(1);
    }
    struct sockaddr_in addr ;
    addr.sin_port = htons(9000);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(listener,(struct sockaddr *)&addr, sizeof(addr))){
        perror("bind() failed");
        exit(1);
    }
    if(listen(listener,5)){
        perror("listen() failed");
        exit(1);
    }
    printf("Server is listening on port 9000...\n");
    int num_threads = THREAD;
    pthread_t thread_id;
    for(int i=0; i< num_threads; i++){
        int ret = pthread_create(&thread_id, NULL, thread_proc, &listener);
        if(ret != 0){
            printf("Could not creat new thread.\n");
            sched_yield();
        }
    }
    pthread_join(thread_id, NULL);
    return 0;
}

void *thread_proc(void *arg){
    int listener = *(int *)arg;
    char buf[256];
    char msg[256];
    while(1){
        int client = accept(listener, NULL, NULL);
        printf("New client %d accepted in thread %ld with pid %d\n", client, pthread_self(), getpid());
        int ret = recv(client, buf, sizeof(buf), 0);
        if(ret <= 0) continue;
        buf[ret]=0;
        printf("Received from %d: %s", client, buf);
        char *msg = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Connection: close\r\n"         
                "\r\n"
                "<!DOCTYPE html>\n"
                "<html><head><meta charset=\"UTF-8\"></head>\n"
                "<body><h1>Xin chao cac ban</h1></body></html>";
        send(client,msg, strlen(msg),0);
        close(client);
    }
    return NULL;
}
