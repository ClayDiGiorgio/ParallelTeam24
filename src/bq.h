#include bqnode.h
#include future.h

template <typename T> 

class BQ {
private:
    BQNode<T> *head;
    BQNode<T> *tail;
public:
    Future<T> prepDeque(Future<T> f);
    Future<T> prepEnque(Future<T> f, T data);
    
    BQNode<T> execute(Future<T> f);
}
