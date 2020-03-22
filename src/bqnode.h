
template <typename T> 

class BQNode {
private:
    T data;
    BQNode *next;
    bool isDummy;
    
public:
    BQNode(T d);
    
    BQNode next();
    void setNext(BQNode *n);
    
    T getData();
    
    bool isDummy();
    void makeDummy();
}
