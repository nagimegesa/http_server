#include "net.h"
#include <algorithm>
#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

void Net_Base::bind_listen(const sockaddr addr, int block) {
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int res = bind(listen_fd, &addr, sizeof(addr));
    if(res < 0) {
        Logger::get_logger()->error("bind error");
        exit(0);
    } 
    listen(listen_fd, 10);
    epoll_add_fd(listen_fd, EPOLLIN, false);
}

void Net_Base::epoll_add_fd(int fd, int ev, bool add_oneshot) {
    epoll_event e;
    e.events = ev | EPOLLHUP | EPOLLRDHUP | EPOLLET;
    if(add_oneshot) e.events |= EPOLLONESHOT;
    e.data.fd = fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &e);
}

void Net_Base::epoll_change_fd(int fd, int ev, bool add_oneshot) {
    epoll_event e;
    e.events = ev | EPOLLHUP | EPOLLRDHUP | EPOLLET;
    if(add_oneshot) e.events |= EPOLLONESHOT;
    e.data.fd = fd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &e) < 0) {
        Logger::get_logger()->error("epoll change fd error, the errno is %d, the fd is %d",
             errno, fd);
        exit(0);
    }
}

void Net_Base::epoll_del_fd(int fd) {
    if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        Logger::get_logger()->error("epoll del fd error, the errno is %d, the fd is %d",
            errno, fd );
        exit(0);
    }
}

int Net_Base::signal_fd[2];

void Net_Base::signal_cb(int signal) {
    int t = errno;
    int send_bytes = 0;
    while(true) {
        send_bytes += write(signal_fd[0], &signal, sizeof(int));
        if(send_bytes >= 4) break;
    }
    errno = t;
}

void Net_Base::init() {
    stop = false;
    epoll_fd = epoll_create(1);
    event_ptr = std::unique_ptr<epoll_event>(new epoll_event[EPOLL_MAX_CNT]);
    socketpair(AF_UNIX, SOCK_STREAM, 0, signal_fd);
    set_fd_noblock(signal_fd[0]);
    set_fd_noblock(signal_fd[1]);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGALRM, signal_cb);
    epoll_add_fd(signal_fd[1], EPOLLIN, false);
    alarm(TIMER_BASE);
    thread_pool = std::make_unique<Thread_Pool_Type>(1, thread_work);
    thread_pool->start(this);

}

void Net_Base::accpet_event(int fd) {
    sockaddr addr;
    socklen_t len = sizeof(addr);
    int accept_fd = accept(fd, &addr, &len);
    set_fd_noblock(accept_fd);

    auto cur_time = std::clock();
    Timer* timer = new Timer(cur_time + 3 * TIMER_BASE * CLOCKS_PER_SEC);
    Accept_Fd_Info a_info { accept_fd, timer, addr, NO_EVEMT};
    info_map[accept_fd] = a_info;
    timer_list.emplace(a_info);
    epoll_add_fd(accept_fd, EPOLLIN);

    Logger::get_logger()->info("a client was accepted, ip address: %s, fd: %d", 
        inet_ntoa(reinterpret_cast<sockaddr_in*>(&addr)->sin_addr), accept_fd);
}

void Net_Base::read_event(int fd) {
    auto& info = info_map[fd];
    info.which_event |= READ_EVENT;
    event_queue.emplace(&info);
}

void Net_Base::write_event(int fd) {
    auto& info = info_map[fd];
    info.which_event |= WRITE_EVENT;
    event_queue.emplace(&info);
}

void Net_Base::signal_event(int fd) {
    int signal;
    read(fd, &signal, sizeof(signal));
    switch (signal) {
    case SIGALRM:
        check_timer();
        break;
    default:
        auto& info = info_map[fd];
        info.which_event |= SIGNAL_EVENT;
        info.signal = signal;
        event_queue.emplace(&info);
        break;
    }
}

void Net_Base::close_event(int fd) {
    epoll_del_fd(fd);
    auto& info = info_map[fd];
    timer_list.erase(info);
    close(info.fd);
    delete info.timer;
    Logger::get_logger()->info("a client was closed, ip address %s, fd: %d"
        , inet_ntoa(reinterpret_cast<sockaddr_in*>(&info.addr)->sin_addr), fd);
    info_map.erase(info_map.find(fd));
}

void Net_Base::set_fd_noblock(int fd) {
    int stat = fcntl(fd, F_GETFL);
    if(stat < 0) Logger::get_logger()->error("get fcntl no nblock error");
    stat |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, stat) < 0)
        Logger::get_logger()->error("set fcntl no nblock error");
}

void Net_Base::run() {
    while(!stop) {
        int n = epoll_wait(epoll_fd, event_ptr.get(), 
            EPOLL_MAX_CNT, -1);
        if(n < 0) {
            if(errno == EINTR) 
                continue; 
            else 
                Logger::get_logger()->error("epoll wait error, the errno is %d", errno);
        }
        for(int i = 0;i < n; ++i) {
            auto e = event_ptr.get()[i].events;
            auto fd = event_ptr.get()[i].data.fd;
            if(fd == listen_fd && (e & EPOLLIN)) {
                accpet_event(fd);
            } else if(fd == signal_fd[1] && (e & EPOLLIN)) {
                signal_event(fd);
            } else { 
                if(e & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
                    close_event(fd);
                    continue;
                } 
                else if(e & EPOLLIN)
                    read_event(fd);
                else if(e & EPOLLOUT)
                    write_event(fd);
                change_timer(fd);
            }
        }
    }

    close(epoll_fd);
    close(listen_fd);

    close(signal_fd[1]);
    close(signal_fd[0]);
}

void Net_Base::thread_work(Net_Base* base) {
    auto& queue = base->event_queue;
    Accept_Fd_Info* info;
    while(true) {
        if(queue.pop(info)) {
            if(info->which_event & READ_EVENT) {
                base->user_read(info);
                info->which_event &= (~READ_EVENT);
            } 
            if(info->which_event & WRITE_EVENT) {
                base->user_write(info);
                info->which_event &= (~WRITE_EVENT);
            }
            if(info->which_event & SIGNAL_EVENT) {
                base->user_signal(info);
                info->signal = -1;
            }
        }
    }
}

void Net_Base::check_timer() {
    auto info = timer_list.front();
    if(info != nullptr) {;
        auto time = info->timer->get_time();
        auto now = std::clock();
        if(time <= clock()) {
            Logger::get_logger()->info(
                "a client time out, start call close event. ip address: %s, fd: %d",
                inet_ntoa(reinterpret_cast<const sockaddr_in*>(&info->addr)->sin_addr)
                , info->fd);
            close_event(info->fd);
        }
    }

    alarm(TIMER_BASE);
}

void Net_Base::change_timer(int fd) {
    std::map<int, Accept_Fd_Info>::iterator it = info_map.end();
    if((it = info_map.find(fd)) != info_map.end()) {
        const auto& info = it->second;
        const auto old_info = info;
        info.timer->set_timer(clock() + 3 * TIMER_BASE * CLOCKS_PER_SEC);
        timer_list.change(old_info, info);
    }
}

void Net_Base::user_read(Accept_Fd_Info* info) {
    char buf[128] = { 0 };
    int pos = 0;
    while(true) {
        int n = read(info->fd, buf + pos, sizeof(buf) - pos);
        if(n == 0) break;
        if(n < 0) {
            if(errno == EAGAIN || errno == EINTR)
                break;
        } else {
        }
        n += pos;
    }

    std::cout << "read from client: " << buf << std::endl;

    epoll_change_fd(info->fd, EPOLLOUT); 
}

void Net_Base::user_write(Accept_Fd_Info* info) {
    constexpr const char* msg = "hello client";
    write(info->fd, msg, std::strlen(msg));
    epoll_change_fd(info->fd, EPOLLIN);
}

void Net_Base::user_signal(Accept_Fd_Info* info) {}