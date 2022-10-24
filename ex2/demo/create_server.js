//创建HTTP服务器

//1. 加载http模块
var http = require('http');

//2. 创建http服务器
// 参数: 请求的回调, 当有人访问服务器的时候,就会自动调用回调函数
var server = http.createServer(function (request, response) {
    console.log('有人访问了服务器')

    //回调数据
    response.write('YHZ is learning to create a HTTP server')
    response.end()
})

//3. 绑定端口
server.listen(3030, '127.0.0.1')

//4. 执行
console.log('执行了3030')