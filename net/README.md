# 网络处理与事件分发模块

## 基本使用方法

1. 在用户代码中实例化一个Reactor对象；
2. 声明连接接受器acceptor channel，在其上设置好关心的事件及相应回调函数；
3. 将acceptor注册到Reactor对象；
4. 一切就绪后，调用Reactor->loop()函数，开启事件循环。

## 扩展

后续可以考虑在程序中使用多个Reactor，采用one loop per thread的模式编程
