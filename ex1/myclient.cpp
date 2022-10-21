#include "loghdr.h"
#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
char servIP[20];//服务端ip
int servPort;//服务端端口号
SOCKET sock;//实例化全局socket
struct sockaddr_in sockAddr;
int MAX_BUFFER_LEN = 1024;//消息缓冲区最大长度
char name[20];//用户名
time_t nowtime;//time
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
    scanf("%s", servIP);
    printf("请输入端口号：");
    scanf("%d", &servPort);
    printf("\n");
    //发起请求
    memset(&sockAddr, 0, sizeof(sockAddr));  //每个字节都用0填充
    //init
    sockAddr.sin_family = PF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(servIP);
    sockAddr.sin_port = htons(servPort);
    if (connect(sock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR))==-1){//进行connect
        perror("无法连接到服务器!");
        exit(-1);
    }
    printf("客户端启动成功\n");
    printf("连接到服务器，IP地址为%s，端口号为%d\n",servIP,servPort);
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
    //每个客户端创建只需一个pthread用于接收，主线程用来发送
    pthread_t id;
    void* recv_thread(void*);
    pthread_create(&id,0,recv_thread,0);
    //向服务器发送连接成功信息
    char buf2[MAX_BUFFER_LEN] = {};
    memset(buf2,0,sizeof(buf2));
    sprintf(buf2,"欢迎%s进入群聊",name);
    addusername(buf2,name);
    send(sock,buf2,strlen(buf2),0);
    while(1){//主线程，主要用来发送
    //接收服务器传回的数据
    //向服务器发送数据
        char buf[MAX_BUFFER_LEN] = {};
        scanf("%s",buf);
        char msg[MAX_BUFFER_LEN] = {};
        sprintf(msg,"%s",buf);
        //在消息前端添加用户名，消息=[@名字]消息
        addusername(msg,name);
        send(sock,msg,strlen(msg),0);
        if(strcmp(buf,"quit")==0){//输入quit退出
            //不用发quit消息，server的recv会检测到
            break;
        }
    }
    //关闭套接字
    closesocket(sock);
}
//接收线程
void* recv_thread(void* p){
    while(1){
        char buf[MAX_BUFFER_LEN] = {};
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