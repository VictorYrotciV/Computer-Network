#include "loghdr.h"
#pragma comment (lib, "ws2_32.lib")  //加载 ws2_32.dll
char myIP[20];
int myPort;
const int MAX_CLIENT_NUM=2;
const int MAX_BUFFER_LEN = 1024;
SOCKET servSock;
SOCKET clntSockArr[MAX_CLIENT_NUM];
time_t nowtime;
struct sockaddr_in sockAddr;
void init(){
    //初始化 DLL
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    //创建socket
    servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(servSock==INVALID_SOCKET){
        perror("SERVER SOCKET INIT ERROR");
        //exit(-1);
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
    // SOCKADDR clntAddr;
    // int nSize = sizeof(SOCKADDR);
    // clntSock = accept(servSock, (SOCKADDR*)&clntAddr, &nSize);
    // if(clntSock==INVALID_SOCKET){
    //     printf("CLIENT SOCKET INIT ERROR");
    // }
}
void SendAll(char* msg){
    int i;
    for (i = 0;i < MAX_CLIENT_NUM;i++){
        if (clntSockArr[i] != 0){
            printf("发送给%d\n",clntSockArr[i]);
			printf("发送的信息是: %s\n",msg);
			//写入文件
			char buf[MAX_BUFFER_LEN];
			FILE *logs = fopen("log.txt", "a+");
			if(logs== NULL)
			{
			printf("open file error: \n");
			}else{
            memset(buf,0,sizeof(buf));
			sprintf(buf, "%s",msg);
            addtimestamp(buf);
			fputs(buf,logs);
            fputs("\n",logs);
			fclose(logs);
           
			}
            send(clntSockArr[i],buf,strlen(buf),0);
        }
    }
}
void* serv_thread(void* p)
{
    int clientFd = *(int*)p;
    printf("pthread = %d\n",clientFd);
    while(1)
    {
        char buf[MAX_BUFFER_LEN]={};
        if(recv(clientFd,buf,sizeof(buf),0)<=0)
        {//客户socket退出
            int i=0;
            for(i=0;i<MAX_CLIENT_NUM;i++)
            {
                if(clientFd==(int)clntSockArr[i])
                {
                    clntSockArr[i]=0;
                    break;
                }
            }
            printf("SOCKET=%d 退出了\n",clientFd);
            memset(buf,0,sizeof(buf));
            FILE *logs = fopen("log.txt","a");
            if(logs==NULL)
            {
                printf("OPEN LOGFILE ERROR");
            }else{
                sprintf(buf, "客户退出，IP地址：%s\n",myIP);
                SendAll(buf);
            }
            pthread_exit(0);
        }
        //把接收到的信息发送到所有客户端
        SendAll(buf);
    }
}
void start()
{
    printf("服务器启动成功\n");
    while(1){
        SOCKADDR clntAddr;
        int nSize = sizeof(SOCKADDR);
        SOCKET clntSock = accept(servSock, (SOCKADDR*)&clntAddr, &nSize);
        //调用accept进入堵塞状态，等待客户端的连接
        if(clntSock==INVALID_SOCKET){
            printf("CLIENT SOCKET CONNECT ERROR\n");
            continue;
        }
        int i=0;
        for(i=0;i<MAX_CLIENT_NUM;i++)
        {
            if(clntSockArr[i]==0)
            {
                //用数组记录客户端socket
                clntSockArr[i] =  clntSock;
                printf("线程号= %d\n",(int)clntSock);
                //启动一个线程为该客户服务
                pthread_t tid;
                pthread_create(&tid,0,serv_thread,&clntSock);
                break;
            }
            if (MAX_CLIENT_NUM == i-1){
            //发送给客户端说聊天室满了
            char const *msg_room_full = "对不起，聊天室已经满了!";
            send(clntSock,msg_room_full,strlen(msg_room_full),0);
            closesocket(clntSock);
            }
        }
        // char str[100];
        // printf( "Enter a value :");
        // scanf("%s", str);
        // send(clntSock, str, strlen(str)+sizeof(char), NULL);
        // printf( "\nYou entered: %s\n", str);
    }
    // closesocket(clntSock);
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