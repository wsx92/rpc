#include<assert.h>
#include"event_dispatcher.h"
#include<sys/epoll.h>

EventDispatcher::EventDispatcher()
	: _epfd(-1), _tid(0), _stop(true) {
		_epfd = epoll_create(1024 * 1024);
		assert(_epfd);
	}

void EventDispatcher::Run() {
	epoll_event e[32];
	while (!_stop) {
		const int n = epoll_wait(_epfd, e, 32, -1);
		if (_stop) {
			break;
		}
		if (n < 0) {
			//TODO
			break;
		}
		for (int i = 0; i < n; ++i) {
			if (e[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP) ) {
				//Socket::StartInputEvent(e[i].data.u64, e[i].events);
			}
		}
		for (int i = 0; i < n; ++i) {
			if (e[i].events & (EPOLLOUT | EPOLLERR | EPOLLHUP)) {
				//Socket::HandleEventOut(e[i].data.u64);
			}
		}
	}
}

int EventDispatcher::Start() {
	if (_epfd < 0) {
		return -1;
	}
	if (_tid != 0) {
		return -1;
	}
	int rc = pthread_create(&_tid, NULL, RunThis, this);
	if (rc) {
		return -1;
	}
	return 0;
}

void* EventDispatcher::RunThis(void* arg) {
	((EventDispatcher*)arg)->Run();
	return NULL;
}

static EventDispatcher g_edisp;

void InitializeGlobalDispatcher() {
}
