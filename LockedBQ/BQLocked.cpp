#include "BQStructs.h"
#include "mrlock.h"
#include "bitset.h"
#include <iostream>
#include <thread> 
#include <stdio.h> 
#include <stdlib.h>   
#include <time.h> 
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

	if(threadData.opsQ.empty()) {
		Node* newNode = new Node(item, NULL);
		SQTail.node->next = newNode;
		SQTail.node = newNode;
		SQTail.cnt++;
		(*lock).Unlock(index);
	} else {
		futureEnq(item);
		(*lock).Unlock(index);
		execute();
	}

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

		(*lock).Unlock(index);
	} else {

		Future* future = futureDeq();
		(*lock).Unlock(index);
		execute();
		result = future->result;

		delete future;
	}

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

// =========================
//
//       Testing
//
// =========================

#include <chrono>
#include <thread> 
#include <stdio.h> 
#include <stdlib.h>   
#include <time.h> 
#include <cstddef>
#include <random>
#include <assert.h>

void test(int** values, int numOpsPerThread, int numOpsPerBatch) {

	//random number generator
	std::random_device rdev;
	std::mt19937 rng(rdev());
	std::uniform_real_distribution<double> dist(0, 1);

	//initialize percentages of operations - must add to 1.0
	double enqpercent = 0.01;
	double deqpercent = 0.01;
	double fenqpercent = 0.49;
	double fdeqpercent = 0.49;

	//set max value for each percentage range
	double enqmark = enqpercent;
	double deqmark = enqmark + deqpercent;
	double fenqmark = deqmark + fenqpercent;
	double fdeqmark = fenqmark + fdeqpercent;

	//ensure percentages add up to 1.0
	assert(fdeqmark == 1.0);

	//test random operations
	for(int i = 0; i < numOpsPerThread; i++) {
		double randNum = dist(rng);
		int operation = rand() % 4;
		int* value = values[rand() % 10];

		if(randNum < enqmark) {
			enqueue(value);
		} else if(randNum < deqmark) {
			dequeue();
		} else if(randNum < fenqmark) {
			futureEnq(value);
		} else if(randNum < fdeqmark) {
			futureDeq();
		}
	}
	execute();
	return;
}

double runTest(const int numThreads, int numOpsPerThread, int numOpsPerBatch) {
	const int valNum = 10;

	auto start = std::chrono::high_resolution_clock::now();

	init();
	resetThread();

	int* values[valNum];
	for(int i = 0; i < valNum; i++) {
		values[i] = new int((i + 1) * 10);
	}
	for(int i = 0; i < 100; i++) {
		enqueue(values[rand()%valNum]);
	}

	std::thread threads[numThreads];
	for(int i = 0; i < numThreads; i++) {
		threads[i] = std::thread(test, values, numOpsPerThread, numOpsPerBatch);
	}
	for(int i = 0; i < numThreads; i++) {
		threads[i].join();
	}

	auto end = std::chrono::high_resolution_clock::now();

	double time_taken =
		std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

	return time_taken;
}

int main(void) {
	srand(time(NULL));
	const int averageOver = 10;
	const int numTests = 4;
	double temp;
	int t, i;

	std::cout << "Num Threads\t" << std::fixed << "Time Taken (microseconds)" << std::endl;
	std::cout << "-----------------------------------------" << std::endl;
	for(t = 0; t < numTests; t++) {
		temp = 0;
		for(i = 0; i < averageOver; i++)
			temp += runTest(t + 1, 100, 30) / (double) averageOver;
		std::cout << "          " << t + 1 << ":\t" << std::fixed << temp << std::endl;
	}
}
