#include "core.h"
#include <liburing.h>
#include <linux/if_packet.h>
#include <net/if.h>

/*
int syscall(SYS_perf_event_open, struct perf_event_attr *attr,
                   pid_t pid, int cpu, int group_fd, unsigned long flags);
*/
static int setup_perf_counter()
{
    struct perf_event_attr global_attr = {
        .type = PERF_TYPE_HARDWARE,
        .config = PERF_COUNT_HW_CPU_CYCLES,
        .size = sizeof(global_attr)};
    return syscall(SYS_perf_event_open, &global_attr, 0, -1, -1, 0); // default
}

#define BUFFER_SIZE 2048
#define ENTRIES_COUNT 32

struct io_uring ring;
int raw_socket;

/*
    SO_SNDBUF: Sets the size of the send buffer, which stores data waiting to be transmitted over the network

    SO_RCVBUF: Sets the size of the receive buffer, which stores incoming data until your application processes it


    ❗ERRORS

       EACCES User tried to send to a broadcast address without having
              the broadcast flag set on the socket.

       EFAULT An invalid memory address was supplied.

       EINVAL Invalid argument.

       EMSGSIZE
              Packet too big.  Either Path MTU Discovery is enabled (the
              IP_MTU_DISCOVER socket flag) or the packet size exceeds the
              maximum allowed IPv4 packet size of 64 kB.

       EOPNOTSUPP
              Invalid flag has been passed to a socket call (like
              MSG_OOB).

       EPERM  The user doesn't have permission to open raw sockets.  Only
              processes with an effective user ID of 0 or the CAP_NET_RAW
              attribute may do that.

       EPROTO An ICMP error has arrived reporting a parameter problem.
*/
int create_raw_socket(void)
{
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0)
    {
        switch (errno)
        {
        case EPERM:
            fputs("FATAL: Требуются root-права (CAP_NET_RAW)\n", stderr);
            break;
        case EACCES:
            fputs("FATAL: Доступ запрещён (проверьте SELinux/AppArmor)\n", stderr);
            break;
        default:
            fprintf(stderr, "FATAL: Ошибка сокета: %s\n", strerror(errno));
        }
        return -1;
    }
    return sock;
}

/*
    SQE (Submission Queue Entry) — request for an operation (for example, "read data from socket").

    CQE (Completion Queue Entry) — the result of the operation (for example, "100 bytes read").
*/
void add_recv_request(int sock)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe)
    {
        fprintf(stderr, "Не удалось получить SQE\n");
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

void process_packets()
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
            printf("Пойман пакет на %d байт\n", cqe->res);
            // Здесь можно разбирать Ethernet/IP/TCP заголовки
        }
        else if (cqe->res < 0)
        {
            printf("Ошибка: %s\n", strerror(-cqe->res));
        }

        free(buffer);
        io_uring_cqe_seen(&ring, cqe);
        add_recv_request(raw_socket);
    }
}

int main()
{
    const int buffer_len = 65536; // 64KB (receive - 128KB)
    char buffer[buffer_len];
    struct sockaddr_ll saddr;

    // Инициализация io_uring
    if (io_uring_queue_init(ENTRIES_COUNT, &ring, 0) < 0)
    {
        perror("io_uring_queue_init");
        exit(1);
    }

    // Создание RAW сокета
    raw_socket = create_raw_socket();
    if (raw_socket < 0)
    {
        exit(1);
    }

    // Привязка сокета к интерфейсу
    memset(&saddr, 0, sizeof(saddr)); // There may be random values (garbage) in the saddr if it is not zeroed.
    // bind() requires all unused fields of the structure to be null. But memset() does it in one line.
    saddr.sll_family = AF_PACKET;
    saddr.sll_protocol = htons(ETH_P_ALL);
    saddr.sll_ifindex = if_nametoindex("eth0");
    if (saddr.sll_ifindex == 0)
    {
        perror("if_nametoindex");
        close(raw_socket);
        exit(1);
    }

    if (bind(raw_socket, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
    {
        perror("bind failed");
        close(raw_socket);
        exit(1);
    }

    printf("Sniffing is running.\n");

    // Добавляем первый запрос
    add_recv_request(raw_socket);

    // Обработка пакетов
    process_packets();

    // Очистка
    io_uring_queue_exit(&ring);
    close(raw_socket);
    return 0;
}