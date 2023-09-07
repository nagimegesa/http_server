#include <iostream>
#include <string>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

int main() {
    std::string path("../html/resource");
    struct stat file_stat;
    
    stat(path.c_str(), &file_stat);
    if(S_ISDIR(file_stat.st_mode)) {
        std::cout << "is dirent" << std::endl;
    }
    return 0;
}