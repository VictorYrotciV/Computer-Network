#include "loghdr.h"
#pragma comment (lib, "ws2_32.lib")  //加载 ws2_32.dll
char servIP[20];
char usermode[50];//用户传过来的模式，对标myclient中的mode数组
char name_for_priv_chat_dest[20];//私聊目标的名字
int servPort;
const int MAX_CLIENT_NUM=3;//最大客户端数量
const int MAX_BUFFER_LEN = 1024;
SOCKET servSock;
SOCKET clntSockArr[MAX_CLIENT_NUM];//记录客户端socket数组
time_t nowtime;
struct sockaddr_in sockAddr;
char clntip[MAX_CLIENT_NUM][20] = {};//记录客户端ip数组
char clntname[MAX_CLIENT_NUM][22] = {};//记录客户端名字数组
//用于将clntAddr.sin_addr.s_addr转换为字符串
#define IP2UCHAR(addr) \
((unsigned char *)&addr)[0], \
((unsigned char *)&addr)[1], \
((unsigned char *)&addr)[2], \
((unsigned char *)&addr)[3]


void init(){
    //初始化 DLL
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    //创建socket
    servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(servSock==INVALID_SOCKET){
        perror("SERVER SOCKET INIT ERROR");
        exit(-1);
    }
    //绑定套接字
    //输入IP和端口号
    printf("请输入IP地址：");
    scanf("%s", servIP);
    printf("请输入端口号：");
    scanf("%d", &servPort);
    printf("\n");
    memset(&sockAddr, 0, sizeof(sockAddr));  //每个字节都用0填充
    //init
    sockAddr.sin_family = PF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(servIP);
    sockAddr.sin_port = htons(servPort);
    if(bind(servSock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR))==-1)//绑定
    {
        perror("BIND FAILED!");
        //exit(-1);
    }
    if(listen(servSock, MAX_CLIENT_NUM)==-1)//监听
    {
        perror("LISTEN FAILED!");
        //exit(-1);
    }
    //记录创建成功信息
    char buf[MAX_BUFFER_LEN];
	FILE *logs = fopen("log.txt", "a+");
	if(logs== NULL)
	{
	printf("open file error: \n");
	}else{
    memset(buf,0,sizeof(buf));
    char const *msg_serv_init = "****************************************************\n SERVER INIT SUCCESS";
	sprintf(buf, "%s",msg_serv_init);
    //为消息添加时间戳
    addtimestamp(buf);
	fputs(buf,logs);
    //记录到日志
    fputs("\n",logs);
	fclose(logs);
	}
}
void SendAll(char* msg){
    int i;
    for (i = 0;i < MAX_CLIENT_NUM;i++){
        if (clntSockArr[i] != 0){
			//写入文件
			char buf[MAX_BUFFER_LEN];
			FILE *logs = fopen("log.txt", "a+");
			if(logs== NULL)
			{
			printf("open file error: \n");
			}else{
            memset(buf,0,sizeof(buf));
			sprintf(buf, "%s",msg);
            //为消息添加时间戳和用户ip
            addtimestamp(buf);
            adduserip(buf,clntip[i]);
			fputs(buf,logs);
            fputs("\n",logs);
			fclose(logs);
			}
            send(clntSockArr[i],buf,strlen(buf),0);
        }
    }
}
void SendPriv(char* msg,char* destname,int originclntid){
    //参数分别为：消息、目标的用户名、源消息的发出客户的id
    //即其存储的pthread的id，还能用于发送没找到消息
    int i;
    char buf[MAX_BUFFER_LEN];
    for (i = 0;i < MAX_CLIENT_NUM;i++){
        if (strcmp(destname,clntname[i])==0){
            printf("发送给%d\n",clntSockArr[i]);
			printf("发送的信息是: %s\n",msg);
			//写入文件
			FILE *logs = fopen("log.txt", "a+");
			if(logs== NULL)
			{
			printf("open file error: \n");
			}else{
            memset(buf,0,sizeof(buf));
			sprintf(buf, "%s",msg);
            //为消息添加时间戳和用户ip
            addtimestamp(buf);
            adduserip(buf,clntip[i]);
			fputs(buf,logs);
            fputs("\n",logs);
			fclose(logs);
			}
            send(clntSockArr[i],buf,strlen(buf),0);
            send(clntSockArr[originclntid],buf,strlen(buf),0);
            //发送方和接收方都显示私聊消息
            return;
        }
    }
    //到达这里表示没有找到目标用户名，向客户发送寻找失败消息
    memset(buf,0,sizeof(buf));
    sprintf(buf, "没有找到：%s。。。",destname);
    char serv_name[10];
    char serv_mode[20];
    strcpy(serv_name,"server");
    strcpy(serv_mode,"#SERVER");
    addusername(buf,serv_name);
    addusermode(buf,serv_mode);
    addtimestamp(buf);
    adduserip(buf,clntip[originclntid]);
    send(clntSockArr[originclntid],buf,strlen(buf),0);
}
void* serv_thread(void* p)//为每个客户端创建一个线程处理
{//服务端不命令行输入消息，只管接收然后全部发出去
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
                    printf("SOCKET=%d 退出了\n",clientFd);
                    memset(buf,0,sizeof(buf));
                    FILE *logs = fopen("log.txt","a");
                    if(logs==NULL)
                    {
                        printf("OPEN LOGFILE ERROR");
                    }else{
                        sprintf(buf, "%s退出了群聊",clntname[i]);
                        char serv_name[10];
                        char serv_mode[20];
                        strcpy(serv_name,"server");
                        strcpy(serv_mode,"#SERVER");
                        addusername(buf,serv_name);
                        addusermode(buf,serv_mode);
                        //不同于客户端发出的其他消息，带有用户名
                        //这里是客户端退出后进行处理，客户端没有发消息
                        //遵守协议，在这里加上服务器的名字和模式
                        //代表从服务器发出，模式为服务器
                        SendAll(buf);
                    }
                    clntSockArr[i]=0;
                    memset(clntip[i],0,sizeof(clntip[i]));
                    memset(clntname[i],0,sizeof(clntname[i]));
                    break;
                }
            }
            
            pthread_exit(0);
        }
        //把接收到的信息发送到所有客户端
        //接收到用户名消息是在链接之后
        //客户端发送一个带用户名的消息
        //要在这里解析
        //*********************************************
        //解析用户信息
        //*********************************************
        int i=0;
        for(i=0;i<MAX_CLIENT_NUM;i++)
        {
            if(clientFd==(int)clntSockArr[i])
            {
                parseusername(clntname[i],buf);
                printf("%s\n",clntname[i]);
                break;
            }
        }
        parseusermode(usermode,name_for_priv_chat_dest,buf);
        if(strcmp(usermode,mode_bdcs)==0)
        {
            SendAll(buf);
        }else if(strcmp(usermode,mode_priv)==0)
        {
            SendPriv(buf,name_for_priv_chat_dest,i);
        }
    }
}
void start()
{
    printf("服务器启动成功\n");
    while(1){
        //SOCKADDR clntAddr;
        struct sockaddr_in clntAddr;
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
                memset(clntip[i],0,sizeof(clntip[i]));
                sprintf(clntip[i], "%u.%u.%u.%u", IP2UCHAR(clntAddr.sin_addr.s_addr));
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