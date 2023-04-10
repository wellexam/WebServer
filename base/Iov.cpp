#include "Iov.hpp"
#include "../log/log.h"

#ifdef _WIN32

int writev(int socket, struct iovec* iov, int count)
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
	return totallen;
}

int readv(int handle, struct iovec* iov, int count)
{

	long r = 0, t = 0;
	while (count)
	{
		r = recv(handle, (char*)iov->iov_base, iov->iov_len, 0);
		if (r < 0)
			if (t == 0)
				return r;
			else
				break;
		t += r;
		iov++;
		count--;

	}
	LOG_DEBUG("%d", t);
	return t;
}
#endif // _WIN32