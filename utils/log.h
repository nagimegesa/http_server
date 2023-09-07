#pragma once

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <unistd.h>
#include <ctime>

class Logger {
private:
    int error_file_fd;
    int warn_file_fd;
    int debug_file_fd;
    int info_file_fd;
    std::mutex file_lock;
public:
    template <typename ...Args>
    void format_logstr(const char* which, const char* format, Args...arg) {
        char buf[BUFSIZ];
        get_local_time(buf, sizeof(buf));
        std::strcat(buf, which);
        sprintf(buf + strlen(buf), format, arg...);
        std::strcat(buf, "\n");
        finally_write(error_file_fd, buf, strlen(buf));
    }

    template <typename ...Args>
    void error(const char* format, Args...arg) {
        format_logstr(" [error] ", format, arg...);
    }

    template <typename ...Args>
    void warn(const char* format, Args...arg) {
        format_logstr(" [warn] ", format, arg...);
    }

    template <typename ...Args>
    void debug(const char* format, Args...arg) {
        format_logstr(" [debug] ", format, arg...);
    }

    template <typename ...Args>
    void info(const char* format, Args...arg) {
        format_logstr(" [info] ", format, arg...);
    }

    void set_error_file(const char* s) {
        int t = open(s, O_WRONLY | O_APPEND);
        if(t < 0) {
            error("set error file error, the file name is %s", s);
            return;
        }
        error_file_fd = t;
    }

    void set_warn_file(const char* s) {
        int t = open(s, O_WRONLY| O_APPEND);
        if(t < 0) {
            error("set warn file error, the file name is %s", s);
            return;
        }
        warn_file_fd = t;
    }

    void set_debug_file(const char* s) {
        int t = open(s, O_WRONLY| O_APPEND);
        if(t < 0) {
            error("set debug file error, the file name is %s", s);
            return;
        }
        debug_file_fd = t;
    }

    void set_info_file(const char* s) {
        int t = open(s, O_WRONLY| O_APPEND);
        if(t < 0) {
            error("set info file error, the file name is %s", s);
            return;
        }
        info_file_fd = t;
    }

private:
    Logger() {
        error_file_fd = STDERR_FILENO;
        warn_file_fd = STDOUT_FILENO;
        debug_file_fd = STDOUT_FILENO;
        info_file_fd = STDOUT_FILENO;
    }

    void finally_write(int fd, const char* msg, size_t cnt) {
        file_lock.lock();
        write(fd, msg, cnt);
        file_lock.unlock();
    }
    
    void get_local_time(char* buf, size_t buf_cnt) {
        time_t time = std::time(nullptr);
        auto tm = std::localtime(&time);
        std::strftime(buf, buf_cnt, "[%Y年%m月%d日 星期%w %X]", tm);
    }
public:
    static Logger* get_logger() {
        static Logger* p = new Logger();
        return p;
    }
};
