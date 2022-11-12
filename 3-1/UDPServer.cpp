#include<iostream>
#include<stdio.h>
#include<time.h>
#include<string>
#include<stdint.h>
#include<WinSock2.h>
#include "UDPHead.h"
using namespace std;




int main(){
    //start
    WSADATA wsaData;
    WORD wVersionRequested;
    wVersionRequested = MAKEWORD(2, 2);
    int err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        wprintf(L"{---WSAStartup failed with error: %d\n}", err);
        return 1;
    }else{
        wprintf(L"{---WSAStartup Success}\n");
    }
    //socket
    SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockSrv == INVALID_SOCKET) {
        wprintf(L"{---socket function failed with error: %u}\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }else{
        wprintf(L"{---socket function Success}\n");
    }

    int addrlen = sizeof(SOCKADDR_IN);
    sockaddr_in addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    addrSrv.sin_port = htons(8000);

    //bind
    int iResult = bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));
    if (iResult == SOCKET_ERROR) {
        wprintf(L"{---bind failed with error %u}\n", WSAGetLastError());
        closesocket(sockSrv);
        WSACleanup();
        return 1;
    }
    else {
        wprintf(L"{---bind returned success}\n");
    }

    sockaddr_in addrClient;
	char recvBuf[1024];
	char sendBuf[1024];
    int len = sizeof(SOCKADDR);
    int sendlen = 1024;

    //TODO:
    while(1){
        recvfrom(sockSrv, recvBuf, 1024, 0, (SOCKADDR*)&addrClient, &len);
        // TODO:


        sendto(sockSrv, sendBuf, sendlen, 0, (SOCKADDR*)&addrClient, len);
    }

    closesocket(sockSrv);
    wprintf(L"{---Server socket closed!}\n");
    WSACleanup();
    wprintf(L"{---WSA cleaned!----------}\n");
    wprintf(L"==============================================================\n");

    return 0;
}