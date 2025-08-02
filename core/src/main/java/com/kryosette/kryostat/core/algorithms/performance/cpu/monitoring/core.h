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

// The structure of the Ethernet header
struct ethheader
{
    uint8_t ether_dhost[6];  // Destination MAC
    uint8_t ether_shost[6];  // Source MAC
    uint8_t hort ether_type; // Protocol type (IPv4/IPv6)
};

// IP header structure (standard 20-byte header)
struct ipheader
{
    unsigned char iph_ihl : 4, iph_ver : 4;           // Header length and version
    unsigned char iph_tos;                            // Service type
    unsigned short int iph_len;                       // Packet length
    unsigned short int iph_ident;                     // Identifier
    unsigned short int iph_flag : 3, iph_offset : 13; // Flags and offset
    unsigned char iph_ttl;                            // Lifetime
    unsigned char iph_protocol;                       // Protocol (TCP/UDP/ICMP)
    unsigned short int iph_chksum;                    // Checksum
    struct in_addr iph_sourceip;                      // Source IP
    struct in_addr iph_destip;                        // Destination IP
};

// TCP header structure
struct tcpheader
{
    unsigned short int th_sport; // Source port
    unsigned short int th_dport; // Destination port
    unsigned int th_seq;         // Sequence number
    unsigned int th_ack;         // Confirmation number
    unsigned char th_off : 4;    // Data offset
    unsigned char th_flags;      // Flags (SYN/ACK/FIN/RST)
    unsigned short int th_win;   // Window size
    unsigned short int th_sum;   // Checksum
    unsigned short int th_urp;   // Urgent pointer
};

#endif