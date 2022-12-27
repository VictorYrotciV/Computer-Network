#include "loghdr.h"
#pragma comment (lib, "ws2_32.lib")  //加载 ws2_32.dll
char servIP[20];
char* ippointer;
char* myippointer;
int servPort;
int servSeq=0;
int servACK=0;
SOCKET servSock;
char message[100];
clock_t now_clocktime;//这里用clock_t因为他能精确到ms，time_t只能精确到秒
//用clock_t来计时，time_t用于时间戳的日志记录
SOCKADDR_IN sockAddr;
SOCKADDR_IN addrRouter;// 发送端IP和端口
//用于将clntAddr.sin_addr.s_addr转换为字符串
#define IP2UCHAR(addr) \
((unsigned char *)&addr)[0], \
((unsigned char *)&addr)[1], \
((unsigned char *)&addr)[2], \
((unsigned char *)&addr)[3]



//将sendfile中的一些变量放到全局，方便传入多线程
long int tailpointer;//由于按包来传输，记录每次拼接后末尾的指针便于下一次拼接
HEADER header;
HEADER sndpkt;//只发送伪首部
// make sndpkt
// servSeq=0;//初始化seq为0 
    // sndpkt.SEQ=servSeq;
u_short calc_chksum_rst;
    // calc_chksum_rst=CalcChecksum((u_short*)&sndpkt,sizeof(sndpkt));
    // sndpkt.checksum=calc_chksum_rst;


char* buffer;
//向pthread中传参的结构体
struct pthread_para
{
    SOCKET servSock;
    SOCKADDR_IN clntAddr;
    int clntAddrLen;
    char* fullData;
};
//
int thread_begin=0;
int thread_end=0;
int ConnectWith3Handsks(SOCKET& servSock,SOCKADDR_IN& clntAddr, int& clntAddrLen){
    HEADER header;
    u_short calc_chksum_rst;
    char* buffer=new char[sizeof(header)];//头部初始化
    //********************************************************************
    //recv第一次握手
    while(1)
    {//第一次recv不重传，不用计时
        if(recvfrom(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr,&clntAddrLen)<0)
        {
            addtoservlog("[ERR ]第一次握手接收错误",myippointer);
            printf("第一次握手接收错误\n");
            return -1;
        }
        addtoservlog("[MESG]第一次握手接收成功，进行校验",myippointer);
        printf("第一次握手接收了\n");
        memcpy(&header,buffer,sizeof(header));
        u_short checksum_from_recv=header.checksum;
        header.checksum=0;
        calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
        if(header.flag==HSK1OF3&&calc_chksum_rst==checksum_from_recv)
        {
            addtoservlog("[MESG]第一次握手校验成功",myippointer);
            printf("第一次握手校验成功\n");
            break;
        }else{
            addtoservlog("[ERR ]第一次握手校验失败",myippointer);
            printf("校验码错误\n");
        }
    }
    printf("第一次握手结束\n");
    addtoservlog("[MESG]第一次握手完毕",myippointer);
    //********************************************************************
    //send第二次握手
    header.flag=HSK2OF3;
    calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
    header.checksum=calc_chksum_rst;
    memcpy(buffer,&header,sizeof(header));
    if (sendto(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr, clntAddrLen) == -1)
    {
        printf("第二次握手发送失败\n");
        addtoservlog("[ERR ]第二次握手发送失败",myippointer);
        return -1;
    }
    printf("第二次握手发送成功\n");
    addtoservlog("[MESG]第二次握手发送成功，开启计时器",myippointer);
    now_clocktime=clock();
    //********************************************************************
    //recv第三次握手
    while(recvfrom(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr,&clntAddrLen)<=0){
        if(clock()-now_clocktime>MAX_TIME)
        {//超时重传
            header.flag=HSK2OF3;
            header.checksum=0;
            calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
            memcpy(&header,buffer,sizeof(header));
            if (sendto(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr, clntAddrLen) == -1)
            {
                addtoservlog("[ERR ]第二次握手超时重传失败",myippointer);
                return -1;
            }
            now_clocktime=clock();
            printf("第三次握手超时，第二次握手重传发送成功\n");
            addtoservlog("[MESG]第三次握手超时，重传发送成功",myippointer);
        }
    }

    printf("第三次握手接收成功，关闭计时器\n");
    addtoservlog("[MESG]第三次握手接收成功，关闭计时器",myippointer);
    //校验
    HEADER temp1;
    memcpy(&temp1, buffer, sizeof(header));
    u_short checksum_from_recv=temp1.checksum;
    temp1.checksum=0;
    calc_chksum_rst=CalcChecksum((u_short*)&temp1,sizeof(temp1));
    if(temp1.flag==HSK3OF3&&calc_chksum_rst==checksum_from_recv)
    {
        addtoservlog("[MESG]三次握手完成，已建立连接",myippointer);
        printf("三次握手完成，已建立连接\n");
    }else{
        if(temp1.flag!=HSK3OF3){printf("flag=%u,flag有误\n",temp1.flag);}
        addtoservlog("[ERR ]三次握手传输有误，请重新建立连接",myippointer);
        printf("三次握手传输有误，请重新建立连接\n");
        return -1;
    }
    return 1;
}

int DisconnectWith4Waves(SOCKET& servSock,SOCKADDR_IN& clntAddr, int& clntAddrLen){
    HEADER header;
    u_short calc_chksum_rst;
    char* buffer = new char[sizeof(header)];
    //recvfrom(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr,&clntAddrLen)
    //sendto(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr, clntAddrLen)
    //****************************************************************************
    //recv第一次挥手
    while(1)
    {//肯定是服务端收到客户的FIN后才开始四次挥手的，第一次recv不重传，不用计时
        if(recvfrom(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr,&clntAddrLen)<0)
        {
            addtoservlog("[ERR ]第一次挥手接收错误",myippointer);
            printf("第一次挥手接收错误\n");
            return -1;
        }
        addtoservlog("[MESG]第一次挥手接收成功，进行校验",myippointer);
        printf("第一次挥手接收了\n");
        memcpy(&header,buffer,sizeof(header));
        u_short checksum_from_recv=header.checksum;
        header.checksum=0;
        calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
        if(header.flag==WAV1OF4&&calc_chksum_rst==checksum_from_recv)
        {
            printf("第一次挥手校验成功\n");
            addtoservlog("[MESG]第一次挥手校验成功",myippointer);
            break;
        }else{
            addtoservlog("[ERR ]第一次挥手校验错误",myippointer);
            printf("校验码错误\n");
        }
    }
    //*******************************************************
    //send第二次挥手
    header.checksum=0;
    header.ack=header.SEQ+1;
    //记录这里的ack=u+1，因为第三次挥手的ack=u+1，第四次挥手的seq=u+1
    u_short ackin2ndwave=header.ack;
    header.flag=WAV2OF4;
    calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
    header.checksum=calc_chksum_rst;
    memcpy(buffer,&header,sizeof(header));
    if (sendto(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr, clntAddrLen) == -1)
    {
        printf("第二次挥手发送错误\n");
        addtoservlog("[ERR ]第二次挥手发送错误",myippointer);
        return -1;
    }
    addtoservlog("[MESG]第二次挥手发送成功",myippointer);
    printf("第二次挥手发送成功\n");
    //*******************************************************
    //send第三次挥手
    //这里ack等于第二次挥手的ack=u+1
    header.checksum=0;
    header.ack=ackin2ndwave;
    //记录这里的seq=w，因为第四次挥手的ack=w+1
    u_short seqin3rdwave=header.SEQ;
    header.flag=WAV3OF4;
    calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
    header.checksum=calc_chksum_rst;
    memcpy(buffer,&header,sizeof(header));
    if (sendto(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr, clntAddrLen) == -1)
    {
        addtoservlog("[ERR ]第三次挥手发送失败",myippointer);
        return -1;
    }
    printf("第三次挥手发送成功\n");
    addtoservlog("[MESG]第三次挥手发送成功，开启计时器",myippointer);
    now_clocktime=clock();
    //recv第四次握手
    while(recvfrom(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr,&clntAddrLen)<=0){
        if(clock()-now_clocktime>MAX_TIME)
        {//第三次挥手超时重传
            header.ack=ackin2ndwave;
            header.flag=WAV3OF4;
            u_short seqin3rdwave=header.SEQ;
            header.checksum=0;
            calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
            memcpy(&header,buffer,sizeof(header));
            if (sendto(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr, clntAddrLen) == -1)
            {
                addtoservlog("[ERR ]第四次挥手超时，第三次挥手进行重传发送失败",myippointer);
                return -1;
            }
            now_clocktime=clock();
            addtoservlog("[MESG]第四次挥手接收超时，第三次挥手重传发送成功",myippointer);
            printf("第四次挥手接收超时，第三次挥手重传发送成功\n");
        }
    }
    addtoservlog("[MESG]第四次挥手接收成功，进行校验，关闭计时器",myippointer);
    //校验
    HEADER temp1;
    memcpy(&temp1, buffer, sizeof(header));
    u_short checksum_from_recv=temp1.checksum;
    temp1.checksum=0;
    calc_chksum_rst=CalcChecksum((u_short*)&temp1,sizeof(temp1));
    if(temp1.flag==WAV4OF4&&(temp1.ack==seqin3rdwave+1)&&(temp1.SEQ=ackin2ndwave)&&calc_chksum_rst==checksum_from_recv)
    {
        addtoservlog("[MESG]第四次挥手校验成功，四次挥手完成，已断开连接",myippointer);
        printf("四次挥手完成，已断开连接\n");
    }else{
        addtoservlog("[ERR ]四次挥手校验错误",myippointer);
        printf("四次挥手错误\n");
        return -1;
    }
    addtoservlog("[MESG]四次挥手所有过程成功完成",myippointer);
    printf("四次挥手所有过程成功完成\n");
    return 1;
}

int RecvFile(SOCKET& servSock,SOCKADDR_IN& clntAddr, int& clntAddrLen, char* fullData){
    //参数：fulldata记录接收完毕后除了头部的数据
    tailpointer=0;//由于按包来传输，记录每次拼接后末尾的指针便于下一次拼接
    header=HEADER();
    sndpkt=HEADER();
    // HEADER header;
    // HEADER sndpkt;//只发送伪首部
    // make sndpkt
    servSeq=0;//初始化seq为0 
    sndpkt.SEQ=servSeq;
    sndpkt.flag=DFTSNDPKT;
    // u_short calc_chksum_rst;
    calc_chksum_rst=CalcChecksum((u_short*)&sndpkt,sizeof(sndpkt));
    sndpkt.checksum=calc_chksum_rst;


    buffer = new char[MAX_BUFFER_SIZE+sizeof(header)];
    
    //servACK=0;//初始时接收ACK0
    thread_begin=0;
    thread_end=0;
    addtoservlog("[MESG]准备接收文件",myippointer);
    printf("准备接收\n");

    //**************************************************
        //pthread
        //initiaize pthread params

        // struct pthread_para para;
        // para.servSock=servSock;
        // para.clntAddr=clntAddr;
        // para.clntAddrLen=clntAddrLen;
        // para.fullData=fullData;
        // pthread_t id;
        // void* server_send_thread(void*);
        // pthread_create(&id,0,server_send_thread,&(para));

    //**************************************************
    mode = 0;
    ioctlsocket(servSock, FIONBIO, &mode);
    while(1){
        
        //接收并放到缓冲区
        int recvLen=recvfrom(servSock,buffer,sizeof(header)+MAX_BUFFER_SIZE,0,(sockaddr*)&clntAddr,&clntAddrLen);
        if(recvLen<0)
        {
            addtoservlog("[ERR ]udp接收错误",myippointer);
            printf("接收错误\n");
            return -1;
        }
        
        addtoservlog("[MESG]udp包接收成功",myippointer);
        memset(message,0,sizeof(message));
        sprintf(message,"[INFO]收到UDP包长度=%d",recvLen);
        addtoservlog((const char*)message,myippointer);
        printf("udp接收成功\n");
        memcpy(&header,buffer,sizeof(header));
        printf("flag=%d\n",header.flag);
        u_short checksum_from_recv=header.checksum;
        header.checksum=0;                                                                      
        memcpy(buffer,&header,sizeof(header));
        calc_chksum_rst=CalcChecksum((u_short*)buffer,recvLen);
        printf("%d\n",sizeof(buffer));
        printf("接收到的校验和=%u，计算出的校验和为%u\n",checksum_from_recv,calc_chksum_rst);
        memset(message,0,sizeof(message));
        sprintf(message,"[INFO]接收到的校验和=%u，计算出的校验和=%u",checksum_from_recv,calc_chksum_rst);
        printf("接收到的seq=%d\n",header.SEQ);
        addtoservlog((const char*)message,myippointer);
        if(calc_chksum_rst!=checksum_from_recv){
            printf("校验码校验失败\n");addtoservlog("[ERR ]udp包校验码校验失败",myippointer);
            memset(message,0,sizeof(message));
            printf("不能正确接收与验证，发送default sndpkt，seq=%d",servSeq);
            sprintf(message,"[INFO]不能正确接收与验证，发送default sndpkt，seq=%d",servSeq);
            addtoservlog((const char*)message,myippointer);
            while(1)
            {
                memcpy(buffer,&sndpkt,sizeof(sndpkt));
                if (sendto(servSock,buffer,sizeof(sndpkt),0,(sockaddr*)&clntAddr, clntAddrLen) == -1)
                {
                    addtoservlog("[ERR ]default sndpkt发送错误",myippointer);
                    printf("Default sndpkt发送错误\n");
                    continue;
                }
                else break;
            }
            addtoservlog("[INFO]default sndpkt发送成功",myippointer);
            printf("Default sndpkt发送成功\n");
            continue;
        }
        if(header.flag==OVERFLAG&&calc_chksum_rst==checksum_from_recv)
        {
            addtoservlog("[MESG]收到结束标志，所有文件接收完毕",myippointer);
            printf("所有文件接收完毕\n");
            break;
        }
        
        if(header.flag==INITFLAG&&calc_chksum_rst==checksum_from_recv)
        {//rdt_rcv(rcvpkt) && notcorrupt(rcvpkt) 
            addtoservlog("[MESG]udp包校验码校验成功",myippointer);
            printf("校验码校验成功\n");
            //GBN协议中 seq不对直接丢弃即可
            //不用像停等协议一样超时重传
            if(servSeq==(int)header.SEQ)
            {
                memset(message,0,sizeof(message));
                sprintf(message,"[INFO]应收到的SEQ=%d，收到UDP包的SEQ=%d",servSeq,(int)header.SEQ);
                addtoservlog((const char*)message,myippointer);
                addtoservlog("[INFO]SEQ值验证正确",myippointer);
                header=HEADER();
                header.SEQ=(unsigned char)servSeq;
                // header.flag=123;
                header.checksum=0;
                //sndpkt=makepkt(expextedseqnum)
                //udt send
                //seq++
                //header.flag=DFTSNDPKT;
                calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
                header.checksum=calc_chksum_rst;
                memcpy(buffer,&header,sizeof(header));
                memcpy(&sndpkt,&header,sizeof(header));
                //********************************
                sndpkt.flag=DFTSNDPKT;
                sndpkt.checksum=0;
                calc_chksum_rst=CalcChecksum((u_short*)&sndpkt,sizeof(sndpkt));
                sndpkt.checksum=calc_chksum_rst;
                //*********************************
                if (sendto(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr, clntAddrLen) == -1)
                {
                    printf("ack发送错误\n");
                    continue;
                    //return -1;
                }
                printf("校验码和SEQ均校验成功，确认消息发送成功\n");
                printf("发送的校验码=%d\n",calc_chksum_rst);
                addtoservlog("[MESG]校验码和SEQ均校验成功，确认消息发送成功",myippointer);
                memset(message,0,sizeof(message));
                sprintf(message,"[INFO]发送的校验码chechsum=%d",calc_chksum_rst);
                addtoservlog((const char*)message,myippointer);
                servSeq++;
                if(servSeq>255)//SEQ只有八位
                {
                    servSeq=0;
                }
                memset(message,0,sizeof(message));
                sprintf(message,"[INFO]更新服务端应收到的SEQ=%d",servSeq);
                addtoservlog((const char*)message,myippointer);
                //extract and deliver data
                memset(message,0,sizeof(message));
                printf("收到 %d bytes数据，序列号为%d\n",recvLen-sizeof(header),int(header.SEQ));
                sprintf(message,"[INFO]成功收到 %d bytes数据，序列号为%d",recvLen-sizeof(header),int(header.SEQ));
                addtoservlog((const char*)message,myippointer);
                char* tail=new char[recvLen-sizeof(header)];
                //把除了头部的缓冲区（及数据）解码记录
                memcpy(tail,buffer+sizeof(header),recvLen-sizeof(header));
                //将这部分作为尾部，接到已接收的包后面
                memcpy(fullData+tailpointer,tail,recvLen-sizeof(header));
                //更新尾部指针
                tailpointer=tailpointer+recvLen-sizeof(header);
            }else{
                printf("服务器supposed seq=%d,接收到seq=%d\n",servSeq,(int)header.SEQ);
                addtoservlog("[ERR ]seq值不对应，丢弃当前包",myippointer);
                memset(message,0,sizeof(message));
                printf("不能正确接收与验证，发送default sndpkt，seq=%d",servSeq);
                sprintf(message,"[INFO]不能正确接收与验证，发送default sndpkt，seq=%d",servSeq);
                addtoservlog((const char*)message,myippointer);
                while(1)
                {
                    memcpy(buffer,&sndpkt,sizeof(sndpkt));
                    if (sendto(servSock,buffer,sizeof(sndpkt),0,(sockaddr*)&clntAddr, clntAddrLen) == -1)
                    {
                        addtoservlog("[ERR ]default sndpkt发送错误",myippointer);
                        printf("Default sndpkt发送错误\n");
                        continue;
                    }
                    else break;
                }
                addtoservlog("[INFO]default sndpkt发送成功",myippointer);
                printf("Default sndpkt发送成功\n");
            }
            
        }
        // memcpy(buffer,&sndpkt,sizeof(sndpkt));
        // if (sendto(servSock,buffer,sizeof(sndpkt),0,(sockaddr*)&clntAddr, clntAddrLen) == -1)
        // {
        //     printf("Default sndpkt发送错误\n");
        //     continue;
        // }
        // addtoservlog("default sndpkt发送成功",myippointer);
    }
    // thread_end=1;
    // printf("thread_end=%d\n",thread_end);
    return tailpointer;
}
void* server_send_thread(void* p)
{
    char* bufferp = new char[MAX_BUFFER_SIZE+sizeof(header)];
    struct pthread_para *para;
    para=(struct pthread_para *)p;
    //SOCKET& servSock,SOCKADDR_IN& clntAddr, int& clntAddrLen, char* fullData
    SOCKET& servSock=para->servSock;
    SOCKADDR_IN& clntAddr=para->clntAddr;
    int& clntAddrLen=para->clntAddrLen;
    char* fullData=para->fullData;
    //default send

    while(1){
        if(thread_end==1){break;}
        else{
        Sleep(1000);
        if(thread_end==1){break;}
        //sndpkt.datasize=11451;
        
        sndpkt.flag=DFTSNDPKT;
        sndpkt.checksum=0;
        u_short calc_chksum_rst1=CalcChecksum((u_short*)&sndpkt,sizeof(sndpkt));
        sndpkt.checksum=calc_chksum_rst1;
        memcpy(bufferp,&sndpkt,sizeof(sndpkt));
        //sendto(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr, clntAddrLen)
        //sendto(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr, clntAddrLen)
        //recvfrom(servSock,buffer,sizeof(header)+MAX_BUFFER_SIZE,0,(sockaddr*)&clntAddr,&clntAddrLen)
        if (sendto(servSock,bufferp,sizeof(sndpkt),0,(sockaddr*)&clntAddr, clntAddrLen) == -1)
        {
            if(thread_end==1){break;}
            printf("thread_end等于%d\n",thread_end);
            printf("Default sndpkt发送错误\n");
            continue;
        }
        addtoservlog("default sndpkt发送成功",myippointer);
        printf("default sndpkt发送成功\n");
        }
        
    }
    
    pthread_exit((char *)"pthread exit!");
}
void RecvFileHelper()
{
    char* filename=new char[10000];
    char* filebuff=new char[INT_MAX];
    int lenn=sizeof(sockAddr);
    //发送端会发回文件名和文件的char数组
    int namelen=0;
    //int nameLen=RecvFile(servSock,sockAddr, lenn,filename);
    int nameLen=RecvFile(servSock,addrRouter, lenn,filename);
    memset(message,0,sizeof(message));
    sprintf(message,"[MESG]文件名:%s接收完毕",filename);
    addtoservlog((const char*)message,myippointer);
    printf("%s\n",message);
    //int fileLen=RecvFile(servSock,sockAddr, lenn,filebuff);
    int fileLen=RecvFile(servSock,addrRouter, lenn,filebuff);
    memset(message,0,sizeof(message));
    sprintf(message,"[MESG]文件:%s接收完毕,长度为%dbytes",filename,fileLen);
    addtoservlog((const char*)message,myippointer);
    printf("%s\n",message);
    string namestr=filename;
    // ofstream fout(namestr.c_str(),ofstream::binary);
    ofstream fout(namestr.c_str(),ios::out|ios::binary);
    for(int i=0;i<fileLen;i++){fout<<filebuff[i];}
    fout.close();
    addtoservlog("[MESG]文件写入完毕",myippointer);
    printf("文件写入完毕\n");
}
void init(){
    //初始化DLL
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    //创建socket(UDP)
    servSock = socket(AF_INET, SOCK_DGRAM, 0);
    if(servSock==INVALID_SOCKET){
        perror("SERVER SOCKET INIT ERROR");
        exit(0);
    }
    
    //***************************服务端socket**********************
    servPort=4321;
    string myip="127.1.2.3";
    ippointer=servIP;
    ippointer = strcpy(ippointer,myip.c_str());
    memset(&sockAddr, 0, sizeof(sockAddr));  //每个字节都用0填充
    //init
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(servIP);
    sockAddr.sin_port = htons(servPort);
    //绑定
    if(bind(servSock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR))==-1)//绑定
    {
        perror("BIND FAILED!");
        exit(-1);
    }printf("进入监听状态，等待连接。。。\n");
    //*************************
    myippointer=ippointer;
    addtoservlog("[INIT]进入监听状态，等待连接",myippointer);
    //******************************路由器****
    char rterIP[20];
    string routerip="127.7.8.9";
    char* rterippointer=rterIP;
    rterippointer = strcpy(rterippointer,routerip.c_str());

	
	memset(&addrRouter, 0, sizeof(SOCKADDR_IN));
	addrRouter.sin_family = AF_INET; 
	addrRouter.sin_addr.s_addr = inet_addr(rterIP); 
	addrRouter.sin_port = htons(6666);
    //*********************************************
    int lenn=sizeof(addrRouter);
    if(ConnectWith3Handsks(servSock,addrRouter, lenn)<=0)
    {
        addtoservlog("[ERR ]三次握手连接错误",myippointer);
        return;
    }
    addtoservlog("[MESG]三次握手连接成功，返回值为1",myippointer);
    RecvFileHelper();
    if(DisconnectWith4Waves(servSock,addrRouter, lenn)<=0)
    {
        addtoservlog("[ERR ]四次挥手错误",myippointer);
        return;
    }
    addtoservlog("[MESG]四次挥手断连成功，返回值为1",myippointer);
    // int lenn=sizeof(sockAddr);
    // if(ConnectWith3Handsks(servSock,sockAddr, lenn)<=0)
    // {
    //     addtolog("三次握手连接错误",myippointer);
    //     return;
    // }
    // addtolog("三次握手连接成功，返回值为1",myippointer);
    // RecvFileHelper();
    // if(DisconnectWith4Waves(servSock,sockAddr, lenn)<=0)
    // {
    //     addtolog("四次挥手错误",myippointer);
    //     return;
    // }
    // addtolog("四次挥手断连成功，返回值为1",myippointer);
    // char buf[MAX_BUFFER_LEN];
	// FILE *logs = fopen("log.txt", "a+");
	// if(logs== NULL)
	// {
	// printf("open file error: \n");
	// }else{
    // memset(buf,0,sizeof(buf));
    // char const *msg_serv_init = "****************************************************\n SERVER INIT SUCCESS";
	// sprintf(buf, "%s",msg_serv_init);
    // //为消息添加时间戳
    // addtimestamp(buf);
	// fputs(buf,logs);
    // //记录到日志
    // fputs("\n",logs);
	// fclose(logs);
	// }
}

// void start()
// {
//     printf("服务器启动成功\n");
//     while(1){
//         //SOCKADDR clntAddr;
//         struct sockaddr_in clntAddr;
//         int nSize = sizeof(SOCKADDR);
//         SOCKET clntSock = accept(servSock, (SOCKADDR*)&clntAddr, &nSize);
//         //调用accept进入堵塞状态，等待客户端的连接
//         if(clntSock==INVALID_SOCKET){
//             printf("CLIENT SOCKET CONNECT ERROR\n");
//             continue;
//         }
//         int i=0;
//         for(i=0;i<MAX_CLIENT_NUM;i++)
//         {
//             if(clntSockArr[i]==0)
//             {
//                 memset(clntip[i],0,sizeof(clntip[i]));
//                 sprintf(clntip[i], "%u.%u.%u.%u", IP2UCHAR(clntAddr.sin_addr.s_addr));
//                 //用数组记录客户端socket
//                 clntSockArr[i] =  clntSock;
//                 printf("线程号= %d\n",(int)clntSock);
//                 //启动一个线程为该客户服务
//                 pthread_t tid;
//                 pthread_create(&tid,0,serv_thread,&clntSock);
//                 break;
//             }
//             if (MAX_CLIENT_NUM == i-1){
//             //发送给客户端说聊天室满了
//             char const *msg_room_full = "对不起，聊天室已经满了!";
//             send(clntSock,msg_room_full,strlen(msg_room_full),0);
//             closesocket(clntSock);
//             }
//         }
//     }
//     // closesocket(clntSock);
//     closesocket(servSock);
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