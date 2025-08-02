#include "core.h"

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

int main()
{
    int raw_socket;
    struct sockaddr_ll saddr;
    const buffer_len = 65536;
    char buffer[buffer_len];

    /*

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
}