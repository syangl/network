#include<string>
#include<stdint.h>
#include<WinSock2.h>
using namespace std;

#define BUFSIZE 1024
#define SYN 100
#define SYNACK 101
#define ACK 1
#define FIN 10
#define FINACK 11

int SEQ = 0;

struct UDPPackage
{//首部16字节
    uint32_t seq;
    uint32_t ack;
    uint8_t FLAG;
    uint8_t NOTUSED;
    uint16_t WINDOWSIZE;
    uint16_t Length; //不包含首部，data字节数
    uint16_t Checksum;
    char data[BUFSIZE];
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

// string pack(UDPPackage u) {
//     char s[1040];
//     s = u.seq;s += "\n";
//     s += u.ack;s += "\n";
//     s += u.FLAG;s += "\n";
//     s += u.NOTUSED;s += "\n";
//     s += u.WINDOWSIZE;s += "\n";
//     s += u.Length;s += "\n";
//     s += u.Checksum;s += "\n";
//     int i = 0;
//     while(s[i] != '\n') ++i;
//     //
//     return (string)s;
// }

// UDPPackage unPack(string s){
//     UDPPackage u;
//     u.seq = s.substr();
//     u.ack = ;
//     u.FLAG = ;
//     u.NOTUSED = ;
//     u.WINDOWSIZE = ;
//     u.Length = ;
//     u.Checksum = ;
//     return u;
// }
