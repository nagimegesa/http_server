#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

constexpr const char* IP_ADDRESS = "127.0.0.1";
constexpr short PORT = 8800;

int main() {

    int tar_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = inet_addr(IP_ADDRESS);
    addr.sin_port = htons(PORT);
    addr.sin_family = AF_INET;

    int res = connect(tar_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    
    if(res < 0) {
        std::cout << "connect error" << std::endl;
        return 0;
    }

	std::cout << "connect successfully" << std::endl;
    constexpr const char* msg = "hello server";
    write(tar_fd, msg, strlen(msg));
    char buf[40] = { 0 };
//	shutdown(tar_fd, SHUT_RD);
    read(tar_fd, buf, sizeof(buf));
	std::cout << "recv from server: ";
    std::cout << buf << std::endl;
	close(tar_fd);
    return 0;
}
