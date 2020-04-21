#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <netinet/ip_icmp.h>
#define PACKET_S 64
#define TTL_VAL 64
#define TIMEOUT_VAL 1


// ICMP packet struct
struct packet
{
    struct icmphdr hdr;
    char msg[PACKET_S - sizeof(struct icmphdr)]; 
};


void ping(struct sockaddr_in* server, int sockfd, int ttl, int timeout)
{
    // Set time to live
    if(ttl == -1)
    {
        ttl = TTL_VAL;
    }

    // Time for request timeout

}

int main(int argc, char* argv[])
{
    // Socket file descriptor
    int sockfd;
    char ch;
    int ttl = TTL_VAL;
    int timeout = TIMEOUT_VAL;
    while((ch = getopt(argc, argv, "t:w:")) != -1)
    {
        switch (ch){
            case 't':
                std::cout << "t option\n";
                std::cout << optarg;
                //ttl = strtol(optarg, NULL, 0);
                break;
            case 'w':
                std::cout << "w option\n";
                std::cout << optarg;
                //timeout = strtol(optarg, NULL, 0);
        }
    }
    std::cout << "reach";
    // Get the ip address
    auto target = argv[optind];
    auto server_info = gethostbyname2(target, AF_INET);
    if(!server_info)
    {
        std::cout << "Invalid destination";
    }
    else
    {
        // Set up sopcket
        sockfd = socket(server_info->h_addrtype, SOCK_STREAM, 0);
        struct sockaddr_in server = {(sa_family_t) server_info->h_addrtype, 7000};
        memcpy(&server.sin_addr, server_info->h_addr_list[0], sizeof(server.sin_addr));

        if(sockfd == -1)
        {
            std::cout << "Fail to get a file desc";
            exit (1);
        }
        // Bind the address to the socket
        if(connect(sockfd, (struct sockaddr*) &server, sizeof(struct sockaddr_in)) == -1)
        {
            std::cout <<"Unable to connect";
            exit(1);
        }
        while(1)
        {

        }
    }
    
}