#include descriptor.h

template <typename T> 

class Future {
private:
    std::vector<Descriptor<T>> operations;
public:
    int size();
    Descriptor<T> getOperation(int index);
    void append(Descriptor<T> d);
}
