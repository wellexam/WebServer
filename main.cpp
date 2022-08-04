#include "WebServer.hpp"
#include <csignal>
#include "Server.hpp"

int main() {
    signal(SIGPIPE, SIG_IGN); // 忽略SIGPIPE信号
                              // WebServer server(1316, 20, 60000);
                              // server.start();
    Server server{1316, 20, 60000, true, 0};
    server.start();
}
