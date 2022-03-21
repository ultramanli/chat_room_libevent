#include <stddef.h>
#include <assert.h>
#include <string.h>
#include "redis_subscriber.h"

#define STD_IN 0
#define EXIT_CHAT 0
#define CHECK_CHAT_ROOM 1
#define REMOVE_CHAT_ROOM 2
#define ENTER_CHAT_ROOM 3
#define LIST_CHAT_ROOM 4
#define LIST_USER 5
using namespace std;
RedisSubscriber::RedisSubscriber() : eventBase(0), eventThread(0),
                                     sub_context(0), pub_context(0) {
}

RedisSubscriber::~RedisSubscriber() {
	// while(this->chat_name != " ")
	// {
	// 	cout<<chat_name<<endl;

	// 	this->exit_chat();
	// 	this->check_chat_room();//检查是否聊天室无人 无人则关闭
	// 	sleep(1);
	// }
	disconnect();
    uninit();

}

bool RedisSubscriber::init(const NotifyMessageFn &fn) {
	// initialize the event
	notifyMessageFn = fn;
	eventBase = event_base_new();    // 创建libevent对象
	if (NULL == eventBase) {
		std::cout << "Create redis event failed." << std::endl;
		return false;
	}

	memset(&eventSem, 0, sizeof(eventSem));
	int ret = sem_init(&eventSem, 0, 0);
	if (ret != 0) {
		std::cout << "Init sem failed." << std::endl;
		return false;
	}

	return true;
}

bool RedisSubscriber::uninit() {
	eventBase = NULL;
	sem_destroy(&eventSem);
	cout<<"Uninit done"<<endl;
	return true;
}

// bool RedisSubscriber::connect(char* ip, int port) {
bool RedisSubscriber::connect() {
	
	// connect redis
	// context = redisAsyncConnect(ip, port);    // 异步连接到redis服务器上，使用默认端口
	sub_context = redisAsyncConnect(server_ip, 6379);    //输出连接 异步连接到redis服务器上，使用默认端口
	pub_context = redisAsyncConnect(server_ip, 6379);    //输入连接 异步连接到redis服务器上，使用默认端口
	if (NULL == sub_context || NULL == pub_context) {
		std::cout << "Connect redis failed." << std::endl;
		return false;
	}

	// 输出错误信息
	if (sub_context->err) {
		std::cout << "Connect redis error: " << sub_context->err << sub_context->errstr << std::endl;
		return false;
	}
	if (pub_context->err) {
		std::cout << "Connect redis error: " << pub_context->err << pub_context->errstr << std::endl;
		return false;
	}

	// attach the event
	redisLibeventAttach(sub_context, eventBase);    // 将事件绑定到redis context上，使设置给redis的回调跟事件关联
	redisLibeventAttach(pub_context, eventBase);    // 将事件绑定到redis context上，使设置给redis的回调跟事件关联

	// 设置连接回调，当异步调用连接后\服务器处理连接请求结束后 调用，通知调用者连接的状态
	redisAsyncSetConnectCallback(sub_context, &RedisSubscriber::sub_connect_callback);
	redisAsyncSetConnectCallback(pub_context, &RedisSubscriber::pub_connect_callback);
	// 设置断开连接回调，当服务器断开连接后，通知调用者连接断开，调用者可以利用这个函数实现重连
	redisAsyncSetDisconnectCallback(sub_context, &RedisSubscriber::disconnect_callback);
	redisAsyncSetDisconnectCallback(pub_context, &RedisSubscriber::disconnect_callback);
	
	//键盘输入事件
	cin_bev = bufferevent_socket_new(eventBase, STD_IN, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(cin_bev,cin_cb, NULL, NULL, this);//设置 可读事件的回调函数  连接关闭事件的回调函数
	bufferevent_enable(cin_bev, EV_READ);//注意在这里打开开关，才能够触发对应类型的事件
	//键盘退出事件
	evsignal = evsignal_new(eventBase, SIGINT, signal_cb, this);
    evsignal_add(evsignal, NULL);

    //连接服务器 私聊功能
    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(server_ip);//注意
    sin.sin_port = htons(server_port);

	conn_bev = bufferevent_socket_new(eventBase, -1, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(conn_bev,to_read_cb, to_write_cb, to_event_cb, this);//设置 可写事件的回调函数  连接关闭事件的回调函数
	bufferevent_enable(conn_bev, EV_READ | EV_PERSIST);//注意在这里打开开关，才能够触发对应类型的事件

    int ret;
    ret = bufferevent_socket_connect(conn_bev, (sockaddr*)&sin, sizeof(sin));
    if(ret != 0)
    {
    	std::cout << "Connect server failed." << std::endl;
		disconnect();
		return false;
    }else{
    	std::cout << "Private chat server connected!" << std::endl;
    }
    
	string msg = "reg|" + user_name;
	bufferevent_write(conn_bev, msg.c_str(), msg.size());//服务器注册用户


// 创建事件处理线程
	ret = pthread_create(&eventThread, 0, &RedisSubscriber::event_thread, this);
	if (ret != 0) {
		std::cout << "Create event thread failed." << std::endl;
		disconnect();
		return false;
	}


	// 启动事件线程
	sem_post(&eventSem);
	return true;
}

bool RedisSubscriber::disconnect() {
	if (sub_context) {
		redisAsyncDisconnect(sub_context);
		redisAsyncFree(sub_context);
		sub_context = NULL;
		cout<<"Disconnect sub_context done"<<endl;
	}

	if (pub_context) {
		redisAsyncDisconnect(pub_context);
		redisAsyncFree(pub_context);
		pub_context = NULL;
		cout<<"Disconnect pub_context done"<<endl;
	}
	bufferevent_free(conn_bev);
	return true;
}

bool RedisSubscriber::subscribe(const std::string &channel_name) {
	chat_name = channel_name;//记录聊天室
	int ret = redisAsyncCommand(sub_context,
	                            &RedisSubscriber::sub_command_callback, this, "SUBSCRIBE %s",
	                            channel_name.c_str());
	if (REDIS_ERR == ret) {
		std::cout << "Subscribe command failed: " << ret << std::endl;
		return false;
	}

	std::cout << "Subscribe success: " << channel_name.c_str() << std::endl;
	return true;
}

bool RedisSubscriber::unsubscribe(const std::string &channel_name) {

	int ret = redisAsyncCommand(sub_context,
	                            &RedisSubscriber::sub_command_callback, this, "UNSUBSCRIBE %s",
	                            channel_name.c_str());

	if (REDIS_ERR == ret) {
		std::cout << "Unsubscribe command failed: " << ret << std::endl;
		return false;
	}

	std::cout << "Unsubscribe success: " << channel_name.c_str() << std::endl;
	return true;
}

bool RedisSubscriber::publish(const std::string &cout_data) {

	int ret = redisAsyncCommand(pub_context,
								&RedisSubscriber::pub_command_callback,
								NULL,
								"PUBLISH %s %s",
	                            chat_name.c_str(), (user_name +":\n"+cout_data).c_str());

	if (REDIS_ERR == ret) {
		std::cout << "Publish command failed: " << ret << std::endl;
		return false;
	}
	return true;
}

bool RedisSubscriber::exit_chat() {

	Call_back_arg *cbarg = new Call_back_arg(this, EXIT_CHAT);
	cbarg->info = chat_name;

	int ret = redisAsyncCommand(this->pub_context,
								&RedisSubscriber::pub_command_callback,
								cbarg,
								"SREM %s %s",
	                            chat_name.c_str(), user_name.c_str());
	if (REDIS_ERR == ret) {
		std::cout << "SREM command failed: " << ret << std::endl;
		return false;
	}
	return true;
}

bool RedisSubscriber::check_chat_room() {
	Call_back_arg *cbarg = new Call_back_arg(this, CHECK_CHAT_ROOM);
	int ret = redisAsyncCommand(pub_context,
								&RedisSubscriber::pub_command_callback,
								cbarg,
								"SCARD %s",
	                            chat_name.c_str());
	if (REDIS_ERR == ret) {
		std::cout << "SCARD command failed: " << ret << std::endl;
		return false;
	}
	return true;
}

bool RedisSubscriber::remove_chat_room(string old_chat_name) {


	Call_back_arg *cbarg = new Call_back_arg(this, REMOVE_CHAT_ROOM);
	cbarg->info = old_chat_name;
	int ret = redisAsyncCommand(pub_context,
								&RedisSubscriber::pub_command_callback,
								cbarg,
								"SREM %s %s",
	                            "chat_room", old_chat_name.c_str());
	if (REDIS_ERR == ret) {
		std::cout << "SREM command failed: " << ret << std::endl;
		return false;
	}
	return true;
}

bool RedisSubscriber::enter_chat_room(string chat_name) {
	Call_back_arg *cbarg = new Call_back_arg(this, ENTER_CHAT_ROOM);
	cbarg->info = chat_name;
	int ret = redisAsyncCommand(pub_context,
								&RedisSubscriber::pub_command_callback,
								cbarg,
								"SADD %s %s",
	                            "chat_room", chat_name.c_str());
	if (REDIS_ERR == ret) {
		std::cout << "SADD chat_room command failed: " << ret << std::endl;
		return false;
	}

	ret = redisAsyncCommand(pub_context,
								&RedisSubscriber::pub_command_callback,
								cbarg,
								"SADD %s %s",
	                            chat_name.c_str(), user_name.c_str());

	if (REDIS_ERR == ret) {
		std::cout << "SADD user command failed: " << ret << std::endl;
		return false;
	}
	return true;
}

bool RedisSubscriber::list_chat_room() {
	Call_back_arg *cbarg = new Call_back_arg(this, LIST_CHAT_ROOM);
	cbarg->info = chat_name;
	int ret = redisAsyncCommand(pub_context,
								&RedisSubscriber::pub_command_callback,
								cbarg,
								"SMEMBERS %s",
	                            "chat_room");
	if (REDIS_ERR == ret) {
		std::cout << "SMEMBERS chat_room command failed: " << ret << std::endl;
		return false;
	}

	return true;
}

bool RedisSubscriber::list_user(string chat_room) {
	Call_back_arg *cbarg = new Call_back_arg(this, LIST_USER);
	cbarg->info = chat_room;
	int ret = redisAsyncCommand(pub_context,
								&RedisSubscriber::pub_command_callback,
								cbarg,
								"SMEMBERS %s",
	                            chat_room.c_str());
	if (REDIS_ERR == ret) {
		std::cout << "SMEMBERS chat_room command failed: " << ret << std::endl;
		return false;
	}

	return true;
}



void RedisSubscriber::sub_connect_callback(const redisAsyncContext *redis_context,int status) 
{
	if (status != REDIS_OK) {
		std::cout << "Error: " << redis_context->errstr << std::endl;
		exit(0);
	} else {
		std::cout << "Redis sub connected!" << std::endl;
	}
}

void RedisSubscriber::pub_connect_callback(const redisAsyncContext *redis_context,int status) 
{
	if (status != REDIS_OK) {
		std::cout << "Error: " << redis_context->errstr << std::endl;
		exit(0);
	} else {
		std::cout << "Redis pub connected!" << std::endl;
	}
}

void RedisSubscriber::disconnect_callback(const redisAsyncContext *redis_context, int status)
 {
	if (status != REDIS_OK) {
		// 这里异常退出，可以尝试重连
		std::cout << "Error: " << redis_context->errstr << std::endl;
	}
}

void RedisSubscriber::cin_cb(bufferevent* bev, void* arg)
{
	RedisSubscriber* subscriber = static_cast<RedisSubscriber*>(arg);
	char data[1024];
	memset(data, 0, sizeof(data));
	evbuffer *input = bufferevent_get_input(bev);//获得输入缓冲区
	while(evbuffer_get_length(input) != 0)
	{
		bufferevent_read(bev, data, evbuffer_get_length(input));
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


	//公共操作 查询聊天室人员
	if(cin_data.size() > 1)//有命令行
	{
		if(cin_data[0] == "list_room")//查看聊天室列表
		{
			subscriber->list_chat_room();
			return;
		}
		else if(cin_data[0] == "list_user")//查看聊天室成员列表
		{
			if(cin_data[1] == "\n")
			{
				return;
			}

			subscriber->list_user(data_s.substr(10, data_s.size() - 10 - 1));

			return;
		}
		else if(cin_data[0] == "to")//私聊用户
		{
			if(cin_data.size() == 2 || cin_data[1] == "\n" || cin_data[2] == "\n" )
			{
				cout<<"INFO: Receiver or data is empty"<<endl;
				return;
			}
			bufferevent_write(subscriber->conn_bev, data_s.c_str(), data_s.size());
			return;
		}
	}
	

	//在聊天室内
	if(subscriber->chat_name != " ")
	{
		if(cin_data.size() > 1)//有命令行
		{
			if(cin_data[0] == "exit")//退出聊天室
			{
				//静态成员函数的bind
				// function<void(int)> sig_cb = bind((void(*)(int, short, void*))&RedisSubscriber::signal_cb, std::placeholders::_1,0, subscriber);
				// function<void(int)> sig_cb = bind(RedisSubscriber::signal_cb, std::placeholders::_1,0, subscriber);
				
				subscriber->exit_chat();//移出聊天室人员名单
				subscriber->check_chat_room();//检查是否聊天室无人 无人则关闭
				// subscriber->chat_name = "";//重置聊天室 在回调函数里重置了
			}
			else if(cin_data[0] == "out")//发消息
			{
				if(cin_data[1] == "\n")
				{
					return;
				}
				subscriber->publish(data_s.substr(4, data_s.size() - 4 - 1));

			}
			else//未定义命令 发送全部输入
			{
				subscriber->publish(data_s.substr(0, data_s.size() - 1));
			}
		}
		else //无命令行 发送全部输入
		{
			subscriber->publish(data_s.substr(0, data_s.size() - 1));

		}
	}
	else//不在聊天室内 chat_name = “”
	{ 
		if(cin_data.size() > 1)//有命令行
		{
			if(cin_data[0] == "exit")//退出程序
			{
				raise(SIGINT);//唤起sigint信号
			}
			else if(cin_data[0] == "enter")//进入或创建聊天室
			{
				if(cin_data[1] == "\n" || data_s[6] == ' ')
				{
					return;
				}
				string chat_name = data_s.substr(6, data_s.size() - 6 - 1);
				subscriber->subscribe(chat_name);
				subscriber->enter_chat_room(chat_name);

			}
			else//未定义命令 发送全部输入
			{
				subscriber->publish(data_s.substr(0, data_s.size() - 1));
			}
		}
		else //无命令行 发送全部输入
		{
			subscriber->publish(data_s.substr(0, data_s.size() - 1));

		}
	}

}

// 消息接收回调函数
void RedisSubscriber::sub_command_callback(redisAsyncContext *redis_context,
                                       void *reply, void *privdata) 
{

	if (NULL == reply || NULL == privdata) {
		return;
	}

	// 静态函数中，要使用类的成员变量，把当前的this指针传进来，用this指针间接访问
	// RedisSubscriber *self_this = reinterpret_cast<RedisSubscriber *>(privdata);
	redisReply *redis_reply = reinterpret_cast<redisReply *>(reply);

	// 订阅接收到的消息是一个带三元素的数组
	if (redis_reply->type == REDIS_REPLY_ARRAY &&
	    redis_reply->elements == 3 && redis_reply->element[2]->str) {
		printf("\n%s-%s\n",redis_reply->element[1]->str, redis_reply->element[2]->str);
		// 调用函数对象把消息通知给外层
		// self_this->notifyMessageFn(redis_reply->element[1]->str,
		//                            redis_reply->element[2]->str, redis_reply->element[2]->len);
	}
}

void RedisSubscriber::pub_command_callback(redisAsyncContext *redis_context,
                                       void *reply, void *privdata) 
{
	if (NULL == reply || NULL == privdata) 
	{
		return;
	}
	redisReply *redis_reply = reinterpret_cast<redisReply *>(reply);

	int ret = redis_reply->integer;
	Call_back_arg* cbarg = (Call_back_arg*) privdata;
	if(cbarg->flag == EXIT_CHAT)
	{
		cout<<"INFO: Exit from chat room \""<<cbarg->subscriber->chat_name<<"\""<<endl;
		//取消订阅
		cbarg->subscriber->unsubscribe(cbarg->info);
	}
	else if(cbarg->flag == CHECK_CHAT_ROOM)
	{
		if(ret == 0)
		{
			cbarg->subscriber->remove_chat_room(cbarg->subscriber->chat_name);
		}
		cbarg->subscriber->chat_name = " ";//重置聊天室
	}
	else if(cbarg->flag == REMOVE_CHAT_ROOM)
	{
		if(ret == 1)
		{
			cout<<"INFO: Nobody in chat room, destory \""<<cbarg->info<<"\""<<endl;
		}
	}
	else if(cbarg->flag == LIST_CHAT_ROOM)
	{
		//注意不是ret
		if(redis_reply->elements == 0)
		{
			cout<<"INFO: No chat room exist"<<endl;
		}
		else
		{
			for(int i = 0; i < redis_reply->elements; ++i)
			{
				cout<<"\""<<redis_reply->element[i]->str<<"\""<<endl;
			}
		}
	}
	else if(cbarg->flag == LIST_USER)
	{
		//注意不是ret
		if(redis_reply->elements == 0)
		{
			cout<<"INFO: Nobody in \""<<cbarg->info<<"\""<<endl;
		}
		else
		{
			for(int i = 0; i < redis_reply->elements; ++i)
			{
				cout<<"\""<<redis_reply->element[i]->str<<"\""<<endl;
			}
		}
	}
	
}

void *RedisSubscriber::event_thread(void *data) {
	if (NULL == data) {
		std::cout << "Error!" << std::endl;
		assert(false);
		return NULL;
	}

	RedisSubscriber *self_this = reinterpret_cast<RedisSubscriber *>(data);
	return self_this->event_proc();
}

void *RedisSubscriber::event_proc() {
	sem_wait(&eventSem);

	// 开启事件分发，event_base_dispatch会阻塞
	event_base_dispatch(eventBase);

	return NULL;
}

void RedisSubscriber::signal_cb(int fd, short events, void *arg)
{
    RedisSubscriber *subscriber = (RedisSubscriber*) arg;
	while(subscriber->chat_name != " ")
	{
		cout<<subscriber->chat_name<<endl;

		subscriber->exit_chat();
		subscriber->check_chat_room();//检查是否聊天室无人 无人则关闭
		sleep(5);
	}
    delete(subscriber);
    exit(0);
}

void RedisSubscriber::to_event_cb(bufferevent *bev, short events, void *ctx)
{
	if(events & BEV_EVENT_ERROR)
	{
		perror("Wrong");
	}
	if(events & (BEV_EVENT_ERROR | BEV_EVENT_EOF))
	{
		RedisSubscriber* subscriber = (RedisSubscriber*)ctx;
		cout<<"Private chat server offline "<<endl;
		subscriber->private_server = false;
	}
}

void RedisSubscriber::to_read_cb(bufferevent *bev, void *ctx)
{
	RedisSubscriber* subscriber = (RedisSubscriber*)ctx;
	char data[1024];
	memset(data, 0, sizeof(data));
	evbuffer *input = bufferevent_get_input(bev);//获取服务器返回
	while(evbuffer_get_length(input))
	{
		bufferevent_read(bev, data, sizeof(data));
	}
	cout<<'\n'<<data<<endl;

}

void RedisSubscriber::to_write_cb(bufferevent *bev, void *ctx)
{

}