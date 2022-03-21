// #ifndef REDIS_SUBSCRIBER_H
// #define REDIS_SUBSCRIBER_H

#include <stdlib.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <functional>
#include <iostream>
#include <signal.h>
#include <sstream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
using namespace std;

/**
 * 封装 hiredis， 实现消息订阅 redis 功能
 */
class RedisSubscriber {
public:
    typedef std::function<void(const char *, const char *, int)> NotifyMessageFn;    // 回调函数对象类型，当接收到消息后调用回调把消息发送出去

    RedisSubscriber();

    ~RedisSubscriber();

    bool init(const NotifyMessageFn &fn);    // 传入回调对象
    bool uninit();
    // bool connect(char* ip, int port);
    bool connect();

    bool disconnect();

    // 可以多次调用，订阅多个频道
    bool subscribe(const std::string &channel_name);
    bool unsubscribe(const std::string &channel_name);
    bool publish(const std::string &channel_name);
    bool exit_chat();
    bool check_chat_room();
    bool remove_chat_room(const string);
    bool enter_chat_room(const string);
    bool list_chat_room();
    bool list_user(const string);


    // 下面三个回调函数供redis服务调用
    // 连接回调
    static void sub_connect_callback(const redisAsyncContext *redis_context, int status);
    static void pub_connect_callback(const redisAsyncContext *redis_context, int status);

    // 断开连接的回调
    static void disconnect_callback(const redisAsyncContext *redis_context, int status);

    // 执行命令回调
    static void sub_command_callback(redisAsyncContext *redis_context, void *reply, void *privdata);
    static void pub_command_callback(redisAsyncContext *redis_context, void *reply, void *privdata);

    // 事件分发线程函数
    static void *event_thread(void *data);
    void *event_proc();

    //libevent回调
    static void cin_cb(bufferevent* bev, void* arg);
    static void signal_cb(int fd, short events, void *arg);

    static void to_event_cb(bufferevent *bev, short events, void *ctx);
    static void to_read_cb(bufferevent *bev, void *ctx);
    static void to_write_cb(bufferevent *bev, void *ctx);


    string user_name = "";
    string chat_name = " ";

    char *server_ip;
    short server_port;

    event_base *eventBase;
    bufferevent *cin_bev;//输入事件
    bufferevent *conn_bev;//连接私聊服务器
    event *evsignal;//退出信号
    bool private_server = false;

    // 事件线程ID
    pthread_t eventThread;
    // 事件线程的信号量
    sem_t eventSem;
    // hiredis异步对象
    redisAsyncContext *sub_context;
    redisAsyncContext *pub_context;

    // 通知外层的回调函数对象
    NotifyMessageFn notifyMessageFn;
};

struct Call_back_arg
{
    Call_back_arg(){}
    Call_back_arg(RedisSubscriber *sub, int fl):subscriber(sub),flag(fl){}
    RedisSubscriber *subscriber = NULL;
    int flag = -1;
    string info = " ";
};

// #endif