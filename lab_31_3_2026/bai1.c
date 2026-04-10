#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <ctype.h>

void trim(char *s) {
    int l = strlen(s);
    while (l > 0 && (s[l-1] == '\n' || s[l-1] == '\r' || isspace(s[l-1]))) {
        s[--l] = 0;
    }
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    unsigned long ul = 1;
    ioctl(listener, FIONBIO, &ul);

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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

    printf("Server is listening on port 8080 (Non-blocking mode)...\n");

    int clients[64];
    int nclients = 0;
    char buf[256];
    int len;

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client != -1) {
            printf("New client connected: %d\n", client);
            ul = 1;
            ioctl(client, FIONBIO, &ul);
            clients[nclients++] = client;

            char *welcome = "Vui long nhap 'Ho ten' va 'MSSV':\n";
            send(client, welcome, strlen(welcome), 0);
        }

        for (int i = 0; i < nclients; i++) {
            len = recv(clients[i], buf, sizeof(buf) - 1, 0);
            
            if (len == -1) {
                if (errno != EWOULDBLOCK) {
                    close(clients[i]);
                    clients[i] = clients[--nclients];
                    i--;
                }
            } else if (len == 0) {
                printf("Client %d disconnected\n", clients[i]);
                close(clients[i]);
                clients[i] = clients[--nclients];
                i--;
            } else {
                buf[len] = 0;
                trim(buf);
                printf("Received from %d: %s\n", clients[i], buf);

               
                char *words[16];
                int count = 0;
                char *token = strtok(buf, " ");
                while (token != NULL && count < 16) {
                    words[count++] = token;
                    token = strtok(NULL, " ");
                }

                if (count >= 2) {
                    char email[128];
                    char name[64], mssv[20], initials[20] = "";
                    
                   
                    strcpy(mssv, words[count - 1]);
                    strcpy(name, words[count - 2]);
                    
                    for (int j = 0; j < count - 2; j++) {
                        char c = (char)tolower(words[j][0]);
                        strncat(initials, &c, 1);
                    }
                    
                    for (int j = 0; name[j]; j++) name[j] = tolower(name[j]);
                    
                    char *short_mssv = mssv;
                    if (strlen(mssv) >= 8) {
                        short_mssv = mssv + 2; 
                    }

                    sprintf(email, "Email cua ban: %s.%s%s@sis.hust.edu.vn\n", name, initials, short_mssv);
                    send(clients[i], email, strlen(email), 0);
                } else {
                    char *err = "Dinh dang sai! Hay nhap: Ho Ten MSSV\n";
                    send(clients[i], err, strlen(err), 0);
                }
            }
        }
        usleep(10000); 
    }

    close(listener);
    return 0;
}