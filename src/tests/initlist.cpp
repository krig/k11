#include <initializer_list>
#include <iostream>

int main() {
    for (int i : { 5, 7, 3 }) {
        std::cout << i << " ";
    }
    std::cout << std::endl;
}
