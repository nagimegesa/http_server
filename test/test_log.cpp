#include <iostream>
#include "../utils/log.h"

int main() {
    Logger* logger = Logger::get_logger();
    logger->error("here is a error log %d", 1);
    logger->warn("here is a warn log");
    logger->debug("here is a debug log");
    logger->info("here is a info log");
    return 0;
}