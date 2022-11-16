#include "loghdr.h"
#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
char servIP[20];//服务端ip
int servPort;//服务端端口号
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
        printf("第一次握手发送错误");
        return -1;
    }
    printf("第一次握手发送成功\n");
    now_clocktime=clock();
    //********************************************************************
    //recv第二次握手
    while (recvfrom(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrLen) <= 0)
    {
        if (clock() - now_clocktime > MAX_TIME)//超时，重新传输第一次握手
        {//重传第一次握手
            header.flag = HSK1OF3;
            header.checksum=0;
            header.checksum = CalcChecksum((u_short*)&header, sizeof(header));//计算校验和
            memcpy(buffer, &header, sizeof(header));//将首部放入缓冲区
            sendto(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrLen);
            now_clocktime = clock();
            printf("第一次握手超时，进行重传\n");
        }
    }
    printf("第二次握手接收数据\n");
    HEADER temp1;
    memcpy(&temp1, buffer, sizeof(header));
    u_short checksum_from_recv=temp1.checksum;
    temp1.checksum=0;
    calc_chksum_rst=CalcChecksum((u_short*)&temp1,sizeof(temp1));
    if(temp1.flag==HSK2OF3&&calc_chksum_rst==checksum_from_recv)
    {
        printf("二次握手完成，继续三次握手\n");
    }else{
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
        return -1;
    }
    printf("三次握手发送成功\n");
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
        printf("第一次挥手发送错误");
        return -1;
    }
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
            printf("第一次挥手超时，进行重传\n");
        }
    }
    printf("第二次挥手接收数据\n");
    HEADER temp1;
    memcpy(&temp1, buffer, sizeof(header));
    u_short checksum_from_recv=temp1.checksum;
    temp1.checksum=0;
    calc_chksum_rst=CalcChecksum((u_short*)&temp1,sizeof(temp1));
    if(temp1.flag==WAV2OF4&&calc_chksum_rst==checksum_from_recv)
    {
        //记录这里的ack=u+1，因为第三次挥手的ack=u+1，第四次挥手的seq=u+1
        ackin2ndwave=temp1.ack;
        printf("二次挥完成，继续三次挥手\n");
    }else{
        printf("二次挥手传输有误，请重新连接\n");
        return -1;
    }
    //********************************************************************
    //recv第三次挥手
    while (1)
    {
        if (recvfrom(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrLen)<0)
        {
            printf("第三次挥手接收错误\n");
            return -1;
        }
        memcpy(&header,buffer,sizeof(header));
        u_short checksum_from_recv=header.checksum;
        header.checksum=0;
        calc_chksum_rst=CalcChecksum((u_short*)&header,sizeof(header));
        if(header.flag==WAV3OF4&&header.ack==ackin2ndwave&&calc_chksum_rst==checksum_from_recv)
        {
            printf("第三次挥手接收成功\n");
            //记录这里的seq=w，因为第四次挥手的ack=w+1
            seqin3rdwave=header.SEQ;
            break;
        }else{
            printf("第三次挥手校验错误\n");
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
        printf("第一次挥手发送错误");
        return -1;
    }
    printf("第四次挥手发送成功\n");
    now_clocktime=clock();
    //第四次挥手后两个MSL后没有收到传回的信息，断开连接
    while(recvfrom(clntSock, buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrLen)<0)
    {
        if(clock()-now_clocktime>2*MAX_TIME)
        {
            return 1;
        }
    }
    //如果到达这里，说明发送第四次挥手后又收到信息
    //暂时不做处理，只退出，代表四次挥手错误
    printf("四次挥手失败，因为第四次挥手后仍接到数据\n");
    return -1;
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
    //向服务器发起请求
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
    int lenn=sizeof(sockAddr);
    
    if(ConnectWith3Handsks(sock,sockAddr, lenn)==-1){
        printf("与服务端连接错误\n");
    }
    DisconnectWith4Waves(sock,sockAddr, lenn);
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