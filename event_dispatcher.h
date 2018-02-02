#ifndef EVENT_DISPATCHER_H
#define EVENT_DISPATCHER_H

#include<pthread.h>

class EventDispatcher {
public:
	EventDispatcher();

	int Start();

	static void* RunThis(void* arg);

private:
	void Run();

	int _epfd;

	pthread_t _tid;

	volatile bool _stop;
};

EventDispatcher& GetGlobalEventDispatcher(int fd);

#endif
