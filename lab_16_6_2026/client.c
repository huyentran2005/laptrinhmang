/*
 * Chat Client - TCP/IP  (giao diện Terminal)
 * Giao thức theo slide
 *
 * Cách dùng: ./chat_client [host] [port]
 *
 * Lệnh:
 *   /join <nick>          - Tham gia phòng
 *   /msg <noi dung>       - Gửi tin cho cả phòng
 *   /pmsg <nick> <tin>    - Gửi tin riêng
 *   /op <nick>            - Chuyển quyền OP
 *   /kick <nick>          - Kick người dùng (chỉ OP)
 *   /topic <chu de>       - Đặt chủ đề (chỉ OP)
 *   /quit                 - Thoát
 *   /help                 - Hướng dẫn
 *   <noi dung>            - Tương đương /msg (gửi cho cả phòng)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUF_SIZE 4096
#define NAME_LEN  64

static int  g_sock    = -1;
static char g_nick[NAME_LEN] = "";
static int  g_running = 1;

static void raw_send(const char *msg) {
    send(g_sock, msg, strlen(msg), 0);
}

/* ===== Thread nhận từ server ===== */
static void *recv_thread(void *arg) {
    (void)arg;
    char buf[BUF_SIZE];
    char line[BUF_SIZE];
    int  llen = 0;

    while (g_running) {
        int n = recv(g_sock, buf, sizeof(buf)-1, 0);
        if (n <= 0) {
            if (g_running) printf("\n[!] Mat ket noi server\n");
            g_running = 0;
            break;
        }
        buf[n] = '\0';

        for (int i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                line[llen] = '\0';
                if (llen > 0 && line[llen-1] == '\r') line[--llen] = '\0';
                if (llen == 0) { llen = 0; continue; }

                /* Parse thông điệp server */
                if (strncmp(line, "JOIN ", 5) == 0) {
                    printf("\n  *** %s da tham gia phong ***\n> ", line+5);
                } else if (strncmp(line, "MSG ", 4) == 0) {
                    /* MSG <nick> <noi dung> */
                    char from[NAME_LEN], msg[BUF_SIZE];
                    if (sscanf(line+4, "%63s %[^\n]", from, msg) == 2)
                        printf("\n  [%s] %s\n> ", from, msg);
                    else
                        printf("\n  %s\n> ", line+4);
                } else if (strncmp(line, "PMSG ", 5) == 0) {
                    char from[NAME_LEN], msg[BUF_SIZE];
                    if (sscanf(line+5, "%63s %[^\n]", from, msg) == 2)
                        printf("\n  [PM tu %s] %s\n> ", from, msg);
                    else
                        printf("\n  [PM] %s\n> ", line+5);
                } else if (strncmp(line, "OP ", 3) == 0) {
                    printf("\n  *** %s la chu phong ***\n> ", line+3);
                } else if (strncmp(line, "KICK ", 5) == 0) {
                    char kicked[NAME_LEN], by[NAME_LEN];
                    if (sscanf(line+5, "%63s %63s", kicked, by) == 2)
                        printf("\n  *** %s bi kick boi %s ***\n> ", kicked, by);
                    else
                        printf("\n  *** %s ***\n> ", line+5);
                } else if (strncmp(line, "TOPIC ", 6) == 0) {
                    char op[NAME_LEN], topic[BUF_SIZE];
                    if (sscanf(line+6, "%63s %[^\n]", op, topic) == 2)
                        printf("\n  *** Chu de moi [%s]: %s ***\n> ", op, topic);
                    else
                        printf("\n  *** Chu de: %s ***\n> ", line+6);
                } else if (strncmp(line, "QUIT ", 5) == 0) {
                    printf("\n  *** %s da roi phong ***\n> ", line+5);
                } else if (strncmp(line, "ERROR ", 6) == 0) {
                    printf("\n  [LOI] %s\n> ", line+6);
                } else {
                    printf("\n  %s\n> ", line);
                }
                fflush(stdout);
                llen = 0;
            } else {
                if (llen < BUF_SIZE-1) line[llen++] = buf[i];
            }
        }
    }
    return NULL;
}

static void print_help(void) {
    printf("\n+------------------------------------------------+\n");
    printf("|          CHAT CLIENT - HUONG DAN              |\n");
    printf("+------------------------------------------------+\n");
    printf("| /join <nick>         - Tham gia phong chat    |\n");
    printf("| /msg <noi dung>      - Gui tin cho ca phong   |\n");
    printf("| /pmsg <nick> <tin>   - Gui tin nhan rieng     |\n");
    printf("| /op <nick>           - Chuyen quyen chu phong |\n");
    printf("| /kick <nick>         - Dua nguoi dung ra      |\n");
    printf("| /topic <chu de>      - Dat chu de phong       |\n");
    printf("| /quit                - Thoat                  |\n");
    printf("| /help                - Hien thi huong dan     |\n");
    printf("| <noi dung>           - Gui cho ca phong       |\n");
    printf("+------------------------------------------------+\n\n");
}

int main(int argc, char *argv[]) {
    const char *host = "127.0.0.1";
    int         port = 8080;
    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = atoi(argv[2]);

    signal(SIGPIPE, SIG_IGN);

    g_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_sock < 0) { perror("socket"); exit(1); }

    struct hostent *he = gethostbyname(host);
    if (!he) { fprintf(stderr, "Khong tim thay host: %s\n", host); exit(1); }

    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(port);
    memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(g_sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("connect"); exit(1);
    }

    printf("================================================\n");
    printf("  DA KET NOI DEN %s:%d\n", host, port);
    printf("================================================\n");
    print_help();

    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread, NULL);
    pthread_detach(tid);

    char  input[BUF_SIZE];
    char  cmdbuf[BUF_SIZE + 32];

    printf("> ");
    fflush(stdout);

    while (g_running && fgets(input, sizeof(input), stdin)) {
        input[strcspn(input, "\r\n")] = '\0';
        if (!strlen(input)) { printf("> "); fflush(stdout); continue; }

        if (input[0] == '/') {
            if (strncmp(input, "/join ", 6) == 0) {
                char nick[NAME_LEN];
                sscanf(input+6, "%63s", nick);
                snprintf(cmdbuf, sizeof(cmdbuf), "JOIN %s\n", nick);
                strncpy(g_nick, nick, NAME_LEN-1);
                raw_send(cmdbuf);

            } else if (strncmp(input, "/msg ", 5) == 0) {
                snprintf(cmdbuf, sizeof(cmdbuf), "MSG %s\n", input+5);
                raw_send(cmdbuf);

            } else if (strncmp(input, "/pmsg ", 6) == 0) {
                char to[NAME_LEN], msg[BUF_SIZE];
                if (sscanf(input+6, "%63s %[^\n]", to, msg) == 2) {
                    snprintf(cmdbuf, sizeof(cmdbuf), "PMSG %s %s\n", to, msg);
                    raw_send(cmdbuf);
                } else printf("[!] Cu phap: /pmsg <nick> <tin nhan>\n");

            } else if (strncmp(input, "/op ", 4) == 0) {
                char nick[NAME_LEN];
                sscanf(input+4, "%63s", nick);
                snprintf(cmdbuf, sizeof(cmdbuf), "OP %s\n", nick);
                raw_send(cmdbuf);

            } else if (strncmp(input, "/kick ", 6) == 0) {
                char nick[NAME_LEN];
                sscanf(input+6, "%63s", nick);
                snprintf(cmdbuf, sizeof(cmdbuf), "KICK %s\n", nick);
                raw_send(cmdbuf);

            } else if (strncmp(input, "/topic ", 7) == 0) {
                snprintf(cmdbuf, sizeof(cmdbuf), "TOPIC %s\n", input+7);
                raw_send(cmdbuf);

            } else if (strcmp(input, "/quit") == 0) {
                raw_send("QUIT\n");
                g_running = 0;
                break;

            } else if (strcmp(input, "/help") == 0) {
                print_help();

            } else {
                printf("[!] Lenh khong hop le. Go /help de xem\n");
            }
        } else {
            /* Gõ thẳng = broadcast */
            if (!g_nick[0]) {
                printf("[!] Chua tham gia phong. Dung: /join <nick>\n");
            } else {
                snprintf(cmdbuf, sizeof(cmdbuf), "MSG %s\n", input);
                raw_send(cmdbuf);
            }
        }

        printf("> ");
        fflush(stdout);
    }

    printf("\n[!] Tam biet!\n");
    close(g_sock);
    return 0;
}