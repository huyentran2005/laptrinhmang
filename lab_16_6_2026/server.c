/*
 * Chat Server - TCP/IP
 * Giao thức CHÍNH XÁC theo file test:
 *
 * CLIENT -> SERVER:
 *   JOIN <nickname>
 *   MSG <message>
 *   PMSG <nickname> <message>
 *   OP <nickname>
 *   KICK <nickname>
 *   TOPIC <topic>
 *   QUIT
 *
 * SERVER -> CLIENT (người gửi):
 *   100 OK\n             - lệnh thành công
 *   200 NICKNAME IN USE\n
 *   201 INVALID NICK NAME\n
 *   202 UNKNOWN NICKNAME\n
 *   203 DENIED\n
 *
 * SERVER -> ALL (broadcast, KHÔNG gửi lại người gửi):
 *   JOIN <nickname>\n
 *   MSG <nickname> <message>\n
 *   PMSG <nickname> <message>\n   (chỉ gửi người nhận)
 *   OP <nickname>\n
 *   KICK <kicked> <op>\n
 *   TOPIC <op> <topic>\n
 *   QUIT <nickname>\n
 *
 * Lưu ý:
 *   - Người gửi JOIN nhận "100 OK\n", người KHÁC nhận "JOIN <nick>\n"
 *   - Nickname phải là chữ thường a-z, 0-9, không chứa chữ hoa -> 201
 *   - Nickname trùng -> 200
 *   - Nickname không tồn tại -> 202
 *   - Không phải OP -> 203
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT        9000
#define MAX_CLIENTS 50
#define BUF_SIZE    4096
#define NAME_LEN    64

typedef struct {
    int  fd;
    char nick[NAME_LEN];
    int  active;
    struct sockaddr_in addr;
} Client;

static Client clients[MAX_CLIENTS];
static int    client_count = 0;
static char   op_nick[NAME_LEN] = "";
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

/* ===== Tiện ích ===== */
static void send_fd(int fd, const char *msg) {
    send(fd, msg, strlen(msg), 0);
}

/* Gửi cho tất cả NGOẠI TRỪ fd */
static void broadcast_except(int exclude_fd, const char *msg) {
    for (int i = 0; i < client_count; i++)
        if (clients[i].active && clients[i].fd != exclude_fd)
            send_fd(clients[i].fd, msg);
}

/* Tìm theo nick (phải giữ lock trước) */
static int find_nick(const char *nick) {
    for (int i = 0; i < client_count; i++)
        if (clients[i].active && strcmp(clients[i].nick, nick) == 0)
            return i;
    return -1;
}

/* Tìm theo fd */
static int find_fd(int fd) {
    for (int i = 0; i < client_count; i++)
        if (clients[i].fd == fd)
            return i;
    return -1;
}

static int is_op(const char *nick) {
    return strcmp(op_nick, nick) == 0;
}

/* Kiểm tra nickname hợp lệ: chỉ chữ thường và số */
static int valid_nick(const char *nick) {
    if (!nick || !nick[0]) return 0;
    for (int i = 0; nick[i]; i++) {
        if (!islower((unsigned char)nick[i]) && !isdigit((unsigned char)nick[i]))
            return 0;
    }
    return 1;
}

/* ===== Xử lý lệnh ===== */

static void handle_join(int fd, int idx, char *args) {
    char nick[NAME_LEN] = {0};
    if (args) sscanf(args, "%63s", nick);

    /* Validate nickname */
    if (!valid_nick(nick)) {
        send_fd(fd, "201 INVALID NICK NAME\n");
        return;
    }

    pthread_mutex_lock(&mtx);

    if (clients[idx].active) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "201 INVALID NICK NAME\n");
        return;
    }

    if (find_nick(nick) != -1) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "200 NICKNAME IN USE\n");
        return;
    }

    strncpy(clients[idx].nick, nick, NAME_LEN - 1);
    clients[idx].active = 1;

    int first = (op_nick[0] == '\0');
    if (first) strncpy(op_nick, nick, NAME_LEN - 1);

    /* Trả 100 OK cho người join */
    send_fd(fd, "100 OK\n");

    /* Broadcast JOIN cho người khác */
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "JOIN %s\n", nick);
    broadcast_except(fd, buf);

    pthread_mutex_unlock(&mtx);

    printf("[JOIN] %s\n", nick);
}

static void handle_msg(int fd, int idx, char *args) {
    pthread_mutex_lock(&mtx);
    if (!clients[idx].active) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "203 DENIED\n");
        return;
    }
    char nick[NAME_LEN];
    strncpy(nick, clients[idx].nick, NAME_LEN - 1);
    pthread_mutex_unlock(&mtx);

    if (!args || !args[0]) { send_fd(fd, "203 DENIED\n"); return; }

    char msg[BUF_SIZE];
    strncpy(msg, args, sizeof(msg) - 1);
    msg[strcspn(msg, "\r\n")] = '\0';

    /* Trả 100 OK cho người gửi */
    send_fd(fd, "100 OK\n");

    /* Broadcast MSG <nick> <msg> cho người khác */
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "MSG %s %s\n", nick, msg);
    pthread_mutex_lock(&mtx);
    broadcast_except(fd, buf);
    pthread_mutex_unlock(&mtx);

    printf("[MSG] %s: %s\n", nick, msg);
}

static void handle_pmsg(int fd, int idx, char *args) {
    pthread_mutex_lock(&mtx);
    if (!clients[idx].active) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "203 DENIED\n");
        return;
    }
    char from[NAME_LEN];
    strncpy(from, clients[idx].nick, NAME_LEN - 1);
    pthread_mutex_unlock(&mtx);

    char to[NAME_LEN] = {0};
    char msg[BUF_SIZE] = {0};
    if (!args || sscanf(args, "%63s %[^\n]", to, msg) < 2) {
        send_fd(fd, "202 UNKNOWN NICKNAME\n");
        return;
    }

    pthread_mutex_lock(&mtx);
    int tidx = find_nick(to);
    if (tidx == -1) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "202 UNKNOWN NICKNAME\n");
        return;
    }

    /* Trả 100 OK cho người gửi */
    send_fd(fd, "100 OK\n");

    /* Gửi PMSG <from> <msg> cho người nhận */
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "PMSG %s %s\n", from, msg);
    send_fd(clients[tidx].fd, buf);
    pthread_mutex_unlock(&mtx);

    printf("[PMSG] %s -> %s: %s\n", from, to, msg);
}

static void handle_op(int fd, int idx, char *args) {
    pthread_mutex_lock(&mtx);
    if (!clients[idx].active) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "203 DENIED\n");
        return;
    }
    if (!is_op(clients[idx].nick)) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "203 DENIED\n");
        return;
    }

    char new_op[NAME_LEN] = {0};
    if (args) sscanf(args, "%63s", new_op);

    if (find_nick(new_op) == -1) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "202 UNKNOWN NICKNAME\n");
        return;
    }

    strncpy(op_nick, new_op, NAME_LEN - 1);

    /* Trả 100 OK cho người gửi */
    send_fd(fd, "100 OK\n");

    /* Broadcast OP <new_op> cho người khác */
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "OP %s\n", new_op);
    broadcast_except(fd, buf);
    pthread_mutex_unlock(&mtx);

    printf("[OP] Chu phong moi: %s\n", new_op);
}

static void handle_kick(int fd, int idx, char *args) {
    pthread_mutex_lock(&mtx);
    if (!clients[idx].active) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "203 DENIED\n");
        return;
    }
    if (!is_op(clients[idx].nick)) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "203 DENIED\n");
        return;
    }

    char kicked[NAME_LEN] = {0};
    if (args) sscanf(args, "%63s", kicked);

    int kidx = find_nick(kicked);
    if (kidx == -1) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "202 UNKNOWN NICKNAME\n");
        return;
    }

    char op_name[NAME_LEN];
    strncpy(op_name, clients[idx].nick, NAME_LEN - 1);

    /* Trả 100 OK cho OP */
    send_fd(fd, "100 OK\n");

    /* Broadcast KICK <kicked> <op> cho người khác (kể cả người bị kick) */
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "KICK %s %s\n", kicked, op_name);
    broadcast_except(fd, buf);  /* gửi tất cả ngoại trừ OP */

    /* Đánh dấu và đóng kết nối người bị kick */
    int kicked_fd = clients[kidx].fd;
    clients[kidx].active = 0;
    pthread_mutex_unlock(&mtx);

    close(kicked_fd);
    printf("[KICK] %s bi kick boi %s\n", kicked, op_name);
}

static void handle_topic(int fd, int idx, char *args) {
    pthread_mutex_lock(&mtx);
    if (!clients[idx].active) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "203 DENIED\n");
        return;
    }
    if (!is_op(clients[idx].nick)) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "203 DENIED\n");
        return;
    }

    if (!args || !args[0]) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "203 DENIED\n");
        return;
    }

    char topic[BUF_SIZE];
    strncpy(topic, args, sizeof(topic) - 1);
    topic[strcspn(topic, "\r\n")] = '\0';

    char nick[NAME_LEN];
    strncpy(nick, clients[idx].nick, NAME_LEN - 1);

    /* Trả 100 OK cho người gửi */
    send_fd(fd, "100 OK\n");

    /* Broadcast TOPIC <op> <topic> cho người khác */
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "TOPIC %s %s\n", nick, topic);
    broadcast_except(fd, buf);
    pthread_mutex_unlock(&mtx);

    printf("[TOPIC] %s: %s\n", nick, topic);
}

static void handle_quit(int fd, int idx) {
    pthread_mutex_lock(&mtx);
    if (!clients[idx].active) {
        pthread_mutex_unlock(&mtx);
        return;
    }

    char nick[NAME_LEN];
    strncpy(nick, clients[idx].nick, NAME_LEN - 1);
    clients[idx].active = 0;

    /* Trả 100 OK cho người quit */
    send_fd(fd, "100 OK\n");

    /* Broadcast QUIT <nick> cho người khác */
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "QUIT %s\n", nick);
    broadcast_except(fd, buf);

    /* Nếu OP quit -> chuyển OP */
    if (is_op(nick)) {
        op_nick[0] = '\0';
        for (int i = 0; i < client_count; i++) {
            if (clients[i].active && clients[i].fd != fd) {
                strncpy(op_nick, clients[i].nick, NAME_LEN - 1);
                char op_msg[BUF_SIZE];
                snprintf(op_msg, sizeof(op_msg), "OP %s\n", op_nick);
                broadcast_except(fd, op_msg);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx);

    printf("[QUIT] %s\n", nick);
}

/* ===== Thread xử lý client ===== */
static void *client_thread(void *arg) {
    int fd = *(int *)arg;
    free(arg);

    char buf[BUF_SIZE];
    char line[BUF_SIZE];
    int  llen = 0;

    while (1) {
        int n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';

        for (int i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                line[llen] = '\0';
                if (llen > 0 && line[llen-1] == '\r') line[--llen] = '\0';
                if (llen == 0) { llen = 0; continue; }

                printf("[RECV fd=%d] %s\n", fd, line);

                char cmd[32] = {0};
                char *args   = NULL;
                char *sp     = strchr(line, ' ');
                if (sp) {
                    int cl = sp - line < 31 ? sp - line : 31;
                    strncpy(cmd, line, cl);
                    args = sp + 1;
                } else {
                    strncpy(cmd, line, 31);
                }

                pthread_mutex_lock(&mtx);
                int idx = find_fd(fd);
                pthread_mutex_unlock(&mtx);
                if (idx == -1) goto done;

                if      (strcmp(cmd, "JOIN")  == 0) handle_join(fd, idx, args);
                else if (strcmp(cmd, "MSG")   == 0) handle_msg(fd, idx, args);
                else if (strcmp(cmd, "PMSG")  == 0) handle_pmsg(fd, idx, args);
                else if (strcmp(cmd, "OP")    == 0) handle_op(fd, idx, args);
                else if (strcmp(cmd, "KICK")  == 0) handle_kick(fd, idx, args);
                else if (strcmp(cmd, "TOPIC") == 0) handle_topic(fd, idx, args);
                else if (strcmp(cmd, "QUIT")  == 0) {
                    handle_quit(fd, idx);
                    goto done;
                }

                llen = 0;
            } else {
                if (llen < BUF_SIZE - 1) line[llen++] = buf[i];
            }
        }
    }

done:
    /* Ngắt kết nối đột ngột */
    pthread_mutex_lock(&mtx);
    int idx = find_fd(fd);
    if (idx != -1) {
        if (clients[idx].active) {
            char nick[NAME_LEN];
            strncpy(nick, clients[idx].nick, NAME_LEN - 1);
            clients[idx].active = 0;

            char buf2[BUF_SIZE];
            snprintf(buf2, sizeof(buf2), "QUIT %s\n", nick);
            broadcast_except(fd, buf2);

            if (is_op(nick)) {
                op_nick[0] = '\0';
                for (int i = 0; i < client_count; i++) {
                    if (clients[i].active && clients[i].fd != fd) {
                        strncpy(op_nick, clients[i].nick, NAME_LEN - 1);
                        char op_msg[BUF_SIZE];
                        snprintf(op_msg, sizeof(op_msg), "OP %s\n", op_nick);
                        broadcast_except(fd, op_msg);
                        break;
                    }
                }
            }
        }
        for (int i = idx; i < client_count - 1; i++)
            clients[i] = clients[i+1];
        client_count--;
    }
    pthread_mutex_unlock(&mtx);

    close(fd);
    return NULL;
}

/* ===== main ===== */
int main(int argc, char *argv[]) {
    int port = PORT;
    if (argc >= 2) port = atoi(argv[1]);

    signal(SIGPIPE, SIG_IGN);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in sa = {0};
    sa.sin_family      = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port        = htons(port);

    if (bind(sfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) { perror("bind"); exit(1); }
    if (listen(sfd, 10) < 0) { perror("listen"); exit(1); }

    printf("============================================\n");
    printf("  Chat Server dang chay tren port %d\n", port);
    printf("  Protocol: JOIN MSG PMSG OP KICK TOPIC QUIT\n");
    printf("============================================\n");

    while (1) {
        struct sockaddr_in ca;
        socklen_t cal = sizeof(ca);
        int cfd = accept(sfd, (struct sockaddr *)&ca, &cal);
        if (cfd < 0) { perror("accept"); continue; }

        pthread_mutex_lock(&mtx);
        if (client_count >= MAX_CLIENTS) {
            pthread_mutex_unlock(&mtx);
            close(cfd);
            continue;
        }
        clients[client_count].fd     = cfd;
        clients[client_count].active = 0;
        clients[client_count].addr   = ca;
        memset(clients[client_count].nick, 0, NAME_LEN);
        client_count++;
        pthread_mutex_unlock(&mtx);

        printf("[CONNECT] %s:%d fd=%d\n",
               inet_ntoa(ca.sin_addr), ntohs(ca.sin_port), cfd);

        int *p = malloc(sizeof(int));
        *p = cfd;
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, p);
        pthread_detach(tid);
    }

    close(sfd);
    return 0;
}