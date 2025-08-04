#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <errno.h>
#include <liburing.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>

#define BUFFER_SIZE 4096
#define ENTRIES_COUNT 32
#define LOCALHOST_IP "127.0.0.1"

struct io_uring ring;
int raw_socket;

// Портовая конфигурация
typedef struct
{
    int frontend_port;
    int backend_ports[3];
    int backend_count;
} port_config_t;

port_config_t config = {
    .frontend_port = 3000,
    .backend_ports = {8088, 8091, 8092},
    .backend_count = 3};

// Ethernet header
struct ethheader
{
    uint8_t ether_dhost[6];
    uint8_t ether_shost[6];
    uint16_t ether_type;
};

// IP header
struct ipheader
{
    unsigned char iph_ihl : 4, iph_ver : 4;
    unsigned char iph_tos;
    unsigned short iph_len;
    unsigned short iph_ident;
    unsigned short iph_flag : 3, iph_offset : 13;
    unsigned char iph_ttl;
    unsigned char iph_protocol;
    unsigned short iph_chksum;
    struct in_addr iph_sourceip;
    struct in_addr iph_destip;
};

// TCP header
struct tcpheader
{
    unsigned short th_sport;
    unsigned short th_dport;
    unsigned int th_seq;
    unsigned int th_ack;
    unsigned char th_off : 4;
    unsigned char th_flags;
    unsigned short th_win;
    unsigned short th_sum;
    unsigned short th_urp;
};

// HTTP statistics
typedef struct
{
    size_t frontend_requests;
    size_t backend_responses;
    size_t other_traffic;
    size_t malformed_packets;
} http_stats_t;

http_stats_t http_stats = {0};

int create_raw_socket(void)
{
    int sock = socket(AF_PACKET, SOCK_RAW | SOCK_NONBLOCK, htons(ETH_P_ALL));
    if (sock < 0)
    {
        perror("socket");
        return -1;
    }

    // Увеличиваем буфер
    int buf_size = 2 * 1024 * 1024; // 2MB
    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size)) < 0)
    {
        perror("setsockopt SO_RCVBUF");
    }

    return sock;
}

void add_recv_request(int sock)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe)
    {
        fprintf(stderr, "Failed to get SQE\n");
        return;
    }

    char *buffer = malloc(BUFFER_SIZE);
    if (!buffer)
    {
        perror("malloc");
        return;
    }

    struct msghdr msg = {0};
    struct iovec iov = {
        .iov_base = buffer,
        .iov_len = BUFFER_SIZE};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    io_uring_prep_recvmsg(sqe, sock, &msg, 0);
    io_uring_sqe_set_data(sqe, buffer);
    io_uring_submit(&ring);
}

bool is_target_port(int port)
{
    // Проверяем порты фронтенда и бекенда
    if (port == config.frontend_port)
        return true;

    for (int i = 0; i < config.backend_count; i++)
    {
        if (port == config.backend_ports[i])
        {
            return true;
        }
    }
    return false;
}

void process_http_traffic(const char *payload, size_t len,
                          const char *src_ip, int src_port,
                          const char *dst_ip, int dst_port)
{
    // Определяем направление трафика
    bool is_frontend = (src_port == config.frontend_port);
    bool is_backend = false;

    for (int i = 0; i < config.backend_count; i++)
    {
        if (dst_port == config.backend_ports[i])
        {
            is_backend = true;
            break;
        }
    }

    if (len >= 3 && strncmp(payload, "GET", 3) == 0)
    {
        if (is_frontend)
        {
            printf("[FRONTEND] GET %s:%d -> %s:%d\n",
                   src_ip, src_port, dst_ip, dst_port);
            http_stats.frontend_requests++;
        }
    }
    else if (len >= 4 && strncmp(payload, "POST", 4) == 0)
    {
        if (is_frontend)
        {
            printf("[FRONTEND] POST %s:%d -> %s:%d\n",
                   src_ip, src_port, dst_ip, dst_port);
            http_stats.frontend_requests++;
        }
    }
    else if (len >= 4 && strncmp(payload, "HTTP", 4) == 0)
    {
        if (is_backend)
        {
            printf("[BACKEND] Response %s:%d <- %s:%d\n",
                   dst_ip, dst_port, src_ip, src_port);
            http_stats.backend_responses++;
        }
    }
    else
    {
        http_stats.other_traffic++;
    }
}

void process_packet(char *buffer, int len)
{
    // Basic validation
    if (len < (sizeof(struct ethheader) + sizeof(struct ipheader) + sizeof(struct tcpheader)))
    {
        http_stats.malformed_packets++;
        return;
    }

    struct ethheader *eth = (struct ethheader *)buffer;
    if (ntohs(eth->ether_type) != ETH_P_IP)
        return;

    struct ipheader *ip = (struct ipheader *)(buffer + sizeof(struct ethheader));
    if (ip->iph_ihl < 5 || ip->iph_protocol != IPPROTO_TCP)
        return;

    int ip_header_len = ip->iph_ihl * 4;
    struct tcpheader *tcp = (struct tcpheader *)(buffer + sizeof(struct ethheader) + ip_header_len);
    int tcp_header_len = tcp->th_off * 4;

    // Get ports
    int src_port = ntohs(tcp->th_sport);
    int dst_port = ntohs(tcp->th_dport);

    // Check if this is our local traffic
    char src_ip[INET_ADDRSTRLEN], dst_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ip->iph_sourceip), src_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip->iph_destip), dst_ip, INET_ADDRSTRLEN);

    if (strcmp(src_ip, LOCALHOST_IP) != 0 && strcmp(dst_ip, LOCALHOST_IP) != 0)
    {
        return; // Не локальный трафик
    }

    if (!is_target_port(src_port) && !is_target_port(dst_port))
    {
        return; // Не наши порты
    }

    // Process HTTP payload
    char *payload = buffer + sizeof(struct ethheader) + ip_header_len + tcp_header_len;
    int payload_len = len - (sizeof(struct ethheader) + ip_header_len + tcp_header_len);

    if (payload_len > 0)
    {
        process_http_traffic(payload, payload_len, src_ip, src_port, dst_ip, dst_port);
    }
}

void process_completions()
{
    struct io_uring_cqe *cqe;
    while (1)
    {
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0)
        {
            perror("io_uring_wait_cqe");
            break;
        }

        char *buffer = (char *)io_uring_cqe_get_data(cqe);
        if (cqe->res > 0)
        {
            process_packet(buffer, cqe->res);
        }
        else if (cqe->res < 0)
        {
            fprintf(stderr, "Error: %s\n", strerror(-cqe->res));
        }

        free(buffer);
        io_uring_cqe_seen(&ring, cqe);
        add_recv_request(raw_socket);
    }
}

void print_stats()
{
    printf("\n=== Local HTTP Traffic Statistics ===\n");
    printf("Frontend requests: %zu\n", http_stats.frontend_requests);
    printf("Backend responses: %zu\n", http_stats.backend_responses);
    printf("Other local traffic: %zu\n", http_stats.other_traffic);
    printf("Malformed packets: %zu\n", http_stats.malformed_packets);
    printf("====================================\n");
}

void handle_signal(int sig)
{
    printf("\nShutting down...\n");
    print_stats();
    io_uring_queue_exit(&ring);
    close(raw_socket);
    exit(0);
}

int main()
{
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (io_uring_queue_init(ENTRIES_COUNT, &ring, 0) < 0)
    {
        perror("io_uring_queue_init");
        return 1;
    }

    raw_socket = create_raw_socket();
    if (raw_socket < 0)
    {
        io_uring_queue_exit(&ring);
        return 1;
    }

    // Для локального трафика можно использовать lo интерфейс
    struct sockaddr_ll saddr = {0};
    saddr.sll_family = AF_PACKET;
    saddr.sll_protocol = htons(ETH_P_ALL);
    saddr.sll_ifindex = if_nametoindex("lo"); // Слушаем loopback интерфейс
    if (saddr.sll_ifindex == 0)
    {
        perror("if_nametoindex");
        close(raw_socket);
        io_uring_queue_exit(&ring);
        return 1;
    }

    if (bind(raw_socket, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
    {
        perror("bind failed");
        close(raw_socket);
        io_uring_queue_exit(&ring);
        return 1;
    }

    printf("Local HTTP Traffic Analyzer started\n");
    printf("Monitoring ports: Frontend:%d, Backends:%d,%d,%d\n",
           config.frontend_port,
           config.backend_ports[0],
           config.backend_ports[1],
           config.backend_ports[2]);
    printf("Press Ctrl+C to stop...\n");

    add_recv_request(raw_socket);
    process_completions();

    io_uring_queue_exit(&ring);
    close(raw_socket);
    return 0;
}