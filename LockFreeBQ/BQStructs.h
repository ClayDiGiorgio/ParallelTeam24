#include <cstdint>
#include <cstddef>
#include <queue>
#include <atomic>

enum Type{ENQ,DEQ};

struct Future{
  void* result;
  bool isDone;
  
  Future(){
    result = NULL;
    isDone = false;
  }
};

struct Node{
  void* item;
  std::atomic <Node*> next;
  
  Node(void* Nitem, Node* Nnext){
    item = Nitem;
    next.store(Nnext);
  }
};

struct BatchRequest{
  Node* firstEnq;
  Node* lastEnq;
  unsigned int enqsNum;
  unsigned int deqsNum;
  unsigned int excessDeqsNum;
};

struct PtrCnt{
  Node* node;
  uintptr_t cnt;
};

struct Ann {
    BatchRequest batchReq;
    std::atomic <PtrCnt> oldHead;
    std::atomic <PtrCnt> oldTail;
    
    Ann(BatchRequest newB){
        batchReq = newB;
        PtrCnt tail {(Node*)0,40};
        PtrCnt head {(Node*)0,40};

        oldHead.store(head);
        oldTail.store(tail);
    }
};

union PtrCntOrAnn {
  PtrCnt ptrCnt;
  struct {
    uintptr_t tag;
    Ann* ann;
  } container;    
};

struct FutureOp{
    Type type;
    Future* future;
    
    FutureOp(Type nType){
        type = nType;
        future = new Future();
    }
};

struct ThreadData {
	std::queue<FutureOp*> opsQ;
	Node* enqsHead;
	Node* enqsTail;
	unsigned int enqsNum;
	unsigned int deqsNum;
	unsigned int excessDeqsNum;
};

struct UIntNode {
    unsigned int num;
    Node* node;
};
