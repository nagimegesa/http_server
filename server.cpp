#include "./base/http.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>

constexpr const char* IP_ADDRESS = "192.168.31.59";
//constexpr const char* IP_ADDRESS = "127.0.0.1";
constexpr short PORT = 8800;

int main() {
    Http base("./html/index.html");
    base.init();

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = inet_addr(IP_ADDRESS);
    addr.sin_port = htons(PORT);
    addr.sin_family = AF_INET;

    base.bind_listen(* reinterpret_cast<sockaddr*>(&addr), 5);

    base.run();

    return 0;
}