#include "thread_pool.h"

class connectionRAII;

connection_pool* threadPool::m_conn_pool;
pthread_t threadPool::accept_id;
pthread_t threadPool::worker_ids[WORKER_THREAD_NUM];
pthread_mutex_t threadPool::accept_mutex;
pthread_mutex_t threadPool::client_mutex;
pthread_cond_t threadPool::accept_cond;
pthread_cond_t threadPool::client_cond;
std::list<http_conn*> threadPool::client_list;

void threadPool::thread_init() {
    m_conn_pool = WebServer::m_connPool;

    accept_id = 0;
    worker_ids[WORKER_THREAD_NUM] = {0};

    pthread_mutex_init(&accept_mutex, nullptr);
    pthread_cond_init(&accept_cond, nullptr);

    pthread_mutex_init(&client_mutex, nullptr);
    pthread_cond_init(&client_cond, nullptr);

    ::pthread_create(&threadPool::accept_id, nullptr, accept_thread_func, nullptr);
    for (int i = 0; i < WORKER_THREAD_NUM; i++) {
        ::pthread_create(&threadPool::worker_ids[i], nullptr, worker_thread_func, nullptr);
    }
}

void threadPool::thread_destroy() {
    ::pthread_mutex_destroy(&accept_mutex);
    ::pthread_cond_destroy(&accept_cond);
    ::pthread_mutex_destroy(&client_mutex);
    ::pthread_cond_destroy(&client_cond);
}

void* threadPool::accept_thread_func(void *arg) {

    while (true) {
        ::pthread_mutex_lock(&threadPool::accept_mutex);
        ::pthread_cond_wait(&threadPool::accept_cond, &threadPool::accept_mutex);
        ::pthread_mutex_unlock(&threadPool::accept_mutex);
        printf("this is threadPool::accept_thread_func\n");
        // 接收新客户端
        WebServer::add_newclient();
    }
    return nullptr;
}

void* threadPool::worker_thread_func(void *arg) {
    while (true) {
        ::pthread_mutex_lock(&threadPool::client_mutex);
        while (threadPool::client_list.empty())
            ::pthread_cond_wait(&threadPool::client_cond, &threadPool::client_mutex);
        http_conn* request = threadPool::client_list.front();// 取出队首clientfd
        threadPool::client_list.pop_front();
        ::pthread_mutex_unlock(&threadPool::client_mutex);
        // 处理客户端发来的数据
        if (0 == request->m_state) {
            if (request->read_once()) {
                request->improv = 1;
                connectionRAII mysql_con(&request->mysql, m_conn_pool);
                request->process();
            } else {
                printf("in thread_pool, read失败\n");
                request->improv = 1;
                request->timer_flag = 1;
            }
        } else {
            printf("in thread_pool, 进行写\n");
            if (request->write()) {
                request->improv = 1;
            } else {
                request->improv = 1;
                request->timer_flag = 1;
            }
        }

    }
}