#include "loghdr.h"
#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
char servIP[20];//服务端ip
int servPort;//服务端端口号
int clntSeq=0;
char message[200];
char* myippointer;
SOCKET sock;//实例化全局socket
SOCKADDR_IN sockAddr;
clock_t now_clocktime;

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
        addtolog("第一次握手发送错误",myippointer);
        printf("第一次握手发送错误");
        return -1;
    }
    addtolog("第一次握手发送成功，开启计时器",myippointer);
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
            if(sendto(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrLen)){addtolog("第一次握手超时重传发送错误",myippointer);return -1;}
            now_clocktime = clock();
            addtolog("第二次握手接收超时，第一次挥手重传发送成功",myippointer);
            printf("第二次握手接收超时，第一次挥手进行重传\n");
        }
    }
    addtolog("第二次握手接收成功，进行校验，关闭计时器",myippointer);
    printf("第二次握手接收成功，进行校验，关闭计时器\n");
    HEADER temp1;
    memcpy(&temp1, buffer, sizeof(header));
    u_short checksum_from_recv=temp1.checksum;
    temp1.checksum=0;
    calc_chksum_rst=CalcChecksum((u_short*)&temp1,sizeof(temp1));
    if(temp1.flag==HSK2OF3&&calc_chksum_rst==checksum_from_recv)
    {
        addtolog("第二次握手校验成功",myippointer);
        printf("二次握手完成，继续三次握手\n");
    }else{
        addtolog("二次握手校验有误，请重新连接",myippointer);
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
        addtolog("第三次握手发送有误，请重新连接",myippointer);
        return -1;
    }
    addtolog("第三次握手发送成功",myippointer);
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
        addtolog("第一次挥手发送有误，请重新连接",myippointer);
        printf("第一次挥手发送错误");
        return -1;
    }
    addtolog("第一次挥手发送成功，开启计时器",myippointer);
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
            addtolog("第二次挥手接收超时，重传第一次挥手成功",myippointer);
            printf("第二次挥手接收超时，重传第一次挥手成功\n");
        }
    }
    addtolog("第二次挥手接收成功，进行校验，关闭计时器",myippointer);
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
        addtolog("第二次挥手校验成功",myippointer);
        printf("第二次挥手校验成功\n");
    }else{
        addtolog("第二次挥手校验成功",myippointer);
        printf("二次挥手校验有误，请重新连接\n");
        return -1;
    }
    //********************************************************************
    //recv第三次挥手
    while (1)
    {
        if (recvfrom(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrLen)<=0)
        {
            continue;
        }
        addtolog("第三次挥手接收成功，进行校验",myippointer);
        memcpy(&header,buffer,sizeof(header));
        u_short checksum_from_recv=header.checksum;
        header.checksum=0;
        calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
        if(header.flag==WAV3OF4&&header.ack==ackin2ndwave&&calc_chksum_rst==checksum_from_recv)
        {
            printf("第三次挥手校验成功\n");
            addtolog("第三次挥手校验成功",myippointer);
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
        addtolog("第四次挥手发送错误",myippointer);
        printf("第四次挥手发送错误");
        return -1;
    }
    addtolog("第四次挥手发送成功，开启计时器",myippointer);
    printf("第四次挥手发送成功\n");
    now_clocktime=clock();
    //第四次挥手后两个MSL后没有收到传回的信息，断开连接
    while(recvfrom(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrLen)<0)
    {
        if(clock()-now_clocktime>2*MAX_TIME)
        {
            addtolog("第四次挥手后经2*MSL未收到数据，成功断开",myippointer);
            return 1;
        }
    }
    //如果到达这里，说明发送第四次挥手后又收到信息
    //暂时不做处理，只退出，代表四次挥手错误
    addtolog("四次挥手失败，因为第四次挥手后仍接到数据",myippointer);
    printf("四次挥手失败，因为第四次挥手后仍接到数据\n");
    return -1;
}
int SendFileAsBinary(SOCKET& clntSock,SOCKADDR_IN& servAddr, int& servAddrLen, char* fullData,int dataLen){
    int pktnum=dataLen%MAX_BUFFER_SIZE==0?dataLen/MAX_BUFFER_SIZE:dataLen/MAX_BUFFER_SIZE+1;
    //int pktnum=dataLen/MAX_BUFFER_SIZE+(dataLen%MAX_BUFFER_SIZE!=0);
    int nowpkt=0;
    long int tailpointer=0;//由于按包来传输，记录每次拼接后末尾的指针便于下一次拼接
    HEADER header;
    HEADER temp1;
    u_short calc_chksum_rst;
    char* buffer = new char[MAX_BUFFER_SIZE+sizeof(header)];
    //需要记录上一个udp包以在序列号不对时回传
    //用以下两个char数组来记录
    //第一个包时，两个都记为第一个该发送的buffer
    //下一个包时，将lst赋值为now，now赋值为当前计算出来该发送的buffer
    // char* lstbuffer=new char[MAX_BUFFER_SIZE+sizeof(header)];
    char* nowbuffer=new char[MAX_BUFFER_SIZE+sizeof(header)];
    int flagforwrongseq=0;//为1时重传上一个包，详见下面注释说明
    u_short checksum_from_recv1;
    clntSeq=0;//初始化seq为0 
    int pktlen=0;
    while(1){
        //make_pkt
        header=HEADER();
        if(nowpkt==pktnum-1){
            //最后一个
            pktlen=dataLen-(pktnum-1)*MAX_BUFFER_SIZE;
        }else{
            pktlen=MAX_BUFFER_SIZE;
        }
        header.datasize=pktlen;//此时pktlen为不加头部的大小
        header.SEQ=(unsigned char)(clntSeq);
        header.checksum=0;
        header.flag=0;
        header.ack=0;
        //初始化为0
        char* nowPktPointer=fullData+nowpkt*MAX_BUFFER_SIZE;
        memcpy(buffer,&header,sizeof(header));
        memcpy(buffer+sizeof(header),nowPktPointer,pktlen);
        calc_chksum_rst=CalcChecksum((u_short*)buffer,sizeof(header)+pktlen);
        header.checksum=calc_chksum_rst;
        memcpy(buffer,&header,sizeof(header));
        memcpy(nowbuffer,&header,sizeof(header));
        memcpy(nowbuffer+sizeof(header),nowPktPointer,pktlen);
        // if(nowpkt==0)
        // {
        //     memcpy(nowbuffer,&header,sizeof(header));
        //     memcpy(nowbuffer+sizeof(header),nowPktPointer,pktlen);
        //     memcpy(lstbuffer,&header,sizeof(header));
        //     memcpy(lstbuffer+sizeof(header),nowPktPointer,pktlen);
        // }else{
        //     memcpy(lstbuffer,nowbuffer,sizeof(header)+pktlen);
        //     memcpy(nowbuffer,&header,sizeof(header));
        //     memcpy(nowbuffer+sizeof(header),nowPktPointer,pktlen);
        // }
        if(sendto(clntSock, buffer, sizeof(header)+pktlen, 0, (sockaddr*)&servAddr, servAddrLen)<0){
            printf("udp发送错误\n");
            addtolog("udp包发送错误",myippointer);
            continue;
        }
        memset(message,0,sizeof(message));
        sprintf(message,"成功发送 %d bytes数据，序列号为%d",pktlen,int(header.SEQ));
        addtolog((const char*)message,myippointer);
        addtolog("udp包发送成功",myippointer);
        printf("udp发送成功\n");
        //**************************************************
        //start_timer
        
        while(1){//外层循环，表示接收接收端返回的消息后是否因为有误继续recv
            now_clocktime=clock();
            mode = 1;
            ioctlsocket(clntSock, FIONBIO, &mode);
            while(recvfrom(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrLen)<0){
                if(clock()-now_clocktime>MAX_TIME){
                    //超时重传
                    //udt_send,不用makepkt因为没变
                    //收到的buffer带校验码，用带校验码的算校验码，再传回去肯定错啊。。。
                    //应该记录makepkt的结果这里发回去
                    // if(sendto(clntSock, buffer, sizeof(header)+pktlen, 0, (sockaddr*)&servAddr, servAddrLen)<0){
                    //     addtolog("确认消息超时，udp包重传发送错误",myippointer);
                    //     printf("超时重传错误\n");
                    //     return -1;
                    // }
                    memcpy(buffer,nowbuffer,sizeof(header)+pktlen);
                    if(sendto(clntSock, buffer, sizeof(header)+pktlen, 0, (sockaddr*)&servAddr, servAddrLen)<0){
                        addtolog("确认消息超时，udp包重传发送错误",myippointer);
                        printf("超时重传错误\n");
                        return -1;
                    }
                    addtolog("确认消息超时，udp包重传发送成功",myippointer);
                    printf("超时重传\n");
                    now_clocktime=clock();
                }
            }
            addtolog("接收到确认信息，进行校验",myippointer);
            printf("接收到回复信息，进行校验\n");
            //接收到了，进行校验，包括校验码和seq
            //不对则继续外层循环，recv正确的
            //接收端只返回一个header，
            HEADER temp1;
            memcpy(&temp1, buffer, sizeof(header));
            checksum_from_recv1=temp1.checksum;
            temp1.checksum=0;
            calc_chksum_rst=CalcChecksum((u_short*)&temp1,sizeof(temp1));
            if(calc_chksum_rst!=checksum_from_recv1){
            printf("%u!=%u,回传校验码不通过\n",calc_chksum_rst,checksum_from_recv1);
            addtolog("确认信息校验码校验不通过",myippointer);
            continue;
            }//没continue则校验码通过
            if(temp1.SEQ!=clntSeq){
                addtolog("确认信息序列号校验不通过",myippointer);
                printf("%d!=%d,回传序列号不对\n",int(temp1.SEQ),clntSeq);
                
                //序列号不对，重传上一个
                //这个也需要超时重传，想要continue回去，复用上面的代码
                //设置一个flag,表示要重传上一个seq的包，这时重传上一个
                //
                flagforwrongseq=1;
                continue;
            }
            break;
        }
        
        //没continue则序列号验证通过，这些都通过代表发回的ack正确，否则重发当前包（continue）
        mode = 0;
        ioctlsocket(clntSock, FIONBIO, &mode);
        //至此一个pkt传输并验证成功，传输下一个
        addtolog("udp包确认信息校验通过，传输下一个包",myippointer);
        if(nowpkt==pktnum-1){break;}//所有包都传输完毕
        //否则更新当前包和seq
        nowpkt++;
        clntSeq++;
        if(clntSeq>255)
        {
            clntSeq=0;
        }
    }
    //跳到这里则所有pkt发送成功，发送一个over的flag
    addtolog("所有包发送完成，发送结束标志",myippointer);
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
void SendFileHelper()
{
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
    fin.close();
    addtolog("文件读取成功",myippointer);
    printf("文件读取成功\n");
    int lenn=sizeof(sockAddr);
    SendFileAsBinary(sock,sockAddr,lenn,(char*)filenamestr.c_str(),filenamestr.length());
    memset(message,0,sizeof(message));
    sprintf(message,"文件名:%s接收完毕",filename);
    addtolog((const char*)message,myippointer);
    printf("文件名发送完毕\n");
    SendFileAsBinary(sock,sockAddr,lenn,filebuff,index);
    memset(message,0,sizeof(message));
    sprintf(message,"文件:%s接收完毕,长度为%dbytes",filename,index);
    addtolog((const char*)message,myippointer);
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
    addtolog("绑定套接字成功",myippointer);
    if(ConnectWith3Handsks(sock,sockAddr, lenn)==-1){
        addtolog("三次握手连接错误",myippointer);
        return;
    }
    addtolog("三次握手连接成功，返回值为1",myippointer);
    SendFileHelper();
    if(DisconnectWith4Waves(sock,sockAddr, lenn)<0){
        addtolog("四次挥手错误",myippointer);
        return;
    }
    addtolog("四次挥手断连成功，返回值为1",myippointer);
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