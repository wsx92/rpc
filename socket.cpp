
#include"socket.h"
#include"event_dispatcher.h"

Socket::Socket()
	: _fd(-1) { }

int Socket::Create(int fd) {
	Socket* const m = new Socket();
	if (m == NULL) {
		//TODO
		return -1;
	}
	if (m->ResetFileDescriptor(fd) != 0) {
		//TODO
		return -1;
	}
	_socket_map[fd] = m;
	return 0;
}

int Socket::StartInputEvent(int fd, uint32_t epoll_events) {
	if (_socket_map.find(fd) == _socket_map.end()) {
		return -1;
	}
	Socket* s = _socket_map[fd];
	pthread_t tid;
	pthread_create(&tid, NULL, ProcessEvent, s);
}

void* Socket::ProcessEvent(void* arg) {
	Socket* s = static_cast<Socket*>(arg);
	s->DoRead(0);
	return NULL;
}

ssize_t Socket::DoRead(size_t size_hint) {
	int ssl_error = 0;
	ssize_t nr = 0;
	//nr = _read_buf.append_from_SSL_channel(_ssl_session, &ssl_error);
	return nr;
}

int Socket::ResetFileDescriptor(int fd) {
	_fd.store(fd, std::memory_order_release);
	if(GetGlobalEventDispatcher().AddConsumer(fd) != 0) {
		_fd.store(-1, std::memory_order_release);
		return -1;
	}
	return 0;
}


