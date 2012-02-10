#include <future>
#include <iostream>

int calculate_the_answer_to_LtUaE()
{
  return 42;
}

void do_stuff()
{
  std::cout << "doing stuff" << std::endl;
}

int main()
{
  std::future<int> the_answer=std::async(calculate_the_answer_to_LtUaE);
  do_stuff();
  std::cout<<"The answer to life, the universe and everything is "
    <<the_answer.get()<<std::endl;
}
