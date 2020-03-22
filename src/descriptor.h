template <typename T> 

class Descriptor {
private:
    bool isEnq;
    T data;
public:    
    Descriptor(bool b, T d);
    
    bool isEnq();
    bool isDeq();
    T getData();
}
