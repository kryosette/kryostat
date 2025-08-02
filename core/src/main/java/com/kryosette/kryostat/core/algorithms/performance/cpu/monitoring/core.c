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

/*
u_char is a type alias, commonly found in C and C++ programming,
that refers to unsigned char.
*/
void packet_handler(u_char *user, const struct, const u_char *bytes)
