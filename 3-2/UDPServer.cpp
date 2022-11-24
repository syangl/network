#include<iostream>
#include<fstream>
#include<stdio.h>
#include<time.h>
// #include<string>
#include<stdint.h>
#include<WinSock2.h>
#include "UDPPackage.h"

using namespace std;

int state = 0;
DWORD RTO = 2000;//ms

//滑动窗口
int slide_left = 0;
int slide_right = slide_left + N;//right是滑动窗口右边界+1
//缓冲区
char sendbuf[BUFSIZE];
int bufpos = 0;//值等于BUFSIZE则重新置0
int buf_endpos = 511;

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
    sockaddr_in addrClient;

    UDPPackage *rpkg = new UDPPackage(); initUDPPackage(rpkg);//收
    UDPPackage *spkg = new UDPPackage(); initUDPPackage(spkg);//发
    int seq = 0, ack = 0; //server seq, ack, can be random only at init
    int buf_seq = seq;//buf读入用的seq，和发送的seq是不同步的
    int sent_offset = 0; // file data has sent
    int len = sizeof(SOCKADDR);

    int FIN_backnum = N;//最后一次超时要回退的数量
    bool CONNECT = true;
    int recvret = -1;
    int sendret = -1;
    int file_len = 0;
    int setoptres = -1;
    memset(sendbuf, 0, sizeof(sendbuf));


    //listening
    printf("[log] listening for client connect\n");
    int retlen = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len); //收到客户端请求建连

    //读文件，file_data读到整个文件
    printf("[input] please input a infilename under dir test/* (such as: test/1.jpg): \n");
    #if debug
        strcpy(infilename,"test/2.jpg");
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
    char *file_data = new char[file_len];
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
                    recvret = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                    if (recvret < 0){
                        printf("[log] server recvfrom fail\n");
                        CONNECT = false;
                        state = 0;
                    }
                    else{ //跳转至状态2传输文件，在2状态下不停发送，完毕后状态跳转
                        if (rpkg->FLAG == ACK){
                            ack = (rpkg->seq + 1)%SEQMAX;
                            printf("[log] client to server ACK, seq=%d, ack=%d\n", rpkg->seq, rpkg->ack);
                            state = 2;
                        }
                    }
                    break;
                case 2://正常发送
                    #if debug
                        printf("state 2 send:\n");
                    #endif
                    //如果bufpos回到开头则重新用spkg填满sendbuf
                    if (bufpos == 0){
                        for (int i = 0; i < (BUFSIZE/PACKSIZE); ++i){
                            initUDPPackage(spkg);
                            spkg->seq = buf_seq;
                            buf_seq = (buf_seq + 1) % SEQMAX;
                            spkg->Length = PACKDATASIZE < (file_len - sent_offset) ? PACKDATASIZE : (file_len - sent_offset); //传剩于文件大小和最大报文的较小值
                            memcpy(spkg->data, file_data + sent_offset, spkg->Length);
                            sent_offset += (int)spkg->Length;//sent_offset仅在这里改变
                            spkg->WINDOWSIZE = N;//设置发送窗口当前大小
                            spkg->Checksum = checksumFunc(spkg, spkg->Length + UDPHEADLEN);//校验和最后算
                            memcpy(sendbuf + i*PACKSIZE, (char*)spkg, sizeof(*spkg));
                            //若发完了则终止
                            if (sent_offset >= file_len){
                                buf_endpos = i;
                                break;
                            }
                        }
                    }

                    //set socketopt timeout
                    setoptres = setsockopt(sockSrv, SOL_SOCKET, SO_RCVTIMEO, (char*)&RTO, sizeof(RTO));
                    if (setoptres == SOCKET_ERROR){
                        wprintf(L"setsockopt for SO_RCVTIMEO failed with error: %u\n", WSAGetLastError());
                    }

                    //连续发送N个spkg
                    for (int i = 0; i < N; ++i,++bufpos){
                        if (bufpos > buf_endpos){//结束时小于N次的情况
                            state = 4;
                            // FIN_backnum = i;
                            break;
                        }
                        sendret = sendto(sockSrv, sendbuf+bufpos*PACKSIZE, sizeof(*spkg), 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
                        UDPPackage* tmp = (UDPPackage*)(sendbuf+bufpos*PACKSIZE);//use for print
                        seq = (tmp->seq+1)%SEQMAX;//seq要和发送的seq同步
                        printf("[log] server to client file data, seq=%d, checksum=%u\n",
                                tmp->seq, tmp->Checksum);
                        if (sendret < 0){
                            printf("[log] send message error");
                            CONNECT = false;
                            state = 0;
                        }
                        Sleep(10);
                    }
                    bufpos %= SEQMAX;
                    printf("[log] Send Slide Window Current Position=%d\n      Send Slide Window Current Size=%d\n", bufpos*PACKSIZE, N);

                    if (bufpos <= buf_endpos){
                        state = 3;
                    }

                    break;
                case 3://正常接收和重传检查
                    #if debug
                        printf("state 3 recv:\n");
                    #endif
                    //recv ACK
                    recvret = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                    if (recvret > 0){
                        printf("[log] client to server ACK, seq=%d, ack=%d\n      Recv Slide Window Current Size=%d\n",
                                    rpkg->seq, rpkg->ack,rpkg->WINDOWSIZE);
                        if (rpkg->ack != seq) { //重传
                            printf("%d",seq);
                            seq = rpkg->ack;
                            bufpos = seq; //窗口位置，下一次要发送的起始位置
                            printf("[log] resent message\n");
                            state = 2;//回到发送
                        }else if ((sent_offset >= file_len) && (bufpos > buf_endpos)){//发完了到达状态4 FIN
                            state = 4;
                        }else{
                            /***
                             * TODO:3-3将根据rpkg->WINDOWSIZE设置当前N的值
                            ***/
                            state = 2;//回到发送
                        }
                    }
                    else{
                        //超时重传
                        seq -= N;//(FIN_backnum < N ? FIN_backnum : N);
                        bufpos = seq;//窗口位置，下一次要发送的起始位置
                        printf("[log] timeout resent\n");
                        state = 2;
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
