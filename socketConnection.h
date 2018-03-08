#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#define BACKLOG 15
#define RECV_BUFFER_SIZE 10485760

using namespace std;

enum TIMEOUT_TYPE {
    SELECT_TIMEOUT = 1,
    SETSOCKOPT_TIMEOUT = 2,
    SIGALRM_TIMEOUT = 3
};

enum RES_TYPE {
    ACK = 1,
    RTS = 2,
    SYN = 3,
    RNACK = 4,
    NACK = 5,
    NNACK = 6,
    FIN = 7,
    RST = 8,
    DATA = 0,
    TIMEOUT = -2,
    CLOSE = -3
};

int establishListener(const char *SERVER_PORT) {
    int sockfd;
    struct addrinfo hints = { 0 };
    struct addrinfo* res = 0;
    int yes = 1;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    int addrInfoStatus;
    if((addrInfoStatus = getaddrinfo(NULL , SERVER_PORT , &hints , &res)) != 0) {
        if(res)
            freeaddrinfo(res);
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addrInfoStatus));
        return -1;
    }

    if ((sockfd = socket(res -> ai_family , res -> ai_socktype , 0)) < 0) {
        perror("socket() failed");
        if(res)
            freeaddrinfo(res);
        return -1;
    }

    if(setsockopt(sockfd , SOL_SOCKET , SO_REUSEADDR , (char*)&yes , sizeof(int)) == -1) {
        perror("setsockopt() failed");
        if(res)
            freeaddrinfo(res);
        return -1;
    }

    if(bind(sockfd , res -> ai_addr , res -> ai_addrlen) == -1) {
        close(sockfd);
        perror("bind() failed");
        if(res)
            freeaddrinfo(res);
        return -1;
    }

    int outputValue = 0;
    socklen_t outputValue_len = sizeof(outputValue);
    int defaultBuf;

    if(getsockopt(sockfd , SOL_SOCKET , SO_RCVBUF , (char*)&outputValue , &outputValue_len) != 0) {
        perror("getsockopt failed");
        if(res)
            freeaddrinfo(res);
        return -1;
    }
    defaultBuf = outputValue;
    outputValue = RECV_BUFFER_SIZE;
    if(setsockopt(sockfd , SOL_SOCKET , SO_RCVBUF , (char*)&outputValue , sizeof(outputValue)) != 0) {
        perror("setsockopt failed");
        if(res)
            freeaddrinfo(res);
        return -1;
    }
    if(getsockopt(sockfd , SOL_SOCKET , SO_RCVBUF , (char*)&outputValue , &outputValue_len) != 0) {
        perror("getsockopt failed");
        if(res)
            freeaddrinfo(res);
        return -1;
    }
    printf("tried to set socket receive buffer from %d to %d, got %d\n", defaultBuf , RECV_BUFFER_SIZE , outputValue);

    if(res)
        freeaddrinfo(res);

    return sockfd;
}
int connectToServer(const char *SERVER_IP , const char *SERVER_PORT) {
    int sockfd;
    struct addrinfo hints = { 0 };
    struct addrinfo* res = 0;
    int yes = 1;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int addrInfoStatus;
    if((addrInfoStatus = getaddrinfo(SERVER_IP , SERVER_PORT , &hints , &res)) != 0) {
        if(res)
            freeaddrinfo(res);
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addrInfoStatus));
        return -1;
    }

    if ((sockfd = socket(res -> ai_family , res -> ai_socktype , 0)) < 0) {
        perror("socket() failed");
        if(res)
            freeaddrinfo(res);
        return -1;
    }

    if(setsockopt(sockfd , SOL_SOCKET , SO_REUSEADDR , (char*)&yes , sizeof(int)) == -1) {
        perror("setsockopt() failed");
        if(res)
            freeaddrinfo(res);
        return -1;
    }

    if (connect(sockfd , res -> ai_addr , res -> ai_addrlen) == -1) {
        perror("connect() failed");
        if(res)
            freeaddrinfo(res);
        return -1;
    }

    if(res)
        freeaddrinfo(res);
    return sockfd;
}

int recvWithSelectTimeout(int sockfd , unsigned char *buffer , int bufferLen , int to) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd , &read_fds);
    struct timeval tv = {0 , to * 1000};

    struct sockaddr clientAddr = { 0 };
    socklen_t fromlen = sizeof clientAddr;

    int result = -1;

    result = select(sockfd+1 , &read_fds , NULL , NULL , &tv);

    if(result < 0)
        return -1;
    else if(result > 0 && FD_ISSET(sockfd , &read_fds)) {
        result = recvfrom(sockfd , buffer , bufferLen , 0 , &clientAddr , &fromlen);
        if (connect(sockfd , &clientAddr , fromlen) == -1) {
            perror("connect() failed");
        }
        return result;
    }

    return -2;
}
int setsockoptTimeout(int sockfd , int to) {
    struct timeval tv = {0 , to * 1000};

    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv , sizeof(struct timeval)) == -1) {
        perror("setsockopt() failed");
        return -1;
    }

    return 0;
}
int recvWithSetTimeout(int sockfd , unsigned char *buffer , int bufferLen) {
    struct sockaddr clientAddr = { 0 };
    socklen_t fromlen = sizeof clientAddr;

    int result = -1;
    result = recvfrom(sockfd , buffer , bufferLen , 0 , &clientAddr , &fromlen);
    if(result < 0) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            return -2;
        else {
            printf("ERROR: %s\n", strerror(errno));
            return -1;
        }
    }
    else if(result > 0) {
        if (connect(sockfd , &clientAddr , fromlen) == -1) {
            perror("connect() failed");
        }
        return result;
    }

    return -2;
}

static void sig_alarm(int signo) {
    return;
}

int recvWithSignalTimeout(int sockfd , unsigned char *buffer , int bufferLen) {
    signal(SIGALRM ,  sig_alarm);
    siginterrupt(SIGALRM , 1);
    ualarm(100000 , 0);

    struct sockaddr clientAddr = { 0 };
    socklen_t fromlen = sizeof clientAddr;

    int result = -1;
    result = recvfrom(sockfd , buffer , bufferLen , 0 , &clientAddr , &fromlen);
    if(result < 0) {
        if(errno == EINTR)
            return -2;
        else {
            printf("ERROR: %s\n", strerror(errno));
            return -1;
        }
    }
    else if(result > 0) {
        ualarm(0 , 0);
        if (connect(sockfd , &clientAddr , fromlen) == -1) {
            perror("connect() failed");
        }
        return result;
    }

    return 0;
}
