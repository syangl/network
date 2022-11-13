#include<iostream>
#include<stdio.h>
#include<time.h>
#include<string>
#include<stdint.h>
#include<WinSock2.h>
#include "UDPPackage.h"
using namespace std;

int sendLen = BUFSIZE;

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
	// char recvBuf[BUFSIZE];
	// char sendBuf[BUFSIZE];
    UDPPackage *rpkg; initUDPPackage(rpkg);//收
    UDPPackage *spkg; initUDPPackage(spkg);//发
    int len = sizeof(SOCKADDR);
    int SENDLEN = sizeof(BUFSIZE);

    int state = 0;
    bool CONNECT = true;
    // memset(recvBuf, 0, BUFSIZE);
    // memset(sendBuf, 0, BUFSIZE);
    int retlen = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len); //收到客户端请求建连
    if (retlen && rpkg->FLAG == SYN){
        while (CONNECT){
            switch (state){
                case 0: //回复确认
                    initUDPPackage(spkg);
                    spkg->FLAG = SYNACK;
                    spkg->Length = sendLen;
                    sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                    state = 1;
                    break;
                case 1: //等待连接
                    int retsize = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                    if (retsize < 0){
                        //TODO：
                    }
                    else{ //传输文件
                        if (rpkg->FLAG == ACK){
                            initUDPPackage(rpkg);
                            initUDPPackage(spkg);
                            // TODO：读文件到spkg
                            char tmptest[10] = {1,2,3,4,5,6,7,8,9,10};//临时测试
                            memcpy(spkg->data, tmptest, sizeof(tmptest));
                            spkg->Length = 10;
                            spkg->seq = (SEQ++)%(BUFSIZE-1)+1;
                            sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                            state = 2;
                        }
                    }
                    break;
                case 2: //传输完毕，单向传输，挥手
                    initUDPPackage(spkg);
                    spkg->FLAG = FIN;
                    sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                    CONNECT = false;
                    state = 0;
                    break;
                // case 2: //两次挥手，等待客户端FIN，收到断开
                //     int retsize = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                //     if (retsize < 0){
                //         //TODO：
                //     }
                //     else{
                //         if (rpkg->FLAG == FIN){
                //             initUDPPackage(rpkg);
                //             initUDPPackage(spkg);
                //             spkg->FLAG = FINACK;
                //             sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                //             CONNECT = false;
                //             state = 0;
                //             break;
                //         }
                //     }
                //     break;
                default:
                    break;
            }//switch
        }//while
    }//if

    closesocket(sockSrv);
    wprintf(L"{---Server socket closed!}\n");
    WSACleanup();
    wprintf(L"{---WSA cleaned!----------}\n");
    wprintf(L"==============================================================\n");

    return 0;
}
