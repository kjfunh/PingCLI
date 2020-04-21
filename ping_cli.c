#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <netinet/ip_icmp.h>
#include <stdlib.h>
#include <time.h>

#define PACKET_S 64
#define TTL_VAL 64
#define TIMEOUT_VAL 1

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

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

void print_usage()
{
    printf("Usage: Ping [-w timeout] [-t TTL] address\n");
}

void ping(struct sockaddr_in *server, int sockfd, int ttl, int timeout_val)
{
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

    // Ip address sending to
    char ip[NI_MAXHOST];
    strcpy(ip, inet_ntoa(server->sin_addr));

    while (1)
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

                //double diff_in_ms = ((double) time_diff/CLOCKS_PER_SEC)*1000000;

                double diff_in_ms = (tend.tv_sec - tstart.tv_sec) * 1000 + ((double)(tend.tv_nsec - tstart.tv_nsec)) / 1000000;

                receive_count++;
                printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%f ms\n", PACKET_S, ip, count, ttl, diff_in_ms);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    // Socket file descriptor
    int sockfd;
    // Arg char
    char ch;
    // Timeout and TTL value
    int ttl = TTL_VAL;
    int timeout = TIMEOUT_VAL;
    while ((ch = getopt(argc, argv, "t:w:")) != -1)
    {
        switch (ch)
        {
        case 't':
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
    struct hostent *server_info = gethostbyname(target);
    if (!server_info)
    {
        printf("Invalid destination\n");
    }
    else
    {
        // Set up sopcket
        // IPV4
        if (server_info->h_addrtype == AF_INET)
        {
            sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
            if (sockfd < 0)
            {
                printf("Fail to get a file descriptor. Try to run the command with admin\n");
                exit(1);
            }

            ping(&server, sockfd, ttl, timeout);
        }
        //IPV6
        else
        {
            sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
        }

        struct sockaddr_in server = {(sa_family_t)server_info->h_addrtype, htons(8000), *(struct in_addr *)server_info->h_addr};

        if (sockfd < 0)
        {
            printf("Fail to get a file descriptor. Try to run the command with admin\n");
            exit(1);
        }

        ping(&server, sockfd, ttl, timeout);
    }
}
