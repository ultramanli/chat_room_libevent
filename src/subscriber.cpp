#include "redis_subscriber.h"
using namespace std;
void recieve_message(const char *channel_name,
                     const char *message, int len) {
    printf("订阅信息:\n    频道名称: %s\n    来自频道的内容: %s\n",
           channel_name, message);
}


/**
 * 实现消息的订阅
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {
    if(argc < 4)
    {
        cout<<"Command as: client name ip port"<<endl;
        return 0;
    }

    RedisSubscriber* subscriber = new RedisSubscriber();
    subscriber->user_name = argv[1];
    subscriber->server_ip = argv[2];
    subscriber->server_port = atoi(argv[3]);

    RedisSubscriber::NotifyMessageFn fn =
            bind(recieve_message, std::placeholders::_1,
                 std::placeholders::_2, std::placeholders::_3);

    bool ret = subscriber->init(fn);
    if (!ret) {
        std::cout << "Init failed." << std::endl;
        return 0;
    }

    ret = subscriber->connect();
    if (!ret) {
        std::cout << "connect failed." << std::endl;
        return 0;
    }
    

    while (true) {
        sleep(1);

    }

    return 0;
}