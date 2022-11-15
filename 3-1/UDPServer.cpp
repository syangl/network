#include<iostream>
#include<fstream>
#include<stdio.h>
#include<time.h>
#include<string>
#include<stdint.h>
#include<WinSock2.h>
#include "UDPPackage.h"
using namespace std;

int sendLen = BUFSIZE;
int state = 0;


// void sendFunc(UDPPackage *spkg, int &seq, int &ack, int &sent_offset, SOCKET &sockSrv, sockaddr_in &addrClient){
//     //read file
//     ifstream ifile;
//     ifile.open("test/helloworld.txt", ifstream::in | ios::binary);
//     if (!ifile) {
// 	    printf("[log] open file error");
// 	    return;
//     }
//     ifile.seekg(0, ifile.end);
//     int file_len = ifile.tellg();
//     ifile.seekg(0, ifile.beg);
//     char *data = new char[file_len];
//     memset(data, 0, sizeof(data));
//     ifile.read(data, file_len);
//     ifile.close();

//     initUDPPackage(spkg);
//     spkg->seq = seq; seq = (seq+1)%BUFSIZE;
//     spkg->Length = SENTPACKSIZE;
//     memcpy(spkg->data, data + sent_offset, spkg->Length);
//     sent_offset += (int)spkg->Length;
//     spkg->Checksum = checksumFunc();

//     int sendret = sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
//     printf("[log] server to client SYN,ACK, seq=%d\n", spkg->seq);
//     if (sendret < 0){
//         printf("[log] send message error");
//         return;
//     }

//     if (sent_offset >= file_len){//发完了则FIN让客户端状态跳转
//         initUDPPackage(spkg);
//         spkg->seq = seq; seq = (seq+1)%BUFSIZE;
//         spkg->FLAG = FIN;
//         int sendret = sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
//         printf("[log] server to client FIN, seq=%d\n", spkg->seq);
//         if (sendret < 0){
//             printf("[log] send message error");
//             return;
//         }
//     }

// }













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

    UDPPackage *rpkg = new UDPPackage(); initUDPPackage(rpkg);//收
    UDPPackage *spkg = new UDPPackage(); initUDPPackage(spkg);//发
    int seq = 10, ack = 0; //server seq, ack, can be random only at init
    int sent_offset = 0; // file data has sent
    int len = sizeof(SOCKADDR);
    // int SENDLEN = sizeof(BUFSIZE);

    bool CONNECT = true;
    int recvret = -1;
    int sendret = -1;
    int file_len = 0;
    char *file_data;

    printf("[log] listening for client connect\n");
    int retlen = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len); //收到客户端请求建连
    if (retlen && rpkg->FLAG == SYN){
        ack = (rpkg->seq + 1)%BUFSIZE;
        printf("[log] client to server SYN, seq=%d\n",rpkg->seq);
        while (CONNECT){
            ifstream ifile;
            switch (state){
                case 0: //回复确认
                    initUDPPackage(spkg);
                    spkg->FLAG = SYNACK;
                    spkg->Length = sendLen;
                    spkg->seq = seq; seq = (seq+1)%BUFSIZE;
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
                    else{ //跳转至状态2传输文件，在2状态下不停发送，完毕后状态跳转
                        if (rpkg->FLAG == ACK){
                            ack = (rpkg->seq + 1)%BUFSIZE;
                            printf("[log] client to server ACK, seq=%d, ack=%d\n", rpkg->seq, rpkg->ack);
                            state = 2;
                        }
                    }
                    break;
                case 2:
                    //sendFunc(spkg,seq,ack,sent_offset,sockSrv,addrClient);
                    //读文件，TODO：改进成getline输入文件名读入文件
                    ifile.open("test/helloworld.txt", ifstream::in | ios::binary);
                    if (!ifile){
                        printf("[log] open file error");
                        CONNECT = false;
                        state = 0;
                    }
                    ifile.seekg(0, ifile.end);
                    file_len = ifile.tellg();
                    ifile.seekg(0, ifile.beg);
                    file_data = new char[file_len];
                    memset(file_data, 0, sizeof(file_data));
                    ifile.read(file_data, file_len);
                    ifile.close();

                    initUDPPackage(spkg);
                    spkg->seq = seq; seq = (seq+1)%BUFSIZE;
                    spkg->Length = SENTPACKSIZE;
                    memcpy(spkg->data, file_data + sent_offset, spkg->Length);
                    sent_offset += (int)spkg->Length;
                    spkg->Checksum = checksumFunc(spkg,spkg->Length+UDPHEADLEN);
                    delete []file_data;

                    sendret = sendto(sockSrv, (char *)spkg, sizeof(*spkg), 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
                    printf("[log] server to client file data, seq=%d, checksum=%u\n", spkg->seq,spkg->Checksum);
                    if (sendret < 0){
                        printf("[log] send message error");
                        CONNECT = false;
                        state = 0;
                    }

                    //recv ACK
                    recvret = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                    printf("[log] client to server ACK, seq=%d, ack=%d\n", rpkg->seq, rpkg->ack);
                    if (recvret > 0){
                        if (rpkg->ack != seq){//验证ack
                            // 差错重传
                            //读文件，TODO：改进成getline输入文件名读入文件
                            ifile.open("test/1.jpg", ifstream::in | ios::binary);
                            if (!ifile){
                                printf("[log] open file error");
                                CONNECT = false;
                                state = 0;
                            }
                            ifile.seekg(0, ifile.end);
                            file_len = ifile.tellg();
                            ifile.seekg(0, ifile.beg);
                            file_data = new char[file_len];
                            memset(file_data, 0, sizeof(file_data));
                            ifile.read(file_data, file_len);
                            ifile.close();

                            initUDPPackage(spkg);
                            spkg->seq = rpkg->ack; //重传
                            spkg->Length = SENTPACKSIZE;
                            sent_offset -= (int)spkg->Length; //回退file offset到上一次位置
                            memcpy(spkg->data, file_data + sent_offset, spkg->Length);
                            sent_offset += (int)spkg->Length;
                            spkg->Checksum = checksumFunc(spkg, spkg->Length + UDPHEADLEN);
                            delete[] file_data;

                            sendret = sendto(sockSrv, (char *)spkg, sizeof(*spkg), 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
                            printf("[log] resent: server to client file data, seq=%d,checksum=%d\n", spkg->seq, spkg->Checksum);
                            if (sendret < 0){
                                printf("[log] send message error");
                                CONNECT = false;
                                state = 0;
                            }
                        }
                    }
                    else{
                        //TODO：超时重传
                    }

                    //发完了则FIN让客户端状态跳转
                    if (sent_offset >= file_len){
                        initUDPPackage(spkg);
                        spkg->FLAG = FIN;
                        spkg->seq = seq; seq = (seq+1)%BUFSIZE;
                        sendto(sockSrv, (char *)spkg, sizeof(*spkg), 0, (SOCKADDR *)&addrClient, len);
                        printf("[log] server to client file transmit done, FileLength=%dB, TotalTransLength=%dB\n",file_len,sent_offset);
                        printf("[log] server to client FIN, seq=%d\n", spkg->seq);
                        state = 4;
                    }
                    break;
                case 3: //传输完毕，单向传输，挥手
                    initUDPPackage(spkg);
                    spkg->FLAG = FIN;
                    spkg->seq = seq; seq = (seq+1)%BUFSIZE;
                    sendto(sockSrv, (char*)spkg, sizeof(*spkg), 0, (SOCKADDR*)&addrClient, len);
                    printf("[log] server to client FIN, seq=%d\n", spkg->seq);
                    state = 4;
                    break;
                case 4: //等待客户端ACK,因为单向传输，收到客户端ACK后直接跳转到回复ACK阶段
                    recvret = recvfrom(sockSrv, (char*)rpkg, sizeof(*rpkg), 0, (SOCKADDR*)&addrClient, &len);
                    if (recvret < 0){
                        printf("[log] server recvfrom fail\n");
                        CONNECT = false;
                        state = 0;
                    }
                    else{
                        ack = (rpkg->seq + 1)%BUFSIZE;
                        printf("[log] client to server ACK, seq=%d, ack=%d\n", rpkg->seq, rpkg->ack);
                        state = 5;
                    }
                    break;
                case 5: //回复ACK，等待两倍延时关闭
                    initUDPPackage(spkg);
                    spkg->FLAG = ACK;
                    spkg->seq = seq; seq = (seq+1)%BUFSIZE;
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
    system("pause");
    closesocket(sockSrv);
    wprintf(L"[log] server socket closed\n");
    WSACleanup();
    wprintf(L"[log] WSA cleaned\n");
    wprintf(L"==============================================================\n");

    return 0;
}
