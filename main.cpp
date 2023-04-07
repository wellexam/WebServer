#include <csignal>
#include "server/Server.hpp"

int main() {
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN); // 忽略SIGPIPE信号
#endif // !_WIN32
    Server server{1316, 20, 60000, false, 1};
    server.start();
}
