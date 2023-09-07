#pragma once

#include <map>
#include <mutex>
#include <utility>

template <typename Key, typename Value>
class Safe_Map {
    std::map<Key, Value> _map;
    std::mutex lock;

public:
    template<typename Arg>
    void emplace(Arg&& arg) {
        lock.lock();
        _map.emplace(std::forward(arg));
        lock.unlock();
    }

    template<typename Arg>
    void erase(Arg&& arg) {
        lock.lock();
        _map.erase(arg);
        lock.unlock();
    }

    auto find(const Key& k) {
        lock.lock();
        auto res = _map.find(k);
        lock.unlock();
        return res;
    }

    auto end() {
        lock.lock();
        return _map.end();
        lock.unlock();
    }

    Value& operator[](const Key& key) {
        lock.lock();
        return _map[key];
        lock.unlock();
    }

};