#pragma once

#include "net.h"
#include <cstdio>
#include <string>
#include <unordered_map>
enum HTTP_PRE_STAT {
    HTTP_OPEN,
    HTTP_ERROR,
    HTTP_HEAD,
    HTTP_REQ,
    HTTP_CONTNET,
    HTTP_FINISH
};

enum LINE_STAT {
    LINE_OPEN,
    LINE_FINSH,
    LINE_ERROR,
};

enum METHOD {
    GET,
    POST,
    OPTIONS,
    NONE
};

enum PRASE_HEAD_STAT {
    PRASE_HEAD_FINISH,
    PRASE_HEAD_ERROR,
    PRASE_HEAD_OPEN
};

enum HTTP_STAT {
    _404_NOT_FOUND,
    _200_OK
};

struct Prase_Res {
    METHOD which;
    std::string file;
    int content_length;

    Prase_Res() : which(NONE), content_length(0){

    }
};

struct Prase_Info {
    int fd;
    char* buf;
    int prase_stat;
    int prase_index;
    int end_index;
    int buf_size;

    int line_start_index;
    int line_end_index;

    Prase_Res res;

    Prase_Info(int fd) : fd(fd), prase_index(0), 
                end_index(0), buf_size(0), prase_stat(HTTP_REQ), 
                line_start_index(0), line_end_index(0) {
        buf = new char[BUFSIZ]{ 0 };
        buf_size = BUFSIZ;
    }

    ~Prase_Info() {
        delete [] buf;
    }
};


class Http : public Net_Base {
public:
    virtual void user_read(Accept_Fd_Info* info) override;
    virtual void user_write(Accept_Fd_Info* info) override;
    virtual void user_signal(Accept_Fd_Info* info) override;

private:
    void read_data(Prase_Info* info);
    HTTP_PRE_STAT prase_body(Prase_Info* info);
    HTTP_PRE_STAT prase_content(Prase_Info* info);
    HTTP_PRE_STAT prase_data(Prase_Info* info);

    LINE_STAT prase_line(Prase_Info* info);
    bool prase_request(Prase_Info* info);
    PRASE_HEAD_STAT prase_head(Prase_Info* info);
    void send_msg(int fd, const std::string& path);
    int append_file(std::string& buf, const std::string& file);
    int append_dir(std::string& buf, const std::string& path);
    void send_200_OK(int fd);

    void append_request(HTTP_STAT stat, std::string& b);
    void append_file_type(const std::string& file, std::string& b);
public:
    Http(const std::string& file);

private:
    std::string file_buf;
    std::map<int, Prase_Info*> prase_map;

    static std::unordered_map<std::string, std::string> file_type2content;
};