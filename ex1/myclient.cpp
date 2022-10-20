
#include "loghdr.h"
#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
char myIP[20];
int myPort;
SOCKET sock;
struct sockaddr_in sockAddr;
int MAX_BUFFER_LEN = 1024;
char name[20];//用户名
time_t nowtime;
void init()
{
    //初始化DLL
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    //创建socket
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sock==INVALID_SOCKET){
        printf("SOCKET INIT ERROR");
    }
    //向服务器发起请求
    //输入IP和端口号
    printf("请输入IP地址：");
    scanf("%s", myIP);
    printf("请输入端口号：");
    scanf("%d", &myPort);
    printf("\n");
    //发起请求
    memset(&sockAddr, 0, sizeof(sockAddr));  //每个字节都用0填充
    sockAddr.sin_family = PF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(myIP);
    sockAddr.sin_port = htons(myPort);
    if (connect(sock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR))==-1){
        perror("无法连接到服务器!");
        exit(-1);
    }
    printf("客户端启动成功\n");
    printf("连接到服务器，IP地址为%s，端口号为%d\n",myIP,myPort);
    //初始化用户名
    printf("请输入用户名：");
    scanf("%s",name);
    printf("\n\n*****************************\n");
    printf("欢迎%s 进入聊天！\n",name);
    printf("输入 quit 以退出\n");
    printf("\n*****************************\n\n");
}
void start()
{
    pthread_t id;
    void* recv_thread(void*);
    pthread_create(&id,0,recv_thread,0);
    //向服务器发送连接成功信息
    char buf2[MAX_BUFFER_LEN] = {};
    memset(buf2,0,sizeof(buf2));
    sprintf(buf2,"欢迎%s进入群聊",name);
    send(sock,buf2,strlen(buf2),0);
    //在serv_thread函数中，刚连接时没有进入其while(1)的循环
    //而是
    while(1){
    //接收服务器传回的数据
    //向服务器发送数据
        char buf[MAX_BUFFER_LEN] = {};
        scanf("%s",buf);
        char msg[MAX_BUFFER_LEN] = {};
        sprintf(msg,"%s:%s",name,buf);
        send(sock,msg,strlen(msg),0);
        if(strcmp(buf,"quit")==0){
            memset(buf2,0,sizeof(buf2));
            sprintf(buf2,"%s退出了群聊",name);
            //addtimestamp(buf2);
            send(sock,buf2,strlen(buf2),0);
            break;
        }
        // char szBuffer[MAX_BUFFER_LEN] = {0};
        // if(recv(sock, szBuffer, MAX_BUFFER_LEN, NULL)==SOCKET_ERROR){
        //     printf("接收信息错误");
        //     break;
        // }
        // //输出接收到的数据
        // else{printf("Message form server: %s\n", szBuffer);}
    }
    //关闭套接字
    closesocket(sock);
}
//接收线程
void* recv_thread(void* p){
    while(1){
        char buf[MAX_BUFFER_LEN] = {};
        //发送的消息在发送那一端组织好
        //只管接收就好
        if (recv(sock,buf,sizeof(buf),0) <=0){
            break;
        }
        printf("%s\n",buf);
    }
}
int main()
{
    init();
    start();
    //终止使用 DLL
    WSACleanup();
    system("pause");
    return 0;
}