#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <WinSock2.h>
#include <pthread.h>
#include <time.h>
time_t nowtime1;
//模式，包含广播与私聊
//不想用define了
char const *mode_bdcs="#BROADCAST";
char const *mode_priv="#PRIVCHAT";

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
//为消息添加字符串（模式）
void addusermode(char* msg,char* mode)
{
    char msg2[1024];
    memset(msg2,0,sizeof(msg2));
    strcpy(msg2,msg);
    memset(msg,0,sizeof(msg)); 
    sprintf(msg,"[%s]%s",mode,msg2);
}
void parseusername(char* nameaddr,char* buf)
{
    //协议中，客户端和服务端发送出的消息都是以方框包住的一些信息开头
    //只有用户名前有@
    //strchr能得到字符串中某个字符第一次出现的位置
    //解析出@到@下一个]之前的字符串即为用户名
    char tmp[1024];
    memset(tmp,0,sizeof(tmp));
    char rst[22];
    memset(rst,0,sizeof(rst));
    strcpy(tmp,buf);
    char* dest_at=strchr(tmp,'@');
    char* dest_rightc=strchr(dest_at,']');
    int begin = dest_at-tmp;
    int end = dest_rightc-dest_at+begin;
    int cnt=0;
    for(int i=begin+1;i<end;i++)
    {
        rst[i-begin-1] = tmp[i];
        cnt++;
    }
    rst[cnt]='\0';
    memset(nameaddr,0,sizeof(nameaddr));
    sprintf(nameaddr,"%s",rst);
}
void parseusermode(char* usermode,char* nameaddr,char* buf)
{
    //解释同上
    //解析用户模式（私聊or广播）
    //模式#开头
    char tmp[1024];
    memset(tmp,0,sizeof(tmp));
    char destname[22];
    memset(destname,0,sizeof(destname));
    strcpy(tmp,buf);
    char* dest_jing=strchr(tmp,'#');
    if(*(dest_jing+1)=='B'){//BROADCAST 只判断头一个字符，以后功能扩展再改解析代码
        sprintf(usermode,"%s",mode_bdcs);
    }else if(*(dest_jing+1)=='P'){
        sprintf(usermode,"%s",mode_priv);
        char* dest_eq=strchr(tmp,'=');
        char* dest_rightc=strchr(dest_eq,']');
        int begin = dest_eq-tmp;
        int end = dest_rightc-dest_eq+begin;
        int cnt=0;
        for(int i=begin+1;i<end;i++)
        {
            destname[i-begin-1] = tmp[i];
            cnt++;
        }
        destname[cnt]='\0';
        memset(nameaddr,0,sizeof(nameaddr));
        sprintf(nameaddr,"%s",destname);
    }
}