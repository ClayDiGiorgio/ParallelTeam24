g++ -pthread BQLockFree.cpp -std=c++11 -latomic -g
gdb ./a.out
rm a.out
