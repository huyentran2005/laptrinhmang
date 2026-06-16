/*
 * Chat Server - TCP/IP
 * Giao thức theo slide:
 *
 * CLIENT -> SERVER:
 *   JOIN <nickname>
 *   MSG <room message>
 *   PMSG <nickname> <message>
 *   OP <nickname>
 *   KICK <nickname>
 *   TOPIC <topic>
 *   QUIT
 *
 * SERVER -> ALL CLIENTS (broadcast):
 *   JOIN <nickname>
 *   MSG <nickname> <room message>
 *   PMSG <nickname> <message>       (chỉ gửi cho người nhận)
 *   OP <nickname>
 *   KICK <kicked nickname> <op nickname>
 *   TOPIC <op nickname> <topic>
 *   QUIT <nickname>
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

#define PORT        8080
#define MAX_CLIENTS 50
#define BUF_SIZE    4096
#define NAME_LEN    64

typedef struct {
    int  fd;
    char nick[NAME_LEN];
    int  active;       /* 1 = đã JOIN */
    struct sockaddr_in addr;
} Client;

static Client clients[MAX_CLIENTS];
static int    client_count = 0;
static char   op_nick[NAME_LEN] = "";   /* Chủ phòng hiện tại */
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

/* ===== Tiện ích ===== */
static void send_fd(int fd, const char *msg) {
    send(fd, msg, strlen(msg), 0);
}

/* Gửi cho tất cả client đang active */
static void broadcast(const char *msg) {
    for (int i = 0; i < client_count; i++)
        if (clients[i].active)
            send_fd(clients[i].fd, msg);
}

/* Gửi cho tất cả TRỪ fd */
static void broadcast_except(int fd, const char *msg) {
    for (int i = 0; i < client_count; i++)
        if (clients[i].active && clients[i].fd != fd)
            send_fd(clients[i].fd, msg);
}

/* Tìm theo nick */
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

/* ===== Xử lý lệnh ===== */
static void handle_join(int fd, int idx, char *args) {
    char nick[NAME_LEN] = {0};
    sscanf(args, "%63s", nick);
    if (!nick[0]) { send_fd(fd, "ERROR Thieu nickname\n"); return; }

    pthread_mutex_lock(&mtx);
    if (clients[idx].active) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Ban da tham gia roi\n");
        return;
    }
    if (find_nick(nick) != -1) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Nickname da ton tai\n");
        return;
    }

    strncpy(clients[idx].nick, nick, NAME_LEN - 1);
    clients[idx].active = 1;

    /* Người đầu tiên vào là OP */
    int no_op = (op_nick[0] == '\0');
    if (no_op) strncpy(op_nick, nick, NAME_LEN - 1);

    /* Broadcast JOIN <nickname> */
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "JOIN %s\n", nick);
    broadcast(buf);  /* Gửi cả cho người vừa join */
    pthread_mutex_unlock(&mtx);

    if (no_op) {
        /* Thông báo OP cho người mới */
        char op_msg[BUF_SIZE];
        snprintf(op_msg, sizeof(op_msg), "OP %s\n", nick);
        pthread_mutex_lock(&mtx);
        broadcast(op_msg);
        pthread_mutex_unlock(&mtx);
    }

    printf("[JOIN] %s (%s)\n", nick, inet_ntoa(clients[idx].addr.sin_addr));
}

static void handle_msg(int fd, int idx, char *args) {
    pthread_mutex_lock(&mtx);
    if (!clients[idx].active) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Chua tham gia phong\n");
        return;
    }
    char nick[NAME_LEN];
    strncpy(nick, clients[idx].nick, NAME_LEN - 1);
    pthread_mutex_unlock(&mtx);

    if (!args || !args[0]) { send_fd(fd, "ERROR Tin nhan trong\n"); return; }

    /* Cắt \r\n */
    char msg[BUF_SIZE];
    strncpy(msg, args, sizeof(msg) - 1);
    msg[strcspn(msg, "\r\n")] = '\0';

    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "MSG %s %s\n", nick, msg);

    pthread_mutex_lock(&mtx);
    broadcast(buf);
    pthread_mutex_unlock(&mtx);

    printf("[MSG] %s: %s\n", nick, msg);
}

static void handle_pmsg(int fd, int idx, char *args) {
    pthread_mutex_lock(&mtx);
    if (!clients[idx].active) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Chua tham gia phong\n");
        return;
    }
    char from[NAME_LEN];
    strncpy(from, clients[idx].nick, NAME_LEN - 1);
    pthread_mutex_unlock(&mtx);

    char to[NAME_LEN], msg[BUF_SIZE];
    if (sscanf(args, "%63s %[^\n]", to, msg) < 2) {
        send_fd(fd, "ERROR Cu phap: PMSG <nick> <message>\n");
        return;
    }

    pthread_mutex_lock(&mtx);
    int tidx = find_nick(to);
    if (tidx == -1) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Nguoi dung khong ton tai\n");
        return;
    }
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "PMSG %s %s\n", from, msg);
    send_fd(clients[tidx].fd, buf);
    /* Cũng gửi lại cho người gửi để họ thấy tin nhắn của mình */
    send_fd(fd, buf);
    pthread_mutex_unlock(&mtx);

    printf("[PMSG] %s -> %s: %s\n", from, to, msg);
}

static void handle_op(int fd, int idx, char *args) {
    pthread_mutex_lock(&mtx);
    if (!clients[idx].active) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Chua tham gia phong\n");
        return;
    }
    if (!is_op(clients[idx].nick)) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Ban khong phai chu phong\n");
        return;
    }
    char new_op[NAME_LEN] = {0};
    sscanf(args, "%63s", new_op);
    if (!new_op[0]) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Thieu nickname\n");
        return;
    }
    if (find_nick(new_op) == -1) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Nguoi dung khong ton tai\n");
        return;
    }
    strncpy(op_nick, new_op, NAME_LEN - 1);

    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "OP %s\n", new_op);
    broadcast(buf);
    pthread_mutex_unlock(&mtx);

    printf("[OP] Chu phong moi: %s\n", new_op);
}

static void handle_kick(int fd, int idx, char *args) {
    pthread_mutex_lock(&mtx);
    if (!clients[idx].active) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Chua tham gia phong\n");
        return;
    }
    if (!is_op(clients[idx].nick)) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Ban khong phai chu phong\n");
        return;
    }
    char kicked[NAME_LEN] = {0};
    sscanf(args, "%63s", kicked);
    if (!kicked[0]) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Thieu nickname\n");
        return;
    }
    int kidx = find_nick(kicked);
    if (kidx == -1) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Nguoi dung khong ton tai\n");
        return;
    }

    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "KICK %s %s\n", kicked, clients[idx].nick);
    broadcast(buf);

    /* Đánh dấu người bị kick là không active, đóng kết nối */
    int kicked_fd = clients[kidx].fd;
    clients[kidx].active = 0;
    pthread_mutex_unlock(&mtx);

    close(kicked_fd);
    printf("[KICK] %s bi kick boi %s\n", kicked, clients[idx].nick);
}

static void handle_topic(int fd, int idx, char *args) {
    pthread_mutex_lock(&mtx);
    if (!clients[idx].active) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Chua tham gia phong\n");
        return;
    }
    if (!is_op(clients[idx].nick)) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Ban khong phai chu phong\n");
        return;
    }
    char topic[BUF_SIZE];
    if (!args || !args[0]) {
        pthread_mutex_unlock(&mtx);
        send_fd(fd, "ERROR Thieu chu de\n");
        return;
    }
    strncpy(topic, args, sizeof(topic) - 1);
    topic[strcspn(topic, "\r\n")] = '\0';

    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "TOPIC %s %s\n", clients[idx].nick, topic);
    broadcast(buf);
    pthread_mutex_unlock(&mtx);

    printf("[TOPIC] %s dat chu de: %s\n", clients[idx].nick, topic);
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

    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "QUIT %s\n", nick);
    broadcast_except(fd, buf);

    /* Nếu OP quit, chuyển OP cho người đầu tiên còn lại */
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

    char  buf[BUF_SIZE];
    char  line[BUF_SIZE];
    int   llen = 0;

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

                /* Tách lệnh và tham số */
                char cmd[32] = {0};
                char *sp = strchr(line, ' ');
                char *args = NULL;
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
                if (idx == -1) break;

                if      (strcmp(cmd, "JOIN")  == 0) handle_join(fd, idx, args ? args : "");
                else if (strcmp(cmd, "MSG")   == 0) handle_msg(fd, idx, args ? args : "");
                else if (strcmp(cmd, "PMSG")  == 0) handle_pmsg(fd, idx, args ? args : "");
                else if (strcmp(cmd, "OP")    == 0) handle_op(fd, idx, args ? args : "");
                else if (strcmp(cmd, "KICK")  == 0) handle_kick(fd, idx, args ? args : "");
                else if (strcmp(cmd, "TOPIC") == 0) handle_topic(fd, idx, args ? args : "");
                else if (strcmp(cmd, "QUIT")  == 0) {
                    handle_quit(fd, idx);
                    goto done;
                }
                else send_fd(fd, "ERROR Lenh khong hop le\n");

                llen = 0;
            } else {
                if (llen < BUF_SIZE - 1) line[llen++] = buf[i];
            }
        }
    }

done:
    /* Xử lý ngắt kết nối đột ngột */
    pthread_mutex_lock(&mtx);
    int idx = find_fd(fd);
    if (idx != -1 && clients[idx].active) {
        pthread_mutex_unlock(&mtx);
        handle_quit(fd, idx);
        pthread_mutex_lock(&mtx);
    }
    /* Xóa khỏi danh sách */
    if (idx != -1) {
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
    printf("  Giao thuc: JOIN MSG PMSG OP KICK TOPIC QUIT\n");
    printf("============================================\n");

    while (1) {
        struct sockaddr_in ca;
        socklen_t cal = sizeof(ca);
        int cfd = accept(sfd, (struct sockaddr *)&ca, &cal);
        if (cfd < 0) { perror("accept"); continue; }

        pthread_mutex_lock(&mtx);
        if (client_count >= MAX_CLIENTS) {
            pthread_mutex_unlock(&mtx);
            send_fd(cfd, "ERROR Server day\n");
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