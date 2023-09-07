#include "http.h"
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <bits/utility.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <ios>
#include <iterator>
#include <stdexcept>
#include <string>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/file.h>
#include <unordered_map>
#include <dirent.h>
#include <vector>

Http::Http(const std::string& file) {
    std::fstream file_stream(file, std::ios::in);
    std::string buf;
    while(file_stream) {
        std::getline(file_stream, buf);
        file_buf.append(buf);
    }

    buf = new char[BUFSIZ];
}

void Http::user_read(Accept_Fd_Info *info) {
    int fd = info->fd;
    Prase_Info* prase_info = nullptr;
    if(prase_map.find(fd) == prase_map.end())
        prase_map[fd] = new Prase_Info(info->fd);
    prase_info = prase_map[fd];
    read_data(prase_info);
    int stat = prase_data(prase_info);
    if(stat == HTTP_FINISH)
        epoll_change_fd(info->fd, EPOLLOUT);
    else if(stat == HTTP_OPEN)
        epoll_change_fd(info->fd, EPOLLIN);
}

void Http::user_write(Accept_Fd_Info *info) {
    int fd = info->fd;
    auto& prase_info = prase_map[fd];
    std::string& path = prase_info->res.file;
    auto method = prase_info->res.which;
    switch (method) {
    case GET:
        send_msg(fd, path); break;
    case POST:
    case OPTIONS:
        send_200_OK(fd);
    default:break;
    }
    prase_map.erase(prase_map.find(fd));
    epoll_change_fd(info->fd, EPOLLIN);
}

void Http::user_signal(Accept_Fd_Info *info) {
    switch (info->signal) {
    case SIGPIPE:
        close_event(info->fd);
        break;
    default:
        break;
    }
}

void Http::read_data(Prase_Info* info) {
    while(true) {
        int read_start_index = info->end_index;
        int read_size = read(info->fd, info->buf + read_start_index, 
            info->buf_size - read_start_index);
        if(read_size == 0) break;
        if(read_size < 0) {
            if(errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
                break;
            Logger::get_logger()->error("read error. errno: %d, fd: %d", errno, info->fd);
            break;
        }
        info->end_index += read_size;
        if(info->buf_size - info->end_index < 128) {
            int size_new = info->buf_size * 2;
            char* buf_new = new char[size_new]{0};
            std::memcpy(buf_new, info->buf, info->buf_size);
            delete[] info->buf;
            info->buf = buf_new;
            info->buf_size = size_new;
        }
    }
}

HTTP_PRE_STAT Http::prase_data(Prase_Info* info) {
    switch (info->prase_stat) {
    case HTTP_CONTNET: 
        return prase_content(info);
    default: 
        int stat = prase_body(info);
        if(stat == HTTP_ERROR) break;
        if(stat == HTTP_OPEN) return HTTP_OPEN;
        if(stat == HTTP_FINISH && info->res.which == POST) {
            info->prase_stat = HTTP_CONTNET;
            return prase_content(info);
        }
        return HTTP_FINISH;
    }
    return HTTP_ERROR;
}

HTTP_PRE_STAT Http::prase_body(Prase_Info* info) {
    LINE_STAT line_stat = LINE_OPEN;
    PRASE_HEAD_STAT head_stat = PRASE_HEAD_OPEN;
    while((line_stat = prase_line(info)) == LINE_FINSH) {
        switch (info->prase_stat) {
        case HTTP_REQ:
            if(prase_request(info)) {
                 info->prase_stat = HTTP_HEAD;
                 break;
            }
            goto req_error;
        case HTTP_HEAD:
            if((head_stat == prase_head(info)) == PRASE_HEAD_FINISH)
                goto prase_finsh;
            else if(head_stat == PRASE_HEAD_OPEN) 
                continue;
            goto head_error;
        default:break;
        }
    }
    if(line_stat == LINE_OPEN) return HTTP_OPEN;
    if(line_stat == LINE_ERROR) {
        Logger::get_logger()->warn("recv a error http line, try to close fd.");
        close_event(info->fd);
        return HTTP_ERROR;
    }
prase_finsh:
    return HTTP_FINISH;

req_error:
    Logger::get_logger()->warn("recv a error request, try to close fd");
    close_event(info->fd);
    return HTTP_ERROR;

head_error:
    Logger::get_logger()->warn("recv a error head, try to close fd");
    close_event(info->fd);
    return HTTP_ERROR;
}

HTTP_PRE_STAT Http::prase_content(Prase_Info* info) {
    char* buf = info->buf;
    int start_index = info->line_start_index;
    auto real_length = info->end_index - start_index;
    auto content_length = info->res.content_length;
    if(real_length < content_length) return HTTP_OPEN;
    int fd = open(info->res.file.c_str(), O_CREAT | O_WRONLY, 0644);
    write(fd, buf + start_index, content_length);

    Logger::get_logger()->info("successsfully write file %s from fd %d",
        info->res.file.c_str(), info->fd);
    close(fd);
    return HTTP_FINISH;
}

LINE_STAT Http::prase_line(Prase_Info* info) {
    char* buf = info->buf;
    int index = info->prase_index;
    int end = info->end_index;
    info->line_start_index = info->line_end_index;
    for(; index < end; ++index) {
        if(buf[index] == '\r') {
            if(index + 1 < end && buf[index + 1] == '\n') {
                info->prase_index = index + 2;
                info->line_end_index = info->prase_index;
                return LINE_FINSH;
            } else if(index + 1 == end) {
                return LINE_OPEN;
            } else return LINE_ERROR;
        }
        if(buf[index] == '\n') {
            if(buf[index - 1] == '\r') {
                info->prase_index = index + 1;
                info->line_end_index = info->prase_index;
                return LINE_FINSH;
            } else {
                return LINE_ERROR;
            }
        }
    }

    return LINE_OPEN;
}

bool Http::prase_request(Prase_Info* info) {
    int start = info->line_start_index;
    int end = info->line_end_index;
    auto& res = info->res;
    char* buf = info->buf;
    if(strncasecmp(buf + start, "GET", 3) == 0) {
        res.which = GET;
        start += 3;
    } else if(strncasecmp(buf + start, "POST", 4) == 0) {
        res.which = POST;
        start += 4;
    } else if(strncasecmp(buf + start, "OPTIONS", 7) == 0) {
        res.which = OPTIONS;
        start += 7;
    } else return false;

    while(buf[start] == ' ' || buf[start] == '\t') {
        ++start;
        if(start >= end)
            return false;
    }
    if(res.which == GET)res.file = "./html";
    else if(res.which == POST) res.file = ("./html/resource");
    while(start < end && buf[start] != ' ' && buf[start] != '\t') {
        res.file += buf[start++];
    }
    if(res.file == "./html/") res.file.append("index.html");
    while(buf[start] == ' ' || buf[start] == '\t') {
        ++start;
        if(start >= end)
            return false;
    }

    if(strncasecmp(buf + start, "HTTP/1.1\r\n", 10) != 0) {
        return false;
    }
    return true;
}

PRASE_HEAD_STAT Http::prase_head(Prase_Info* info) {
    
    if(strncmp(info->buf + info->line_start_index, "\r\n", 2) == 0) {
        info->line_start_index += 2;
        return PRASE_HEAD_FINISH;
    }

    char* buf = info->buf;
    int start_index = info->line_start_index;
    int end_index = info->line_end_index;

    if(strncasecmp(buf + start_index,"Content-Length:", 15) == 0) {
        start_index += 15;
        info->res.content_length = std::atoi(buf + start_index);
    } else if(strncasecmp(buf + start_index, "Content-Name:", 13) == 0) {
        start_index += 13;
        while(buf[start_index] == ' ') ++start_index;
        auto& file = info->res.file;
        file.append(std::string(buf + start_index, end_index - start_index -2));
    }
    return PRASE_HEAD_OPEN;
}

void Http::send_msg(int fd, const std::string& path) {
    std::string buf;
    buf.reserve(BUFSIZ);
    struct stat path_stat;
    int ret = ::stat(path.c_str(), &path_stat);
    int length = 0;
    if(ret < 0) {
        if(errno == ENOENT) {
            append_request(_404_NOT_FOUND, buf);
            write(fd, buf.c_str(), buf.size());
            return;
        }
        Logger::get_logger()->error("function stat error, the errno is %d", errno);
        return;
    }
    append_request(_200_OK, buf);
    buf.append("Content-Length: \r\n");
    int spl_pos = buf.size() - 2;

    if(S_ISDIR(path_stat.st_mode)) {
        buf.append("Content-Type: text/html\r\n\r\n");
        length = append_dir(buf, path);
    } else if(S_ISREG(path_stat.st_mode)) {
        append_file_type(path, buf);
        buf.append("\r\n");
        length = append_file(buf, path);
    }
    buf.insert(spl_pos, std::to_string(length));
    write(fd, buf.c_str(), buf.size());
}

int Http::append_file(std::string& buf, const std::string& file) {
    int length = 0;

    std::fstream fs(file, std::ios::in | std::ios::binary);
    fs.seekg(0, fs.end);
    length = fs.tellg();
    fs.seekg(0, fs.beg);

    char* f_buf = new char[length]{ 0 };

    fs.read(f_buf, length);
    buf.append(f_buf, f_buf + length);

    delete [] f_buf;

    fs.close();
    return length;
}

std::string get_template_file(const std::string& file) {
    std::string buf;
    std::fstream s(file, std::ios::in);
    int length = 0;
    s.seekg(0, s.end);
    length = s.tellg();
    s.seekg(0, s.beg);

    char* t_buf = new char[length + 1];

    s.read(t_buf, length);
    buf.append(t_buf, t_buf + length);
    delete[] t_buf;
    s.close();
    return buf;
}

const std::string& get_dir_html_template_start() {
    static std::string file = get_template_file("./html/file_list_base_start");
    return file;
}

const std::string& get_dir_html_template_end() {
    static std::string file = get_template_file("./html/file_list_base_end");
    return file;
}

int Http::append_dir(std::string& buf, const std::string& path) {
    int length = 0;
    static const char* base_string = "<tr><td><a href=\"%s\">%s</a></td><td>%s</td></tr>";
    const std::string& start = get_dir_html_template_start();
    const std::string& end = get_dir_html_template_end();
    length += (start.size() + end.size());
    buf.append(start);
    DIR* dir = opendir(path.c_str());
    dirent* file;
    char* html_str = new char[BUFSIZ]{ 0 };
    char* t_path = new char[128]{ 0 };

    std::vector<std::string> html_vec;
    while((file = readdir(dir)) != NULL) {
        // if(strcmp(file->d_name, "..") == 0) {
        //     if(path == "./html/resource/") continue;
        // }

        memset(t_path, 0, 128);
        memcpy(t_path, "./", 2);
        if(path != "./html/resource/") {
            strcat(t_path, path.c_str() + 16);
            strcat(t_path, "/");
        }
        strcat(t_path, file->d_name);
        memset(html_str, 0, BUFSIZ);
        if(file->d_type == 4) {
            sprintf(html_str, base_string, t_path, file->d_name, "文件夹");
        } else {
            sprintf(html_str, base_string, t_path, file->d_name, "文件");
        }
        int t_len = strlen(html_str);
        length += t_len;
        html_vec.emplace_back(std::string(html_str, html_str + t_len));
    }
    delete [] html_str;
    delete [] t_path;
    closedir(dir);
    std::sort(html_vec.begin(), html_vec.end());
    for(const auto& s : html_vec) {
        buf.append(std::move(s));
    }
    buf.append(end);
    return length;
}

void Http::send_200_OK(int fd) {
    std::string buf;
    append_request(_200_OK, buf);
    buf.append("Content-Length: 0\r\n")
        .append("Access-Control-Allow-Origin: *\r\n")
        .append("\r\n");
    write(fd, buf.c_str(), buf.size());
    return;
}

void Http::append_request(HTTP_STAT stat, std::string& buf) {
    switch (stat) {
    case _200_OK:
        buf.append("HTTP/1.1 200 OK\r\n");break;
    case _404_NOT_FOUND:
        buf.append("HTTP/1.1 404 NOT FOUND\r\n");break;
    default:break;
    }
}

std::unordered_map<std::string, std::string> Http::file_type2content = {
    {"html", "text/html"},
    {"jpeg", "image/jpeg"}, 
    {"css", "text/css"},
    {"js", "application/javascript"}
};

void Http::append_file_type(const std::string& file, std::string& buf) {
    std::string s = file.substr(file.find_last_of(".") + 1);
    buf.append("Content-Type: ")
        .append(file_type2content[s]).append("\r\n");
}