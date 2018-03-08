#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <iostream>
#include <cstring>
#include <string>
#include <algorithm>
#include "senderUtility.h"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT "8000"
#define FILE_NAME "test.mp3"
#define MAXDATASIZE 1500

using namespace std;

int main(int argc , char *argv[]) {
    // ./sender <receiver IP> <receiver port> <file name>
    string serverIP , serverPort , fileName;

    if(argc != 4) {
        return 1;
        serverIP = SERVER_IP;
        serverPort = SERVER_PORT;
        fileName = FILE_NAME;
    }
    else {
        serverIP = argv[1];
        serverPort = argv[2];
        fileName = argv[3];
    }

    FILE * pFile = fopen ( fileName.c_str() , "rb" );
    if (pFile == NULL) {fputs ("File error",stderr); exit (1);}
    fseek (pFile , 0 , SEEK_END);
    unsigned int lSize = ftell (pFile);
    rewind (pFile);

    int segment = lSize / (1316 * 256); //4
    int lastSegmentTotal = lSize - segment * 1316 * 256; //333381
    int lastSegmentPacketCount = lastSegmentTotal / 1316; //253
    int lastPacketBytes = lastSegmentTotal - lastSegmentPacketCount * 1316; //433
    int bytes = 0;
    unsigned char *buffer = (unsigned char *)malloc(1316 * 256 * sizeof(char));

    cout << "Total Segment: " << segment + 1 << "\n";

    sender *mySender = new sender(serverIP , serverPort , fileName , SELECT_TIMEOUT);
    uint16_t ACK_SN;

    ACK_SN = mySender -> sendRTS(0);
    mySender -> waitRTS_ACK(ACK_SN);

    for(int i = 1 ; i <= segment ; i++) {
        mySender -> clearDATA();
        memset(buffer , 0 , 1316 * 256);
        bytes = fread(buffer , 1 , 1316 * 256 , pFile);
        if(bytes != 1316 * 256) {perror("fread");}

        mySender -> myDataSegment = i;
        mySender -> myDataSNBase = 1;
        mySender -> myDataSNCieling = 256;

        ACK_SN = mySender -> sendSYN(0);
        mySender -> waitSYN_ACK(ACK_SN);

        for(int j = 1 ; j <= 256 ; j++) {
            mySender -> sendDATA(j , buffer+(j-1)*1316 , 1316);
        }

        mySender -> sendRNACK(0);
        mySender -> waitNACK();
    }

    //last segment
    mySender -> clearDATA();
    memset(buffer , 0 , 1316 * 256);
    bytes = fread(buffer , 1 , lastSegmentPacketCount*1316 , pFile);
    if(bytes != lastSegmentPacketCount*1316) {perror("fread");}

    mySender -> myDataSegment = segment + 1;
    mySender -> myDataSNBase = 1;
    mySender -> myDataSNCieling = lastSegmentPacketCount + 1; // for last packet < 1316

    ACK_SN = mySender -> sendSYN(0);
    mySender -> waitSYN_ACK(ACK_SN);

    for(int i = 1 ; i <= lastSegmentPacketCount ; i++) {
        mySender -> sendDATA(i , buffer+(i-1)*1316 , 1316);
    }
    //last segment - last packet
    memset(buffer , 0 , 1316 * 256);
    bytes = fread(buffer , 1 , lastPacketBytes , pFile);
    if(bytes != lastPacketBytes) {perror("fread");}
    mySender -> sendDATA(lastSegmentPacketCount + 1 , buffer , lastPacketBytes);

    mySender -> sendRNACK(0);
    mySender -> waitNACK();

    ACK_SN = mySender -> sendFIN(0);
    mySender -> waitFIN();
    cout << "\nFile: " << mySender -> myFileName << " transferred complete" << endl;

    fclose (pFile);
    return 0;
}
