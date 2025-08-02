#ifndef CODE_H
#define CODE_H

#include <stdio.h>
#include <pcap.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <linux/hw_breakpoint.h>
#include <unistd.h>
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

extern struct perf_event_attr global_attr;

void packet_handler(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes);

#endif