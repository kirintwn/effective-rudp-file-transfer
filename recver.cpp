#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <iostream>
#include <cstring>
#include <string>
#include <algorithm>
#include "recverUtility.h"
#define SERVER_PORT "8000"
#define MAXDATASIZE 1500

using namespace std;

int main(int argc , char *argv[]) {
    // ./receiver <port>
    string serverPort;
    if(argc != 2) {
        return 1;
        serverPort = SERVER_PORT;
    }
    else {
        serverPort = argv[1];
    }



    while(1) {
        recver *myRecver = new recver(serverPort , SELECT_TIMEOUT);
        int isFIN = myRecver -> waitRTS();
        if(isFIN) continue;

        myRecver -> openFile();
        int isRTS = 1;

        while(1) {
            int isFIN = myRecver -> waitSYN(isRTS);
            isRTS = 0;
            if(isFIN) break;
            
            myRecver -> clearDATA();
            myRecver -> waitDATA();
            myRecver -> writeFile();
        }

        myRecver -> closeFile();
        cout << "\nFile: " << myRecver -> myFileName << " transferred complete" << endl;
        myRecver -> ~recver();
    }

    return 0;
}
