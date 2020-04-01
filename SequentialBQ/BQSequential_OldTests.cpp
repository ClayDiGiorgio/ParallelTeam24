#include "BQStructs.h"
#include <iostream> 
#include <chrono>

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
Node* GetNthNode(Node* node, unsigned int n);


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
	unsigned int oldQueueSize = SQTail.cnt - SQHead.cnt;

	SQTail.node->next = threadData.enqsHead;
	SQTail.node = threadData.enqsTail;
	SQTail.cnt += threadData.enqsNum;

	updateHead(oldHead,oldTail,oldQueueSize);

}

void updateHead(PtrCnt oldHead, PtrCnt oldTail, int oldSize){
	
	unsigned int successfullDeqsNum = threadData.deqsNum;
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

Node* GetNthNode(Node* node, unsigned int n){
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

void performBatch(int** values) {
	for(int i = 0; i < 10; i++) {
		switch(rand() % 2) {
			case 0:
				futureDeq();
				break;
			case 1:
				futureEnq(values[rand() % 10]);
				break;
		}
	}
	execute();
}

double runTests(/*const int numThreads*/) {

	const int valNum = 10;

	auto start = std::chrono::high_resolution_clock::now();

	init();
	resetThread();

	int* values[valNum];
	for(int i = 0; i < valNum; i++) {
		values[i] = new int((i + 1) * 10);
	}
	for(int i = 0; i < 100; i++) {
		enqueue(values[rand() % 10]);
	}

	for(int i = 0; i < 300; i++) {
		int operation = rand() % 4;
		int* value = values[rand() % 10];
		switch(rand() % 4) {
			case 0:
				enqueue(value);
				break;
			case 1:
				dequeue();
				break;
			case 2:
				performBatch(values);
				break;
		}
	}

	auto end = std::chrono::high_resolution_clock::now();

	double time_taken =
		std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

// 	std::cout << "Time taken by program is : " << std::fixed
// 		<< time_taken;
// 	std::cout << " microsec" << std::endl;

    return time_taken;
}

int main(void) {
    //runTests(4);
    
    srand(time(NULL));
    const int averageOver = 10;
    const int numTests = 4;
    double temp;
    int t, i;
    
    std::cout << "Num Threads\t"<<std::fixed<<"Time Taken (microseconds)" << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
    for (t = 0; t < numTests; t++) {
        temp = 0;
        for (i = 0; i < averageOver; i++)
            temp += runTests(/*t+1, 20000, 30*/) / (double)averageOver;
        std::cout << "          " << t+1 << ":\t" << std::fixed << temp << std::endl;
    }
}
