#include<iostream>
#include<fstream>
#include<stdio.h>
#include<time.h>
// #include<string>
#include<stdint.h>
#include<WinSock2.h>
#include "UDPPackage.h"

using namespace std;
SOCKET sockSrv;
sockaddr_in addrClient;
//状态
atomic_int32_t state;
DWORD RTO = 4000;//ms
//thread
HANDLE hThread;
HANDLE hCheckFinThread;
DWORD dwThreadId;
DWORD dwCheckFinThread;
//滑动窗口
atomic_int32_t base;//值等于BUFSIZE则重新置0
atomic_int32_t old_base;
// int nextseq = 0;
//缓冲区
char sendbuf[BUFSIZE];
int buf_endpos = 511;
//是否传输结束
atomic_bool isEnd;
atomic_int32_t timerCount;

bool CONNECT = true;
int recvret = -1;
int sendret = -1;
//读取的文件总大小
int file_len = 0;
//rpkg,spkg
UDPPackage *rpkg;
UDPPackage *spkg;
//进入state 3 后，seq始终位于窗口上限的后一位置
uint32_t seq = 1, ack = 0;
// buf索引
int buf_idx = 0;

//读入的文件
char *file_data;
// file data has sent
int sent_offset = 0;

//定时器
static HANDLE hTimerQueue;
static HANDLE hTimer[TIMER_MAX];//每个定时器记录自己被创建时对应的报文序号
int timer_idx = 0;

//use for bufferIsEnd
int step_n = 0;

static VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired);
DWORD WINAPI sendThread(LPVOID lparam);
DWORD WINAPI checkFinThread(LPVOID lparam);

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
    sockSrv = socket(AF_INET, SOCK_DGRAM, 0);
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
    addrSrv.sin_addr.S_un.S_addr = inet_addr("192.168.159.1");
    addrSrv.sin_port = htons(4000);

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

    //init
    timerCount = 0;
    isEnd = false;
    old_base = 0;
    base = 0;
    state = 0;
    rpkg = new UDPPackage();initUDPPackage(rpkg);
    spkg = new UDPPackage();initUDPPackage(spkg);
    int len = sizeof(SOCKADDR);
    memset(sendbuf, 0, sizeof(sendbuf));

    //creat TimerQueue
    hTimerQueue = CreateTimerQueue();

    //listening
    printf("[log] listening for client connect\n");
    int retlen = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len); //收到客户端请求建连

    //读文件，file_data读到整个文件
    printf("[input] please input a infilename under dir test/* (such as: test/1.jpg): \n");
    #if debug
        string str = "test/"+debug_filename;
        strcpy(infilename, str.c_str());
    #else
        scanf("%s", infilename);
    #endif
    ifstream ifile;
    ifile.open(infilename, ifstream::in | ios::binary);
    if (!ifile)
    {
        printf("[log] open file error\n");
        return 1;
    }
    ifile.seekg(0, ifile.end);
    file_len = ifile.tellg();
    ifile.seekg(0, ifile.beg);
    file_data = new char[file_len];
    memset(file_data, 0, sizeof(file_data));
    ifile.read(file_data, file_len);
    ifile.close();
    //bufferInit
    for (int i = 0; i < BUFNUM; ++i){
        initUDPPackage(spkg);
        spkg->seq = seq;
        seq = (seq + 1) % SEQMAX;
        spkg->Length = PACKDATASIZE < (file_len - sent_offset) ? PACKDATASIZE : (file_len - sent_offset); //传剩于文件大小和最大报文的较小值
        memcpy(spkg->data, file_data + sent_offset, spkg->Length);
        sent_offset += (int)spkg->Length;                               // sent_offset仅在这里改变
        spkg->WINDOWSIZE = N;                                           //设置发送窗口当前大小
        spkg->Checksum = checksumFunc(spkg, spkg->Length + UDPHEADLEN); //校验和最后算
        memcpy(sendbuf + i * PACKSIZE, (char *)spkg, sizeof(*spkg));
        // printf("sent_offset=%d\n",sent_offset);
        //若文件读完了则终止
        if (sent_offset >= file_len){
            ((UDPPackage *)(sendbuf + i%BUFNUM * PACKSIZE))->FLAG = FIN;
            break;
        }
        //printf("sendbuf i=%d, FLAG=%d\n",i, ((UDPPackage *)(sendbuf + i * PACKSIZE))->FLAG);
    }

    //create send thread
    hThread = CreateThread(NULL, NULL, sendThread, NULL, 0, &dwThreadId);
    if (hThread == NULL){
        wprintf(L"{---CreateSendThread error: %d}\n", GetLastError());
        return 1;
        // system("pause");
    }
    CloseHandle(hThread);
    // check fin thread
    hCheckFinThread = CreateThread(NULL, NULL, checkFinThread, NULL, 0, &dwThreadId);
    if (hCheckFinThread == NULL){
        wprintf(L"{---CreateCheckFinThread error: %d}\n", GetLastError());
        return 1;
        // system("pause");
    }
    CloseHandle(hCheckFinThread);


    if (retlen && rpkg->FLAG == SYN){
        ack = (rpkg->seq + 1)%SEQMAX;
        printf("[log] client to server SYN, seq=%d, Recv Slide Window Size=%d\n",rpkg->seq, rpkg->WINDOWSIZE);
        while (CONNECT){
            switch (state){
                case 0: //回复确认
                    #if debug
                        printf("state 0:\n");
                    #endif
                    initUDPPackage(spkg);
                    spkg->FLAG = SYNACK;
                    spkg->seq = 0; //seq = (seq+1)%SEQMAX;
                    spkg->ack = ack;
                    sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                    state = 1;
                    printf("[log] server to client SYN,ACK seq=%d,ack=%d\n", spkg->seq, spkg->ack);
                    break;
                case 1: //等待连接
                    #if debug
                        printf("state 1:\n");
                    #endif
                    recvret = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                    if (recvret < 0){
                        printf("[log] server recvfrom fail\n");
                        CONNECT = false;
                        state = 0;
                    }
                    else{
                        //跳转至状态2
                        if (rpkg->FLAG == ACK){
                            ack = (rpkg->seq + 1)%SEQMAX;
                            printf("[log] client to server ACK, seq=%d, ack=%d\n", rpkg->seq, rpkg->ack);
                            state = 2;
                            old_base = -N;
                        }
                    }
                    break;
                case 2://recv
                    #if debug
                        printf("state 2:\n");
                    #endif
                    recvret = recvfrom(sockSrv, (char *)rpkg, sizeof(*rpkg), 0, (SOCKADDR *)&addrClient, &len);
                    if (recvret <= 0){
                        /*do nothing*/
                        continue;
                    }
                    else if(rpkg->FLAG == FINACK){
                        state = 6;
                        continue;
                    }
                    else{
                        if (rpkg->ack >= base){//已被确认，滑动窗口
                            step_n = (rpkg->ack - base + BUFNUM) % BUFNUM;
                            //销毁 < base的timer
                            for (int del_idx = base; del_idx < (base + step_n); ++del_idx){
                                // Sleep(500);
                                DeleteTimerQueueTimer(hTimerQueue, hTimer[del_idx], NULL);
                                hTimer[del_idx] = NULL;
                                --timerCount;
                                // printf("delete timer idx = %d\n",del_idx);
                            }
                            printf("[log recvThread] client to server ACK, ack=%d\n", rpkg->ack);
                            // printf("base=%d\n",(int32_t)base);
                            //read
                            if (sent_offset < file_len){
                                for (int i = 0; i < step_n; ++i){
                                    initUDPPackage(spkg);
                                    spkg->seq = seq;
                                    seq = (seq + 1) % SEQMAX;
                                    spkg->Length = PACKDATASIZE < (file_len - sent_offset) ? PACKDATASIZE : (file_len - sent_offset); //传剩于文件大小和最大报文的较小值
                                    memcpy(spkg->data, file_data + sent_offset, spkg->Length);
                                    sent_offset += (int)spkg->Length;                               // sent_offset仅在这里改变
                                    spkg->WINDOWSIZE = N;                                           //设置发送窗口当前大小
                                    spkg->Checksum = checksumFunc(spkg, spkg->Length + UDPHEADLEN); //校验和最后算
                                    memcpy(sendbuf + (base + i) * PACKSIZE, (char *)spkg, sizeof(*spkg));
                                    printf("seq=%d\n",((UDPPackage*)(sendbuf + (base + i) * PACKSIZE))->seq);
                                    //若文件读完了则终止
                                    if (sent_offset >= file_len)
                                    {
                                        initUDPPackage(spkg);
                                        spkg->FLAG = FIN;
                                        memcpy(sendbuf + ((base + i) % BUFNUM) * PACKSIZE, (char *)spkg, sizeof(*spkg));
                                        break;
                                    }
                                }
                            }
                            //base变化时窗口滑动，发送线程才会捕捉到变化
                            base = rpkg->ack % BUFNUM; // base是下标，本来就比ack和seq小1
                        }
                        else{ /*do nothing*/}
                    }
                    break;
                case 4: //传输完毕，单向传输，挥手
                    #if debug
                        printf("state 4:\n");
                    #endif
                    initUDPPackage(spkg);
                    spkg->FLAG = FIN;
                    spkg->seq = seq; seq = (seq+1)%SEQMAX;
                    sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                    printf("[log] server to client file transmit done, FileLength=%fMB, TotalTransLength=%fMB\n[log] server to client FIN, seq=%d\n",
                                (file_len*1.0)/1048576.0, (sent_offset*1.0)/1048576.0, spkg->seq);
                    state = 5;
                    break;
                case 5: //等待客户端FINACK,因为单向传输，收到客户端FINACK后直接跳转到回复ACK阶段
                    #if debug
                        printf("state 5:\n");
                    #endif
                    recvret = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                    if (recvret < 0){
                        printf("[log] server recvfrom fail and exit\n");
                        CONNECT = false;
                        state = 0;
                    }
                    else{
                        ack = (rpkg->seq + 1)%SEQMAX;
                        printf("[log] client to server ACK, seq=%d, ack=%d\n", rpkg->seq, rpkg->ack);
                        state = 6;
                    }
                    break;
                case 6: //回复ACK，关闭
                    #if debug
                        printf("state 6 close:\n");
                    #endif
                    initUDPPackage(spkg);
                    spkg->FLAG = ACK;
                    spkg->seq = seq; seq = (seq+1)%SEQMAX;
                    spkg->ack = ack;
                    sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                    printf("[log] server to client ACK, seq=%d,ack=%d\n", spkg->seq, spkg->ack);
                    CONNECT = false;
                    state = 0;
                    break;
                default:
                    break;
            }//switch
        }//while
    }//if

    printf("[log] server to client file transmit done, FileLength=%fMB, TotalTransLength=%fMB\n",
                                (file_len*1.0)/1048576.0, (sent_offset*1.0)/1048576.0);
    DeleteTimerQueueEx(hTimerQueue, NULL);
    delete[] file_data;
    system("pause");
    closesocket(sockSrv);
    wprintf(L"[log] server socket closed\n");
    WSACleanup();
    wprintf(L"[log] WSA cleaned\n");
    wprintf(L"==============================================================\n");

    return 0;
}



//send thread
DWORD WINAPI sendThread(LPVOID lparam){
    bool threadEnd = false;
    while (true){
        if (old_base != base){//窗口滑动了
            UDPPackage *tmp = (UDPPackage *)(sendbuf + buf_idx * PACKSIZE); //buf_idx start from 0
            //结束
            if (tmp->FLAG == FIN){
                ((UDPPackage *)(sendbuf + (buf_idx-1) * PACKSIZE))->FLAG = 0;
                threadEnd = true;
            }
            //create timer
            if (hTimer[buf_idx] != NULL){
                DeleteTimerQueueTimer(hTimerQueue, hTimer[buf_idx],  NULL);
            }
            const int resent_seq = tmp->seq;
            CreateTimerQueueTimer( &hTimer[buf_idx], hTimerQueue, (WAITORTIMERCALLBACK)TimerRoutine, (PVOID)resent_seq, RTO, RTO, 0);
            ++timerCount;
            // printf("resent_seq=%d, buf_idx=%d\n",resent_seq,buf_idx);
            //send
            sendret = sendto(sockSrv, sendbuf + buf_idx * PACKSIZE, sizeof(*spkg), 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
            printf("[log sendThread] server to client file data, seq=%d, checksum=%u\n",
                   tmp->seq, tmp->Checksum);
            if (sendret < 0){
                printf("[log sendThread] send message error\n");
            }
            buf_idx = (buf_idx + 1) % BUFNUM;
            Sleep(20);
            if(old_base != base){
                old_base = (old_base + 1) % BUFNUM;
            }

            if (threadEnd){
                isEnd = true;
                return 0;
            }
        }//if
    }//while
    return 0;
}



//定时器
static VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired){
    int t_seq = (int)lpParam;
    int t_buf_idx = (t_seq - 1 + BUFNUM) % BUFNUM;
    printf("[TimerRoutine %d] resent %d to %d\n",t_seq, t_seq, (t_seq + N)%BUFNUM);
    for (int i = t_buf_idx; i < (t_buf_idx+N); ++i){
        // UDPPackage *tmp = (UDPPackage *)(sendbuf + i % BUFNUM * PACKSIZE);
        if (((UDPPackage *)(sendbuf + i % BUFNUM * PACKSIZE))->FLAG != FIN){
            sendret = sendto(sockSrv, sendbuf + i % BUFNUM * PACKSIZE, sizeof(*spkg), 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
            // printf("[log TimerRoutine %d] server to client file data, seq=%d, checksum=%u\n", t_seq,
            //        tmp->seq, tmp->Checksum);
            if (sendret < 0){
                printf("[log TimerRoutine %d] send message error\n", ((UDPPackage *)(sendbuf + i % BUFNUM * PACKSIZE))->seq);
            }
            Sleep(20);
        }
    }
}

//轮询检查，如果定时器全部销毁且传输完毕，说明传输结束，挥手断连
DWORD WINAPI checkFinThread(LPVOID lparam){
    while(true){
        if (isEnd && !timerCount){//TODO:计数为零了但没有删除全部的定时器？定时器必须全部销毁才可以FIN，否则就提前结束了
            initUDPPackage(spkg);
            spkg->FLAG = FIN;
            spkg->seq = seq;
            seq = (seq + 1) % SEQMAX;
            sendto(sockSrv, (char *)spkg, sizeof(*spkg), 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
            return 0;
        }
    }
    return 0;
}