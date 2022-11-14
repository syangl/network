#include<iostream>
#include<stdio.h>
#include<time.h>
#include<string>
#include<stdint.h>
#include<WinSock2.h>
#include "UDPPackage.h"
#include<fstream>
using namespace std;

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

    UDPPackage *rpkg = new UDPPackage(); initUDPPackage(rpkg);//收
    UDPPackage *spkg = new UDPPackage(); initUDPPackage(spkg);//发
    int seq = 0, ack = 0; //client seq, ack, can be random only at init
    int len = sizeof(SOCKADDR);
    int SENDLEN = sizeof(BUFSIZE);
    int recvret = -1;

    bool CONNECT = true;

    spkg->FLAG = SYN;
    spkg->seq = (seq++)%(BUFSIZE-1)+1;
    sendto(sockClient, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrSrv, len);
    printf("[log] client to server SYN, seq=%d\n", spkg->seq);
    while(CONNECT){
        ofstream outfile;
        switch (state){
            case 0://等待回复SYNACK，收到发送ACK
                recvret = recvfrom(sockClient, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrSrv, &len);
                if (recvret < 0){
                    printf("[log] client recvfrom fail\n");
                    CONNECT = false;
                    state = 0;
                }
                else{
                    ack = (rpkg->seq%(BUFSIZE-1)+1);
                    printf("[log] server to client SYN,ACK, seq=%d,ack=%d\n",rpkg->seq,rpkg->ack);
                    initUDPPackage(rpkg);
                    initUDPPackage(spkg);
                    spkg->FLAG = ACK;
                    spkg->seq = seq; //ACK不消耗seq， 下一个仍为当前seq
                    spkg->ack = ack;
                    sendto(sockClient, (char *)spkg, sizeof(*spkg), 0, (SOCKADDR *)&addrSrv, len);
                    printf("[log] client to server ACK, seq=%d,ack=%d\n", spkg->seq, spkg->ack);
                    state = 1;
                }
                break;
            case 1: //接收文件，在此状态下不停接收，完毕后状态跳转
                recvret = recvfrom(sockClient, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrSrv, &len);
                if (recvret <= 0){
                    //printf("[log] client recvfrom fail\n");
                    //CONNECT = false;
                    //state = 0;
                    //do nothing
                }
                else if (rpkg->FLAG == FIN){
                    ack = (rpkg->seq%(BUFSIZE-1)+1);
                    state = 2;
                }
                else{
                    if (ack == rpkg->seq && !checksumFunc()){//没错
                        ack = (rpkg->seq%(BUFSIZE-1)+1);
                        printf("[log] server to client file data, seq=%d\n",rpkg->seq);
                        //文件写入
                        outfile.open("output/1.jpg", ofstream::out | ios::binary | ios::app);
                        if (!outfile){
	                        printf("[log] open file error");
                            CONNECT = false;
                            state = 0;
                        }
                        outfile.write(rpkg->data, rpkg->Length);
                        outfile.close();

                        initUDPPackage(spkg);
                        spkg->FLAG = ACK;
                        spkg->seq = (seq++)%(BUFSIZE-1)+1;
                        spkg->ack = ack;
                        spkg->Checksum = checksumFunc();
                        sendto(sockClient, (char *)spkg, sizeof(*spkg), 0, (SOCKADDR *)&addrSrv, len);
                        printf("[log] client to server ACK, seq=%d,ack=%d\n", spkg->seq, spkg->ack);
                    }
                    else{//有错重传
                        initUDPPackage(spkg);
                        spkg->FLAG = ACK;
                        spkg->seq = (seq++)%(BUFSIZE-1)+1;
                        spkg->ack = ack;//还是上一个ack
                        spkg->Checksum = checksumFunc();
                        sendto(sockClient, (char *)spkg, sizeof(*spkg), 0, (SOCKADDR *)&addrSrv, len);
                        printf("[log] client to server ACK, seq=%d,ack=%d\n", spkg->seq, spkg->ack);
                    }
                }
                break;
            case 2: //挥手，回复ACK
                printf("[log] server to client FIN, seq=%d\n", rpkg->seq);
                initUDPPackage(spkg);
                spkg->FLAG = ACK;
                spkg->seq = (seq++)%(BUFSIZE-1)+1;
                spkg->ack = ack;
                sendto(sockClient, (char *)spkg, sizeof(*spkg), 0, (SOCKADDR *)&addrSrv, len);
                printf("[log] client to server ACK, seq=%d,ack=%d\n", spkg->seq, spkg->ack);
                state = 3;
                break;
            case 3: //收到ACK，断开连接
                recvret = recvfrom(sockClient, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrSrv, &len);
                if (recvret < 0){
                    printf("[log] client recvfrom fail and closed\n");
                    CONNECT = false;
                    state = 0;
                }
                else{
                    printf("[log] server to client ACK, seq=%d, ack=%d\n", rpkg->seq, rpkg->ack);
                    CONNECT = false;
                    state = 0;
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