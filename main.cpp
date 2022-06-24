#include "WebServer.hpp"
#include <csignal>

int main() {
    signal(SIGPIPE, SIG_IGN); // 忽略SIGPIPE信号
    WebServer server(1316, 20, 60000);
    server.start();
}
