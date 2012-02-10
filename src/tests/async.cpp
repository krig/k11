#include <string>
#include <vector>
#include <algorithm>
#include <future>
#include <iostream>

std::string flip(std::string s) {
    std::reverse(s.begin(), s.end());
    return s;
}

int main() {
    std::vector<std::future<std::string>> v;

    v.push_back(std::async([](){ return flip(" ,olleH"); }));
    v.push_back(std::async([](){ return flip(" evitaNgnioG"); }));
    v.push_back(std::async([](){ return flip("\n!2102"); }));

    for (auto& e : v) {
        std::cout << e.get();
    }
}
