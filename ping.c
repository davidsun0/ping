#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

// for networking and sending ICMP packets
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>

// for DNS lookup
#include <netdb.h>
#include <arpa/inet.h>

// for timer to send packets at regular intervals
#include <time.h>
#include <unistd.h>

struct icmp_packet {
    struct icmphdr header;
    // icmp packets are a total of 64 bytes long
    char body[64 - sizeof(struct icmphdr)];
};

uint16_t icmp_checksum(uint16_t* data, size_t len) {
    uint32_t acc = 0;
    for(size_t i = 0; i < len / sizeof(uint16_t); i ++) {
        acc += data[i];
    }
    acc = (acc & 0xFFFF) + (acc >> 16);
    acc += acc >> 16;
    return ~acc;
}                           

int main(int argc, char** argv) {
    int ttl = 64;
    int waitms = 1000;
    
    int i;
    for(i = 1; i < argc - 1; i ++) {
        if(strcmp(argv[i], "-w") == 0 && argc > i + 1) {
            waitms = atoi(argv[i + 1]);
            i ++;
        } else if(strcmp(argv[i], "-t") == 0 && argc > i + 1) {
            ttl = atoi(argv[i + 1]);
            i ++;
        } else {
            printf("usage: %s [-t ttl] [-w deadline] destination\n", argv[0]);
            exit(1);
        }
    }
    
    if(i >= argc) {
        printf("usage: %s [-t ttl] [-w deadline] destination\n", argv[0]);
        exit(1);
    }
    // DNS lookup
    char* host_name = argv[argc - 1];
    struct hostent* host = gethostbyname(host_name);

    // construct socket
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = host->h_addrtype;
    dest_addr.sin_port = 0;
    dest_addr.sin_addr.s_addr = *(long*)host->h_addr;

    // connect to host
    // ipv6 doesn't work :(
    // int sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        fprintf(stderr, "Failed to open raw socket. Are you root?\n");
        perror("socket error");
        exit(EXIT_FAILURE);
    }
    if(connect(sock, (struct sockaddr *) &dest_addr, sizeof dest_addr) < 0) {
        perror("failed to connect socket");
        exit(EXIT_FAILURE);
    }       

    // timeout if response doesn't arrive in time
    struct timeval waittime;
    waittime.tv_sec = waitms >= 1000 ? waitms / 1000 : 0;
    waittime.tv_usec = (waitms % 1000) * 1000;
    if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &waittime, sizeof waittime) < 0) {
        perror("failed to set wait timeout");
        exit(EXIT_FAILURE);
    }

    if(setsockopt(sock, SOL_IP, IP_TTL, &ttl, sizeof ttl) < 0) {
        perror("failed to set time to live");
        exit(EXIT_FAILURE);
    }
    // packet information and buffer for response message
    struct icmp_packet packet;
    uint16_t sequence_num = 0;
    char* buffer = malloc(sizeof(char) * 512);

    // timer and variable for delay in ms
    struct timespec start_time;
    double msdelta;

    printf("PING %s (%s) %ld(%ld) bytes of data\n", host_name,
            inet_ntoa(*(struct in_addr*)host->h_addr),
            sizeof packet - sizeof(struct icmphdr),
            sizeof packet + 20); // IP header is 20 bytes long
    for(;;) {
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        // send icmp packet
        bzero(&packet, sizeof packet);
        packet.header.type = ICMP_ECHO;
        packet.header.code = 0;
        packet.header.checksum = 0;
        packet.header.un.echo.id = 0xABCD;
        packet.header.un.echo.sequence = sequence_num ++;
        packet.header.checksum = icmp_checksum((uint16_t *)&packet, sizeof packet);
        if(send(sock, &packet, sizeof packet, 0) < 1) {
            perror("failed to send packet");
        }

        // receive response
        int read = recv(sock, buffer, 512, 0);
        if(read > 0) {
            struct timespec end_time;
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            /*
             * calculate difference in time between send and response:
             * there are 1000 ms in 1 second and
             * there are 1000 * 1000 ns in 1 ms
             */
            msdelta = end_time.tv_sec * 1000
                + (double) end_time.tv_nsec / 1000 / 1000
                - start_time.tv_sec * 1000
                - (double) start_time.tv_nsec / 1000 / 1000;

            // index into buffer to get icmp (26th, 27th byte) and seq (8th byte)
            // bytes received in ICMP is 20 less than actual (IP header)
            printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.1f ms\n",
                    read - 20, inet_ntoa(*(struct in_addr*)host->h_addr), (uint16_t)buffer[26],
                    buffer[8], msdelta);
        } else {
            /*
            struct msghdr message;
            recvmsg(sock, &message, 0);
            char* msgbody = CMSG_DATA(&message);
            printf("%s\n", msgbody);
            */
            printf("timeout occured\n");
        }

        msdelta = 0;
        do {
            // sleep for 1ms at a time so to wait one second between requests
            usleep(1000);
            struct timespec end_time;
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            msdelta = end_time.tv_sec * 1000
                + (double) end_time.tv_nsec / 1000 / 1000
                - start_time.tv_sec * 1000
                - (double) start_time.tv_nsec / 1000 / 1000;
        } while (msdelta < waitms);
    }
    return 1;
}
