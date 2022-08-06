# LINUX C++ WebServer

## 特性

使用基于Reactor + 线程池的并发模型封装的简单网络库编写

能够处理对静态资源的GET请求

支持HTTP长连接

使用基于小根堆实现的定时器管理超时连接，可选择二叉堆或四叉堆

静态资源应放置于服务器程序所在目录的resources文件夹内，运行过程中产生的日志位于log文件夹内

Reactor及线程池代码位于 [net](https://github.com/wellexam/WebServer/tree/main/net) 目录下

## 目前的不足

整个服务器程序只有单个Reactor，高并发场景下可能出现性能不足；
同时也因为只有单个Reactor，部分操作需要进行额外的同步，高压场景下可能出现锁争用，影响程序性能

参考 <https://github.com/markparticle/WebServer> 编写

