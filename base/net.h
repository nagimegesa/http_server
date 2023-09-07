#pragma once

#include "../utils/thread_pool.hpp"
#include "../utils/timer.h"
#include "../utils/safe_queue.hpp"
#include "../utils/safe_map.hpp"
#include <algorithm>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <memory>

enum EVENT {
    READ_EVENT = 0x01,
    WRITE_EVENT = 0x02,
    SIGNAL_EVENT = 0x08,
    NO_EVEMT = 0x00
};

struct Accept_Fd_Info {
    int fd;
    Timer* timer;
    sockaddr addr;
    int which_event;
    int signal;
    bool operator<(const Accept_Fd_Info& rhs) const {
        return *timer < *rhs.timer && (fd != rhs.fd);
    }
    bool operator>(const Accept_Fd_Info& rhs) const {
        return *timer > *rhs.timer && (fd != rhs.fd);
    }
};

class Net_Base {  
private:
    static constexpr int EPOLL_MAX_CNT = 65536;
    static constexpr int FD_MAX_CNT = EPOLL_MAX_CNT;
    static constexpr int TIMER_BASE = 3;
    static void signal_cb(int signal);
    static void thread_work(Net_Base* base);

protected:
    void epoll_add_fd(int fd, int ev, bool add_oneshot = true);
    void epoll_del_fd(int fd);
    void epoll_change_fd(int fd, int ev, bool add_oneshot = true);
    
    void set_fd_noblock(int fd);

protected:
    void accpet_event(int fd);
    void read_event(int fd);
    void write_event(int fd);
    void close_event(int fd);
    void signal_event(int fd);

    void check_timer();
    void change_timer(int fd);

public:
    void init();
    void bind_listen(const sockaddr addr, int block);
    void run();

protected:
    virtual void user_read(Accept_Fd_Info* info);
    virtual void user_write(Accept_Fd_Info* info);
    virtual void user_signal(Accept_Fd_Info* info);

private:
    using Thread_Pool_Type = Thread_Pool<decltype(thread_work)>;

private:
    int listen_fd;
    int epoll_fd;
    bool stop;
    static int signal_fd[2];
    std::unique_ptr<epoll_event> event_ptr;
    Sort_List<Accept_Fd_Info> timer_list;
    std::unique_ptr<Thread_Pool_Type> thread_pool;
    Safe_Queue<Accept_Fd_Info*> event_queue; 

protected:
    std::map<int, Accept_Fd_Info> info_map;
};