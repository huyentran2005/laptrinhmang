#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 8080

// Hàm tiện ích để gửi HTTP Response kèm dữ liệu JSON
void send_json_response(int client_socket, const char *status, const char *json_body) {
    char response[2048];
    
    // Xây dựng HTTP Header chuẩn chỉnh trả về JSON định dạng UTF-8
    sprintf(response,
            "HTTP/1.1 %s\r\n"
            "Content-Type: application/json; charset=UTF-8\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            status, strlen(json_body), json_body);
            
    send(client_socket, response, strlen(response), 0);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // 1. Tạo Socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Tạo socket thất bại");
        exit(EXIT_FAILURE);
    }

    // Gắn cấu hình để tránh lỗi "Address already in use" khi khởi động lại server nhanh
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt thất bại");
        exit(EXIT_FAILURE);
    }

    // 2. Cấu hình địa chỉ IP và Cổng (Port) cho Server
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Lắng nghe ở mọi card mạng
    address.sin_port = htons(PORT);

    // 3. Bind socket vào cổng 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind thất bại");
        exit(EXIT_FAILURE);
    }

    // 4. Lắng nghe kết nối (Hàng đợi tối đa 10 kết nối)
    if (listen(server_fd, 10) < 0) {
        perror("Listen thất bại");
        exit(EXIT_FAILURE);
    }

    printf("Server đang chạy tại cổng: http://localhost:%d\n", PORT);
    printf("Sẵn sàng nhận request...\n\n");

    // 5. Vòng lặp vô tận để chấp nhận các kết nối từ Client
    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept thất bại");
            continue;
        }

        // Đọc dữ liệu Request từ Client gửi lên
        char buffer[2048] = {0};
        read(client_socket, buffer, sizeof(buffer) - 1);
        
        // In request thô ra màn hình terminal để dễ quan sát (Tùy chọn)
        printf("--- Nhận Request ---\n%s\n", buffer);

        // Phân tích dòng đầu tiên của HTTP Request (Ví dụ: "GET /me HTTP/1.1")
        if (strstr(buffer, "GET /me ") == buffer || strstr(buffer, "GET /me HTTP")) {
            // Trường hợp /me
            const char *json_me = "{\n  \"name\": \"Nguyễn Văn A\",\n  \"mssv\": \"20211234\"\n}";
            send_json_response(client_socket, "200 OK", json_me);
            
        } else if (strstr(buffer, "GET /friend ") == buffer || strstr(buffer, "GET /friend HTTP")) {
            // Trường hợp /friend
            const char *json_friend = "{\n  \"name\": \"Trần Thị B\",\n  \"mssv\": \"20215678\"\n}";
            send_json_response(client_socket, "200 OK", json_friend);
            
        } else {
            // Các đường dẫn khác -> Trả về lỗi 404 Not Found
            const char *json_error = "{\n  \"error\": \"Đường dẫn không hợp lệ. Vui lòng dùng /me hoặc /friend\"\n}";
            send_json_response(client_socket, "404 Not Found", json_error);
        }

        // Đóng kết nối với client hiện tại để giải phóng socket
        close(client_socket);
        printf("--------------------\n\n");
    }

    // Đóng server (Thực tế vòng lặp phía trên vô tận nên dòng này để làm cảnh)
    close(server_fd);
    return 0;
}