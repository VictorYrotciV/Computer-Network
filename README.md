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
