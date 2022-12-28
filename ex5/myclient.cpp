#include "loghdr.h"
#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
//reno***********************
#define SLOWSTART 0
#define CONGAVOID 1
#define FASTRECOV 2
int renoState=0;
int cwnd=1;
int lastSeq=0;
int dupACKnum=0;
int ssthresh=WINDOW_SIZE;//一个缓冲区即数据段长度为1024，即1KB，WINDOWSIZE=64，恰好64KB

//在拥塞避免中，new ack时需要cwnd=cwnd+mss*mss/cwnd
//收到cwnd个ack后，cwnd+1，使其线性增长
//即若cwnd=4，每收到四个段cwnd才+1
//cwnd为整数，用一个全局变量，表示统计此时状态累积收到的cwnd数
//每收到cwnd个，或状态转换到拥塞避免时清零
int dupcwndnum=0;
//***************************
char servIP[20];//服务端ip
int servPort;//服务端端口号
int base=0;//FSM中的base
int basejwnum=0;//base进位256的次数，用于重发时的指针操作
//因为recv成功时base会更新为收到的seq+1
//但收到的seq为0-255
//如果重发，需要重发base到nextseqnum-1的那些包
//上面那一行的base实际上不等于我们定义的base，因为上面那个base肯定没有经过取模处理
//就要记录下进位256的次数basejwnum，那么base到nextseqnum-1的实际的base
//就等于basejwnum*256+base(我们定义的base，实际上只有0-255)
//实际上，base到nextseqnum-1中的base由resend_index记录
int nextseqnum=1;//FSM中的nextseqnum
int clntSeq=0;
char message[200];
char* myippointer;
SOCKET sock;//实例化全局socket
SOCKADDR_IN sockAddr;
clock_t now_clocktime;

//将sendfile中的一些变量放到全局，方便传入多线程
int pktnum;
int nowpkt;
int recvtime;
long int tailpointer;//由于按包来传输，记录每次拼接后末尾的指针便于下一次拼接
HEADER header;
HEADER temp1;
u_short calc_chksum_rst;
char* buffer = new char[MAX_BUFFER_SIZE+sizeof(header)];
//重传记录
char* nowbuffer=new char[MAX_BUFFER_SIZE+sizeof(header)];
u_short checksum_from_recv1;
int pktlen;

//由于改成了多线程，需要显示表明是否开启计时器
int timerIsOpen=0;
//向pthread中传参的结构体
struct pthread_para
{
    SOCKET clntSock;
    SOCKADDR_IN servAddr;
    int servAddrLen;
    char* fullData;
    int dataLen;
};

int send_end_flag=0;
//计时
long long head,tail,freq;
time_t nowtime;//time
int ConnectWith3Handsks(SOCKET& clntSock,SOCKADDR_IN& servAddr, int& servAddrLen){
    HEADER header;
    u_short calc_chksum_rst;
    char* buffer=new char[sizeof(header)];//头部初始化
    //********************************************************************
    //send第一次握手
    header.checksum=0;
    header.flag=HSK1OF3;
    calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
    header.checksum=calc_chksum_rst;
    memcpy(buffer,&header,sizeof(header));
    if (sendto(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrLen) <= 0)
    {
        addtoclntlog("第一次握手发送错误",myippointer);
        printf("第一次握手发送错误");
        return -1;
    }
    addtoclntlog("第一次握手发送成功，开启计时器",myippointer);
    printf("第一次握手发送成功\n");
    now_clocktime=clock();

    //********************************************************************
    //recv第二次握手
    //sock创建时自动设置阻塞模式，阻塞后不会重传
    //这里要设置成非阻塞模式，iotclsocket的第三个参数非0
    mode = 1;
    ioctlsocket(clntSock, FIONBIO, &mode);
    while (recvfrom(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrLen) <= 0)
    {
        if (clock() - now_clocktime > MAX_TIME)//超时，重新传输第一次握手
        {//重传第一次握手
            header.flag = HSK1OF3;
            header.checksum=0;
            header.checksum = CalcChecksum((u_short*)&header, sizeof(header));//计算校验和
            memcpy(buffer, &header, sizeof(header));//将首部放入缓冲区
            if(sendto(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrLen)<0){addtoclntlog("第一次握手超时重传发送错误",myippointer);return -1;}
            now_clocktime = clock();
            addtoclntlog("第二次握手接收超时，第一次握手重传发送成功",myippointer);
            printf("第二次握手接收超时，第一次握手进行重传\n");
        }
    }
    addtoclntlog("第二次握手接收成功，进行校验，关闭计时器",myippointer);
    printf("第二次握手接收成功，进行校验，关闭计时器\n");
    HEADER temp1;
    memcpy(&temp1, buffer, sizeof(header));
    u_short checksum_from_recv=temp1.checksum;
    temp1.checksum=0;
    calc_chksum_rst=CalcChecksum((u_short*)&temp1,sizeof(temp1));
    if(temp1.flag==HSK2OF3&&calc_chksum_rst==checksum_from_recv)
    {
        addtoclntlog("第二次握手校验成功",myippointer);
        printf("二次握手完成，继续三次握手\n");
    }else{
        if(temp1.flag!=HSK2OF3){printf("flag有误\n");}
        addtoclntlog("二次握手校验有误，请重新连接",myippointer);
        printf("二次握手传输有误，请重新连接\n");
        return -1;
    }
    //********************************************************************
    //send第三次握手
    header.flag=HSK3OF3;
    header.checksum=0;
    calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
    header.checksum=calc_chksum_rst;
    memcpy(buffer,&header,sizeof(header));
    if (sendto(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrLen) <= 0)
    {
        printf("第三次握手发送错误\n");
        addtoclntlog("第三次握手发送有误，请重新连接",myippointer);
        return -1;
    }
    printf("第三次握手发送成功\n");
    addtoclntlog("第三次握手发送成功",myippointer);
    return 1;
}
int DisconnectWith4Waves(SOCKET& clntSock,SOCKADDR_IN& servAddr, int& servAddrLen){
    HEADER header;
    u_short calc_chksum_rst;
    char* buffer = new char[sizeof(header)];
    
    //记录第三次挥手seq=w，因为第四次挥手的ack=w+1
    u_short seqin3rdwave;
    u_short ackin2ndwave;
    //********************************************************************
    //send第一次握手
    header.flag=WAV1OF4;
    header.checksum=0;
    calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
    header.checksum=calc_chksum_rst;
    memcpy(buffer,&header,sizeof(header));
    if (sendto(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrLen) <= 0)
    {
        addtoclntlog("第一次挥手发送有误，请重新连接",myippointer);
        printf("第一次挥手发送错误");
        return -1;
    }
    addtoclntlog("第一次挥手发送成功，开启计时器",myippointer);
    printf("第一次挥手发送成功\n");
    now_clocktime=clock();
    //sock创建时自动设置阻塞模式，阻塞后不会重传
    //这里要设置成非阻塞模式，iotclsocket的第三个参数非0
    mode = 1;
    ioctlsocket(clntSock, FIONBIO, &mode);
    //********************************************************************
    //recv第二次握手，需要则重传第一次握手
    while (recvfrom(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrLen) <= 0)
    {
        if (clock() - now_clocktime > MAX_TIME)//超时，重新传输第一次握手
        {//重传第一次握手
            header.flag = WAV1OF4;
            header.checksum=0;
            header.checksum = CalcChecksum((u_short*)&header, sizeof(header));//计算校验和
            memcpy(buffer, &header, sizeof(header));//将首部放入缓冲区
            sendto(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrLen);
            now_clocktime = clock();
            addtoclntlog("第二次挥手接收超时，重传第一次挥手成功",myippointer);
            printf("第二次挥手接收超时，重传第一次挥手成功\n");
        }
    }
    addtoclntlog("第二次挥手接收成功，进行校验，关闭计时器",myippointer);
    printf("第二次挥手接收成功，进行校验，关闭计时器\n");
    HEADER temp1;
    memcpy(&temp1, buffer, sizeof(header));
    u_short checksum_from_recv=temp1.checksum;
    temp1.checksum=0;
    calc_chksum_rst=CalcChecksum((u_short*)&temp1,sizeof(temp1));
    if(temp1.flag==WAV2OF4&&calc_chksum_rst==checksum_from_recv)
    {
        //记录这里的ack=u+1，因为第三次挥手的ack=u+1，第四次挥手的seq=u+1
        ackin2ndwave=temp1.ack;
        addtoclntlog("第二次挥手校验成功",myippointer);
        printf("第二次挥手校验成功\n");
    }else{
        printf("flag=%u,seq=%u,checksum=%u\n",temp1.flag,temp1.SEQ,checksum_from_recv);
        if(temp1.flag!=WAV2OF4){printf("flag=%u,flag不对\n",temp1.flag);}
        if(calc_chksum_rst!=checksum_from_recv){printf("校验码不对\n");}
        addtoclntlog("第二次挥手校验不成功",myippointer);
        printf("二次挥手校验有误\n");
        return -1;
    }
    //********************************************************************
    //recv第三次挥手
    mode = 0;
    ioctlsocket(clntSock, FIONBIO, &mode);
    while (1)
    {
        if (recvfrom(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrLen)<=0)
        {
            continue;
        }
        addtoclntlog("第三次挥手接收成功，进行校验",myippointer);
        memcpy(&header,buffer,sizeof(header));
        u_short checksum_from_recv=header.checksum;
        header.checksum=0;
        calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
        if(header.flag==WAV3OF4&&header.ack==ackin2ndwave&&calc_chksum_rst==checksum_from_recv)
        {
            printf("第三次挥手校验成功\n");
            addtoclntlog("第三次挥手校验成功",myippointer);
            //记录这里的seq=w，因为第四次挥手的ack=w+1
            seqin3rdwave=header.SEQ;
            break;
        }else{
            printf("第三次挥手校验错误\n");
            printf("第三次挥手校验有误，请重新连接\n");
            return -1;
        }
    }
    //********************************************************************
    //send第四次挥手
    header.flag=WAV4OF4;
    header.ack=seqin3rdwave+1;
    header.SEQ=ackin2ndwave;
    header.checksum=0;
    calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
    header.checksum=calc_chksum_rst;
    memcpy(buffer,&header,sizeof(header));
    if (sendto(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrLen) <= 0)
    {
        addtoclntlog("第四次挥手发送错误",myippointer);
        printf("第四次挥手发送错误");
        return -1;
    }
    addtoclntlog("第四次挥手发送成功，开启计时器",myippointer);
    printf("第四次挥手发送成功\n");
    now_clocktime=clock();
    //第四次挥手后两个MSL后没有收到传回的信息，断开连接
    mode = 1;
    ioctlsocket(clntSock, FIONBIO, &mode);    
    while(recvfrom(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrLen)<0)
    {
        if(clock()-now_clocktime>2*MAX_TIME)
        {
            addtoclntlog("第四次挥手后经2*MSL未收到数据，成功断开",myippointer);
            return 1;
        }
    }
    //如果到达这里，说明发送第四次挥手后又收到信息
    //暂时不做处理，只退出，代表四次挥手错误
    addtoclntlog("四次挥手失败，因为第四次挥手后仍接到数据",myippointer);
    printf("四次挥手失败，因为第四次挥手后仍接到数据\n");
    return -1;
}
int SendFileAsBinary(SOCKET& clntSock,SOCKADDR_IN& servAddr, int& servAddrLen, char* fullData,int dataLen){
    //reno
    lastSeq=0;
    dupACKnum=0;
    cwnd=1;
    ssthresh=WINDOW_SIZE;
    renoState=SLOWSTART;//start with slowstart

    pktnum=dataLen%MAX_BUFFER_SIZE==0?dataLen/MAX_BUFFER_SIZE:dataLen/MAX_BUFFER_SIZE+1;
    //int pktnum=dataLen/MAX_BUFFER_SIZE+(dataLen%MAX_BUFFER_SIZE!=0);
    nowpkt=0;
    recvtime=0;
    tailpointer=0;//由于按包来传输，记录每次拼接后末尾的指针便于下一次拼接
    // HEADER header;
    header=HEADER();
    // HEADER temp1;
    temp1=HEADER();
    // u_short calc_chksum_rst;
    buffer = new char[MAX_BUFFER_SIZE+sizeof(header)];
    //重传记录
    nowbuffer=new char[MAX_BUFFER_SIZE+sizeof(header)];
    // u_short checksum_from_recv1;
    base=0;
    basejwnum=0;
    nextseqnum=0;
    clntSeq=0;//初始化seq为0 
    pktlen=0;

    //**************************************************
        //pthread
        //initiaize pthread params
        struct pthread_para para;
        para.clntSock=clntSock;
        para.servAddr=servAddr;
        para.servAddrLen=servAddrLen;
        para.fullData=fullData;
        para.dataLen=dataLen;

        pthread_t id;
        void* client_recv_thread(void*);
        pthread_create(&id,0,client_recv_thread,&(para));

    //**************************************************
    //******time
    QueryPerformanceCounter((LARGE_INTEGER *)&head);
    char* tbuffer = new char[sizeof(head)+1];
    memcpy(tbuffer,&head,sizeof(head));
    sendto(clntSock, tbuffer, sizeof(head), 0, (sockaddr*)&servAddr, servAddrLen);
    //************************************************************
    while(1){
        //**********************************************************
        //step 1 in GBN FSM
        if(nextseqnum<pktnum){
            //如果“下一个”序列号等于包数
            //而包数从0开始，实际上就是
            //下一个序列号超过最大包数
            //这时就不能再发，而是等待剩余的接收端ack
        // if(nextseqnum<base+256*basejwnum+WINDOW_SIZE){//&&nextseqnum<pktnum
        if(nextseqnum<base+256*basejwnum+cwnd){//&&nextseqnum<pktnum
            //make_pkt
            if(nextseqnum==pktnum-1){
                //最后一个
                pktlen=dataLen-(pktnum-1)*MAX_BUFFER_SIZE;
            }else{
                pktlen=MAX_BUFFER_SIZE;
            }
            header=HEADER();
            header.datasize=pktlen;//此时pktlen为不加头部的大小
            header.SEQ=(unsigned char)(nextseqnum%256);
            header.checksum=0;
            header.flag=0;
            header.ack=0;
            //初始化为0
            char* nowPktPointer=fullData+nextseqnum*MAX_BUFFER_SIZE;
            memcpy(buffer,&header,sizeof(header));
            memcpy(buffer+sizeof(header),nowPktPointer,pktlen);
            calc_chksum_rst=CalcChecksum((u_short*)buffer,sizeof(header)+pktlen);
            header.checksum=calc_chksum_rst;
            memcpy(buffer,&header,sizeof(header));
            memcpy(nowbuffer,&header,sizeof(header));
            memcpy(nowbuffer+sizeof(header),nowPktPointer,pktlen);
            // printf("\n发送校验和=%d\n",calc_chksum_rst);
            //udt_send(sndpkt)
            if(sendto(clntSock, buffer, sizeof(header)+pktlen, 0, (sockaddr*)&servAddr, servAddrLen)<0){
            // printf("udp发送错误\n");
            // addtoclntlog("[ERR ]UDP包发送错误",myippointer);
            continue;
            }
            // memset(message,0,sizeof(message));
            // sprintf(message,"[INFO]成功发送 %d bytes数据，nextseqnum=%d",pktlen,nextseqnum);
            // printf("%s\n",message);
            // addtoclntlog((const char*)message,myippointer);
            // sprintf(message,"[INFO]实际基序号base=%d，滑动窗口大小=%d",base+256*basejwnum,nextseqnum-(base+256*basejwnum));
            // printf("%s\n",message);
            // addtoclntlog((const char*)message,myippointer);
            // sprintf(message,"[INFO]发送UDP包的校验和checksum=%u，伪首部的8位SEQ=%d",calc_chksum_rst,int(header.SEQ));
            // printf("%s\n",message);
            // addtoclntlog((const char*)message,myippointer);
            // addtoclntlog("[MESG]UDP包发送成功",myippointer);
            // printf("udp包发送成功\n");
            //start timer
            if(base+256*basejwnum==nextseqnum){
                // addtoclntlog("[MESG]base==nextseqnum,开启计时器",myippointer);
                timerIsOpen=1;
                now_clocktime=clock();
            }
            //if(nextseqnum==pktnum-1){break;}
            //直接break的话，实际上由于recv在send之后，send完但是没有recv完
            //需要等到所有都recv完，即base=nextseqnum
            nextseqnum++;
            // addtoclntlog((const char*)message,myippointer);
            // sprintf(message,"[INFO]更新nextseqnum=%d",nextseqnum);
            // printf("%s\n",message);
            // addtoclntlog((const char*)message,myippointer);
            //remark 这里base是从header的seq复制下来的，应该只有0-255；
            //但如果你还没有注意到，那nextseqnum实际上并没有取余256
            //DONE
            //注意这一点

        }else{
            //if(nextseqnum>=pktnum){break;}
            // memset(message,0,sizeof(message));
            // sprintf(message,"[INFO]实际基序号base=%d，nextseqnum=%d,滑动窗口大小=%d",base+256*basejwnum,nextseqnum,nextseqnum-(base+256*basejwnum));
            // printf("%s\n",message);
            // addtoclntlog((const char*)message,myippointer);
            // memset(message,0,sizeof(message));
            // // sprintf(message,"[ERR ]最大窗口大小=%d,窗口过大，拒绝data",WINDOW_SIZE);
            // sprintf(message,"[ERR ]最大窗口大小=%d,窗口过大，拒绝data",cwnd);
            // printf("%s\n",message);
            // addtoclntlog((const char*)message,myippointer);
            // printf("窗口过大，拒绝data\n");
        }
        //隐含条件：
        //else{refuse_data()}
        //**********************************************************
        }
        if(send_end_flag==1)
        {
            send_end_flag=0;
            break;
        }
        
        
        
    }
    //跳到这里则所有pkt发送成功，发送一个over的flag
    // printf("发送结束标志\n");
    // addtoclntlog("[MESG]所有包发送完成，发送结束标志",myippointer);
    header=HEADER();
    header.flag=OVERFLAG;
    header.checksum=0;
    calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
    header.checksum=calc_chksum_rst;
    memcpy(buffer,&header,sizeof(header));
    sendto(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrLen);
    mode = 0;
    ioctlsocket(clntSock, FIONBIO, &mode);
    
    return 1;
}
void* client_recv_thread(void* p)
{
    printf("pthread create ok\n");
    struct pthread_para *para;
    para=(struct pthread_para *)p;
    //SOCKET& clntSock,SOCKADDR_IN& servAddr, int& servAddrLen, char* fullData,int dataLen
    SOCKET& clntSock=para->clntSock;
    SOCKADDR_IN& servAddr=para->servAddr;
    int& servAddrLen=para->servAddrLen;
    char* fullData=para->fullData;
    int& dataLen=para->dataLen;
    u_long mode = 1;
    ioctlsocket(clntSock, FIONBIO, &mode);
    while(1)
    {
        if(recvfrom(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrLen)>0){
            // addtoclntlog("[MESG]接收到确认信息，进行校验",myippointer);
            // printf("接收到回复信息，进行校验\n");
            //接收到了，进行校验，包括校验码和seq
            //接收端只返回一个header，
            memcpy(&temp1, buffer, sizeof(header));
            checksum_from_recv1=temp1.checksum;
            temp1.checksum=0;
            calc_chksum_rst=CalcChecksum((u_short*)&temp1,sizeof(temp1));
            // memset(message,0,sizeof(message));
            // sprintf(message,"[INFO]接收到的校验和=%u，计算出的校验和为%u",checksum_from_recv1,calc_chksum_rst);
            // printf("%s\n",message);
            // addtoclntlog((const char*)message,myippointer);
            // memset(message,0,sizeof(message));
            // sprintf(message,"[INFO]接收到的伪首部8位序列号SEQ=%d",temp1.SEQ);
            // printf("%s\n",message);
            // addtoclntlog((const char*)message,myippointer);
            if(calc_chksum_rst!=checksum_from_recv1){
                //step 4 in GBN FSM
                // i.e. rdt_rcv && corrupt
                // go back to waiting status, i.e. do nothing indeed and continue
                // printf("%u!=%u,回传校验码不通过\n",calc_chksum_rst,checksum_from_recv1);
                // addtoclntlog("[ERR ]确认信息校验码校验不通过",myippointer);
                continue;
            }
            //到这里则接收并校验成功
            if((int(temp1.SEQ)+1)%256<base){basejwnum++;}
            //seq+1为0-255，如果到了256要变回0
            //**************************************
            // if(temp1.flag==DFTSNDPKT){
            //     addtoclntlog("[INFO]收到dftsndpkt!",myippointer);
            // }
            // memset(message,0,sizeof(message));
            // sprintf(message,"[INFO]接收到的default andpkt 其SEQ=%d",temp1.SEQ);
            // printf("%s\n",message);
            addtoclntlog((const char*)message,myippointer);
            //reno****************************************
            //got new ack or dup ack?
            if(lastSeq==temp1.SEQ){//duplicate ack!
            memset(message,0,sizeof(message));
            sprintf(message,"[INFO]收到重复ack");
            printf("%s\n",message);
            addtoclntlog((const char*)message,myippointer);
                if(renoState==SLOWSTART){
                    dupACKnum++;
                    if(dupACKnum==3){
                        renoState=FASTRECOV;
                        ssthresh=cwnd/2;
                        cwnd=ssthresh+3;
                        memset(message,0,sizeof(message));
                        sprintf(message,"[INFO]慢启动阶段，duplicate ack=3");
                        printf("%s\n",message);
                        addtoclntlog((const char*)message,myippointer);
                        memset(message,0,sizeof(message));
                        sprintf(message,"[INFO]cwnd变为%d,ssthresh变为%d",cwnd,ssthresh);
                        printf("%s\n",message);
                        addtoclntlog((const char*)message,myippointer);
                        //TODO:retransmit missing segment
                        goto resend_missing_seg;
                    } 
                }
                else if(renoState==CONGAVOID){
                    dupACKnum++;
                    if(dupACKnum==3){
                        renoState=FASTRECOV;
                        ssthresh=cwnd/2;
                        cwnd=ssthresh+3;
                        memset(message,0,sizeof(message));
                        sprintf(message,"[INFO]拥塞避免阶段，duplicate ack=3");
                        printf("%s\n",message);
                        addtoclntlog((const char*)message,myippointer);
                        memset(message,0,sizeof(message));
                        sprintf(message,"[INFO]cwnd变为%d,ssthresh变为%d",cwnd,ssthresh);
                        printf("%s\n",message);
                        addtoclntlog((const char*)message,myippointer);
                        //TODO:retransmit missing segment
                        goto resend_missing_seg;
                    }
                }
                else if(renoState==FASTRECOV){
                    cwnd++;
                    //cwnd更新，自动会传输新节
                    memset(message,0,sizeof(message));
                    sprintf(message,"[INFO]快速恢复阶段，收到重复ack");
                    printf("%s\n",message);
                    addtoclntlog((const char*)message,myippointer);
                    memset(message,0,sizeof(message));
                    sprintf(message,"[INFO]cwnd变为%d,ssthresh不变，为%d",cwnd,ssthresh);
                    printf("%s\n",message);
                    addtoclntlog((const char*)message,myippointer);
                }

            }else{//new ack!
                memset(message,0,sizeof(message));
                sprintf(message,"[INFO]收到新ack");
                printf("%s\n",message);
                addtoclntlog((const char*)message,myippointer);
                dupACKnum=0;
                if(renoState==SLOWSTART)
                {
                    cwnd=cwnd+1;//+1 MSS
                    memset(message,0,sizeof(message));
                    sprintf(message,"[INFO]慢启动，收到新ack");
                    printf("%s\n",message);
                    addtoclntlog((const char*)message,myippointer);
                    memset(message,0,sizeof(message));
                    sprintf(message,"[INFO]cwnd变为%d,ssthresh不变，为%d",cwnd,ssthresh);
                    printf("%s\n",message);
                    addtoclntlog((const char*)message,myippointer);
                    if(cwnd>=ssthresh){
                        memset(message,0,sizeof(message));
                        sprintf(message,"[INFO]cwnd>=ssthresh，进入拥塞避免阶段");
                        printf("%s\n",message);
                        addtoclntlog((const char*)message,myippointer);
                        renoState=CONGAVOID;
                        dupcwndnum=0;
                    }
                    //cwnd更新，自动会传输新节
                }
                else if(renoState==CONGAVOID){
                    //cwnd=cwnd+MSS*MSS/cwnd
                    //收到cwnd个ack后，cwnd+1，使其线性增长
                    dupcwndnum++;
                    memset(message,0,sizeof(message));
                    sprintf(message,"[INFO]拥塞避免阶段，收到新ack");
                    printf("%s\n",message);
                    addtoclntlog((const char*)message,myippointer);
                    memset(message,0,sizeof(message));
                    sprintf(message,"[INFO]当前阶段收到ack数为%d，cwnd=%d",dupcwndnum,cwnd);
                    printf("%s\n",message);
                    addtoclntlog((const char*)message,myippointer);
                    if(dupcwndnum==cwnd)
                    {
                        cwnd++;
                        memset(message,0,sizeof(message));
                        sprintf(message,"[INFO]共收到cwnd=%d个ack，cwnd线性+1",cwnd-1);
                        printf("%s\n",message);
                        addtoclntlog((const char*)message,myippointer);
                        dupcwndnum=0;
                    }//cwnd更新，自动会传输新节
                }
                else if(renoState==FASTRECOV){
                    renoState=CONGAVOID;
                    dupcwndnum=0;
                    cwnd=ssthresh;
                    memset(message,0,sizeof(message));
                    sprintf(message,"[INFO]快速恢复阶段，收到新ack");
                    printf("%s\n",message);
                    addtoclntlog((const char*)message,myippointer);
                    memset(message,0,sizeof(message));
                    sprintf(message,"[INFO]回到拥塞避免阶段，cwnd=ssthresh=%d",cwnd);
                    printf("%s\n",message);
                    addtoclntlog((const char*)message,myippointer);
                }
            }
            lastSeq=temp1.SEQ;
            // memset(message,0,sizeof(message));
            // sprintf(message,"[INFO]dupacknum=%d",dupACKnum);
            // printf("%s\n",message);
            // addtoclntlog((const char*)message,myippointer);

            base=(int(temp1.SEQ)+1)%256;
            // memset(message,0,sizeof(message));
            // sprintf(message,"[INFO]base变量更新=%d，实际的基序号base=%d",base,base+256*basejwnum);
            // printf("%s\n",message);
            // addtoclntlog((const char*)message,myippointer);
            // printf("收到的校验码=%d,序列号seq=%d,base更新为%d\n",checksum_from_recv1,temp1.SEQ,base);
            memset(message,0,sizeof(message));
            sprintf(message,"[INFO]base=%d，nextseqnum=%d",base+256*basejwnum,nextseqnum);
            printf("%s\n",message);
            if(base==nextseqnum%256)
            {
                //TODO:stop_timer
                //stop_timer即是不再超时重传，
                //实际上，也就是可以直接跳过下面代码的分支
                //包括这里的else now_clocktime=clock();
                //和下面的超时重传即FSM中的timeout
                //故回到原始状态，模式设为阻塞
                //continue等待即可
                //*************************************************
                //!!!!!!!!!!!!!!!!!!!!!!!!!!上面划掉
                //应用了多线程，需要显式表明stop_timer
                //注明flag=0即可
                timerIsOpen=0;
                //实际上需要判断，此时全部收完，那么就跳出，发送结束消息
                //否则就是没有收完文件的所有包，但当前发送端已经发送的包
                //已经ack完了
                //继续发送，并等待ack
                if(base+basejwnum*256==pktnum){
                    // addtoclntlog("[MESG]所有信息已发送，超时不重传",myippointer);
                    break;
                }
                else{
                    // memset(message,0,sizeof(message));
                    // sprintf(message,"[INFO]实际的基序号base=%d，nextseqnum=%d，包总数=%d",base+256*basejwnum,nextseqnum,pktnum);
                    // printf("%s\n",message);
                    // addtoclntlog((const char*)message,myippointer);
                    // printf("所有包验证完毕，继续发送\n");
                    // addtoclntlog("[MESG]已发送的所有包验证完毕，继续发送与验证",myippointer);
                    continue;
                }
                
                
            }else{
                addtoclntlog("[MESG]开启计时器",myippointer);
                timerIsOpen=1;
                now_clocktime=clock();
            }
        }else{
            //TODO 应把超时重传放到这里
            //step 2 in GBN FSM
            //timer开启时才有超时重传‘
            
            //timeout
            if(clock()-now_clocktime>MAX_TIME){
                //reno****************************
                //go back to slowstart
                ssthresh=cwnd/2;
                cwnd=1;
                dupACKnum=0;
                renoState=SLOWSTART;
                memset(message,0,sizeof(message));
                sprintf(message,"[ERR ]reno超时，回到慢启动阶段");
                printf("%s\n",message);
                addtoclntlog((const char*)message,myippointer);
                memset(message,0,sizeof(message));
                sprintf(message,"[INFO]ssthresh=cwnd/2=%d,cwnd=1MSS",cwnd);
                printf("%s\n",message);
                addtoclntlog((const char*)message,myippointer);
                //********************************
                if(timerIsOpen!=0){//timer开启
                //timer开启，flag一定为1，不用更新flag，更新计时器即可
                now_clocktime=clock();
        //reno判断拥塞重传，goto这里
        resend_missing_seg:
                //base经过%256处理,但nextseqnum没有
                int resend_index=base+256*basejwnum;
                //通过状态机可知，从base到nextseqnum-1
                //udt_send from base to nextseqnum-1

                memset(message,0,sizeof(message));
                sprintf(message,"[INFO]重传序列号从%d到%d的所有数据",resend_index,nextseqnum-1);
                printf("%s\n",message);
                addtoclntlog((const char*)message,myippointer);

                for(resend_index;resend_index<nextseqnum;resend_index++)
                {
                    if(resend_index==pktnum-1){
                        //最后一个
                        pktlen=dataLen-(pktnum-1)*MAX_BUFFER_SIZE;
                    }else{
                        pktlen=MAX_BUFFER_SIZE;
                    }
                    header=HEADER();
                    header.datasize=pktlen;//此时pktlen为不加头部的大小
                    header.SEQ=(unsigned char)(resend_index);
                    header.checksum=0;
                    header.flag=0;
                    header.ack=0;
                    //初始化为0
                    char* nowPktPointer=fullData+resend_index*MAX_BUFFER_SIZE;
                    memcpy(buffer,&header,sizeof(header));
                    memcpy(buffer+sizeof(header),nowPktPointer,pktlen);
                    calc_chksum_rst=CalcChecksum((u_short*)buffer,sizeof(header)+pktlen);
                    header.checksum=calc_chksum_rst;
                    memcpy(buffer,&header,sizeof(header));
                    memcpy(nowbuffer,&header,sizeof(header));
                    memcpy(nowbuffer+sizeof(header),nowPktPointer,pktlen);
                    //udt_send(sndpkt)
                    if(sendto(clntSock, buffer, sizeof(header)+pktlen, 0, (sockaddr*)&servAddr, servAddrLen)<0){
                    // printf("udp发送错误\n");
                    // addtoclntlog("[ERR ][超时重传]udp包发送错误",myippointer);
                    continue;
                    }
                    // memset(message,0,sizeof(message));
                    // sprintf(message,"[INFO][重传]成功发送 %d bytes数据，重发的seq=%d,重发直至seq=nextseqnum=%d",pktlen,resend_index,nextseqnum);
                    // printf("%s\n",message);
                    // addtoclntlog((const char*)message,myippointer);
                    // sprintf(message,"[INFO][重传]当前实际基序号base=%d，滑动窗口大小=%d",resend_index,nextseqnum-(base+256*basejwnum));
                    // printf("%s\n",message);
                    // addtoclntlog((const char*)message,myippointer);
                    // sprintf(message,"[INFO][重传]发送UDP包的校验和checksum=%u，伪首部的8位SEQ=%d",calc_chksum_rst,int(header.SEQ));
                    // printf("%s\n",message);
                    // addtoclntlog((const char*)message,myippointer);
                    // addtoclntlog("[MESG][重传]UDP包发送成功",myippointer);
                    
                }
                memset(message,0,sizeof(message));
                sprintf(message,"[INFO]重传完毕");
                printf("%s\n",message);
                addtoclntlog((const char*)message,myippointer);
            }
            }
        }
        
        //printf("nextseq=%d,pktnum=%d,base=%d\n",nextseqnum,pktnum,base);
        if(base>=pktnum){
            
            printf("exit ok\n");
            break;
        }
    }
    send_end_flag=1;
    pthread_exit((char *)"pthread exit!");

}
void SendFileHelper()
{
    //计时******************************************************
    QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
    //**********************************************************
    char* filename=new char[10000];
    printf("输入您想发送的文件名\n");
    scanf("%s",filename);
    string filenamestr=filename;
    ifstream fin(filenamestr.c_str(),ifstream::binary);
    char* filebuff=new char[INT_MAX];
    int index=0;
    unsigned char nowbin;
    while(fin)
    {
        nowbin=fin.get();
        filebuff[index]=nowbin;
        index++;
    }
    index--;
    fin.close();
    addtoclntlog("[MESG]文件读取成功",myippointer);
    printf("文件读取成功\n");
    int lenn=sizeof(sockAddr);
    //sendto(clntSock, buffer, sizeof(header)+pktlen, 0, (sockaddr*)&servAddr, servAddrLen)
    SendFileAsBinary(sock,sockAddr,lenn,(char*)filenamestr.c_str(),filenamestr.length());
    memset(message,0,sizeof(message));
    sprintf(message,"[MESG]文件名:%s接收完毕",filename);
    addtoclntlog((const char*)message,myippointer);
    printf("文件名发送完毕\n");
    SendFileAsBinary(sock,sockAddr,lenn,filebuff,index);
    //计时******************************************************
    QueryPerformanceCounter((LARGE_INTEGER *)&tail);
    float tttime=(tail-head)*1000.0/freq;
    float ttl = (index*8)/((tail-head)/freq);
    printf("时间=%fms\n",tttime);
    printf("吞吐率=%fbps\n",ttl);
    //**********************************************************
    memset(message,0,sizeof(message));
    sprintf(message,"[MESG]文件:%s接收完毕,长度为%dbytes",filename,index);
    addtoclntlog((const char*)message,myippointer);
    printf("文件发送完毕\n");
}
void init()
{
    //初始化DLL
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    //创建socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock==INVALID_SOCKET){
        printf("SOCKET INIT ERROR");
    }
    // servPort=4321;
    // string myip="127.1.2.3";
    servPort=6666;
    string myip="127.7.8.9";
    char* ippointer=servIP;
    ippointer = strcpy(ippointer,myip.c_str());
    memset(&sockAddr, 0, sizeof(sockAddr));  //每个字节都用0填充
    //init
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(servIP);
    sockAddr.sin_port = htons(servPort);
    int lenn=sizeof(sockAddr);
    //***************************本地socket************************
    //绑定套接字
    char clntIP[20];
    string mylocalip="127.4.5.6";
    myippointer=clntIP;
    myippointer = strcpy(myippointer,mylocalip.c_str());

	SOCKADDR_IN addrLocal;// 发送端IP和端口
	memset(&addrLocal, 0, sizeof(SOCKADDR_IN));
	addrLocal.sin_family = AF_INET; 
	addrLocal.sin_addr.s_addr = inet_addr(clntIP); 
	addrLocal.sin_port = htons(9876);
    if(bind(sock, (SOCKADDR*)&addrLocal, sizeof(addrLocal))==-1)//绑定
    {
        perror("BIND FAILED!");
        exit(-1);
    }
    addtoclntlog("[INIT]绑定套接字成功，准备连接",myippointer);
    if(ConnectWith3Handsks(sock,sockAddr, lenn)==-1){
        addtoclntlog("三次握手连接错误",myippointer);
        return;
    }
    addtoclntlog("[MESG]三次握手连接成功，返回值为1",myippointer);
    SendFileHelper();
    if(DisconnectWith4Waves(sock,sockAddr, lenn)<0){
        addtoclntlog("[ERR ]四次挥手错误",myippointer);
        return;
    }
    addtoclntlog("[MESG]四次挥手断连成功，返回值为1",myippointer);
    // printf("客户端启动成功\n");
    // printf("连接到服务器，IP地址为%s，端口号为%d\n",servIP,servPort);
    // //初始化用户名
    // printf("请输入用户名：");
    // scanf("%s",name);
    // printf("\n\n*****************************\n");
    // printf("*%s ：您已进入聊天\n",name);
    // printf("*输入 QUIT 以退出\n");
    // printf("*默认为广播模式（向所有人发送消息）\n");
    // printf("*输入 PRIVCHAT 以进入私聊模式\n");
    // printf("*输入 BROADCAST 以进入广播模式\n");
    // printf("\n*****************************\n\n");
    // strcpy(mode,mode_bdcs);
    // //初始化模式为广播

}
// void start()
// {
//     //每个客户端创建只需一个pthread用于接收，主线程用来发送
//     pthread_t id;
//     void* recv_thread(void*);
//     pthread_create(&id,0,recv_thread,0);
//     //向服务器发送连接成功信息
//     char buf2[MAX_BUFFER_LEN] = {};
//     memset(buf2,0,sizeof(buf2));
//     sprintf(buf2,"欢迎%s进入群聊",name);
//     addusername(buf2,name);
//     addusermode(buf2,mode);
//     send(sock,buf2,strlen(buf2),0);
//     while(1){//主线程，主要用来发送
//     //接收服务器传回的数据
//     //向服务器发送数据
//         char buf[MAX_BUFFER_LEN] = {};
//         //scanf("%s",buf);
//         gets(buf);
//         if(buf[0]=='\0'){continue;}
//         if(strcmp(buf,"QUIT")==0){//输入quit退出
//             //不用发quit消息，server的recv会检测到
//             break;
//         }else if(strcmp(buf,"PRIVCHAT")==0){
//             strcpy(mode,mode_priv);
//             printf("请输入您想私聊的用户名：");
//             scanf("%s",destname);
//             printf("请输入您要发送的消息\n");
//             strcat(mode,"=");
//             strcat(mode,destname);
//             continue;
//         }else if(strcmp(buf,"BROADCAST")==0){
//             strcpy(mode,mode_bdcs);
//             printf("请输入您要发送的消息\n");
//             continue;
//         }
//         char msg[MAX_BUFFER_LEN] = {};
//         sprintf(msg,"%s",buf);
//         //在消息前端添加用户名，消息=[@名字]消息
//         addusername(msg,name);
//         addusermode(msg,mode);
//         send(sock,msg,strlen(msg),0);
        
//     }
//     //关闭套接字
//     closesocket(sock);
// }
// //接收线程
// void* recv_thread(void* p){
//     while(1){
//         char buf[MAX_BUFFER_LEN] = {};
//         if (recv(sock,buf,sizeof(buf),0) <=0){
//             break;
//         }
//         printf("%s\n",buf);
//     }
// }
int main()
{
    init();
    //start();
    //终止使用 DLL
    WSACleanup();
    system("pause");
    return 0;
}