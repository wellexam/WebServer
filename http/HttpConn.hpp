#pragma once

#ifdef _WIN32
#include "../base/Iov.hpp"
#else
#include <sys/uio.h>   // readv/writev
#include <arpa/inet.h> // sockaddr_in
#endif // _WIN32


#include <sys/types.h>
#include <cstdlib>     // atoi()
#include <cerrno>
#include <atomic>
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

class HttpConn {
public:
    HttpConn();

    ~HttpConn();

    void init(YetiSocketFD fd, const sockaddr_in &addr);

    ssize_t read(int *saveErrno);

    ssize_t write(int *saveErrno);

    void Close();

    YetiSocketFD GetFd() const;

    int GetPort() const;

    const char *GetIP() const;

    sockaddr_in GetAddr() const;

    bool process();

    int ToWriteBytes() { return iov_[0].iov_len + iov_[1].iov_len; }

    bool IsKeepAlive() const { return request_.IsKeepAlive(); }

    static bool isET;
    static const char *srcDir;
    static std::atomic<int> userCount;

private:
    YetiSocketFD fd_;
    struct sockaddr_in addr_;

    bool isClose_;

    int iovCnt_;
    struct iovec iov_[2];

    Buffer readBuff_;  // 读缓冲区
    Buffer writeBuff_; // 写缓冲区

    HttpRequest request_;
    HttpResponse response_;
};