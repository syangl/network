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
        wprintf(L"[log] WSAStartup failed with error: %d\n", err);
        return 1;
    }else{
        wprintf(L"[log] WSAStartup Success\n");
    }
    //socket
    SOCKET sockClient = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockClient == INVALID_SOCKET) {
        wprintf(L"[log] socket function failed with error: %u\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }else{
        wprintf(L"[log] socket function Success\n");
    }
    wprintf(L"==============================================================\n");
    sockaddr_in addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_addr.s_addr = inet_addr("127.0.0.1");
    addrSrv.sin_port = htons(8000);

    // char recvBuf[BUFSIZE];
	// char sendBuf[BUFSIZE];
    UDPPackage *rpkg = new UDPPackage(); initUDPPackage(rpkg);//收
    UDPPackage *spkg = new UDPPackage(); initUDPPackage(spkg);//发
    int len = sizeof(SOCKADDR);
    int SENDLEN = sizeof(BUFSIZE);
    int recvret = -1;

    int state = 0;
    bool CONNECT = true;

    spkg->FLAG = SYN;
    sendto(sockClient, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrSrv, len);
    printf("[log] client to server SYN, seq=%d\n", spkg->seq);
    while(CONNECT){
        switch (state){
            case 0://等待回复SYNACK，收到发送ACK
                recvret = recvfrom(sockClient, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrSrv, &len);
                // if (recvret < 0){
                //         //TODO：
                //         printf("[log] server recvfrom fail\n");
                // }
                // else
                // {

                // }
                printf("[log] server to client SYN,ACK\n");
                totalLen = rpkg->Length;
                initUDPPackage(rpkg);
                initUDPPackage(spkg);
                spkg->FLAG = ACK;
                sendto(sockClient, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrSrv, len);
                printf("[log] client to server ACK\n"); 
                state = 1;
                break;
            case 1: //接收文件
                recvfrom(sockClient, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrSrv, &len);
                printf("[log] start receive files\n");
                //TODO: 文件写入
                //临时测试
                printf("test recv: %s\n" , rpkg->data);
                state = 2;
                printf("[log] receive files done\n");
                break;
            case 2: //挥手，断开连接
                recvfrom(sockClient, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrSrv, &len);
                if (rpkg->FLAG == FIN){
                    printf("[log] server to client FIN\n");
                    CONNECT = false;
                    state = 0;
                    break;
                }
                else{
                    printf("[log] return false and closed\n");
                    CONNECT = false;
                    state = 0;
                    break;
                }
                break;
            default:
                break;
        }//switch
    }//while

    system("pause");
    closesocket(sockClient);
    wprintf(L"[log] client socket closed\n");
    WSACleanup();
    wprintf(L"[log] WSA cleaned\n");
    wprintf(L"==============================================================\n");

    return 0;
}