//引入exprexx模块
var express =require("express");
var path = require('path');
var app =express();
app.use(express.static(path.join(__dirname, 'public')));
//参数‘/’可当作设置url的根显示页面
app.get('/',(req,res)=>{
    res.sendFile(__dirname+"/"+"my1.html")        //设置/ 下访问文件位置
});

app.get("/yhz",(req,res)=>{
    res.sendFile(__dirname+"/"+"my2.html")        //设置/tow下访问文件位置
})


var server =app.listen(5678,()=>{
    var port =server.address().port
    console.log("server start",port)
})
module.exports=app;