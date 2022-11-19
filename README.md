# nku_computer_network_2022



## Getting started

### ex1

#### Start with exe

（可以提前清空log.txt，但不要将它删掉）

在windows中，先启动myserver.exe，根据指引创建服务端成功后，启动（多个）myclient.exe，根据指引连接到服务端，之后在客户端命令行进行操作。

#### Start with cpp

将除了exe的四个文件放到同级目录下。

编译器版本：gcc version 8.1.0 (x86_64-posix-sjlj-rev0, Built by MinGW-W64 project)

编译命令（根据编码方式灵活选择是否要设置charset）：

g++ .\myclient.cpp -lws2_32 -lpthread -fexec-charset=GBK -o myclient

g++ .\myserver.cpp -lws2_32 -lpthread -fexec-charset=GBK -o myserver



### ex2

nodejs提前安装。

demo为练习用的demo

node_modules主要为npm安装好的express以及相关的主键。

下面两个json文件是自动生成的

主要程序在myex2中，www文件在myex2/bin/。

命令行打开myex2文件夹，输入npm start开启服务器。



### ex3

路由程序设置如下

路由器ip：127.7.8.9 port：6666

服务器ip：127.1.2.3 port：4321

开始路由后

打开test文件夹

有两个txt文件，是客户端和服务端的日志文件，它们会在程序运行后根据情况更新

可以提前清空

任意顺序打开serv和clnt的两个exe文件，应该会握手成功

成功后在clnt中输入文件名，可以是给定的四个测试文件，也可以自己给定，放在clnt的exe同级目录即可

传输成功后，最后经过四次挥手自动断连

可以在serv下找到传输过后的文件



------------------------------------

编译指令：

g++ myserver.cpp -lws2_32 -fexec-charset=GBK -o test/serv/myserver.exe

g++ myclient.cpp -lws2_32  -fexec-charset=GBK -o test/clnt/myclirnt.exe