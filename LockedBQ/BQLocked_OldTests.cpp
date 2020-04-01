#include "BQStructs.h"
#include "mrlock.h"
#include "bitset.h"
#include <iostream>
#include <chrono>
#include <thread> 
#include <stdio.h> 
#include <stdlib.h>   
#include <time.h> 
#include <cstddef>
#include <vector>

thread_local ThreadData threadData;
PtrCnt SQHead;
PtrCnt SQTail;
MRLock<uint32_t> *lock;

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


void enqueue(void * item) {
	uint32_t index = (*lock).Lock(TAIL);
	//std::cout << "Lock TAIL" << std::endl;

	if(threadData.opsQ.empty()) {
		Node* newNode = new Node(item, NULL);
		SQTail.node->next = newNode;
		SQTail.node = newNode;
		SQTail.cnt++;
	} else {
		futureEnq(item);
		execute();
	}

	(*lock).Unlock(index);
}
void * dequeue() {

	uint32_t index = (*lock).Lock(HEAD);
	void* result;

	if(threadData.opsQ.empty()) {

		Node* oldHead = SQHead.node;
		Node* headNextNode = oldHead->next;

		if(headNextNode == NULL) {
			(*lock).Unlock(index);
			return NULL;
		}
		result = headNextNode->item;

		SQHead.node = headNextNode;
		SQHead.cnt++;

		delete oldHead;
	} else {

		Future* future = futureDeq();
		execute();
		result = future->result;

		delete future;
	}

	(*lock).Unlock(index);
	return result;

}
Future* futureEnq(void * item) {

	Node* newNode = new Node(item, NULL);

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
Future* futureDeq() {

	FutureOp* op = new FutureOp(DEQ);
	threadData.opsQ.push(op);
	threadData.deqsNum++;

	if(threadData.deqsNum > threadData.enqsNum)
		threadData.excessDeqsNum++;

	return op->future;

}
void execute() {
	uint32_t index = (*lock).Lock(BATCH);
	Node* oldHead = SQHead.node;
	executeBatch();
	(*lock).Unlock(index);

	pairFuturesWithResults(oldHead);

	resetThread();
}

void executeBatch() {

	PtrCnt oldHead = SQHead;
	PtrCnt oldTail = SQTail;
	unsigned int oldQueueSize = SQTail.cnt - SQHead.cnt;

	SQTail.node->next = threadData.enqsHead;
	SQTail.node = threadData.enqsTail;
	SQTail.cnt += threadData.enqsNum;

	updateHead(oldHead, oldTail, oldQueueSize);

}

void updateHead(PtrCnt oldHead, PtrCnt oldTail, int oldSize) {

	unsigned int successfullDeqsNum = threadData.deqsNum;
	if(threadData.excessDeqsNum > oldSize) {
		successfullDeqsNum -= threadData.excessDeqsNum - oldSize;
	}

	if(successfullDeqsNum == 0)
		return;

	if(successfullDeqsNum < oldSize) {
		SQHead.node = GetNthNode(oldHead.node, successfullDeqsNum);
	} else
		SQHead.node = GetNthNode(oldTail.node, successfullDeqsNum - oldSize);

	SQHead.cnt += successfullDeqsNum;

}

Node* GetNthNode(Node* node, unsigned int n) {
	for(int i = 0; i < n; i++)
		node = node->next;
	return node;
}

void pairFuturesWithResults(Node* oldHeadNode) {
	Node* nextEnqNode = threadData.enqsHead;
	Node* currentHead = oldHeadNode;
	bool noMoreSuccessfulDeqs = false;

	while(!threadData.opsQ.empty()) {
		FutureOp* op = threadData.opsQ.front();
		threadData.opsQ.pop();
		if(op->type == ENQ)
			nextEnqNode = nextEnqNode->next;

		else {

			if(noMoreSuccessfulDeqs ||

			   currentHead->next == nextEnqNode)

				op->future->result = NULL;
			else {

				Node* lastHead = currentHead;
				currentHead = currentHead->next;

				if(currentHead == threadData.enqsTail)
					noMoreSuccessfulDeqs = true;

				op->future->result = currentHead->item;
				delete lastHead;

			}
		}

		op->future->isDone = true;
		delete op;
	}
}

void resetThread() {
	threadData.enqsHead = NULL;
	threadData.enqsTail = NULL;
	threadData.enqsNum = 0;
	threadData.deqsNum = 0;
	threadData.excessDeqsNum = 0;

}

void init() {
	lock = new MRLock<uint32_t>(2);

	SQHead.cnt = 0;
	SQTail.cnt = 0;

	Node* sentinal = new Node(0, NULL);

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

void test(int** values) {
	for(int i = 0; i < 75; i++) {
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
	return;
}

int main() {

	const int valNum = 10;
	const int numThreads = 4;
	
	auto start = std::chrono::high_resolution_clock::now();
	
	init();
	resetThread();

	int* values[valNum];
	for(int i = 0; i < valNum; i++) {
		values[i] = new int((i+1) * 10);
	}
	for(int i = 0; i < 100; i++) {
		enqueue(values[rand()%10]);
	}

	std::thread threads[4];
	for(int i = 0; i < numThreads; i++) {
		threads[i] = std::thread(test, values);
	}
	for(int i = 0; i < numThreads; i++) {
		threads[i].join();
	}

	std::cout << std::endl;

	auto end = std::chrono::high_resolution_clock::now();

	double time_taken =
		std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

	std::cout << "Time taken by program is : " << std::fixed
		<< time_taken;
	std::cout << " microsec" << std::endl;

}
