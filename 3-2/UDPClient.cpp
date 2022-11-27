#include<iostream>
#include<stdio.h>
#include<time.h>
// #include<string>
#include<stdint.h>
#include<WinSock2.h>
#include "UDPPackage.h"
#include<fstream>
#include<windows.h>
using namespace std;

SOCKET sockClient;
sockaddr_in addrSrv;
//状态
int state = 0;
//thread
HANDLE hThread;
HANDLE hWriteThread;
DWORD dwThreadId;
//滑动窗口
int base = 0;//值等于BUFSIZE则重新置0
//缓冲区
char recvbuf[BUFSIZE];
//统计写入量
float outputLen = 0.0;
//seq, ack
int seq = 0, ack = 0;
//pkg
UDPPackage *rpkg;
UDPPackage *spkg;

bool CONNECT = true;
//file
ofstream outfile;


DWORD WINAPI ackThread(LPVOID lparam){
    int cumulative_ack = (int)lparam;
    initUDPPackage(spkg);
    spkg->FLAG = ACK;
    spkg->seq = seq;
    seq = (seq + 1) % SEQMAX;
    spkg->ack = (cumulative_ack + SEQMAX) % SEQMAX;
    spkg->WINDOWSIZE = N;
    sendto(sockClient, (char *)spkg, sizeof(*spkg), 0, (SOCKADDR *)&addrSrv, sizeof(SOCKADDR));
    printf("[log] client to server ACK, seq=%d,ack=%d Recv Slide Window Current Size=%d\n", spkg->seq, spkg->ack, spkg->WINDOWSIZE);
    return 0;
}

DWORD WINAPI writeThread(LPVOID lparam){
    Sleep(100);
    int t_base = (int)lparam;
    UDPPackage *tmp = (UDPPackage *)(recvbuf + t_base * PACKSIZE);
    outfile.write(tmp->data, tmp->Length);
    outputLen += (float)tmp->Length;
    return 0;
}




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
    sockClient = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockClient == INVALID_SOCKET) {
        wprintf(L"[log] socket function failed with error: %u\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }else{
        wprintf(L"[log] socket function Success\n");
    }

    addrSrv.sin_family = AF_INET;
    addrSrv.sin_addr.s_addr = inet_addr("127.0.0.1");
    addrSrv.sin_port = htons(4001);

    //init
    rpkg = new UDPPackage(); initUDPPackage(rpkg);//收
    spkg = new UDPPackage(); initUDPPackage(spkg);//发

    int len = sizeof(SOCKADDR);
    int recvret = -1;
    memset(recvbuf, 0, sizeof(recvbuf));

    //file handle
    wprintf(L"==============================================================\n");
    printf("[input] input your output file path(output/*): \n");
    #if debug
        strcpy(outfilename,"output/helloworld.txt");
    #else
        scanf("%s", outfilename);
    #endif
    wprintf(L"==============================================================\n");
    outfile.open(outfilename, ofstream::out | ios::binary | ios::app);
    if (!outfile){
        printf("[log] open file error, file name:%s\n", outfilename);
        return 1;
    }

    spkg->FLAG = SYN;
    spkg->seq = seq; seq = (seq+1)%SEQMAX;
    spkg->WINDOWSIZE = N;//当前接收窗口大小通告
    sendto(sockClient, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrSrv, len);
    printf("[log] client to server SYN, seq=%d\n", spkg->seq);
    while(CONNECT){
        switch (state){
            case 0://等待回复SYNACK，收到发送ACK
                #if debug
                    printf("state 0:\n");
                #endif
                recvret = recvfrom(sockClient, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrSrv, &len);
                if (recvret < 0){
                    printf("[log] client recvfrom fail\n");
                    CONNECT = false;
                    state = 0;
                }
                else{
                    ack = (rpkg->seq + 1)%SEQMAX;
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
                #if debug
                    printf("state 1 recv:\n");
                #endif
                recvret = recvfrom(sockClient, (char *)rpkg, sizeof(*rpkg), 0, (SOCKADDR *)&addrSrv, &len);
                if (recvret <= 0){/*do nothing*/}
                else if(rpkg->FLAG != FIN){
                    if (rpkg->seq == ack && !checksumFunc(rpkg, rpkg->Length + UDPHEADLEN)){
                        printf("[log] server to client file data, seq=%d, checksum=%u, Send Slide Window Size=%d\n",
                                    rpkg->seq, rpkg->Checksum, rpkg->WINDOWSIZE);
                        ack = (rpkg->seq + 1) % SEQMAX;
                        memcpy(recvbuf + base*PACKSIZE, (char*)rpkg, sizeof(*rpkg));
                        base = (ack - 1 + SEQMAX) % SEQMAX;//base下标值从0开始，ack从1开始

                        hWriteThread = CreateThread(NULL, NULL, writeThread, (LPVOID)(base - 1), 0, &dwThreadId);
                        if (hWriteThread == NULL){
                            wprintf(L"{---CreateWriteThread error: %d}\n", GetLastError());
                            return 1;
                        }
                        else{ /*wprintf(L"{---CreateThread For Client----------}\n");*/}
                        CloseHandle(hWriteThread);
                    }else if(rpkg->seq > ack){/*ack not change*/}

                    hThread = CreateThread(NULL, NULL, ackThread, (LPVOID)(ack - 1), 0, &dwThreadId);
                    if (hThread == NULL){
                        wprintf(L"{---CreateThread error: %d}\n", GetLastError());
                        return 1;
                    }
                    else{ /*wprintf(L"{---CreateThread For Client----------}\n");*/}
                    CloseHandle(hThread);
                }else{
                    state = 2;
                    continue;
                }//if-elif-else
                break;
            case 2: //挥手，回复ACK
                #if debug
                    printf("state 2:\n");
                #endif
                printf("[log] server to client FIN, seq=%d\n", rpkg->seq);
                initUDPPackage(spkg);
                spkg->FLAG = ACK;
                spkg->seq = seq; seq = (seq+1)%SEQMAX;
                spkg->ack = ack;
                sendto(sockClient, (char *)spkg, sizeof(*spkg), 0, (SOCKADDR *)&addrSrv, len);
                printf("[log] client to server ACK, seq=%d,ack=%d\n", spkg->seq, spkg->ack);

                state = 3;
                break;
            case 3: //收到ACK，断开连接
                #if debug
                    printf("state 3 close:\n");
                #endif
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

    Sleep(1000);//wait for write done

    printf("[log] outputLen=%fMB\n", outputLen/1048576.0);
    outfile.close();
    system("pause");
    closesocket(sockClient);
    wprintf(L"[log] client socket closed\n");
    WSACleanup();
    wprintf(L"[log] WSA cleaned\n");
    wprintf(L"==============================================================\n");

    return 0;
}