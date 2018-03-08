#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <iostream>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>
#include "socketConnection.h"
#include "packetParser.h"
#define MAXDATASIZE 1500

using namespace std;

class recver {
public:
    int mySockfd;
    string myFileName;
    unsigned int mySSRC;
    uint16_t myFeedback_SNcounter;
    uint16_t currentCF_SN;
    uint16_t currentDATA_SN;
    uint16_t currentSegment;
    uint16_t currentDataSNBase;
    uint16_t currentDataSNCieling;
    unsigned char *recvBuffer;
    DATA_t *myDATA[256];
    int dataFlag[256];

    FILE * pFile;
    int timeoutMethod;
    /////////////////////////////////////////
    recver(string serverPort , int toMethod) {
        mySockfd = establishListener(serverPort.c_str());
        myFileName.clear();
        mySSRC = 0;
        myFeedback_SNcounter = 10001;
        currentCF_SN = 0;
        currentDATA_SN = 0;
        currentSegment = 0;
        currentDataSNBase = 0;
        currentDataSNCieling = 0;
        recvBuffer = (unsigned char *)malloc(MAXDATASIZE * sizeof(uint8_t));
        memset(recvBuffer , 0 , MAXDATASIZE);
        for(int i = 0 ; i < 256 ; i++) {
            myDATA[i] = (DATA_t *)malloc(sizeof(DATA_t) + 1316);
            dataFlag[i] = 0;
        }
        pFile = NULL;
        timeoutMethod = toMethod;
    }
    ~recver() {
        if(mySockfd) {
            close(mySockfd);
        }
        myFileName.clear();
        free(recvBuffer);
        for(int i = 0 ; i < 256 ; i++) {
            free(myDATA[i]);
        }
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

    void recvRTS() {
        RTS_t *myRTS = (RTS_t *)recvBuffer;
        mySSRC = myRTS -> rudpHeader.SSRC;
        currentCF_SN = ntohs(myRTS -> rudpHeader.SN);
        int EHL = ntohs(myRTS -> rudpHeader.exHeaderLength);
        char *temp = (char *)malloc(EHL + 1);
        memcpy(temp , myRTS -> fileName , EHL);
        temp[EHL] = '\0';
        myFileName = temp;
        free(temp);

        cout << "Recved RTS Packet, SN:" << currentCF_SN << "\n";
        cout << "   Set SSRC:" << mySSRC << "\n";
        cout << "   Set fileName:" << myFileName << "\n";
    }
    void recvSYN() {
        SYN_RNACK_t *mySYN = (SYN_RNACK_t *)recvBuffer;
        if(mySYN -> rudpHeader.SSRC != mySSRC) {
            ;//cout << "SYN packet SSRC error" << endl;
        }
        currentCF_SN = ntohs(mySYN -> rudpHeader.SN);
        currentSegment = ntohs(mySYN -> segmentNum);
        currentDataSNBase = ntohs(mySYN -> SNBase);
        currentDataSNCieling = ntohs(mySYN -> SNCeiling);
        cout << "\r" << "Current Segment: " << currentSegment << "\n";
        //cout << "Recved SYN Packet, SN:" << currentCF_SN << endl;
        //cout << "   Set Current Segment:" << currentSegment << endl;
        //cout << "   Set Current SNBase:" << currentDataSNBase << endl;
        //cout << "   Set Current SNCeiling:" << currentDataSNCieling << endl;
    }
    void recvRNACK() {
        SYN_RNACK_t *myRNACK = (SYN_RNACK_t *)recvBuffer;
        currentCF_SN = ntohs(myRNACK -> rudpHeader.SN);
        //cout << "Recved RNACK Packet, SN:" << currentCF_SN << endl;
        if(myRNACK -> rudpHeader.SSRC != mySSRC) {
            ;//cout << "   RNACK packet SSRC error" << endl;
        }
        if(ntohs(myRNACK -> segmentNum) != currentSegment) {
            ;//cout << "   RNACK packet segment error" << endl;
        }
        if(ntohs(myRNACK -> SNBase) != currentDataSNBase) {
            ;//cout << "   RNACK packet SNBase error" << endl;
        }
        if(ntohs(myRNACK -> SNCeiling) != currentDataSNCieling) {
            ;//cout << "   RNACK packet SNCeiling error" << endl;
        }
    }
    void recvDATA() {
        DATA_t *tempDATA = (DATA_t *)recvBuffer;
        uint16_t tempSN = ntohs(tempDATA -> rudpHeader.SN);
        //cout << "Recved DATA Packet, SN:" << tempSN << endl;
        if(tempDATA -> rudpHeader.SSRC != mySSRC) {
            ;//cout << "   DATA packet SSRC error" << endl;
        }
        if(htons(tempDATA -> segmentNum) != currentSegment) {
            ;//cout << "   DATA packet segment error" << endl;
        }
        uint16_t EHL = ntohs(tempDATA -> rudpHeader.exHeaderLength);
        //cout << "   Payload length:" << (EHL -2) << endl;
        memcpy(myDATA[tempSN - 1] , recvBuffer , EHL + 6);
        dataFlag[tempSN - 1] = 1;
    }
    void recvFIN() {
        FIN_RST_t *myFIN = (FIN_RST_t *)recvBuffer;
        currentCF_SN = ntohs(myFIN -> rudpHeader.SN);
        //cout << "Recved FIN Packet, SN:" << currentCF_SN << endl;
        if(myFIN -> rudpHeader.SSRC != mySSRC) {
            ;//cout << "   RNACK packet SSRC error" << endl;
        }
    }

    void sendACK(uint8_t ACKframeType) {
        ACK_t *myACK = (ACK_t *)malloc(sizeof(ACK_t));
        myACK -> rudpHeader.SSRC = mySSRC;
        myACK -> rudpHeader.isRetransmission = 0;
        myACK -> rudpHeader.isControlFrame = 1;
        myACK -> rudpHeader.frameType = ACK;
        myACK -> rudpHeader.SN = htons(myFeedback_SNcounter++);
        myACK -> rudpHeader.exHeaderLength = htons(2);
        myACK -> ACKframeType = ACKframeType;
        send(mySockfd , myACK , sizeof(ACK_t) , 0);
        //cout << "Sent ACK Packet" << endl;
        free(myACK);
    }
    int sendNACK_NNACK() {
        int resendFlag = 0;
        for(unsigned int i = 0 ; i < currentDataSNCieling ; i++) {
            if(dataFlag[i] == 0) {
                resendFlag = 1;
            }
        }

        if(resendFlag) {
            sendNACK();
            return 0;
        }
        else {
            sendNNACK();
            return 1;
        }
    }
    void sendNACK() {
        int packetLength = sizeof(NACK_t);
        vector <uint16_t>NACKlist;
        for(int i = 0 ; i < currentDataSNCieling ; i++) {
            if(dataFlag[i] == 0) {
                uint16_t tempSN = i + 1;
                NACKlist.push_back(tempSN);
                packetLength += 2;
            }
        }

        NACK_t *myNACK = (NACK_t *)malloc(packetLength);
        myNACK -> rudpHeader.SSRC = mySSRC;
        myNACK -> rudpHeader.isRetransmission = 0;
        myNACK -> rudpHeader.isControlFrame = 1;
        myNACK -> rudpHeader.frameType = NACK;
        myNACK -> rudpHeader.SN = htons(myFeedback_SNcounter++);
        myNACK -> rudpHeader.exHeaderLength = htons(packetLength - 6);
        myNACK -> segmentNum = htons(currentSegment);

        for(unsigned int i = 0 ; i < NACKlist.size() ; i++) {
            myNACK -> NACK_SN[i] = htons(NACKlist.at(i)); //legit??
        }
        send(mySockfd , myNACK , packetLength , 0);
        //cout << "Sent NACK Packet, Segment: " << currentSegment << endl;
        free(myNACK);
    }
    void sendNNACK() {
        NNACK_t *myNNACK = (NNACK_t *)malloc(sizeof(NNACK_t));
        myNNACK -> rudpHeader.SSRC = mySSRC;
        myNNACK -> rudpHeader.isRetransmission = 0;
        myNNACK -> rudpHeader.isControlFrame = 1;
        myNNACK -> rudpHeader.frameType = NNACK;
        myNNACK -> rudpHeader.SN = htons(myFeedback_SNcounter++);
        myNNACK -> rudpHeader.exHeaderLength = htons(2);
        myNNACK -> segmentNum = htons(currentSegment);
        send(mySockfd , myNNACK , sizeof(NNACK_t) , 0);
        //cout << "Sent NNACK Packet, Segment: " << currentSegment << endl;
        free(myNNACK);
    }
    void sendFIN(unsigned int r) {
        FIN_RST_t *myFIN = (FIN_RST_t *)malloc(sizeof(FIN_RST_t));
        myFIN -> rudpHeader.SSRC = mySSRC;
        myFIN -> rudpHeader.isRetransmission = r;
        myFIN -> rudpHeader.isControlFrame = 1;
        myFIN -> rudpHeader.frameType = FIN;
        myFIN -> rudpHeader.SN = htons(myFeedback_SNcounter++);
        myFIN -> rudpHeader.exHeaderLength = htons(0);
        send(mySockfd , myFIN , sizeof(FIN_RST_t) , 0);
        //cout << "Sent FIN Packet" << endl;
        free(myFIN);
    }

    int waitRTS() {
        while(1) {
            int res = recvPacket();
            switch (res) {
                case TIMEOUT:
                    break;
                case RTS:
                    recvRTS();
                    sendACK(RTS);
                    return 0;
                case FIN:
                    recvFIN();
                    sendFIN(1);
                    return 1;
                default:
                    break;
            }
        }
    }
    int waitSYN(int isRTS) {
        while(1) {
            int res = recvPacket();
            switch (res) {
                case TIMEOUT:
                    if(isRTS) {
                        sendACK(RTS);
                    }
                    else {
                        sendNACK_NNACK();
                    }
                    break;
                case RTS:
                    recvRTS();
                    sendACK(RTS);
                    break;
                case SYN:
                    recvSYN();
                    sendACK(SYN);
                    return 0;
                case FIN:
                    recvFIN();
                    sendFIN(0);
                    return 1;
                case RNACK:
                    sendNACK_NNACK();
                    break;
                default:
                    break;
            }
        }
    }
    void clearDATA() {
        for(int i = 0 ; i < 256 ; i++) {
            memset(myDATA[i] , 0 , sizeof(DATA_t) + 1316);
            dataFlag[i] = 0;
        }
    }
    void waitDATA() {
        int hadDATA = 0;
        while(1) {
            int res = recvPacket();
            switch (res) {
                case TIMEOUT:
                    if(!hadDATA) {
                        sendACK(SYN);
                    }
                    break;
                case RTS:
                    if(!hadDATA) {
                        recvRTS();
                        sendACK(RTS);
                    }
                    break;
                case SYN:
                    if(!hadDATA) {
                        recvSYN();
                        sendACK(SYN);
                    }
                    break;
                case DATA:
                    hadDATA = 1;
                    recvDATA();
                    break;
                case RNACK: {
                    recvRNACK();
                    int nextFlag = sendNACK_NNACK();
                    if(nextFlag) {
                        return;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    void writeFile() {
        if (pFile==NULL) {fputs ("File error",stderr); exit (1);}
        for(int i = 0 ; i < currentDataSNCieling ; i++) {
            uint16_t EHL = ntohs(myDATA[i] -> rudpHeader.exHeaderLength);
            fwrite(myDATA[i] -> payload , 1 , EHL - 2 , pFile);
        }
    }
    void openFile() {
        pFile = fopen ( myFileName.c_str() , "wb" );
        if (pFile==NULL) {fputs ("File error",stderr); exit (1);}
    }
    void closeFile() {
        fclose (pFile);
    }
};
