#include "redis_publisher.h"

/**
 * 实现发布消息
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {
    RedisPublisher publisher;

    bool ret = publisher.init();
    if (!ret) {
        std::cout << "Init failed." << std::endl;
        return 0;
    }

    ret = publisher.connect();
    if (!ret) {
        std::cout << "connect failed." << std::endl;
        return 0;
    }

    while (true) {
        //设置频道名称，频道内容
        time_t now = time(0);
        char *t = ctime(&now);
        publisher.publish("每日报道", t);
        sleep(1);
    }

    publisher.disconnect();
    publisher.uninit();
    return 0;
}