#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>

int main() {
    SSL_library_init();

    const SSL_METHOD *my_ssl_method = TLS_client_method();

    SSL_CTX *my_ssl_context = SSL_CTX_new(my_ssl_method);
    if (my_ssl_context == NULL) {
        printf("ERROR to create SSL context.\n");
        return 1;
    }

    SSL *my_ssl = SSL_new(my_ssl_context);
    if (my_ssl == NULL) {
        printf("ERROR to create SSL.\n");
        return 1;
    }

    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("185.199.111.153");
    addr.sin_port = htons(443);

    if (connect(client, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("ERROR to connect.\n");
        return 1;
    }

    SSL_set_fd(my_ssl, client);

    if (SSL_connect(my_ssl) <= 0) {
        printf("ERROR to SSL connect.\n");
        return 1;
    }

    // Dùng GET để lấy được nội dung (body) của file
    char req[2048] = "GET /videos/ecard.mp4 HTTP/1.1\r\nHost: lebavui.id.vn\r\nConnection: close\r\n\r\n";
    SSL_write(my_ssl, req, strlen(req));

    FILE *fp = fopen("ecard.mp4", "wb");
    if (fp == NULL) {
        printf("ERROR to open output file.\n");
        return 1;
    }

    char buf[4096];
    int len;
    int header_done = 0;     // đã tách xong header HTTP hay chưa
    long total = 0;

    while ((len = SSL_read(my_ssl, buf, sizeof(buf))) > 0) {
        char *data = buf;
        int data_len = len;

        if (!header_done) {
            // Tìm dấu kết thúc header HTTP: \r\n\r\n
            char *p = memmem(buf, len, "\r\n\r\n", 4);
            if (p != NULL) {
                int header_len = (p - buf) + 4;
                // In ra header HTTP để kiểm tra
                fwrite(buf, 1, header_len, stdout);

                data = p + 4;
                data_len = len - header_len;
                header_done = 1;
            } else {
                // Header chưa đọc hết trong lần read này
                fwrite(buf, 1, len, stdout);
                continue;
            }
        }

        if (data_len > 0) {
            fwrite(data, 1, data_len, fp);
            total += data_len;
        }
    }

    printf("\nDa luu %ld bytes vao file ecard.mp4\n", total);

    fclose(fp);
    SSL_shutdown(my_ssl);
    SSL_free(my_ssl);
    SSL_CTX_free(my_ssl_context);
    close(client);
    return 0;
}