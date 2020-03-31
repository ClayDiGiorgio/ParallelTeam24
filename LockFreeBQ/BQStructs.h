#include <cstdint>
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
  unsigned int cnt;
};

struct Ann {
    BatchRequest batchReq;
    PtrCnt oldHead;
    PtrCnt oldTail;
    
    Ann(BatchRequest newB, PtrCnt tail){
        batchReq = newB;
        oldTail = tail;
        oldTail.node = NULL;
    }
};

union PtrCntOrAnn {
  PtrCnt ptrCnt;
  struct {
    unsigned int tag;
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
