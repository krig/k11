#include "join.hpp"
#include <iostream>
#include <vector>

using namespace std;
using namespace util;

int main() {
    vector<const char*> v {"a", "b", "c", "d"};
    int a[] {1, 2, 3, 4, 5, 6, 7, 8, 9};
    cout << join(v) << std::endl
         << join(" + ", a, a+9) << std::endl;
}
