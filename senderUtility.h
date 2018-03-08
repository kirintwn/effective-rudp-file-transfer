#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <iostream>
#include <cstring>
#include <string>
#include <algorithm>
#include "socketConnection.h"
#include "packetParser.h"
#define MAXDATASIZE 1500

using namespace std;

class sender {
public:
    int mySockfd;
    string myFileName;
    unsigned int mySSRC;
    uint16_t myCF_SNcounter;
    uint16_t myDataSegment;
    uint16_t myDataSNBase;
    uint16_t myDataSNCieling;
    unsigned char *recvBuffer;
    DATA_t *myDATA[256];

    int timeoutMethod;
    /////////////////////////////////////////
    sender(string serverIP , string serverPort , string fileName , int toMethod) {
        mySockfd = connectToServer(serverIP.c_str() , serverPort.c_str());
        myFileName = fileName;
        mySSRC = 31;
        myCF_SNcounter = 1;
        myDataSegment = 1;
        myDataSNBase = 1;
        myDataSNCieling = myDataSNBase + 255;
        recvBuffer = (unsigned char *)malloc(MAXDATASIZE * sizeof(uint8_t));
        memset(recvBuffer , 0 , MAXDATASIZE);
        for(int i = 0 ; i < 256 ; i++) {
            myDATA[i] = (DATA_t *)malloc(sizeof(DATA_t) + 1316);
        }

        timeoutMethod = toMethod;
    }

    int recvPacket() {
        memset(recvBuffer , 0 , MAXDATASIZE);
        int bytes = -1;
        switch (timeoutMethod) {
            case SELECT_TIMEOUT: {
                bytes = recvWithSelectTimeout(mySockfd , recvBuffer , MAXDATASIZE , 100);
                break;
            }
            case SETSOCKOPT_TIMEOUT: {
                bytes = recvWithSetTimeout(mySockfd , recvBuffer , MAXDATASIZE);
                break;
            }
            case SIGALRM_TIMEOUT: {
                bytes = recvWithSignalTimeout(mySockfd , recvBuffer , MAXDATASIZE);
                break;
            }
        }

        if(bytes == -2) {
            return -2;
        }
        else if(bytes == 0) {
            cout << "TODO: self destruct" << endl;
            return -3;
        }
        else if(bytes < 0) {
            cout << "TODO: fucked up situation" << endl;
            exit(1);
        }
        else {
            rudpHeader_t *myRUDPheader = (rudpHeader_t *)recvBuffer;
            return myRUDPheader -> frameType;
        }
    }

    uint16_t sendRTS(unsigned int r) {
        int packetLength = sizeof(RTS_t) + myFileName.length();
        RTS_t *myRTS = (RTS_t *)malloc(packetLength);
        myRTS -> rudpHeader.SSRC = mySSRC;
        myRTS -> rudpHeader.isRetransmission = r;
        myRTS -> rudpHeader.isControlFrame = 1;
        myRTS -> rudpHeader.frameType = RTS;
        myRTS -> rudpHeader.SN = htons(myCF_SNcounter);
        myRTS -> rudpHeader.exHeaderLength = htons(myFileName.length());
        memcpy(myRTS -> fileName , myFileName.c_str() , myFileName.length());
        send(mySockfd , myRTS , packetLength , 0);
        //cout << "Sent RTS Packet, SN: " << myCF_SNcounter << endl;
        free(myRTS);
        return myCF_SNcounter++;
    }
    uint16_t sendSYN(unsigned int r) {
        SYN_RNACK_t *mySYN = (SYN_RNACK_t *)malloc(sizeof(SYN_RNACK_t));
        mySYN -> rudpHeader.SSRC = mySSRC;
        mySYN -> rudpHeader.isRetransmission = r;
        mySYN -> rudpHeader.isControlFrame = 1;
        mySYN -> rudpHeader.frameType = SYN;
        mySYN -> rudpHeader.SN = htons(myCF_SNcounter);
        mySYN -> rudpHeader.exHeaderLength = htons(6);
        mySYN -> segmentNum = htons(myDataSegment);
        mySYN -> SNBase = htons(myDataSNBase);
        mySYN -> SNCeiling = htons(myDataSNCieling);
        send(mySockfd , mySYN , sizeof(SYN_RNACK_t) , 0);
        //cout << "Sent SYN Packet, SN: " << myCF_SNcounter << endl;
        cout << "\r" << "Current Segment: " << myDataSegment;
        free(mySYN);
        return myCF_SNcounter++;
    }
    void clearDATA() {
        for(int i = 0 ; i < 256 ; i++) {
            memset(myDATA[i] , 0 , sizeof(DATA_t) + 1316);
        }
    }
    void sendDATA(int index , const void *buffer , size_t length) {
        myDATA[index - 1] -> rudpHeader.SSRC = mySSRC;
        myDATA[index - 1] -> rudpHeader.isRetransmission = 0;
        myDATA[index - 1] -> rudpHeader.isControlFrame = 0;
        myDATA[index - 1] -> rudpHeader.frameType = DATA;
        myDATA[index - 1] -> rudpHeader.SN = htons(index);
        myDATA[index - 1] -> rudpHeader.exHeaderLength = htons(length + 2);
        myDATA[index - 1] -> segmentNum = htons(myDataSegment);
        memcpy(myDATA[index - 1] -> payload , buffer , 1316);
        send(mySockfd , myDATA[index - 1] , 8 + length , 0);
        //cout << "Sent DATA Packet, SN: " << index << ", segment: " << myDataSegment << endl;
    }
    uint16_t sendRNACK(unsigned int r) {
        SYN_RNACK_t *myRNACK = (SYN_RNACK_t *)malloc(sizeof(SYN_RNACK_t));
        myRNACK -> rudpHeader.SSRC = mySSRC;
        myRNACK -> rudpHeader.isRetransmission = r;
        myRNACK -> rudpHeader.isControlFrame = 1;
        myRNACK -> rudpHeader.frameType = RNACK;
        myRNACK -> rudpHeader.SN = htons(myCF_SNcounter);
        myRNACK -> rudpHeader.exHeaderLength = htons(6);
        myRNACK -> segmentNum = htons(myDataSegment);
        myRNACK -> SNBase = htons(myDataSNBase);
        myRNACK -> SNCeiling = htons(myDataSNCieling);
        send(mySockfd , myRNACK , sizeof(SYN_RNACK_t) , 0);
        //cout << "Sent RNACK Packet, SN: " << myCF_SNcounter << endl;
        free(myRNACK);
        return myCF_SNcounter++;
    }
    uint16_t sendFIN(unsigned int r) {
        FIN_RST_t *myFIN = (FIN_RST_t *)malloc(sizeof(FIN_RST_t));
        myFIN -> rudpHeader.SSRC = mySSRC;
        myFIN -> rudpHeader.isRetransmission = r;
        myFIN -> rudpHeader.isControlFrame = 1;
        myFIN -> rudpHeader.frameType = FIN;
        myFIN -> rudpHeader.SN = htons(myCF_SNcounter);
        myFIN -> rudpHeader.exHeaderLength = htons(0);
        send(mySockfd , myFIN , sizeof(FIN_RST_t) , 0);
        //cout << "Sent FIN Packet, SN: " << myCF_SNcounter << endl;
        free(myFIN);
        return myCF_SNcounter++;
    }
    int recvACK(uint8_t targetACKframeType) {
        ACK_t *myACK = (ACK_t *)recvBuffer;
        //cout << "Recved ACK Packet" << endl;
        if(myACK -> rudpHeader.SSRC != mySSRC) {
            ;//cout << "   ACK packet SSRC error: " << myACK -> rudpHeader.SSRC << endl;
        }
        if(targetACKframeType == myACK -> ACKframeType) {
            //cout << "   ACK confirmed" << endl;
            return 1;
        }
        else {
            //cout << "   ACK not matched" << endl;
            return 0;
        }
    }
    void recvNACK() {
        NACK_t *myNACK = (NACK_t *)recvBuffer;
        //cout << "Recved NACK Packet, Segment:" << htons(myNACK -> segmentNum) << endl;
        if(myNACK -> rudpHeader.SSRC != mySSRC) {
            ;//cout << "   NACK packet SSRC error: " << myNACK -> rudpHeader.SSRC << endl;
        }
        if(ntohs(myNACK -> segmentNum) != myDataSegment) {
            ;//cout << "   NACK packet segment error" << endl;
        }
        uint16_t NACK_SNcounter = (ntohs(myNACK -> rudpHeader.exHeaderLength) -2 ) / 2;
        for(unsigned int i = 0 ; i < NACK_SNcounter ; i++) {
            uint16_t tempNACK_SN = ntohs(myNACK -> NACK_SN[i]);

            DATA_t *tempDATA = myDATA[tempNACK_SN - 1];
            uint16_t EHL = ntohs(tempDATA -> rudpHeader.exHeaderLength);

            send(mySockfd , tempDATA , EHL + 6 , 0);
            //cout << "RE-Sent DATA Packet, SN: " << tempNACK_SN << ", segment: " << myDataSegment << endl;
        }
    }
    void recvNNACK() {
        NNACK_t *myNNACK = (NNACK_t *)recvBuffer;
        //cout << "Recved NNACK Packet, Segment:" << htons(myNNACK -> segmentNum) << endl;
        if(myNNACK -> rudpHeader.SSRC != mySSRC) {
            ;//cout << "   NACK packet SSRC error: " << myNNACK -> rudpHeader.SSRC << endl;
        }
        if(ntohs(myNNACK -> segmentNum) != myDataSegment) {
            ;//cout << "   NACK packet segment error" << endl;
        }
    }
    void waitRTS_ACK(uint16_t ACK_SN) {
        while(1) {
            int res = recvPacket();
            switch (res) {
                case TIMEOUT:
                    sendRTS(1);
                    break;
                case ACK:
                    if(recvACK(RTS)) {
                        return;
                    }
                    sendRTS(1);
                    break;
                default:
                    sendRTS(1);
                    break;
            }
        }
    }
    void waitSYN_ACK(uint16_t ACK_SN) {
        while(1) {
            int res = recvPacket();
            switch (res) {
                case TIMEOUT:
                    sendSYN(1);
                    break;
                case ACK:
                    if(recvACK(SYN)) {
                        return;
                    }
                    sendSYN(1);
                    break;
                case NNACK:
                    recvNNACK();
                    sendSYN(1);
                    break;      
                default:
                    sendSYN(1);
                    break;
            }
        }
    }
    void waitFIN() {
        while(1) {
            int res = recvPacket();
            switch (res) {
                case TIMEOUT:
                    sendFIN(1);
                    break;
                case FIN:
                    return;
                default:
                    sendFIN(1);
                    break;
            }
        }
    }
    void waitNACK() {
        while(1) {
            int res = recvPacket();
            switch (res) {
                case TIMEOUT:
                    sendRNACK(1);
                    break;
                case NACK:
                    recvNACK();
                    break;
                case NNACK:
                    recvNNACK();
                    return;
                default:
                    sendRNACK(1);
                    break;
            }
        }
    }

};
