#include "log.h"
#include <algorithm>
#include <cstddef>
#include <ctime>
#include <deque>
#include <functional>
#include <iterator>
#include <queue>
#include <mutex>
#include <utility>
#include <iostream>

template <typename T>
class Safe_Queue {
private:
    std::mutex lock;
    std::queue<T> queue;

public:
    using size_type = typename std::queue<T>::size_type;
    
public:
    void emplace(T&& args...) {
        lock.lock();
        queue.emplace(args);
        lock.unlock();
    }

    bool pop(T& data) {
        bool res = false;
        lock.lock();
        if(!queue.empty()) { 
            data = queue.front();
            queue.pop();
            res = true;
        }
        lock.unlock();
        return res;
    }
    size_type size() {
        return queue.size();
    }
};

template <typename T, typename Compare = std::less<T>>
class Sort_List {
private:
    struct List_Node {
        T elem;
        List_Node* next;

        List_Node(T&& t) {
            elem = t;
            next = nullptr;
        }

        List_Node() {
            next = nullptr;
        }
    };

    List_Node* head;
    size_t node_cnt;
    Compare compare;
public:
    template <class Arg>
    void emplace(Arg&& arg) {
        auto t = head->next, last = head;
        while(t) {
            if(!compare(t->elem, arg))
                break;
            last = t;
            t = t->next;
        }

        List_Node* node = new List_Node(std::forward<T>(arg));

        if(t == nullptr) {
            last->next = node;
        } else {
            node->next = last->next;
            last->next = node;
        }
        ++node_cnt;
    }

    size_t size() const {
        return node_cnt;
    }  

    template <class Arg>
    void erase(Arg&& e) {
        auto t = head->next, last = head;
        while(t) {
            if(!compare(t->elem, e) 
                && !compare(e, t->elem)) {
                
                if(last == head) {
                    auto td = head->next;
                    head->next = td->next;
                    delete td;
                } else { 
                    last->next = t->next;
                    delete t;
                }
                --node_cnt;
                break;
            }
            last = t;
            t = t->next;
        }
    }
    template <class Arg>
    void change(Arg&& old, Arg&& n) {
        auto t = head->next, last = head;
        while(t) {
            if(!compare(t->elem, old) && !compare(old, t->elem)) 
                break;
            last = t;
            t = t->next;
        }
        if(!t) {
            Logger::get_logger()->error("Sort List change a empty element");
            return;
        }
        
        t->elem = n;
        last->next = t->next;
        t->next = nullptr;
        //print();


        auto tp = head->next;
        last = head;
        while(tp) {
            if(!compare(tp->elem, n)) break;
            last = tp;
            tp = tp->next;
        }

        if(tp == nullptr) {
            last->next = t;
        } else {
            t->next = tp;
            last->next = t;
        }

    }

    const T* front() {
        return &head->next->elem;
    }

    void pop_front() {
        if(head) {
            auto t = head;
            head = head->next;
            delete t;
            --node_cnt;
        }
    }

    Sort_List() : node_cnt(0) {
        head = new List_Node;
    }

    ~Sort_List() {
        while(head) {
            auto t = head->next;
            delete head;
            head = t;
        }
    }

    void print() {
        auto t = head->next;
        while(t) {
            std::cout << t->elem << " ";
            t = t->next;
        }
    }
};