#include <cstdint>
#include <cstddef>
#include <queue>

enum Type{ENQ,DEQ};

enum LockType{HEAD=1, TAIL=2, BATCH=3};

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
	Node* next;

	Node(void* Nitem, Node* Nnext){
		item = Nitem;
		next = Nnext;
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