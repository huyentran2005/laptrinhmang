#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<pthread.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>


struct User{
    int id;
    int joined;
};
int numClients = 0;
struct User users[100];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int find_pair(int client){
    for(int i =0; i< numClients; i++){
        if(client == users[i].id){
            if(i%2 == 0){
                if(i+1 < numClients && users[i+1].joined == 1) return i+1;
                else return -1;
            } else{
                if(i-1 >=0 && users[i-1].joined == 1) return i-1;
                else return -1;
            }
        }
    }
    return -1;
}

void close_pair(int client){
    int idx_pair = find_pair(client);
    close(client);
    close(users[idx_pair].id);
    users[idx_pair] = users[numClients - 1];
    numClients--;
    if(idx_pair %2 ==0){
        if(idx_pair+1 <numClients){
            users[idx_pair + 1] = users[numClients - 1];
            numClients--;
        }
    } else{
        if(idx_pair-1 >=0){
            users[idx_pair - 1] = users[numClients - 1];
            numClients--;
        }
    }
    
}
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
    pthread_t thread_id;
    while(1){
        int client = accept(listener, NULL, NULL);
        printf("New client connected: %d\n", client);
        
        users[numClients].id = client;
        users[numClients].joined = 1;
        numClients++;
        pthread_create(&thread_id, NULL, thread_proc, &client);
        pthread_detach(thread_id);
    }
    return 0;
}

void *thread_proc(void *arg){
    int client = *(int *)arg;
    char buf[256];
    while(1){
       
        int ret = recv(client, buf, sizeof(buf), 0);
        if(ret <= 0) break;
        buf[ret]=0;
        if(strncmp(buf, "exit",4) ==0){
            break;
        } 
        pthread_mutex_lock(&lock);
        int idx_pair = find_pair(client);
        pthread_mutex_unlock(&lock);
        char msg[1024];
        if(idx_pair == -1){
            strcpy(msg,"Error: No pair\n");
            send(client, msg, strlen(msg), 0);
            continue;
        }
        
        sprintf(msg,">> Client id %d: %s",users[idx_pair].id, buf);
        send(users[idx_pair].id, msg, strlen(msg), 0);
    }
    close_pair(client);
}
