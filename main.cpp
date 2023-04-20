#include "server/Server.hpp"

int main() {
    Server server{1316, 2, 60000, false, 0};
    server.start();
}
