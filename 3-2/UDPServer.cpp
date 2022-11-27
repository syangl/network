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
DWORD RTO = 1000;//ms
//thread
HANDLE hThread;
DWORD dwThreadId;
DWORD dwState2ThreadId;
HANDLE hState2Thread;
//滑动窗口
int base = 0;//值等于BUFSIZE则重新置0
// int nextseq = 0;
//缓冲区
char sendbuf[BUFSIZE];
int buf_endpos = 511;
//是否传输结束
bool isEnd = false;

bool CONNECT = true;
int recvret = -1;
int sendret = -1;
//读取的文件总大小
int file_len = 0;
//rpkg,spkg
UDPPackage *rpkg;
UDPPackage *spkg;
//进入state 3 后，seq始终位于窗口上限的后一位置
int seq = 0, ack = 0;
// buf读入用的seq，和发送的seq是不同步的
int buf_seq = seq;

//读入的文件
char *file_data;
// file data has sent
int sent_offset = 0;

//定时器
HANDLE hTimerQueue;
HANDLE hTimer[1000];//保证hTimer足够大，下标等于base，等于seq-1
int timer_idx = 0;

//use for bufferIsEnd
int step_n;

//buffer is full
void bufferIsEnd(int n){
    //如果窗口滑动n个，就把前面空闲下来的填n个新的
    // if ((base + N) > SEQMAX){
    int idx = base;
    for (int i = 0; i < n; ++i){
        int idx_mod = (idx + i) % SEQMAX;
        initUDPPackage(spkg);
        spkg->seq = buf_seq;
        buf_seq = (buf_seq + 1) % SEQMAX;
        spkg->Length = PACKDATASIZE < (file_len - sent_offset) ? PACKDATASIZE : (file_len - sent_offset); //传剩于文件大小和最大报文的较小值
        memcpy(spkg->data, file_data + sent_offset, spkg->Length);
        sent_offset += (int)spkg->Length;                               // sent_offset仅在这里改变
        spkg->WINDOWSIZE = N;                                           //设置发送窗口当前大小
        spkg->Checksum = checksumFunc(spkg, spkg->Length + UDPHEADLEN); //校验和最后算
        memcpy(sendbuf + idx_mod * PACKSIZE, (char *)spkg, sizeof(*spkg));
        //若文件读完了则终止
        if (sent_offset >= file_len){
            // buf_endpos = idx+i;
            initUDPPackage(spkg);
            spkg->FLAG = FIN;
            memcpy(sendbuf + ((idx+i+1)%SEQMAX) * PACKSIZE, (char *)spkg, sizeof(*spkg));
            break;
        }
    } // for
    // }//if
}

static VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired);

//send thread
DWORD WINAPI sendThread(LPVOID lparam){
    int send_idx = ((int)lparam + SEQMAX)% SEQMAX;//防止越界
    UDPPackage *tmp = (UDPPackage *)(sendbuf + send_idx * PACKSIZE); // use for print
    //结束
    if (tmp->FLAG == FIN){
        state = 4;
        return 0;
    }
    // create timer
    // if (hTimer[send_idx] != NULL){
    //     DeleteTimerQueueTimer(hTimerQueue, hTimer[send_idx],  NULL);
    // }
    // CreateTimerQueueTimer( &hTimer[send_idx], hTimerQueue, (WAITORTIMERCALLBACK)TimerRoutine, NULL, RTO, RTO, 0);
    //send
    if ((send_idx < base + N)&&(state != 4)){
        sendret = sendto(sockSrv, sendbuf + send_idx * PACKSIZE, sizeof(*spkg), 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
        printf("[log] server to client file data, seq=%d, checksum=%u\n",
               tmp->seq, tmp->Checksum);
        if (sendret < 0){
            printf("[log] send message error\n");
        }
    }
    return 0;
}

//初始发送线程，只在state2用一次
DWORD WINAPI State2SendThread(LPVOID lparam){
    for (int i = 0; i < N; ++i){
        Sleep(50);
        printf("state2 i=%d\n",i);
        //创建sendThread
        // HANDLE hState2SendThread = CreateThread(NULL, NULL, sendThread, (LPVOID)(seq - 1), 0, &dwThreadId);
        // if (hState2SendThread == NULL){
        //     wprintf(L"{---CreateThread error: %d}\n", GetLastError());
        //     return 1;
        // }
        // else{ /*wprintf(L"{---CreateThread For Client----------}\n");*/
        // }
        // CloseHandle(hState2SendThread);
        UDPPackage *tmp = (UDPPackage *)(sendbuf + i * PACKSIZE); // use for print
        sendret = sendto(sockSrv, sendbuf + i * PACKSIZE, sizeof(*spkg), 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
        printf("[log] server to client file data, seq=%d, checksum=%u\n",
               tmp->seq, tmp->Checksum);
        if (sendret < 0){
            printf("[log] send message error\n");
        }
        seq = (seq + 1) % SEQMAX;
    } // for
}

//定时器
static VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired){//TODO:不要再创建线程，还有就是或者可以定时器一次生命周期即可，回调函数发送后再创一个定时器？
    // if (state == 4 && hTimerQueue != NULL){
    //     DeleteTimerQueueEx(hTimerQueue, NULL);
    // }
    // Sleep(5000);
    for (int i = 0; i < N; ++i){
        Sleep(50);
        //创建sendThread
        // HANDLE hTimerRoutineThread = CreateThread(NULL, NULL, sendThread, (LPVOID)(base+i), 0, &dwThreadId);
        // if (hTimerRoutineThread == NULL){
        //     wprintf(L"{---CreateThread error: %d}\n", GetLastError());
        //     return;
        // }
        // else{ /*wprintf(L"{---CreateThread For Client----------}\n");*/}
        // CloseHandle(hTimerRoutineThread);
        int tr_idx = (base+i) % SEQMAX;
        UDPPackage *tmp = (UDPPackage *)(sendbuf + tr_idx * PACKSIZE); // use for print
        sendret = sendto(sockSrv, sendbuf + tr_idx * PACKSIZE, sizeof(*spkg), 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
        printf("[log] server to client file data, seq=%d, checksum=%u\n",
               tmp->seq, tmp->Checksum);
        if (sendret < 0){
            printf("[log] send message error\n");
        }
    }// for
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
        strcpy(infilename,"test/helloworld.txt");
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
                    spkg->seq = seq; seq = (seq+1)%SEQMAX; buf_seq = seq;
                    spkg->ack = ack;
                    sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                    state = 1;
                    printf("[log] server to client SYN,ACK seq=%d,ack=%d\n", spkg->seq, spkg->ack);
                    break;
                case 1: //等待连接
                    #if debug
                        printf("state 1:\n");
                    #endif
                    //recv
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
                        }
                    }
                    break;
                case 2://创建N个发送线程，进入state 3, seq初始为1,对应buf的下标0位置
                    #if debug
                        printf("state 2:\n");
                    #endif
                    bufferIsEnd(SEQMAX);
                    // 起一个初始发送线程，用来连续发送最开始的N次，这里要立即转到状态3
                    hState2Thread = CreateThread(NULL, NULL, State2SendThread, NULL, 0, &dwState2ThreadId);
                    if (hState2Thread == NULL){
                        wprintf(L"{---CreateState2Thread error: %d}\n", GetLastError());
                        // return 1;
                        system("pause");
                    }
                    else{ /*wprintf(L"{---CreateThread For Client----------}\n");*/}
                    CloseHandle(hState2Thread);
                    state = 3;
                    break;
                case 3://接收及后续发送
                    #if debug
                        printf("state 3:\n");
                    #endif
                    recvret = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                    if (recvret <= 0){
                        wprintf(L"[log] server recvfrom error!\n");
                        // return 1;
                        system("pause");
                    }
                    if (rpkg->ack >= base){
                        step_n = (rpkg->ack + 1 - base + SEQMAX) % SEQMAX;
                        bufferIsEnd(step_n);
                        //销毁 < base的timer
                        for(int del_idx = base; del_idx < (base + step_n); ++del_idx){
                            DeleteTimerQueueTimer(hTimerQueue, hTimer[del_idx],  NULL);
                        }
                        base = (rpkg->ack + 1) % SEQMAX;
                        while(seq - 1 < base + N){
                            //创建sendThread
                            // Sleep(5000);
                            hThread = CreateThread(NULL, NULL, sendThread, (LPVOID)(seq-1), 0, &dwThreadId);
                            if (hThread == NULL){
                                wprintf(L"{---CreateThread error: %d}\n", GetLastError());
                                // return 1;
                                system("pause");
                            }
                            else{
                                int time_idx = (seq-1+SEQMAX) % SEQMAX;
                                CreateTimerQueueTimer( &hTimer[time_idx], hTimerQueue, (WAITORTIMERCALLBACK)TimerRoutine, NULL, RTO, RTO, 0);
                            }
                            CloseHandle(hThread);
                            seq = (base == 0) ? ((seq + 1) % SEQMAX) : (seq + 1);//让seq一直加下去，直到base回到0 seq再取模，解决窗口滑到底要循环的问题
                        }
                        printf("[log] client to server ACK, seq=%d, ack=%d\n", rpkg->seq, rpkg->ack);
                    }
                    else{/*do nothing*/}
                    break;
                case 4: //传输完毕，单向传输，挥手
                    #if debug
                        printf("state 4:\n");
                    #endif
                    Sleep(100000);//TODO:这里必须同步所有线程结束
                    DeleteTimerQueueEx(hTimerQueue, NULL);

                    initUDPPackage(spkg);
                    spkg->FLAG = FIN;
                    spkg->seq = seq; seq = (seq+1)%SEQMAX;
                    sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                    printf("[log] server to client file transmit done, FileLength=%fMB, TotalTransLength=%fMB\n[log] server to client FIN, seq=%d\n",
                                (file_len*1.0)/1048576.0, (sent_offset*1.0)/1048576.0, spkg->seq);
                    state = 5;
                    break;
                case 5: //等待客户端ACK,因为单向传输，收到客户端ACK后直接跳转到回复ACK阶段
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
                case 6: //回复ACK，等待两倍延时关闭
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

    Sleep(10000);//wait for send done

    delete[] file_data;

    // DeleteTimerQueueEx(hTimerQueue, NULL);

    system("pause");
    closesocket(sockSrv);
    wprintf(L"[log] server socket closed\n");
    WSACleanup();
    wprintf(L"[log] WSA cleaned\n");
    wprintf(L"==============================================================\n");

    return 0;
}
