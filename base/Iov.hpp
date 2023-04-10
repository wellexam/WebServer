#pragma once

#include "Platform.hpp"

#ifdef _WIN32
struct iovec {
	void* iov_base; /* Pointer to data. */
	size_t iov_len; /* Length of data. */
};

int writev(int socket, struct iovec* iov, int count);

int readv(int handle, struct iovec* iov, int count);
#endif // _WIN32

