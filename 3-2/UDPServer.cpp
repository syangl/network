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
int state = 0;
DWORD RTO = 2000+N*5;//ms
//thread
HANDLE hThread;
DWORD dwThreadId;
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

int seq = 0, ack = 0; // server seq, ack, can be random only at init
// buf读入用的seq，和发送的seq是不同步的
int buf_seq = seq;

//读入的文件
char *file_data;
// file data has sent
int sent_offset = 0;


//buffer is full
void bufferIsEnd(int n){
    //如果窗口滑动n个，就把前面空闲下来的填n个新的
    // if ((base + N) > SEQMAX){
    int idx = base;
    for (int i = 0; i < n; ++i){
        initUDPPackage(spkg);
        spkg->seq = buf_seq;
        buf_seq = (buf_seq + 1) % SEQMAX;
        spkg->Length = PACKDATASIZE < (file_len - sent_offset) ? PACKDATASIZE : (file_len - sent_offset); //传剩于文件大小和最大报文的较小值
        memcpy(spkg->data, file_data + sent_offset, spkg->Length);
        sent_offset += (int)spkg->Length;                               // sent_offset仅在这里改变
        spkg->WINDOWSIZE = N;                                           //设置发送窗口当前大小
        spkg->Checksum = checksumFunc(spkg, spkg->Length + UDPHEADLEN); //校验和最后算
        memcpy(sendbuf + (idx+i) * PACKSIZE, (char *)spkg, sizeof(*spkg));
        //若文件读完了则终止
        if (sent_offset >= file_len){
            buf_endpos = idx+i;
            // break;
        }
    } // for
    // }//if
}

//send thread
DWORD WINAPI sendThread(LPVOID lparam){
    int send_idx = (int)lparam % SEQMAX;
    if (send_idx < base + N){
        sendret = sendto(sockSrv, sendbuf + send_idx * PACKSIZE, sizeof(*spkg), 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
        UDPPackage *tmp = (UDPPackage *)(sendbuf + send_idx * PACKSIZE); // use for print
        printf("[log] server to client file data, seq=%d, checksum=%u\n",
               tmp->seq, tmp->Checksum);
        if (sendret < 0){
            printf("[log] send message error");
            CONNECT = false;
            state = 0;
        }
        // Sleep(10);
    }
    return 0;
}

//TODO:定时器


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

    int len = sizeof(SOCKADDR);

    // int FIN_backnum = N;//最后一次超时要回退的数量

    // int setoptres = -1;
    memset(sendbuf, 0, sizeof(sendbuf));


    //listening
    printf("[log] listening for client connect\n");
    int retlen = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len); //收到客户端请求建连

    //读文件，file_data读到整个文件
    printf("[input] please input a infilename under dir test/* (such as: test/1.jpg): \n");
    #if debug
        strcpy(infilename,"test/3.jpg");
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
                    for (int i = 0; i < N; ++i){
                        //每次发送前检查缓冲区是否发完
                        //创建sendThread
                        hThread = CreateThread(NULL, NULL, sendThread, (LPVOID)(seq-1), 0, &dwThreadId);
                        if (hThread == NULL){
                            wprintf(L"{---CreateThread error: %d}\n", GetLastError());
                            return 1;
                        }
                        else{/*wprintf(L"{---CreateThread For Client----------}\n");*/}
                        CloseHandle(hThread);
                        seq = (seq + 1) % SEQMAX;
                    }//for

                    // //如果bufpos回到开头则重新用spkg填满sendbuf
                    // if ((base == 0) && (resent == false)){
                    //     for (int i = 0; i < (BUFSIZE/PACKSIZE); ++i){
                    //         initUDPPackage(spkg);
                    //         spkg->seq = buf_seq;
                    //         buf_seq = (buf_seq + 1) % SEQMAX;
                    //         spkg->Length = PACKDATASIZE < (file_len - sent_offset) ? PACKDATASIZE : (file_len - sent_offset); //传剩于文件大小和最大报文的较小值
                    //         memcpy(spkg->data, file_data + sent_offset, spkg->Length);
                    //         sent_offset += (int)spkg->Length;//sent_offset仅在这里改变
                    //         spkg->WINDOWSIZE = N;//设置发送窗口当前大小
                    //         spkg->Checksum = checksumFunc(spkg, spkg->Length + UDPHEADLEN);//校验和最后算
                    //         memcpy(sendbuf + i*PACKSIZE, (char*)spkg, sizeof(*spkg));
                    //         //若文件读完了则终止
                    //         if (sent_offset >= file_len){
                    //             buf_endpos = i;
                    //             break;
                    //         }
                    //     }
                    // }

                    // // //set socketopt timeout
                    // // setoptres = setsockopt(sockSrv, SOL_SOCKET, SO_RCVTIMEO, (char*)&RTO, sizeof(RTO));
                    // // if (setoptres == SOCKET_ERROR){
                    // //     wprintf(L"setsockopt for SO_RCVTIMEO failed with error: %u\n", WSAGetLastError());
                    // // }

                    // //连续发送N个spkg
                    // for (int i = 0; i < N; ++i,++base){
                    //     if (base > buf_endpos){//结束时小于N次的情况
                    //         state = 4;
                    //         // FIN_backnum = i;
                    //         break;
                    //     }
                    //     sendret = sendto(sockSrv, sendbuf+base*PACKSIZE, sizeof(*spkg), 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
                    //     UDPPackage* tmp = (UDPPackage*)(sendbuf+base*PACKSIZE);//use for print
                    //     seq = (tmp->seq+1)%SEQMAX;//seq要和发送的seq同步
                    //     printf("[log] server to client file data, seq=%d, checksum=%u\n",
                    //             tmp->seq, tmp->Checksum);
                    //     if (sendret < 0){
                    //         printf("[log] send message error");
                    //         CONNECT = false;
                    //         state = 0;
                    //     }
                    //     Sleep(10);
                    // }
                    // base %= SEQMAX;
                    // printf("[log] Send Slide Window Current Position=%d Send Slide Window Current Size=%d\n", base*PACKSIZE, N);

                    // if (base <= buf_endpos){
                    // }

                    state = 3;
                    break;
                case 3://接收及后续发送
                    #if debug
                        printf("state 3:\n");
                    #endif
                    recvret = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                    if (recvret <= 0){
                        wprintf(L"[log] server recvfrom error!\n");
                        return 1;
                    }
                    if (rpkg->ack >= base){
                        bufferIsEnd(rpkg->ack - base + 1);
                        base = (rpkg->ack + 1) % SEQMAX;
                        //TODO:销毁<base的timer
                        // destroy(timer);
                        while(seq - 1 < base + N){
                            //创建sendThread
                            hThread = CreateThread(NULL, NULL, sendThread, (LPVOID)(seq-1), 0, &dwThreadId);
                            if (hThread == NULL){
                                wprintf(L"{---CreateThread error: %d}\n", GetLastError());
                                return 1;
                            }
                            else{ /*wprintf(L"{---CreateThread For Client----------}\n");*/}
                            CloseHandle(hThread);
                            seq = (base == 0) ? ((seq + 1) % SEQMAX) : (seq + 1);//让seq一直加下去，直到base回到0 seq再取模，解决窗口滑到底要循环的问题
                        }
                    }
                    else{/*do nothing*/}
                    // //recv ACK
                    // recvret = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                    // if (recvret > 0){
                    //     printf("[log] client to server ACK, seq=%d, ack=%d Recv Slide Window Current Size=%d\n",
                    //                 rpkg->seq, rpkg->ack,rpkg->WINDOWSIZE);
                    //     if (rpkg->ack > seq) { //累积确认
                    //         seq = rpkg->ack;
                    //         base = (seq + SEQMAX - 1)%SEQMAX; //窗口位置，下一次要发送的起始位置
                    //         resent = true;
                    //         printf("[log] resent message\n");
                    //         state = 2;//回到发送
                    //     }else if ((sent_offset >= file_len) && (base > buf_endpos)){//发完了到达状态4 FIN
                    //         resent = false;
                    //         state = 4;
                    //     }else if(rpkg->ack < seq){
                    //         //do nothing
                    //     }
                    //     else{
                    //         /***
                    //          * 3-3将根据rpkg->WINDOWSIZE设置当前N的值
                    //         ***/
                    //         resent = false;
                    //         state = 2;//回到发送
                    //     }
                    // }
                    // else{
                    //     //超时重传
                    //     seq = (seq + SEQMAX - N)%SEQMAX;//(FIN_backnum < N ? FIN_backnum : N);
                    //     base = (seq + SEQMAX - 1)%SEQMAX;//窗口位置，下一次要发送的起始位置
                    //     resent = true;
                    //     printf("[log] timeout resent\n");
                    //     state = 2;
                    // }
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
                    // TODO两倍时延（暂时不做）
                    CONNECT = false;
                    state = 0;
                    break;
                default:
                    break;
            }//switch
        }//while
    }//if

    delete[] file_data;
    system("pause");
    closesocket(sockSrv);
    wprintf(L"[log] server socket closed\n");
    WSACleanup();
    wprintf(L"[log] WSA cleaned\n");
    wprintf(L"==============================================================\n");

    return 0;
}
