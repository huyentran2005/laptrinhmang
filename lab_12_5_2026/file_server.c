#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

void signal_handler(int sig) {
    int pid = wait(NULL);
    printf("Child process terminated: %d\n", pid);
}
struct dirFile{
    char name[50];
    long size;
};
int check(char *name, int len, struct dirFile *files){
    for(int i=0; i< len; i++){
        if(strcmp(name, files[i].name) == 0){
            return i;
        }
    }
    return -1;
}
int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);
    
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }
    
    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }
    
    printf("Server is listening on port 8080...\n");
    
    signal(SIGCHLD, signal_handler);

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (fork() == 0) {
            // Xu ly trong tien trinh con
            close(listener);

            int flag = 0;
            char buf[256];
            char msg[100];
            
            struct dirFile files[100];
            int len =0;
            DIR *dir;
            struct dirent *entry;
            dir = opendir(".");
            if(dir == NULL) exit(1);

            while((entry = readdir(dir)) !=NULL){
                if(strcmp(entry->d_name,".") == 0 || strcmp(entry->d_name,"..") == 0) continue;
                strcpy(files[len].name, entry->d_name);
                struct stat fileStat;
                if(stat(entry->d_name, &fileStat) ==0 )
                    files[len].size = fileStat.st_size;
                len++;
            }
            if(len == 0) strcpy(msg,"ERROR No files to download \r\n");
            else sprintf(msg,"OK %d\r\n",len);
            send(client,msg, strlen(msg),0);
            for(int i=0; i<= len; i++){
                if(i != len)
                    sprintf(msg,"%s   %ld bytes\r\n",files[i].name, files[i].size);
                else 
                    strcpy(msg,"\r\n\r\n");
                send(client, msg, strlen(msg),0);
            }
            int ret = recv(client,buf,sizeof(buf),0);
            if( ret <= 0){
                perror("recv() failed");
                exit(0);
            }
            buf[strcspn(buf,"\n")] = 0;
            int idx ;
            if((idx = check(buf, len, files)) != -1){
                sprintf(msg,"OK %ld\r\n ---------Content---------\n",files[idx].size);
                send(client, msg, strlen(msg),0);
                FILE *f = fopen(files[idx].name,"r");

                char line[1000];
                while(fgets(line,sizeof(len),f) != NULL)
                    send(client, line, strlen(line),0);
                fclose(f);
                
            } else {
                strcpy(msg,"ERROR No files to download\r\n");
                send(client, msg, strlen(msg),0);
            }
            exit(0);
        }
        // Xu ly o tien trinh cha
        close(client);
    }

    close(listener);
    return 0;
}