#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#define PACKET_S 64
#define TTL_VAL 64
#define TIMEOUT_VAL 1
#define MAX_RECV_BUF_LEN 0xFFFF // Max incoming packet size.

// Global interrupt val to handle
int uninterrupted = 1;

// Signal handler
void sigintHandler(int sig_num) 
{ 
    /* Reset handler to catch SIGINT next time. 
       Refer http://en.cppreference.com/w/c/program/signal */
    signal(SIGINT, sigintHandler); 
    uninterrupted = 0;
} 

void print_usage()
{
    printf("Usage: Ping [-w timeout] [-t TTL] address\n");
}

int getAddress(struct sockaddr *sa, int salen, char *host)
{
    char serv[NI_MAXSERV];
    int hostlen = NI_MAXHOST,
        servlen = NI_MAXSERV,
        rc;

    rc = getnameinfo(
        sa,
        salen,
        host,
        hostlen,
        serv,
        servlen,
        NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0)
    {
        fprintf(stderr, "%s: getnameinfo failed: %d\n", __FILE__, rc);
        return rc;
    }
    return 0;
}

int reverseLookup(struct sockaddr *sa, int salen, char *host)
{
    int hostlen = NI_MAXHOST,
        rc;

    rc = getnameinfo(
        sa,
        salen,
        host,
        hostlen,
        NULL,
        0,
        0);
    if (rc != 0)
    {
        fprintf(stderr, "%s: getnameinfo failed: %d\n", __FILE__, rc);
        return rc;
    }
    return 0;
}

// ------------------------ IPV4 Ping and CHECK SUM ----------------------------------------------

// ICMP packet struct
struct packet
{
    struct icmphdr hdr;
    char msg[PACKET_S - sizeof(struct icmphdr)];
};

// Standard ICMP header check sum function
unsigned short checksum(void *b, int len)
{
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= sizeof(unsigned short))
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

void ping(struct sockaddr_in *server, int sockfd, int ttl, int timeout_val, char* target)
{

    // host info
    char ip[NI_MAXHOST];
    char server_name[NI_MAXHOST];
    getAddress((struct sockaddr *)server, sizeof(*server), ip);
    reverseLookup((struct sockaddr *)server, sizeof(*server), server_name);

    struct sockaddr_in r_addr;

    // Set socket ttl option
    setsockopt(sockfd, SOL_IP, IP_TTL, &ttl, sizeof(ttl));
    // Set socket timeout option
    struct timeval timeout;
    timeout.tv_sec = timeout_val;
    timeout.tv_usec = 0;
    time_t start, end;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    int count = 0;
    int send_count = 0;
    int receive_count = 0;
    struct packet pkt;

    // Time the whole process
    struct timespec allstart;
    struct timespec allend;
    clock_gettime(CLOCK_MONOTONIC, &allstart);

    while (uninterrupted)
    {
        sleep(1);
        struct timespec tstart;
        struct timespec tend;
        bzero(&pkt, sizeof(pkt));
        pkt.hdr.type = ICMP_ECHO;
        pkt.hdr.un.echo.id = getpid();
        for (int i = 0; i < sizeof(pkt.msg) - 1; i++)
        {
            pkt.msg[i] = i + '0';
        }
        pkt.msg[sizeof(pkt.msg) - 1] = '\0';
        pkt.hdr.un.echo.sequence = count;
        count++;
        // Insert the checksum into checksum field
        pkt.hdr.checksum = checksum(&pkt, sizeof(pkt));
        // Try sending the package
        start = clock();
        clock_gettime(CLOCK_MONOTONIC, &tstart);
        if (sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)server, sizeof(*server)) <= 0)
        {
            printf("Fail to send a package\n");
        }
        else
        {
            send_count++;
            int len = sizeof(r_addr);
            if (recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&r_addr, &len) <= 0)
            {
                printf("Fail to receive\n");
            }
            else
            {
                end = clock();
                clock_gettime(CLOCK_MONOTONIC, &tend);
                time_t time_diff = end - start;
                double diff_in_ms = (tend.tv_sec - tstart.tv_sec) * 1000 + ((double)(tend.tv_nsec - tstart.tv_nsec)) / 1000000;
                printf("%d bytes from %s (%s): icmp_seq=%d ttl=%d time=%.2f ms\n", PACKET_S, server_name, ip, count, ttl, diff_in_ms);
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &allend);
    double total_time_ms = (allend.tv_sec - allstart.tv_sec) * 1000 + ((double)(allend.tv_nsec - allstart.tv_nsec)) / 1000000;
    double pkt_loss = ((double)(send_count - receive_count))*100/send_count;
    printf("\n");
    printf("--- %s ping statistics ---\n", ip);
    printf("%d packets transmitted, %d received, %.2f%% packet loss, time %.2fms\n",send_count, receive_count, pkt_loss, total_time_ms);
}
// ---------------------------- End IPV4 ping and checksum-----------------------------------------------

// ----------------------------- IPV6 Ping and checksum --------------------------------------------------

struct packetv6
{
    struct icmp6_hdr hdr;
    char msg[PACKET_S - sizeof(struct icmp6_hdr)];
};

unsigned short checksumv6(int sockfd, void *b, int len, struct addrinfo *server_info)
{
    struct sockaddr_storage localif;
    int local_len = sizeof(localif);
    char tmp[MAX_RECV_BUF_LEN] = {'\0'},
         *ptr = NULL,
         proto = 0;
    int rc, total, length, i;
    unsigned int bytes;

    getsockname(sockfd, (struct sockaddr *)&localif, &len);

    ptr = tmp;
    total = 0;

    // Copy source address
    memcpy(ptr, &((struct sockaddr_in6 *)&localif)->sin6_addr, sizeof(struct sockaddr_in6));
    ptr += sizeof(struct in6_addr);
    total += sizeof(struct in6_addr);

    memcpy(ptr, &((struct sockaddr_in6 *)server_info->ai_addr)->sin6_addr, sizeof(struct sockaddr_in6 *));
    ptr += sizeof(struct in6_addr);
    total += sizeof(struct in6_addr);

    // Copy ICMP packet length
    length = htonl(len);
    memcpy(ptr, &length, sizeof(length));
    ptr += sizeof(length);
    total += sizeof(length);

    memset(ptr, 0, 3);
    ptr += 3;
    total += 3;

    proto = IPPROTO_ICMPV6;

    memcpy(ptr, &proto, sizeof(proto));
    ptr += sizeof(proto);
    total += sizeof(proto);

    // Copy the ICMP header and payload
    memcpy(ptr, b, len);
    ptr += len;
    total += len;

    for (i = 0; i < len % 2; i++)
    {
        *ptr = 0;
        ptr++;
        total++;
    }
    return checksum((unsigned short *)tmp, total);
}

void ping6(struct sockaddr_in6 *server, int sockfd, int ttl, int timeout_val, struct addrinfo *server_info, char* target)
{

    // Host info
    char ip[NI_MAXHOST];
    char server_name[NI_MAXHOST];
    getAddress((struct sockaddr *)server, sizeof(*server), ip);
    reverseLookup((struct sockaddr *)server, sizeof(*server), server_name);

    // Set socket ttl option
    struct sockaddr_in6 r_addr;
    setsockopt(sockfd, SOL_IPV6, IP_TTL, &ttl, sizeof(ttl));
    // Set socket timeout option
    struct timeval timeout;
    timeout.tv_sec = timeout_val;
    timeout.tv_usec = 0;
    time_t start, end;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    int count = 0;
    int send_count = 0;
    int receive_count = 0;
    struct packetv6 pkt;

    // Time the whole process
    struct timespec allstart;
    struct timespec allend;
    clock_gettime(CLOCK_MONOTONIC, &allstart);

    while (uninterrupted)
    {
        sleep(1);
        struct timespec tstart;
        struct timespec tend;
        // Fill in the packet
        bzero(&pkt, sizeof(pkt));
        pkt.hdr.icmp6_type = ICMP6_ECHO_REQUEST;
        pkt.hdr.icmp6_id = getpid();
        for (int i = 0; i < sizeof(pkt.msg) - 1; i++)
        {
            pkt.msg[i] = i + '0';
        }
        pkt.msg[sizeof(pkt.msg) - 1] = '\0';
        pkt.hdr.icmp6_seq = count;
        pkt.hdr.icmp6_cksum = 0;
        pkt.hdr.icmp6_cksum = checksumv6(sockfd, &pkt, sizeof(pkt), server_info);
        count++;
        start = clock();
        // Attempt to send and time
        clock_gettime(CLOCK_MONOTONIC, &tstart);
        if (sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)server, sizeof(*server)) <= 0)
        {
            printf("Fail to send a package\n");
        }
        else
        {
            send_count++;
            socklen_t len = sizeof(r_addr);
            if (recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&r_addr, &len) <= 0)
            {
                printf("Fail to receive\n");
            }
            else
            {
                end = clock();
                clock_gettime(CLOCK_MONOTONIC, &tend);
                time_t time_diff = end - start;
                double diff_in_ms = (tend.tv_sec - tstart.tv_sec) * 1000 + ((double)(tend.tv_nsec - tstart.tv_nsec)) / 1000000;
                receive_count++;
                printf("%d bytes from %s (%s): icmp_seq=%d ttl=%d time=%.2f ms\n", PACKET_S, server_name, ip, count, ttl, diff_in_ms);
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &allend);
    double total_time_ms = (allend.tv_sec - allstart.tv_sec) * 1000 + ((double)(allend.tv_nsec - allstart.tv_nsec)) / 1000000;
    double pkt_loss = ((double)(send_count - receive_count))*100/send_count;
    printf("\n");
    printf("--- %s ping statistics ---\n", target);
    printf("%d packets transmitted, %d received, %.2f%% packet loss, time %.2fms\n",send_count, receive_count, pkt_loss, total_time_ms);
}
// ---------------------------- End IPV6 ping and checksum-----------------------------------------------

int main(int argc, char *argv[])
{
    signal(SIGINT, sigintHandler);
    // Socket file descriptor
    int sockfd;
    // Arg char
    char ch;
    // Timeout and TTL value
    int ttl = TTL_VAL;
    int timeout = TIMEOUT_VAL;
    while ((ch = getopt(argc, argv, "T:w:")) != -1)
    {
        switch (ch)
        {
        case 'T':
            ttl = strtol(optarg, NULL, 0);
            break;
        case 'w':
            timeout = strtol(optarg, NULL, 0);
            break;
        case '?':
            print_usage();
            exit(1);
            break;
        }
    }

    // The remaining argument must not be empty and must be the only one left
    if (argc - optind != 1)
    {
        print_usage();
        exit(1);
    }

    // Get the ip address
    char *target = argv[optind];
    struct addrinfo *server_info;
    struct addrinfo hints;

    if (getaddrinfo(target, "0", NULL, &server_info) != 0)
    {
        printf("Address family not supported\n");
        exit(1);
    }
    else
    {

        // Set up socket
        // IPV4
        if (server_info->ai_family == AF_INET)
        {
            sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
            if (sockfd < 0)
            {
                printf("Fail to get a file descriptor. Try to run the command with admin\n");
                exit(1);
            }
            struct sockaddr_in *server = (struct sockaddr_in *)server_info->ai_addr;
            ping(server, sockfd, ttl, timeout, target);
        }
        //IPV6
        else if (server_info->ai_family == AF_INET6)
        {
            sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
            if (sockfd < 0)
            {
                printf("Fail to get a file descriptor. Try to run the command with admin\n");
                exit(1);
            }
            struct sockaddr_in6 *server = (struct sockaddr_in6 *)server_info->ai_addr;
            ping6(server, sockfd, ttl, timeout, server_info, target);
        }
    }
}
