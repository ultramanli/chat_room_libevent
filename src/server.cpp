#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <signal.h>

#include <arpa/inet.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <vector>
#include <sstream>

#include <iostream>
#include <unordered_map>
#include "server.h"
using namespace std;

unordered_map<string, User*> User_map;

static void echo_read_cb(bufferevent *bev, void *ctx)
{
	User* user = (User*)ctx;
	char data[1024];
	memset(data, 0, sizeof(data));
	evbuffer *input = bufferevent_get_input(bev);//获取客户端输入
	while(evbuffer_get_length(input))
	{
		bufferevent_read(bev, data, sizeof(data));
	}

	//输入数据解析
	string data_s = data;
	if(data_s == "\n")
	{
		return;
	}
	vector<string> cin_data;
	string temp;
	stringstream sin(data_s);
	while(getline(sin, temp, '|'))
	{
		cin_data.push_back(temp);
	}

	//登陆与私聊
	if(cin_data.size() > 1)//有命令行
	{
		if(cin_data[0] == "reg")//登陆 注册到map
		{
			string name = cin_data[1];
			if(name.back() == '\n')
			{
				name = name.substr(0, name.size() - 1);
			}
			user->_name = name;
			User_map[name] = user;
			cout<<name<<" login"<<endl;
			return;
		}
		else if(cin_data[0] == "to")//私聊
		{
			string to_name = cin_data[1];
			User *to_user = User_map[to_name];
			if(to_user != nullptr)
			{
				string message = user->_name + ":\n"+ cin_data[2].substr(0, cin_data[2].size() - 1);
				bufferevent *to_bev = to_user->_bev;
				bufferevent_write(to_bev, message.c_str(), message.size());
			}
			else
			{
				string message = "No this user: ";
				message += to_name;
				bufferevent_write(bev, message.c_str(), message.size());
			}
		}
		
	}

}

static void echo_event_cb(bufferevent *bev, short events, void *ctx)
{
	if(events & BEV_EVENT_ERROR)
	{
		perror("Wrong");
	}
	if(events & (BEV_EVENT_ERROR | BEV_EVENT_EOF))
	{
		User* user = (User*)ctx;
		cout<<user->_name<<" logout "<<endl;
		User_map[user->_name] = nullptr;
		delete(user);
		user = nullptr;
	}
}

static void accept_conn_cb(evconnlistener *listener, int fd, sockaddr *address, int socklen, void *ctx)
{
	
	event_base *base = evconnlistener_get_base(listener);
	User *user = new User(fd, base);
	bufferevent_setcb(user->_bev, echo_read_cb, NULL, echo_event_cb, user);
	bufferevent_enable(user->_bev, EV_READ | EV_WRITE);
}

static void accept_error_cb(evconnlistener *listener, void *ctx)
{
	event_base *base = evconnlistener_get_base(listener);
	int err = EVUTIL_SOCKET_ERROR();
	cout<<evutil_socket_error_to_string(err)<<endl;
	event_base_loopexit(base, NULL);
}

void signal_cb(int fd, short events, void *arg)
{
	event_base *base = (event_base*)arg;
	cout<<"clear"<<endl;
	event_base_loopexit(base, NULL);

}

int main()
{
	event_base* base;
	evconnlistener *listener;
	event* signal_event;

	sockaddr_in sin;

	int port = 8888;

	base = event_base_new();

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = htonl(0);

	listener = evconnlistener_new_bind(
		base,
		accept_conn_cb,
		NULL,
		LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
		-1,
		(sockaddr*)&sin,
		sizeof(sin)
		);

	evconnlistener_set_error_cb(listener, accept_error_cb);

	signal_event = event_new(base, SIGINT, EV_SIGNAL, signal_cb, base);
	event_add(signal_event, NULL);
	event_base_dispatch(base);


}