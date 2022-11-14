#include<string>
#include<stdint.h>
#include<WinSock2.h>
using namespace std;

#define BUFSIZE 1024
#define SYN 100
#define SYNACK 101
#define ACK 1
#define FIN 10

#define SENTPACKSIZE 1024

struct UDPPackage
{//首部16字节
    uint32_t seq;
    uint32_t ack;
    uint8_t FLAG;
    uint8_t NOTUSED;
    uint16_t WINDOWSIZE;
    uint16_t Length; //不包含首部，data字节数
    uint16_t Checksum;
    char data[BUFSIZE];//3-1还没有实现缓冲区，发一次就写一次文件
};

void initUDPPackage(UDPPackage *u){
    u->seq = 0;
    u->ack = 0;
    u->FLAG = 0;
    u->NOTUSED = 0;
    u->WINDOWSIZE = 0;
    u->Length = 0;
    u->Checksum = 0;
    memset(u->data, 0, BUFSIZE);
}

uint16_t checksumFunc(){
    //TODO
    return (uint16_t)0;
}
