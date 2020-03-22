#include "BQStructs.h"
#include <iostream> 

thread_local ThreadData threadData;
PtrCnt SQHead;
PtrCnt SQTail;

//Public Functions
void enqueue(void * item);
void* dequeue();
Future* futureEnq(void * item);
Future* futureDeq();
void execute();

//Private Functions
void executeBatch();
void updateHead(PtrCnt oldHead, PtrCnt oldTail, int oldSize);
void pairFuturesWithResults(Node* oldHead);
Node* GetNthNode(Node* node, uint n);


//Initializers
void init();
void resetThread();


void enqueue(void * item){

	if (threadData.opsQ.empty()){
		Node* newNode = new Node(item, NULL);
		SQTail.node->next = newNode;
		SQTail.node = newNode;
		SQTail.cnt++;
	}
	else{
		futureEnq(item);
		execute();
	}
}
void * dequeue(){
	
	if (threadData.opsQ.empty()){

		Node* oldHead = SQHead.node;
		Node* headNextNode= oldHead->next;

		if (headNextNode == NULL)
			return NULL;

		void* result = headNextNode->item;
		
		SQHead.node = headNextNode;
		SQHead.cnt++;

		delete oldHead;
		return result;
	}
	else{
		
		Future* future = futureDeq();
		execute();
		void* res = future->result;

		delete future;
		return res;
	}

}
Future* futureEnq(void * item){

	Node* newNode = new Node(item,NULL);

	if(threadData.enqsTail == NULL)
		threadData.enqsHead = newNode;
	else
		threadData.enqsTail->next = newNode;
	threadData.enqsTail = newNode;

	FutureOp* op = new FutureOp(ENQ);
	threadData.opsQ.push(op);

	threadData.enqsNum++;

	return op->future;

}
Future* futureDeq(){

	FutureOp* op = new FutureOp(DEQ);
	threadData.opsQ.push(op);
	threadData.deqsNum++;

	if(threadData.deqsNum > threadData.enqsNum)
		threadData.excessDeqsNum++;

	return op->future;

}
void execute(){

	Node* oldHead = SQHead.node;

	executeBatch();
	pairFuturesWithResults(oldHead);

	resetThread();

}

void executeBatch(){

	PtrCnt oldHead = SQHead;
	PtrCnt oldTail = SQTail;
	uint oldQueueSize = SQTail.cnt - SQHead.cnt;

	SQTail.node->next = threadData.enqsHead;
	SQTail.node = threadData.enqsTail;
	SQTail.cnt += threadData.enqsNum;

	updateHead(oldHead,oldTail,oldQueueSize);

}

void updateHead(PtrCnt oldHead, PtrCnt oldTail, int oldSize){
	
	uint successfullDeqsNum = threadData.deqsNum;
	if (threadData.excessDeqsNum > oldSize){
		successfullDeqsNum -= threadData.excessDeqsNum - oldSize;
	}

	if(successfullDeqsNum == 0)
		return;

	if(successfullDeqsNum < oldSize){
		SQHead.node = GetNthNode(oldHead.node,successfullDeqsNum);	
	}
	else 
		SQHead.node = GetNthNode(oldTail.node,successfullDeqsNum-oldSize);
	
	SQHead.cnt += successfullDeqsNum;

}

Node* GetNthNode(Node* node, uint n){
	for (int i = 0; i <  n; i++)
 		node = node->next;
	return node;
}

void pairFuturesWithResults(Node* oldHeadNode){
	Node* nextEnqNode = threadData.enqsHead;
	Node* currentHead = oldHeadNode;
	bool noMoreSuccessfulDeqs = false;

	while(!threadData.opsQ.empty()){
		FutureOp* op = threadData.opsQ.front();
		threadData.opsQ.pop();
		if (op->type == ENQ)
			nextEnqNode = nextEnqNode->next;

		else{

			if (noMoreSuccessfulDeqs ||

				currentHead->next == nextEnqNode)
				
				op->future->result = NULL;
			else{

				Node* lastHead = currentHead;
				currentHead = currentHead->next;

				if (currentHead == threadData.enqsTail)
					noMoreSuccessfulDeqs = true;

				op->future->result = currentHead->item;
				delete lastHead;

			}
		}

		op->future->isDone = true;
		delete op;
	}


}

void resetThread(){
	threadData.enqsHead = NULL;
	threadData.enqsTail = NULL;
	threadData.enqsNum = 0;
	threadData.deqsNum = 0;
	threadData.excessDeqsNum = 0;

}

void init(){

	SQHead.cnt = 0;
	SQTail.cnt = 0;

	Node* sentinal = new Node(0,NULL);

	SQTail.node = sentinal;
	SQHead.node = sentinal;
}

int main(){

	init();
	resetThread();

	int * a = new int(10);
	int * b = new int(20);
	int * c = new int(30);

	std::cout << "Adresses:" << std::endl;
	std::cout << a << std::endl;
	std::cout << b << std::endl;
	std::cout << c << std::endl;

	enqueue(a);
	enqueue(b);
	enqueue(c);

	Future* f[8];

	f[0] = futureDeq();
	f[1] = futureDeq();
	f[2] = futureDeq();
	f[3] = futureDeq();
	futureEnq(a);
	f[4] = futureDeq();
	futureEnq(b);
	futureEnq(b);
	f[5] = futureDeq();
	f[6] = futureDeq();
	f[7] = futureDeq();

	execute();

	std::cout << std::endl;

	for (Future* i : f)
		std::cout << i->result << std::endl;



}