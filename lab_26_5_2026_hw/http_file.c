#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>

#define PORT 8081
#define BUFFER_SIZE 8192
#define ROOT_DIR    "."          /* Thu muc goc server phuc vu */

/* ===================== HTTP RESPONSE ===================== */

void send_html_response(int client_socket, const char *status, const char *body) {
    char header[512];
    snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, strlen(body));
    send(client_socket, header, strlen(header), 0);
    send(client_socket, body,   strlen(body),   0);
}

/* Gui file nhi phan / van ban voi Content-Type phu hop */
void send_file_response(int client_socket, const char *filepath, const char *mime) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        send_html_response(client_socket, "404 Not Found",
            "<!DOCTYPE html><html><body><h1>404 - File not found</h1></body></html>");
        return;
    }

    /* Lay kich thuoc file */
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* Gui header */
    char header[512];
    snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n",
        mime, filesize);
    send(client_socket, header, strlen(header), 0);

    /* Gui noi dung file theo tung chunk */
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        send(client_socket, chunk, n, 0);
    }
    fclose(fp);
}

/* ===================== UTILITY ===================== */

/* Giai ma %XX trong URL thanh ky tu tuong ung */
void url_decode(const char *src, char *dest, int dest_size) {
    int i = 0, j = 0;
    while (src[i] && j < dest_size - 1) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = { src[i+1], src[i+2], '\0' };
            dest[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dest[j++] = ' ';
            i++;
        } else {
            dest[j++] = src[i++];
        }
    }
    dest[j] = '\0';
}

/* Xac dinh MIME type dua tren phan mo rong */
const char *get_mime_type(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    ext++;  /* bo dau '.' */

    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) return "text/html; charset=UTF-8";
    if (strcasecmp(ext, "txt")  == 0) return "text/plain; charset=UTF-8";
    if (strcasecmp(ext, "css")  == 0) return "text/css";
    if (strcasecmp(ext, "js")   == 0) return "application/javascript";
    if (strcasecmp(ext, "json") == 0) return "application/json";
    if (strcasecmp(ext, "c")    == 0 || strcasecmp(ext, "h") == 0)  return "text/plain; charset=UTF-8";
    if (strcasecmp(ext, "py")   == 0 || strcasecmp(ext, "sh") == 0) return "text/plain; charset=UTF-8";

    /* Anh */
    if (strcasecmp(ext, "jpg")  == 0 || strcasecmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcasecmp(ext, "png")  == 0) return "image/png";
    if (strcasecmp(ext, "gif")  == 0) return "image/gif";
    if (strcasecmp(ext, "bmp")  == 0) return "image/bmp";
    if (strcasecmp(ext, "webp") == 0) return "image/webp";
    if (strcasecmp(ext, "svg")  == 0) return "image/svg+xml";
    if (strcasecmp(ext, "ico")  == 0) return "image/x-icon";

    /* Audio */
    if (strcasecmp(ext, "mp3")  == 0) return "audio/mpeg";
    if (strcasecmp(ext, "wav")  == 0) return "audio/wav";
    if (strcasecmp(ext, "ogg")  == 0) return "audio/ogg";
    if (strcasecmp(ext, "flac") == 0) return "audio/flac";

    /* Video */
    if (strcasecmp(ext, "mp4")  == 0) return "video/mp4";
    if (strcasecmp(ext, "webm") == 0) return "video/webm";
    if (strcasecmp(ext, "avi")  == 0) return "video/x-msvideo";
    if (strcasecmp(ext, "mkv")  == 0) return "video/x-matroska";
    if (strcasecmp(ext, "mov")  == 0) return "video/quicktime";

    /* PDF */
    if (strcasecmp(ext, "pdf")  == 0) return "application/pdf";

    return "application/octet-stream";
}

/* Kiem tra duong dan co phai thu muc khong */
int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return 1;
    return 0;
}

/* Kiem tra duong dan co phai file khong */
int is_file(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) return 1;
    return 0;
}

/* ===================== HANDLERS ===================== */

/* Liet ke noi dung thu muc, tra ve trang HTML:
   - Thu muc: in dam, co lien ket
   - File   : in nghieng, co lien ket
*/
void handle_directory(int client_socket, const char *url_path, const char *fs_path) {
    DIR *dir = opendir(fs_path);
    if (!dir) {
        send_html_response(client_socket, "403 Forbidden",
            "<!DOCTYPE html><html><body><h1>403 - Khong the mo thu muc</h1></body></html>");
        return;
    }

    /* --- Xay dung trang HTML --- */
    char html[65536] = {0};
    char tmp[2048];

    snprintf(tmp, sizeof(tmp),
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<title>Thu muc: %s</title>"
        "<style>"
        "  body { font-family: monospace; margin: 40px; background: #1e1e2e; color: #cdd6f4; }"
        "  h2   { color: #89b4fa; border-bottom: 1px solid #313244; padding-bottom: 8px; }"
        "  ul   { list-style: none; padding: 0; }"
        "  li   { padding: 4px 0; }"
        "  a    { text-decoration: none; }"
        "  .dir { font-weight: bold;   color: #89dceb; }"  /* Thu muc: in dam  */
        "  .dir::before { content: '\U0001F4C1 '; }"
        "  .fil { font-style: italic; color: #a6e3a1; }"  /* File   : in nghieng */
        "  .fil::before { content: '\U0001F4C4 '; }"
        "  .up  { color: #f38ba8; font-weight: bold; }"
        "  a:hover { text-decoration: underline; }"
        "  .path { color: #6c7086; font-size: 0.85em; margin-bottom: 16px; }"
        "</style></head><body>"
        "<h2>\U0001F4C1 %s</h2>"
        "<p class='path'>Duong dan: %s</p><ul>",
        url_path, url_path, fs_path);
    strncat(html, tmp, sizeof(html) - strlen(html) - 1);

    /* Neu khong phai goc, them lien ket len thu muc cha */
    if (strcmp(url_path, "/") != 0) {
        /* Tinh URL cua thu muc cha */
        char parent[1024];
        strncpy(parent, url_path, sizeof(parent) - 1);
        parent[sizeof(parent)-1] = '\0';
        /* Xoa dau '/' cuoi neu co */
        int plen = strlen(parent);
        if (plen > 1 && parent[plen-1] == '/') parent[--plen] = '\0';
        char *slash = strrchr(parent, '/');
        if (slash) {
            if (slash == parent) slash[1] = '\0';  /* goc */
            else                 slash[0] = '\0';
        }
        snprintf(tmp, sizeof(tmp),
            "<li><a class='up' href='%s'>&#8679; Len thu muc cha</a></li>",
            parent);
        strncat(html, tmp, sizeof(html) - strlen(html) - 1);
    }

    /* Doc tung muc trong thu muc */
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Bo qua cac muc an */
        if (entry->d_name[0] == '.') continue;

        /* Duong dan day du tren he thong file */
        char child_fs[1024];
        snprintf(child_fs, sizeof(child_fs), "%s/%s", fs_path, entry->d_name);

        /* URL tuong ung */
        char child_url[1024];
        if (strcmp(url_path, "/") == 0)
            snprintf(child_url, sizeof(child_url), "/%s", entry->d_name);
        else
            snprintf(child_url, sizeof(child_url), "%s/%s", url_path, entry->d_name);

        if (is_directory(child_fs)) {
            /* Thu muc: in dam */
            snprintf(tmp, sizeof(tmp),
                "<li><a class='dir' href='%s'>%s/</a></li>",
                child_url, entry->d_name);
        } else {
            /* File: in nghieng */
            snprintf(tmp, sizeof(tmp),
                "<li><a class='fil' href='%s'><i>%s</i></a></li>",
                child_url, entry->d_name);
        }
        strncat(html, tmp, sizeof(html) - strlen(html) - 1);
    }
    closedir(dir);

    strncat(html, "</ul></body></html>", sizeof(html) - strlen(html) - 1);
    send_html_response(client_socket, "200 OK", html);
}

/* Phuc vu noi dung mot file */
void handle_file(int client_socket, const char *fs_path) {
    const char *mime = get_mime_type(fs_path);
    send_file_response(client_socket, fs_path, mime);
}

/* Xu ly moi request: phan tich URL -> thu muc hay file */
void handle_request(int client_socket, const char *raw_url) {
    /* Giai ma URL */
    char url_decoded[1024] = {0};
    url_decode(raw_url, url_decoded, sizeof(url_decoded));

    /* Xay dung duong dan tren he thong file */
    char fs_path[2048];
    if (strcmp(url_decoded, "/") == 0) {
        snprintf(fs_path, sizeof(fs_path), "%s", ROOT_DIR);
    } else {
        snprintf(fs_path, sizeof(fs_path), "%s%s", ROOT_DIR, url_decoded);
    }

    /* Phan loai: thu muc hay file */
    if (is_directory(fs_path)) {
        handle_directory(client_socket, url_decoded, fs_path);
    } else if (is_file(fs_path)) {
        handle_file(client_socket, fs_path);
    } else {
        char err[2048];
        snprintf(err, sizeof(err),
            "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body>"
            "<h1>404 Not Found</h1><p>Khong tim thay: %s</p>"
            "<a href='/'>Ve trang chu</a></body></html>",
            url_decoded);
        send_html_response(client_socket, "404 Not Found", err);
    }
}

/* ===================== MAIN LOOP ===================== */

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind"); return 1;
    }
    listen(server_fd, 10);

    printf("=== HTTP File Server ===\n");
    printf("Thu muc goc : %s\n", ROOT_DIR);
    printf("Truy cap    : http://localhost:%d\n", PORT);
    printf("Nhan Ctrl+C de dung server.\n\n");

    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (client_socket < 0) { perror("accept"); continue; }

        char buffer[BUFFER_SIZE] = {0};
        read(client_socket, buffer, sizeof(buffer) - 1);

        /* Chi xu ly GET request */
        if (strncmp(buffer, "GET ", 4) == 0) {
            /* Trich xuat URL: "GET /path HTTP/1.1" */
            char url[1024] = {0};
            char *start = buffer + 4;          /* bo qua "GET " */
            char *end   = strchr(start, ' ');
            if (end) {
                int len = end - start;
                if (len >= (int)sizeof(url)) len = sizeof(url) - 1;
                strncpy(url, start, len);
            }

            /* Bo query string neu co: "/path?..." -> "/path" */
            char *qmark = strchr(url, '?');
            if (qmark) *qmark = '\0';

            handle_request(client_socket, url);
        } else {
            send_html_response(client_socket, "405 Method Not Allowed",
                "<!DOCTYPE html><html><body>"
                "<h1>405 - Chi ho tro phuong thuc GET</h1>"
                "</body></html>");
        }

        close(client_socket);
    }

    close(server_fd);
    return 0;
}