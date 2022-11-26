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

int state = 0;
//滑动窗口
int slide_left = 0;
int slide_right = slide_left + N;//right是滑动窗口右边界+1
//缓冲区
char recvbuf[BUFSIZE];
int base = 0;//值等于BUFSIZE则重新置0
//统计写入量
float outputLen = 0.0;

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
    int time = 2000;
    int setoptres;


    sockaddr_in addrSrv;
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_addr.s_addr = inet_addr("127.0.0.1");
    addrSrv.sin_port = htons(4001);

    UDPPackage *rpkg = new UDPPackage(); initUDPPackage(rpkg);//收
    UDPPackage *spkg = new UDPPackage(); initUDPPackage(spkg);//发
    int seq = 0, ack = 0; //client seq, ack, can be random only at init
    int len = sizeof(SOCKADDR);
    int recvret = -1;

    bool CONNECT = true;

    //file handle
    wprintf(L"==============================================================\n");
    printf("[input] input your output file path(output/*): \n");
    #if debug
        strcpy(outfilename,"output/3.jpg");
    #else
        scanf("%s", outfilename);
    #endif
    wprintf(L"==============================================================\n");
    ofstream outfile;
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
                // setoptres = setsockopt(sockClient, SOL_SOCKET, SO_RCVTIMEO, (char*)&time, sizeof(time));
                // if (setoptres == SOCKET_ERROR){
                //     wprintf(L"setsockopt for SO_RCVTIMEO failed with error: %u\n", WSAGetLastError());
                // }
                #if debug
                    printf("state 1 recv:\n");
                #endif
                for (int i = 0; i < N; ++i,++base){
                    recvret = recvfrom(sockClient, (char *)rpkg, sizeof(*rpkg), 0, (SOCKADDR *)&addrSrv, &len);
                    if (recvret <= 0){
                        // do nothing
                        --base;
                    }
                    else if (rpkg->FLAG == FIN){
                        ack = (rpkg->seq + 1) % SEQMAX;
                        --base;
                        state = 2;
                        break;
                    }
                    else{
                        if (ack == rpkg->seq && !checksumFunc(rpkg, rpkg->Length + UDPHEADLEN))
                        { //没错
                            ack = (rpkg->seq + 1) % SEQMAX;
                            printf("[log] server to client file data, seq=%d, checksum=%u, Send Slide Window Size=%d\n",
                                rpkg->seq, rpkg->Checksum, rpkg->WINDOWSIZE);
                            //写入缓冲区or写入文件
                            if (base < SEQMAX){
                                memcpy(recvbuf + base*PACKSIZE, (char*)rpkg, sizeof(*rpkg));
                            }else{
                                base %= SEQMAX;
                                //buf已经有的写入文件
                                for (int j = 0; j < (BUFSIZE/PACKSIZE); ++j){
                                    UDPPackage* tmp = (UDPPackage*)(recvbuf + j*PACKSIZE);
                                    outfile.write(tmp->data, tmp->Length);
                                    outputLen += (float)tmp->Length;
                                }
                                //本次收到的重新存入buf
                                memcpy(recvbuf + base*PACKSIZE, (char*)rpkg, sizeof(*rpkg));
                            }
                        }
                        else{
                            //失序 do nothing
                            --base;
                        }
                    }//if-elif-else
                }//for

                if (state != 2){
                    initUDPPackage(spkg);
                    spkg->FLAG = ACK;
                    spkg->seq = seq;
                    seq = (seq + 1) % SEQMAX;
                    spkg->ack = ack; //连续收到最高的ack
                    spkg->WINDOWSIZE = N;
                    sendto(sockClient, (char *)spkg, sizeof(*spkg), 0, (SOCKADDR *)&addrSrv, len);
                    printf("[log] client to server Cumulative ACK, seq=%d,ack=%d Recv Slide Window Current Size=%d\n", spkg->seq, spkg->ack, spkg->WINDOWSIZE);
                }

                printf("[log] Recv Slide Window Current Position=%d\n", base*PACKSIZE);

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

                //剩余的buf写入文件
                if (base != 0){
                    for (int j = 0; j < base; ++j){
                        UDPPackage *tmp = (UDPPackage *)(recvbuf + j * PACKSIZE);
                        outfile.write(tmp->data, tmp->Length);
                        outputLen += (float)tmp->Length;
                    }
                }

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