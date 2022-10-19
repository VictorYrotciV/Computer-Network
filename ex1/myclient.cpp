#include <stdio.h>
#include <stdlib.h>
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
char myIP[20];
int myPort;
SOCKET sock;
struct sockaddr_in sockAddr;
void init()
{
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
    printf("\n请输入端口号：");
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
}
void start()
{
    while(1){
    //接收服务器传回的数据
        char szBuffer[MAXBYTE] = {0};
        if(recv(sock, szBuffer, MAXBYTE, NULL)==SOCKET_ERROR){
            printf("接收信息错误");
            break;
        }
        //输出接收到的数据
        else{printf("Message form server: %s\n", szBuffer);}
    }
    //关闭套接字
    closesocket(sock);
}
int main()
{
    //初始化DLL
    
    init();
    start();
    //终止使用 DLL
    WSACleanup();
    system("pause");
    return 0;
}