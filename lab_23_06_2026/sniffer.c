#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/ip.h>   // struct iphdr

int main()
{
    int sock_raw = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock_raw < 0)
    {
        printf("Failed to create socket: %d\n", errno);
        return 1;
    }

    unsigned char *buffer = (unsigned char *)malloc(65535);
    int saddr_size, data_size;
    struct sockaddr saddr;

    unsigned char ip_protocol;

    while (1)
    {
        saddr_size = sizeof(saddr);
        data_size = recvfrom(sock_raw, buffer, 65535, 0, &saddr, (socklen_t *)&saddr_size);
        if (data_size < 0)
        {
            printf("Recvfrom error, failed to get packets\n");
            return 1;
        }

        // Ép kiểu phần đầu buffer thành header IP
        struct iphdr *ip_header = (struct iphdr *)buffer;

        struct in_addr src_addr, dst_addr;
        src_addr.s_addr = ip_header->saddr;
        dst_addr.s_addr = ip_header->daddr;

        printf("Data size: %d\n", data_size);
        printf("Source IP      : %s\n", inet_ntoa(src_addr));
        printf("Destination IP : %s\n", inet_ntoa(dst_addr));

        for (int i = 0; i < 40; i++)
            printf("%x ", buffer[i]);
        printf("\n");

        memcpy(&ip_protocol, buffer + 9, 1);
        if (ip_protocol == 1)
            printf("ICMP\n");
        else if (ip_protocol == 6)
            printf("TCP\n");
        else if (ip_protocol == 17)
            printf("UDP\n");

        printf("------------------------------------\n");
    }

    close(sock_raw);
    return 0;
}