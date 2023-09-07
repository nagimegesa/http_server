#include "../utils/safe_queue.hpp"
#include <iostream>

int main() {
    Sort_List<int> list;

    
    list.emplace(43);
    list.emplace(34);
    list.emplace(33);
    list.emplace(54);
    std::cout << list.size() << std::endl;

    //list.print();

    list.change(34, 53);

    list.print();

    return 0;
}