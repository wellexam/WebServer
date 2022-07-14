#include "Channel.hpp"

Channel::~Channel() {
    printf("channel with fd %d destructed\n", fd_);
}
