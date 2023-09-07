#pragma once
#include <iostream>
#define debug(format) \
    do { \
        printf("%s\n", format); \
    } while(0)

#define debug_format(format, ...) \
    do { \
        printf(format"\n", __VA_ARGS__); \
    } while(0)

