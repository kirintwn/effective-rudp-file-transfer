#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

using namespace std;
/*
0                   1
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|C|R|    SSRC   |      CFT      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|              SN               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|      extend header length     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

typedef struct rudpHeader_t {
    unsigned int SSRC:6;
    unsigned int isRetransmission:1;
    unsigned int isControlFrame:1;
    uint8_t frameType;
    uint16_t SN;
    uint16_t exHeaderLength;
} __attribute__((packed)) rudpHeader_t;

typedef struct ACK_t {
    rudpHeader_t rudpHeader;
    uint8_t ACKframeType;
} __attribute__((packed)) ACK_t;

typedef struct RTS_t {
    rudpHeader_t rudpHeader;
    char *fileName[0];
} __attribute__((packed)) RTS_t;

typedef struct SYN_RNACK_t {
    rudpHeader_t rudpHeader;
    uint16_t segmentNum;
    uint16_t SNBase;
    uint16_t SNCeiling;
} __attribute__((packed)) SYN_RNACK_t;

typedef struct NACK_t {
    rudpHeader_t rudpHeader;
    uint16_t segmentNum;
    uint16_t NACK_SN[0];
} __attribute__((packed)) NACK_t;

typedef struct NNACK_t {
    rudpHeader_t rudpHeader;
    uint16_t segmentNum;
} __attribute__((packed)) NNACK_t;

typedef struct FIN_RST_t {
    rudpHeader_t rudpHeader;
} __attribute__((packed)) FIN_RST_t;

typedef struct DATA_t {
    rudpHeader_t rudpHeader;
    uint16_t segmentNum;
    uint8_t payload[0];
} __attribute__((packed)) DATA_t;
