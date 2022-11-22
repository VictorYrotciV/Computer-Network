#include "loghdr.h"
#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
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
        addtoclntlog("第二次挥手校验成功",myippointer);
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
    int pktnum=dataLen%MAX_BUFFER_SIZE==0?dataLen/MAX_BUFFER_SIZE:dataLen/MAX_BUFFER_SIZE+1;
    //int pktnum=dataLen/MAX_BUFFER_SIZE+(dataLen%MAX_BUFFER_SIZE!=0);
    int nowpkt=0;
    long int tailpointer=0;//由于按包来传输，记录每次拼接后末尾的指针便于下一次拼接
    HEADER header;
    HEADER temp1;
    u_short calc_chksum_rst;
    char* buffer = new char[MAX_BUFFER_SIZE+sizeof(header)];
    //重传记录
    char* nowbuffer=new char[MAX_BUFFER_SIZE+sizeof(header)];
    u_short checksum_from_recv1;
    base=0;
    nextseqnum=0;
    clntSeq=0;//初始化seq为0 
    int pktlen=0;
    while(1){
        //step 1 in GBN FSM
        if(nextseqnum<base+256*basejwnum+WINDOW_SIZE){
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
            //udt_send(sndpkt)
            if(sendto(clntSock, buffer, sizeof(header)+pktlen, 0, (sockaddr*)&servAddr, servAddrLen)<0){
            printf("udp发送错误\n");
            addtoclntlog("udp包发送错误",myippointer);
            continue;
            }
            memset(message,0,sizeof(message));
            sprintf(message,"成功发送 %d bytes数据，序列号为%d",pktlen,int(header.SEQ));
            printf("%s\n",message);
            addtoclntlog((const char*)message,myippointer);
            sprintf(message,"基序号base=%d，seq-base=%d",base,int(header.SEQ)-base);
            printf("%s\n",message);
            addtoclntlog((const char*)message,myippointer);
            addtoclntlog("udp包发送成功",myippointer);
            printf("udp包发送成功\n");
            //start timer
            if(base==nextseqnum%256){
                now_clocktime=clock();
            }
            //nextSeqnum++
            if(nextseqnum==pktnum-1){break;}
            nextseqnum++;
            //remark 这里base是从header的seq复制下来的，应该只有0-255；
            //但如果你还没有注意到，那nextseqnum实际上并没有取余256
            //注意这一点
            // if(nextseqnum>255)
            // {
            //     nextseqnum=0;
            // }
        }else{
            printf("base=%d,nextseqnum=%d\n",base,nextseqnum);
            printf("窗口过大，拒绝data\n");
        }
        //隐含条件：
        //else{refuse_data()}
        
        
        
        
        //redt_rcv && notcorrupt
        u_long mode = 1;
        ioctlsocket(clntSock, FIONBIO, &mode);
        if(recvfrom(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrLen)>0){
            addtoclntlog("接收到确认信息，进行校验",myippointer);
            printf("接收到回复信息，进行校验\n");
            //接收到了，进行校验，包括校验码和seq
            //接收端只返回一个header，
            HEADER temp1;
            memcpy(&temp1, buffer, sizeof(header));
            checksum_from_recv1=temp1.checksum;
            temp1.checksum=0;
            calc_chksum_rst=CalcChecksum((u_short*)&temp1,sizeof(temp1));
            memset(message,0,sizeof(message));
            sprintf(message,"接收到的校验和=%u，计算出的校验和为%u",checksum_from_recv1,calc_chksum_rst);
            printf("%s\n",message);
            addtoclntlog((const char*)message,myippointer);
            if(calc_chksum_rst!=checksum_from_recv1){
                //step 4 in GBN FSM
                // i.e. rdt_rcv && corrupt
                // go back to waiting status, i.e. do nothing indeed and continue
                printf("%u!=%u,回传校验码不通过\n",calc_chksum_rst,checksum_from_recv1);
                addtoclntlog("确认信息校验码校验不通过",myippointer);
                continue;
            }
            //到这里则接收并校验成功，执行FSM中的第三步
            if((int(temp1.SEQ)+1)%256<base){basejwnum++;}
            //seq+1为0-255，如果到了256要变回0
            base=(int(temp1.SEQ)+1)%256;
            if(base==nextseqnum%256)
            {
                //TODO:stop_timer
                //stop_timer即是不再超时重传，
                //实际上，也就是可以直接跳过下面代码的分支
                //包括这里的else now_clocktime=clock();
                //和下面的超时重传即FSM中的timeout
                //故回到原始状态，模式设为阻塞
                //continue等待即可
                printf("所有包验证完毕，继续发送\n");
                continue;
                
            }else{
                now_clocktime=clock();
            }
        }else{
            //TODO 应把超时重传放到这里
            //step 2 in GBN FSM
            //timeout
            if(clock()-now_clocktime>MAX_TIME){
                now_clocktime=clock();
                //base经过%256处理,但nextseqnum没有
                int resend_index=base+256*basejwnum;
                //通过状态机可知，从base到nextseqnum-1
                //udt_send from base to nextseqnum-1
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
                    printf("udp发送错误\n");
                    addtoclntlog("udp包发送错误",myippointer);
                    continue;
                    }
                    memset(message,0,sizeof(message));
                    sprintf(message,"[TIMEOUT RESEND]成功发送 %d bytes数据，序列号为%d",pktlen,int(header.SEQ));
                    printf("%s\n",message);
                    addtoclntlog((const char*)message,myippointer);
                    sprintf(message,"[TIMEOUT RESEND]基序号base=%d，seq-base=%d",base,int(header.SEQ)-base);
                    printf("%s\n",message);
                    addtoclntlog((const char*)message,myippointer);
                    addtoclntlog("[TIMEOUT RESEND]udp包发送成功",myippointer);
                    printf("[TIMEOUT RESEND]udp包发送成功\n");
                }
            }
        }
        mode = 0;
        ioctlsocket(clntSock, FIONBIO, &mode);
        //**************************************************
        
    }
    //跳到这里则所有pkt发送成功，发送一个over的flag
    addtoclntlog("所有包发送完成，发送结束标志",myippointer);
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
    index--;
    fin.close();
    addtoclntlog("文件读取成功",myippointer);
    printf("文件读取成功\n");
    int lenn=sizeof(sockAddr);
    SendFileAsBinary(sock,sockAddr,lenn,(char*)filenamestr.c_str(),filenamestr.length());
    memset(message,0,sizeof(message));
    sprintf(message,"文件名:%s接收完毕",filename);
    addtoclntlog((const char*)message,myippointer);
    printf("文件名发送完毕\n");
    SendFileAsBinary(sock,sockAddr,lenn,filebuff,index);
    memset(message,0,sizeof(message));
    sprintf(message,"文件:%s接收完毕,长度为%dbytes",filename,index);
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
    addtoclntlog("绑定套接字成功",myippointer);
    if(ConnectWith3Handsks(sock,sockAddr, lenn)==-1){
        addtoclntlog("三次握手连接错误",myippointer);
        return;
    }
    addtoclntlog("三次握手连接成功，返回值为1",myippointer);
    SendFileHelper();
    if(DisconnectWith4Waves(sock,sockAddr, lenn)<0){
        addtoclntlog("四次挥手错误",myippointer);
        return;
    }
    addtoclntlog("四次挥手断连成功，返回值为1",myippointer);
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