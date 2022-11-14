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
        wprintf(L"[log] WSAStartup failed with error: %d\n", err);
        return 1;
    }else{
        wprintf(L"[log] WSAStartup Success\n");
    }
    //socket
    SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockSrv == INVALID_SOCKET) {
        wprintf(L"[log] socket function failed with error: %u\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }else{
        wprintf(L"[log] socket function Success\n");
    }

    int addrlen = sizeof(SOCKADDR_IN);
    sockaddr_in addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    addrSrv.sin_port = htons(8000);

    //bind
    int iResult = bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));
    if (iResult == SOCKET_ERROR) {
        wprintf(L"[log] bind failed with error %u\n", WSAGetLastError());
        closesocket(sockSrv);
        WSACleanup();
        return 1;
    }
    else {
        wprintf(L"[log] bind returned success\n");
    }
    wprintf(L"==============================================================\n");
    sockaddr_in addrClient;
	// char recvBuf[BUFSIZE];
	// char sendBuf[BUFSIZE];
    UDPPackage *rpkg = new UDPPackage(); initUDPPackage(rpkg);//收
    UDPPackage *spkg = new UDPPackage(); initUDPPackage(spkg);//发
    int len = sizeof(SOCKADDR);
    int SENDLEN = sizeof(BUFSIZE);

    int state = 0;
    bool CONNECT = true;
    int recvret = -1;
    // memset(recvBuf, 0, BUFSIZE);
    // memset(sendBuf, 0, BUFSIZE);
    printf("[log] waiting for client connect\n");
    int retlen = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len); //收到客户端请求建连
    if (retlen && rpkg->FLAG == SYN){
        printf("[log] client to server SYN\n");
        while (CONNECT){
            switch (state){
                case 0: //回复确认
                    initUDPPackage(spkg);
                    spkg->FLAG = SYNACK;
                    spkg->Length = sendLen;
                    sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                    state = 1;
                    printf("[log] server to client SYN,ACK\n");
                    break;
                case 1: //等待连接
                    recvret = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                    if (recvret < 0){
                        //TODO：
                        printf("[log] server recvfrom fail\n");
                    }
                    else{ //传输文件
                        if (rpkg->FLAG == ACK){
                            printf("[log] client to server ACK\n");
                            printf("[log] server to client file transmit start\n");
                            initUDPPackage(rpkg);
                            initUDPPackage(spkg);
                            // TODO：读文件到spkg
                            char tmptest[10] = "123456789";//临时测试
                            memcpy(spkg->data, tmptest, sizeof(tmptest));
                            spkg->Length = 10;
                            spkg->seq = (SEQ++)%(BUFSIZE-1)+1;
                            sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                            state = 2;
                            printf("[log] server to client file transmit done\n");
                        }
                    }
                    break;
                case 2: //传输完毕，单向传输，挥手
                    initUDPPackage(spkg);
                    spkg->FLAG = FIN;
                    sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                    CONNECT = false;
                    state = 0;
                    printf("[log] server to client FIN\n");
                    break;
                // case 2: //两次挥手，等待客户端FIN，收到断开
                //     int recvret = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                //     if (recvret < 0){
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
    system("pause");
    closesocket(sockSrv);
    wprintf(L"[log] server socket closed\n");
    WSACleanup();
    wprintf(L"[log] WSA cleaned\n");
    wprintf(L"==============================================================\n");

    return 0;
}
