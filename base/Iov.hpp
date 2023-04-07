#pragma once

#include "Platform.hpp"

#ifdef _WIN32
struct iovec {
	void* iov_base; /* Pointer to data. */
	size_t iov_len; /* Length of data. */
};

static long writev(int socket, struct iovec* iov, int count)
{
	long totallen = -1, tlen = -1;
	while (count)
	{
		tlen = send(socket, (const char*)iov->iov_base, iov->iov_len, 0);
		if (tlen < 0)
			return totallen;
		totallen += tlen;
		iov++;
		count--;
	}
}

static int readv(int handle, struct iovec* iov, int count)
{

	long r, t = 0;
	while (count)
	{

		r = read(handle, iov->iov_base, iov->iov_len);
		if (r < 0)
			return r;
		t += r;
		iov++;
		count--;

	}
}
#endif // _WIN32

