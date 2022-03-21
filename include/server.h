#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <signal.h>

#include <arpa/inet.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <iostream>
#include <unordered_map>
using namespace std;
class User
{
public:
	User(int fd, event_base *base)
	{
		_fd = fd;
		_bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	}

	~User()
	{
		if(_bev)
		{
			bufferevent_free(_bev);
		}
	}

	int _fd;
	string _name;
    bufferevent *_bev;
};