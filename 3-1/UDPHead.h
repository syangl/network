#include <stdint.h>
#include <string>

struct UDPHead
{
    uint32_t seq;
    uint32_t ack;
    uint8_t FLAG;
    uint8_t NOTUSED;
    uint16_t WINDOWSIZE;
    uint16_t Length;
    uint16_t Checksum;
    char data[1024];
    void initUDPHead(){
        seq = 0;
        ack = 0;
        FLAG = 0;
        NOTUSED = 0;
        WINDOWSIZE = 0;
        Length = 0;
        Checksum = 0;
        memset(data, 0, 1024);
    }
};
