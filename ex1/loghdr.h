#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <WinSock2.h>
#include <pthread.h>
#include <time.h>
time_t nowtime1;
//为消息添加时间戳
void addtimestamp(char* msg)
{
    char msg2[1024];
    memset(msg2,0,sizeof(msg2));
    strcpy(msg2,msg);
    memset(msg,0,sizeof(msg)); 
    time(&nowtime1);
    //ctime最后一个为换行符，将它替换，美观
    char* timestamp = ctime(&nowtime1);
    char *tmp = NULL;
    if ((tmp = strstr(timestamp, "\n")))
    {
        *tmp = '\0';
    }
    sprintf(msg,"[%s]%s",timestamp,msg2);
}
//为消息添加字符串（用户ip地址）
void adduserip(char* msg,char* ip)
{
    char msg2[1024];
    memset(msg2,0,sizeof(msg2));
    strcpy(msg2,msg);
    memset(msg,0,sizeof(msg)); 
    sprintf(msg,"[%s]%s",ip,msg2);
}
//为消息添加字符串（用户名）
void addusername(char* msg,char* name)
{
    char msg2[1024];
    memset(msg2,0,sizeof(msg2));
    strcpy(msg2,msg);
    memset(msg,0,sizeof(msg)); 
    sprintf(msg,"[@%s]%s",name,msg2);
}