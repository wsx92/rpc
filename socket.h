#include<atomic>
#include<map>
#include"iobuf.h"


class Socket {
public:


	Socket();

	static int StartInputEvent(int fd, uint32_t epoll_events);

	static int Create(int fd);

	static std::map<int, Socket*> _socket_map;


	ssize_t DoRead(size_t size_hint);

	static void* ProcessEvent(void* arg);

	int ResetFileDescriptor(int fd);

	int fd() { return _fd.load(std::memory_order_relaxed); }

private:

	std::atomic_int _fd;

	IOPortal _read_buf;
};
