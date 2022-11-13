#include<iostream>
#include<stdio.h>
#include<time.h>
#include<string>
#include<stdint.h>
#include<WinSock2.h>
#include "UDPPackage.h"
using namespace std;

int totalLen = 0;

int main(){
    //start
    WSADATA wsaData;
    WORD wVersionRequested;
    wVersionRequested = MAKEWORD(2, 2);
    int err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        wprintf(L"{---WSAStartup failed with error: %d}\n", err);
        return 1;
    }else{
        wprintf(L"{---WSAStartup Success}\n");
    }
    //socket
    SOCKET sockClient = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockClient == INVALID_SOCKET) {
        wprintf(L"{---socket function failed with error: %u}\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }else{
        wprintf(L"{---socket function Success}\n");
    }

    sockaddr_in addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_addr.s_addr = inet_addr("127.0.0.1");
    addrSrv.sin_port = htons(8000);

    // char recvBuf[BUFSIZE];
	// char sendBuf[BUFSIZE];
    UDPPackage *rpkg; initUDPPackage(rpkg);//收
    UDPPackage *spkg; initUDPPackage(spkg);//发
    int len = sizeof(SOCKADDR);
    int SENDLEN = sizeof(BUFSIZE);

    int state = 0;
    bool CONNECT = true;

    spkg->FLAG = SYN;
    sendto(sockClient, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrSrv, len);
    while(CONNECT){
        switch (state){
            case 0://等待回复SYNACK，收到发送ACK
                recvfrom(sockClient, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrSrv, &len);
                totalLen = rpkg->Length;
                initUDPPackage(rpkg);
                initUDPPackage(spkg);
                spkg->FLAG = ACK;
                sendto(sockClient, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrSrv, len);
                state = 1;
                break;
            case 1: //接收文件
                recvfrom(sockClient, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrSrv, &len);
                //TODO: 文件写入
                //临时测试
                printf("test recv: " , rpkg->data);
                state = 2;
                break;
            case 2: //挥手，断开连接
                recvfrom(sockClient, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrSrv, &len);
                if (rpkg->FLAG == FIN){
                    CONNECT = false;
                    state = 0;
                    break;
                }
                else{
                    printf("return false");
                    CONNECT = false;
                    state = 0;
                    break;
                }
                break;
            default:
                break;
        }//switch
    }//while


    closesocket(sockClient);
    wprintf(L"{---Client socket closed!}\n");
    WSACleanup();
    wprintf(L"{---WSA cleaned!----------}\n");
    wprintf(L"==============================================================\n");

    return 0;
}