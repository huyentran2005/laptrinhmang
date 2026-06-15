#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#define SERVER_HOST "lebavui.io.vn" 
#define CONTROL_PORT 21 
#define BUFFER_SIZE 4096

#define MSSV "20235347"
#define NGAY_SINH "08"

/**
 * Hàm gửi một câu lệnh FTP tới Server và nhận phản hồi về.
 * @param sock: Socket của kênh điều khiển 
 * @param cmd: Chuỗi câu lệnh FTP cần gửi (Nếu NULL thì chỉ nhận phản hồi)
 * @param response: Bộ đệm dùng để lưu chuỗi phản hồi trả về từ Server
 */
void send_ftp_cmd(int sock, const char *cmd, char *response) {
    char buf[BUFFER_SIZE];
    memset(buf, 0, sizeof(buf));
    
    if (cmd != NULL) {
        send(sock, cmd, strlen(cmd), 0);
        printf(">> %s", cmd);
    }
    
    int bytes_received = recv(sock, buf, sizeof(buf) - 1, 0);
    if (bytes_received > 0) {
        buf[bytes_received] = '\0';
        printf("<< %s", buf);
        if (response) {
            strcpy(response, buf);
        }
    }
}

/**
 * Hàm bóc tách phản hồi của lệnh PASV để lấy IP, Port và khởi tạo kết nối kênh dữ liệu.
 * @param pasv_resp: Chuỗi phản hồi từ lệnh PASV
 * @return: Socket của kênh dữ liệu đã kết nối thành công, hoặc -1 nếu lỗi
 */
int connect_data_channel(char *pasv_resp) {
    char *start = strchr(pasv_resp, '(');
    char *end = strchr(pasv_resp, ')');
    if (!start || !end) return -1;
    
    *end = '\0';
    start++;
    
    int h1, h2, h3, h4, p1, p2;
    sscanf(start, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2);
    
    char ip[32];
    sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    int port = p1 * 256 + p2;
    
    int data_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in data_addr;
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &data_addr.sin_addr);
    
    if (connect(data_sock, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0) {
        perror("Kết nối kênh dữ liệu thất bại");
        return -1;
    }
    return data_sock;
}

/**
 * Hàm đảo ngược một chuỗi ký tự
 * @param str: Con trỏ trỏ tới chuỗi ký tự
 * @param len: Chiều dài của chuỗi cần đảo ngược
 */
void reverse_string(char *str, int len) {
    int i = 0, j = len - 1;
    while (i < j) {
        char temp = str[i];
        str[i] = str[j];
        str[j] = temp; // Tráo đổi ký tự đầu và cuối tiến dần vào giữa
        i++;
        j--;
    }
}

int main() {
    int ctrl_sock;
    struct sockaddr_in server_addr;
    struct hostent *he;
    char cmd[256], resp[BUFFER_SIZE];
    char question_filename[128] = {0};
    char answer_filename[128] = {0};
    char file_content[256] = {0};

    // 1. Phân giải tên miền ra địa chỉ IP tương ứng
    if ((he = gethostbyname(SERVER_HOST)) == NULL) {
        herror("gethostbyname");
        return 1;
    }

    // 2. Tạo Socket điều khiển (TCP)
    ctrl_sock = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(CONTROL_PORT);
    server_addr.sin_addr = *((struct in_addr *)he->h_addr_list[0]);

    // 3. Kết nối tới kênh điều khiển của FTP Server
    if (connect(ctrl_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Kết nối tới Server thất bại");
        return 1;
    }
    
    send_ftp_cmd(ctrl_sock, NULL, resp);

    // 4. Gửi tên đăng nhập: USER user_<MSSV>
    sprintf(cmd, "USER user_%s\r\n", MSSV);
    send_ftp_cmd(ctrl_sock, cmd, resp);

    // 5. Tính toán mật khẩu: 4 số cuối MSSV + Ngày sinh
    char pass[10];
    const char *mssv_ptr = MSSV + strlen(MSSV) - 4;
    sprintf(pass, "%s%s", mssv_ptr, NGAY_SINH);
    
    // Gửi mật khẩu: PASS <mật_khẩu>
    sprintf(cmd, "PASS %s\r\n", pass);
    send_ftp_cmd(ctrl_sock, cmd, resp);

    // 6. Yêu cầu chuyển sang chế độ Passive để chuẩn bị lấy danh sách file
    sprintf(cmd, "PASV\r\n");
    send_ftp_cmd(ctrl_sock, cmd, resp);
    int data_sock = connect_data_channel(resp);

    // Gửi lệnh LIST để lấy danh sách các tệp đang có trên thư mục Server
    sprintf(cmd, "LIST\r\n");
    send_ftp_cmd(ctrl_sock, cmd, resp);

    // Đọc danh sách file trả về từ Kênh dữ liệu
    char list_buf[BUFFER_SIZE] = {0};
    int bytes_read = recv(data_sock, list_buf, sizeof(list_buf) - 1, 0);
    close(data_sock);
    send_ftp_cmd(ctrl_sock, NULL, resp);

    // 7. Tìm kiếm file câu hỏi có dạng "question_xxxx.txt" trong danh sách vừa nhận
    char *q_ptr = strstr(list_buf, "question_");
    if (q_ptr == NULL) {
        printf("Không tìm thấy file câu hỏi trên server!\n");
        close(ctrl_sock);
        return 1;
    }
    
    // Tách lấy chính xác tên file câu hỏi (ngắt bởi khoảng trắng hoặc xuống dòng)
    sscanf(q_ptr, "%s", question_filename);
    printf("--> Phát hiện file câu hỏi: %s\n", question_filename);

    // 8. Xin mở kênh dữ liệu mới (PASV) để tải nội dung file câu hỏi
    sprintf(cmd, "PASV\r\n");
    send_ftp_cmd(ctrl_sock, cmd, resp);
    data_sock = connect_data_channel(resp);

    // Gửi lệnh RETR để tải file câu hỏi về
    sprintf(cmd, "RETR %s\r\n", question_filename);
    send_ftp_cmd(ctrl_sock, cmd, resp);

    // Đọc nội dung chữ bên trong file câu hỏi qua kênh dữ liệu
    memset(file_content, 0, sizeof(file_content));
    bytes_read = recv(data_sock, file_content, sizeof(file_content) - 1, 0);
    file_content[bytes_read] = '\0';
    close(data_sock);
    send_ftp_cmd(ctrl_sock, NULL, resp);

    printf("--> Nội dung file question (Gốc): %s\n", file_content);

    // 9. Tạo tên file câu trả lời 
    char *suffix = strchr(question_filename, '_'); 
    sprintf(answer_filename, "answer%s", suffix);

    // 10. Xử lý chuẩn hóa chuỗi: Xóa các ký tự xuống dòng (\n, \r) thừa ở cuối nội dung file câu hỏi
    int content_len = strlen(file_content);
    while(content_len > 0 && (file_content[content_len-1] == '\n' || file_content[content_len-1] == '\r')) {
        file_content[content_len-1] = '\0';
        content_len--;
    }
    
    // Đảo ngược chuỗi nội dung
    reverse_string(file_content, content_len);
    printf("--> Nội dung file answer (Đảo ngược): %s\n", file_content);

    // 11. Xin mở kênh dữ liệu (PASV) để chuẩn bị tải file đáp án lên Server
    sprintf(cmd, "PASV\r\n");
    send_ftp_cmd(ctrl_sock, cmd, resp);
    data_sock = connect_data_channel(resp);

    // Gửi lệnh STOR để ghi file lên Server
    sprintf(cmd, "STOR %s\r\n", answer_filename);
    send_ftp_cmd(ctrl_sock, cmd, resp);

    // Đẩy dữ liệu đáp án đã đảo ngược sang kênh dữ liệu
    send(data_sock, file_content, strlen(file_content), 0);
    close(data_sock);
    send_ftp_cmd(ctrl_sock, NULL, resp);

    // 12. Gửi lệnh ngắt kết nối (QUIT) và dọn dẹp giải phóng các Socket điều khiển
    sprintf(cmd, "QUIT\r\n");
    send_ftp_cmd(ctrl_sock, cmd, resp);
    close(ctrl_sock);

    return 0;
} 