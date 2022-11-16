// #include<string>
#include<string.h>
// #include<cstring>
#include<stdint.h>
#include<WinSock2.h>
using namespace std;

#define BUFSIZE 8192 //缓冲区大小
#define SYN 100
#define SYNACK 101
#define ACK 1
#define FIN 10

#define SENTPACKSIZE 8192 //报文最大大小

#define UDPHEADLEN 16 //bytes

char infilename[100]; //(server use) "test/1.jpg" "test/2.jpg" "test/3.jpg" "test/helloworld.txt"
char outfilename[100]; //(client use) "output/1.jpg" "output/2.jpg" "output/3.jpg" "output/helloworld.txt"

struct UDPPackage
{//首部16字节
    uint32_t seq;
    uint32_t ack;
    uint8_t FLAG;
    uint8_t NOTUSED;
    uint16_t WINDOWSIZE;
    uint16_t Length; //bytes,不包含首部，data字节数
    uint16_t Checksum;
    char data[BUFSIZE];//3-1还没有实现缓冲区滑动窗口，目前一次发送整个SENTPACKSIZE并立即写一次文件
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

uint16_t checksumFunc(UDPPackage *pkg, int pkg_size){
    int count = (pkg_size + 1) / 2;
    uint16_t *buf = new uint16_t[pkg_size+1];
    memset(buf, 0, pkg_size+1);
    memcpy(buf, (uint16_t*)pkg, pkg_size);
	ULONG checksum = 0;
	while (count--) {
		checksum += *buf++;
		if (checksum & 0xffff0000) {
			checksum &= 0xffff;
			checksum++;
		}
	}
	return ~(checksum & 0xffff);
    // return (uint16_t)0;
}
