#include<iostream>
#include<stdio.h>
#include<time.h>
#include<string>
#include<stdint.h>
#include<WinSock2.h>
#include "UDPPackage.h"
using namespace std;

int sendLen = BUFSIZE;

int state = 0;

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
    int seq = 10, ack = 0; //server seq, ack, can be random only at init
    int len = sizeof(SOCKADDR);
    int SENDLEN = sizeof(BUFSIZE);

    bool CONNECT = true;
    int recvret = -1;
    // memset(recvBuf, 0, BUFSIZE);
    // memset(sendBuf, 0, BUFSIZE);
    printf("[log] listening for client connect\n");
    int retlen = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len); //收到客户端请求建连
    if (retlen && rpkg->FLAG == SYN){
        ack = rpkg->seq + 1;
        printf("[log] client to server SYN, seq=%d\n",rpkg->seq);
        while (CONNECT){
            switch (state){
                case 0: //回复确认
                    initUDPPackage(spkg);
                    spkg->FLAG = SYNACK;
                    spkg->Length = sendLen;
                    spkg->seq = (seq++)%(BUFSIZE-1)+1;
                    spkg->ack = ack;
                    sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                    state = 1;
                    printf("[log] server to client SYN,ACK, seq=%d,ack=%d\n", spkg->seq, spkg->ack);
                    break;
                case 1: //等待连接
                    recvret = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                    if (recvret < 0){
                        printf("[log] server recvfrom fail\n");
                        CONNECT = false;
                        state = 0;
                    }
                    else{ //传输文件，在此状态下不停发送，完毕后状态跳转
                        if (rpkg->FLAG == ACK){
                            ack = rpkg->seq + 1;
                            printf("[log] client to server ACK, seq=%d, ack=%d\n", rpkg->seq, rpkg->ack);
                            printf("[log] server to client file transmit start\n");
                            initUDPPackage(rpkg);
                            initUDPPackage(spkg);

                            // TODO：读文件到spkg
                            { //临时测试
                            char tmptest[10] = "123456789";
                            memcpy(spkg->data, tmptest, sizeof(tmptest));
                            spkg->Length = 10;
                            spkg->seq = (seq++)%(BUFSIZE-1)+1;
                            sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                            state = 2;
                            }
                            /*函数内实现*/
                            //TODO
                            /*函数内实现*/

                            printf("[log] server to client file transmit done\n");
                        }
                    }
                    break;
                case 2: //传输完毕，单向传输，挥手
                    initUDPPackage(spkg);
                    spkg->FLAG = FIN;
                    spkg->seq = (seq++)%(BUFSIZE-1)+1;
                    sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                    printf("[log] server to client FIN, seq=%d\n", spkg->seq);
                    CONNECT = false;
                    state = 3;
                    break;
                case 3: //等待客户端ACK,因为单向传输，收到客户端ACK后直接跳转到回复ACK阶段
                    recvret = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                    if (recvret < 0){
                        printf("[log] server recvfrom fail\n");
                        CONNECT = false;
                        state = 0;
                    }
                    else{
                        ack = rpkg->seq + 1;
                        printf("[log] client to server ACK, seq=%d, ack=%d\n", rpkg->seq, rpkg->ack);
                        state = 4;
                    }
                    break;
                case 4: //回复ACK，等待两倍延时关闭
                    initUDPPackage(spkg);
                    spkg->FLAG = ACK;
                    spkg->seq = (seq++)%(BUFSIZE-1)+1;
                    spkg->ack = ack;
                    sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                    printf("[log] server to client ACK, seq=%d,ack=%d\n", spkg->seq, spkg->ack);
                    // TODO两倍时延
                    
                    break;
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
