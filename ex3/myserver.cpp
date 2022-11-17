#include "loghdr.h"
#pragma comment (lib, "ws2_32.lib")  //加载 ws2_32.dll
char servIP[20];
int servPort;
int servSeq=0;
int servACK=0;
SOCKET servSock;
clock_t now_clocktime;//这里用clock_t因为他能精确到ms，time_t只能精确到秒
//用clock_t来计时，time_t用于时间戳的日志记录
SOCKADDR_IN sockAddr;
//用于将clntAddr.sin_addr.s_addr转换为字符串
#define IP2UCHAR(addr) \
((unsigned char *)&addr)[0], \
((unsigned char *)&addr)[1], \
((unsigned char *)&addr)[2], \
((unsigned char *)&addr)[3]

int ConnectWith3Handsks(SOCKET& servSock,SOCKADDR_IN& clntAddr, int& clntAddrLen){
    HEADER header;
    u_short calc_chksum_rst;
    char* buffer=new char[sizeof(header)];//头部初始化
    //********************************************************************
    //recv第一次握手
    printf("第一次握手开始接收\n");
    while(1)
    {//第一次recv不重传，不用计时
        if(recvfrom(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr,&clntAddrLen)<0)
        {
            printf("第一次握手接收错误\n");
            return -1;
        }
        printf("第一次握手接收了\n");
        memcpy(&header,buffer,sizeof(header));
        u_short checksum_from_recv=header.checksum;
        header.checksum=0;
        calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
        if(header.flag==HSK1OF3&&calc_chksum_rst==checksum_from_recv)
        {
            printf("第一次握手接收成功\n");
            break;
        }else{
            printf("校验码错误\n");
        }
    }
    printf("第一次握手结束接收\n");
    //********************************************************************
    //send第二次握手
    header.flag=HSK2OF3;
    calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
    header.checksum=calc_chksum_rst;
    memcpy(buffer,&header,sizeof(header));
    if (sendto(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr, clntAddrLen) == -1)
    {
        return -1;
    }
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
                return -1;
            }
            now_clocktime=clock();
            printf("第二次握手超时，进行重传\n");
        }
    }
    //校验
    HEADER temp1;
    memcpy(&temp1, buffer, sizeof(header));
    u_short checksum_from_recv=temp1.checksum;
    temp1.checksum=0;
    calc_chksum_rst=CalcChecksum((u_short*)&temp1,sizeof(temp1));
    if(temp1.flag==HSK3OF3&&calc_chksum_rst==checksum_from_recv)
    {
        printf("三次握手完成，已建立连接\n");
    }else{
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
            printf("第一次挥手接收错误\n");
            return -1;
        }
        printf("第一次挥手接收了\n");
        memcpy(&header,buffer,sizeof(header));
        u_short checksum_from_recv=header.checksum;
        header.checksum=0;
        calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
        if(header.flag==WAV1OF4&&calc_chksum_rst==checksum_from_recv)
        {
            printf("第一次挥手接收成功\n");
            break;
        }else{
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
        return -1;
    }
    printf("第二次挥手发送成功\n");
    now_clocktime=clock();
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
        return -1;
    }
    printf("第三次挥手发送成功\n");
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
                return -1;
            }
            now_clocktime=clock();
            printf("第三次挥手超时，进行重传\n");
        }
    }
    //校验
    HEADER temp1;
    memcpy(&temp1, buffer, sizeof(header));
    u_short checksum_from_recv=temp1.checksum;
    temp1.checksum=0;
    calc_chksum_rst=CalcChecksum((u_short*)&temp1,sizeof(temp1));
    if(temp1.flag==WAV4OF4&&(temp1.ack==seqin3rdwave+1)&&(temp1.SEQ=ackin2ndwave)&&calc_chksum_rst==checksum_from_recv)
    {
        printf("四次挥手完成，已断开连接\n");
    }else{
        printf("四次挥手错误\n");
        return -1;
    }
    printf("四次挥手所有过程成功完成\n");
    return 1;
}

int RecvFile(SOCKET& servSock,SOCKADDR_IN& clntAddr, int& clntAddrLen, char* fullData){
    //参数：fulldata记录接收完毕后除了头部的数据
    long int tailpointer=0;//由于按包来传输，记录每次拼接后末尾的指针便于下一次拼接
    HEADER header;
    HEADER temp1;
    u_short calc_chksum_rst;
    char* buffer = new char[MAX_BUFFER_SIZE+sizeof(header)];
    servSeq=0;//初始化seq为0 
    //servACK=0;//初始时接收ACK0
    while(1){
        //接收并放到缓冲区
        int recvLen=recvfrom(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr,&clntAddrLen);
        if(recvLen<0)
        {
            printf("接收错误\n");
            return -1;
        }
        memcpy(&header,buffer,sizeof(header));
        u_short checksum_from_recv=header.checksum;
        header.checksum=0;
        calc_chksum_rst=CalcChecksum((u_short*)&buffer,sizeof(buffer));
        if(header.flag==OVERFLAG&&calc_chksum_rst==checksum_from_recv)
        {
            printf("文件接收完毕\n");
            break;
        }
        if(header.flag==INITFLAG&&calc_chksum_rst==checksum_from_recv)
        {//rdt_rcv(rcvpkt) && notcorrupt(rcvpkt) 
            if(servSeq!=int(header.SEQ))
            {//has_seq1(),重传，当前记录的应该的seq与收到的seq不同
                //buffer不变，checksum不变，传回应该的seq
                header.SEQ=(unsigned char)servSeq;
                header.checksum=0;
                //只传回一个header，不用传回数据
                //也不用数据计算校验和，否则那边无法校验
                //只需要seq
                calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
                header.checksum=calc_chksum_rst;
                //发回
                if (sendto(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr, clntAddrLen) == -1)
                {
                    printf("发送错误\n");
                    return -1;
                }
            }
            else
            {//has_seq0,发回正确的seq，更新服务端的seq
                header.SEQ=(unsigned char)servSeq;
                header.checksum=0;
                calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
                header.checksum=calc_chksum_rst;
                if (sendto(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr, clntAddrLen) == -1)
                {
                    //如果接收正确，这里send的应该是不变的
                    printf("发送错误\n");
                    return -1;
                }
                servSeq++;
                if(servSeq>255)//SEQ只有八位
                {
                    servSeq=0;
                }
                //extract and deliver data
                printf("收到 %d bytes数据，序列号为%d\n",recvLen-sizeof(header),int(header.SEQ));
                char* tail=new char[recvLen-sizeof(header)];
                //把除了头部的缓冲区（及数据）解码记录
                memcpy(tail,buffer+sizeof(header),recvLen-sizeof(header));
                //将这部分作为尾部，接到已接收的包后面
                memcpy(fullData+tailpointer,tail,recvLen-sizeof(header));
                //更新尾部指针
                tailpointer=tailpointer+recvLen-sizeof(header);
            }
        }

    }
    //这里应该接收到结束的flag，最后验证确认over则退出
    now_clocktime=clock();
    while(recvfrom(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr,&clntAddrLen)<=0){
        if(clock()-now_clocktime>5*MAX_TIME){//时间太久没收到就退出
            return -1;
        }
        HEADER temp1;
        memcpy(&temp1, buffer, sizeof(header));
        u_short checksum_from_recv=temp1.checksum;
        temp1.checksum=0;
        calc_chksum_rst=CalcChecksum((u_short*)&temp1,sizeof(temp1));
        if(calc_chksum_rst==checksum_from_recv&&temp1.flag==OVERFLAG){
            return 1;
        }
        return -1;
    }
    // header=HEADER();
    // header.flag=OVERFLAG;
    // header.checksum=0;
    // calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
    // header.checksum=calc_chksum_rst;
    // memcpy(buffer,&header,sizeof(header));
    // if(sendto(servSock,buffer,sizeof(header),0,(sockaddr*)&clntAddr, clntAddrLen))
    // {
    //     return -1;
    // }
    return tailpointer;
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
    //绑定套接字
    //输入IP和端口号
    // printf("请输入IP地址：");
    // scanf("%s", servIP);
    // printf("请输入端口号：");
    // scanf("%d", &servPort);
    // printf("\n");
    servPort=1234;
    string myip="127.0.0.2";
    char* ippointer=servIP;
    ippointer = strcpy(ippointer,myip.c_str());
    memset(&sockAddr, 0, sizeof(sockAddr));  //每个字节都用0填充
    //init
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(servIP);
    sockAddr.sin_port = htons(servPort);
    if(bind(servSock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR))==-1)//绑定
    {
        perror("BIND FAILED!");
        exit(-1);
    }
    printf("进入监听状态，等待连接。。。\n");
    // if(listen(servSock, MAX_CLIENT_NUM)==-1)//监听
    // {
    //     perror("LISTEN FAILED!");
    //     exit(-1);
    // }
    int lenn=sizeof(sockAddr);
    ConnectWith3Handsks(servSock,sockAddr, lenn);
    DisconnectWith4Waves(servSock,sockAddr, lenn);
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