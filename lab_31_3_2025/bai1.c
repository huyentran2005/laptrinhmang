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

// Hàm hỗ trợ xóa các ký tự xuống dòng và khoảng trắng ở cuối chuỗi
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

    // Thiết lập socket ở chế độ non-blocking
    unsigned long ul = 1;
    ioctl(listener, FIONBIO, &ul);

    // Cho phép tái sử dụng địa chỉ port
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
        // 1. Chấp nhận kết nối mới (không chờ đợi do non-blocking)
        int client = accept(listener, NULL, NULL);
        if (client != -1) {
            printf("New client connected: %d\n", client);
            ul = 1;
            ioctl(client, FIONBIO, &ul); // Thiết lập client socket là non-blocking
            clients[nclients++] = client;

            char *welcome = "Vui long nhap 'Ho ten' va 'MSSV' (VD: Nguyen Van An 20201234):\n";
            send(client, welcome, strlen(welcome), 0);
        }

        for (int i = 0; i < nclients; i++) {
            len = recv(clients[i], buf, sizeof(buf) - 1, 0);
            
            if (len == -1) {
                if (errno != EWOULDBLOCK) {
                    // Lỗi kết nối, xóa client
                    close(clients[i]);
                    clients[i] = clients[--nclients];
                    i--;
                }
            } else if (len == 0) {
                // Client ngắt kết nối
                printf("Client %d disconnected\n", clients[i]);
                close(clients[i]);
                clients[i] = clients[--nclients];
                i--;
            } else {
                // Nhận được dữ liệu
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
                    
                    // MSSV là từ cuối cùng, Tên là từ áp cuối
                    strcpy(mssv, words[count - 1]);
                    strcpy(name, words[count - 2]);
                    
                    // Lấy các chữ cái đầu của Họ và Tên đệm
                    for (int j = 0; j < count - 2; j++) {
                        char c = (char)tolower(words[j][0]);
                        strncat(initials, &c, 1);
                    }
                    
                    // Chuyển tên về chữ thường
                    for (int j = 0; name[j]; j++) name[j] = tolower(name[j]);
                    
                    char *short_mssv = mssv;
                    if (strlen(mssv) >= 8) {
                        short_mssv = mssv + 2; 
                    }

                    // Tạo email theo cấu trúc: ten.hotatMSSV@sis.hust.edu.vn
                    sprintf(email, "Email cua ban: %s.%s%s@sis.hust.edu.vn\n", name, initials, short_mssv);
                    send(clients[i], email, strlen(email), 0);
                } else {
                    char *err = "Dinh dang sai! Hay nhap: Ho Ten MSSV\n";
                    send(clients[i], err, strlen(err), 0);
                }
            }
        }
        //tránh chiếm dụng 100% CPU trong vòng lặp vô hạn
        usleep(10000); 
    }

    close(listener);
    return 0;
}