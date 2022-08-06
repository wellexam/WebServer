#include <csignal>
#include "server/Server.hpp"

int main() {
    signal(SIGPIPE, SIG_IGN); // 忽略SIGPIPE信号
    Server server{1316, 20, 60000, false, 0};
    server.start();
}
