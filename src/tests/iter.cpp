#include <unordered_map>
#include <map>
#include <string>
#include <iostream>

using namespace std;

#include "mapping.hpp"

int main() {
    map<string, string> ms {
        { "hello", "world" },
        { "foo", "bar" }
    };
    unordered_map<int, float> ms2 {
        { 3, 2.3f }, {4, 9.1f}, {-339, 0.f}
    };

    cout << "keys:\n";
    for (auto& k : mapping::keys(ms))
        cout << k << "\n";
    cout << "values\n";
    for (auto& v : mapping::values(ms))
        cout << v << "\n";

    cout << "keys:\n";
    for (auto& k : mapping::keys(ms2))
        cout << k << "\n";
    cout << "values\n";
    for (const auto& v : mapping::values(ms2))
        cout << v << "\n";

    // can mutate values (but not keys)
    for (auto& v : mapping::values(ms2))
        v -= 1000.f;

    // values have changed
    for (auto& v : mapping::values(ms2))
        cout << v << "\n";
}
