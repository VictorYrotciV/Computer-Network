#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <WinSock2.h>
#include <pthread.h>
#include <time.h>
#include <string>
using namespace std;
time_t nowtime1;
#define HSK1OF3 0x1 //SYN=1 ACK=0
#define HSK2OF3 0x2 //SYN=0,ACK=1
#define HSK3OF3 0x3 //SYN=1 ACK=1
#define WAV1OF4 0x4 //SYN=0 ACK=0 FIN=1
#define WAV2OF4 0x6 //SYN=0 ACK=1 FIN=1
#define WAV3OF4 0x6 //SYN=0 ACK=1 FIN=1
#define WAV4OF4 0x7 //SYN=0 ACK=0 FIN=1
const double MAX_TIME = 0.5 * CLOCKS_PER_SEC;
struct HEADER
{
    u_short checksum=0;
    u_short datasize=0;
    //为把三个用到的flag“压缩”成三位
    //用一个unsigned char记录
    //使用低3位
    //分别是FIN ACK SYN
    unsigned char flags=0;
    unsigned char SEQ=0;
    HEADER()
    {
        checksum=0;
        datasize=0;
        flags=0;
        SEQ=0;
    }
};
u_short CalcChecksum(u_short* bufin,int size)
{
    int count = (size + 1) / 2;
    u_short* buf = new u_short[size+1];
    memset(buf, 0, size + 1);
    memcpy(buf, bufin, size);
    u_long sum = 0;
    while (count--) {
        sum += *buf++;
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum++;
        }
    }
    return ~(sum & 0xffff);
}