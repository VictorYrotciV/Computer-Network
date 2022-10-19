#include <stdio.h>
#include <stdlib.h>
#include <WinSock2.h>
#pragma comment (lib, "ws2_32.lib")  //加载 ws2_32.dll
char myIP[20];
int myPort;
int MAX_CLIENT_NUM=20;
SOCKET servSock;
SOCKET clntSock;

struct sockaddr_in sockAddr;
void init(){
    //初始化 DLL
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    //创建socket
    servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(servSock==INVALID_SOCKET){
        printf("SERVER SOCKET INIT ERROR");
    }
    //绑定套接字
    //输入IP和端口号
    printf("请输入IP地址：");
    scanf("%s", myIP);
    printf("请输入端口号：");
    scanf("%d", &myPort);
    printf("\n");
    memset(&sockAddr, 0, sizeof(sockAddr));  //每个字节都用0填充
    sockAddr.sin_family = PF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(myIP);
    sockAddr.sin_port = htons(myPort);
    if(bind(servSock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR))==-1)
    {
        perror("BIND FAILED!");
        //exit(-1);
    }
    if(listen(servSock, MAX_CLIENT_NUM)==-1)
    {
        perror("LISTEN FAILED!");
        //exit(-1);
    }
    SOCKADDR clntAddr;
    int nSize = sizeof(SOCKADDR);
    clntSock = accept(servSock, (SOCKADDR*)&clntAddr, &nSize);
    if(clntSock==INVALID_SOCKET){
        printf("CLIENT SOCKET INIT ERROR");
    }
}
void start()
{
    while(1){
        char str[100];
        printf( "Enter a value :");
        scanf("%s", str);
        send(clntSock, str, strlen(str)+sizeof(char), NULL);
        printf( "\nYou entered: %s\n", str);
    }
    closesocket(clntSock);
    closesocket(servSock);
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