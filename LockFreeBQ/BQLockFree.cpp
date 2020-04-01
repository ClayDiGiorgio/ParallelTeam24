#include "BQStructs.h"
#include <iostream>
#include <atomic>

#define CAS compare_exchange_weak

thread_local ThreadData threadData;
std::atomic <PtrCntOrAnn> SQHead;
std::atomic <PtrCnt> SQTail;

//Public Functions
void enqueue(void * item);
void* dequeue();
Future* futureEnq(void * item);
Future* futureDeq();
void execute();

//Private Functions

void EnqueueToShared(void* item);
void* DequeueFromShared();
PtrCnt HelpAnnAndGetHead();
Node*  ExecuteBatch(BatchRequest batchRequest);
void ExecuteAnn(Ann* ann);
void UpdateHead(Ann* ann);
void PairFuturesWithResults(Node* oldHead);
Node* GetNthNode(Node* node, uint n);
UIntNode ExecuteDeqsBatch();
void PairFutureDeqsWithResults(Node* oldHEadNode, uint successfulDeqsNum);

//Initializers
void init();
void resetThread();

//Helpers for the garbage that is C++ Unions and Strict Typing
PtrCntOrAnn toPtr(Ann* ann){
    PtrCntOrAnn toReturn;
    toReturn.container.tag = 1;
    toReturn.container.ann = ann;
    return toReturn;
}
PtrCntOrAnn toPtr(Node* node, uint cnt){
    PtrCntOrAnn toReturn;
    toReturn.ptrCnt.node = node;
    toReturn.ptrCnt.cnt = cnt;
    return toReturn;
}
PtrCntOrAnn toPtr(PtrCnt ptrCnt){
    PtrCntOrAnn toReturn;
    toReturn.ptrCnt = ptrCnt;
    return toReturn;
}

void enqueue(void * item){

    if (threadData.opsQ.empty())
        EnqueueToShared(item);
    else{
        futureEnq(item);
        execute();
    }
}

void * dequeue(){
    
  if (threadData.opsQ.empty())
      return DequeueFromShared();
      
  else{
     
      Future* future = futureDeq();
      execute();
      void* res = future->result;
      
      return res;
  }
  
}
Future* futureEnq(void * item){
    
    Node* newNode = new Node(item,NULL);
    
    if(threadData.enqsTail == NULL)
        threadData.enqsHead = newNode;
    else
        threadData.enqsTail->next.store(newNode);
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

    if(threadData.enqsNum == 0){
        UIntNode res = ExecuteDeqsBatch();
        PairFutureDeqsWithResults(res.node,res.num);
    }
    else{

            
        Node* oldHead = HelpAnnAndGetHead().node;
        
        BatchRequest bq {threadData.enqsHead,
	     threadData.enqsTail,
	     threadData.enqsNum,
	     threadData.deqsNum,
	     threadData.excessDeqsNum};
        
        ExecuteBatch(bq);
        PairFuturesWithResults(oldHead);
    }
    resetThread();

}

void EnqueueToShared(void* item){
    
    Node* newNode = new Node(item,NULL);
    Node* empty;
    PtrCnt tailAndCnt;
    
    while(true){
        tailAndCnt = SQTail.load();
        empty = NULL;
        if (tailAndCnt.node->next.CAS(empty,newNode)){
            PtrCnt newTail {newNode,tailAndCnt.cnt + 1};
            SQTail.CAS(tailAndCnt,newTail);
            break;
        }
        PtrCntOrAnn head = SQHead;
        if ((head.container.tag & 1) == 1)
            ExecuteAnn(head.container.ann);
        else{
            PtrCnt newTail {tailAndCnt.node->next.load(),tailAndCnt.cnt + 1};
            SQTail.CAS(tailAndCnt,newTail);
        }
    }
}

void* DequeueFromShared(){
    while(true){
        PtrCnt headAndCnt = HelpAnnAndGetHead();
        Node* headNextNode = headAndCnt.node->next;
        if (headNextNode == NULL)
            return NULL;
        PtrCnt newHead {headNextNode,headAndCnt.cnt + 1};
        PtrCntOrAnn oldHead = toPtr(headAndCnt);
        if (SQHead.CAS(oldHead,toPtr(newHead)))
            return headNextNode->item;
    }
   
}
PtrCnt HelpAnnAndGetHead(){
    while (true){
        PtrCntOrAnn head = SQHead.load();
        
        if ((head.container.tag & 1) == 0)
            return head.ptrCnt;
        
        ExecuteAnn(head.container.ann);
    }
}
        

Node* ExecuteBatch(BatchRequest batchRequest){
    
    Ann* ann = new Ann(batchRequest);
    PtrCnt oldHeadAndCnt;

    while(true){
        oldHeadAndCnt = HelpAnnAndGetHead();
        ann->oldHead.store(oldHeadAndCnt);
        PtrCntOrAnn oldPtr = toPtr(ann->oldHead.load());
        if (SQHead.CAS(oldPtr,toPtr(ann)))
            break;
    }
    ExecuteAnn(ann);
    return oldHeadAndCnt.node;
     
}

void ExecuteAnn(Ann* ann){
    PtrCnt tailAndCnt, annOldTailAndCnt;
    Node* empty;
    while (true){
        tailAndCnt = SQTail.load();
        annOldTailAndCnt = ann->oldTail.load();

        if (annOldTailAndCnt.node != NULL)
            break;

        empty = NULL;
        tailAndCnt.node->next.CAS(empty,ann->batchReq.firstEnq);

        if (tailAndCnt.node->next.load() == ann->batchReq.firstEnq){
            annOldTailAndCnt = tailAndCnt;
            ann->oldTail.store(tailAndCnt);
            break;
        }
        else{
            PtrCnt newTail {tailAndCnt.node->next,tailAndCnt.cnt + 1};
            SQTail.CAS(annOldTailAndCnt,newTail);
        }
    }
    PtrCnt newTailAndCnt {ann->batchReq.lastEnq,annOldTailAndCnt.cnt + ann->batchReq.enqsNum};
    SQTail.CAS(annOldTailAndCnt,newTailAndCnt);
    UpdateHead(ann);
}

void UpdateHead(Ann* ann){
   
    uint oldQueueSize = ann->oldTail.load().cnt - ann->oldHead.load().cnt;
    uint successfulDeqsNum = ann->batchReq.deqsNum;
    Node* newHeadNode;
    
    if (ann->batchReq.excessDeqsNum > oldQueueSize)
        successfulDeqsNum -= ann->batchReq.excessDeqsNum - oldQueueSize;
    if (successfulDeqsNum == 0 ){
        PtrCntOrAnn ptrAnn = toPtr(ann);
        PtrCntOrAnn newHead = toPtr(ann->oldHead);
        SQHead.CAS(ptrAnn,newHead);
        return;
    }
  
    if(oldQueueSize > successfulDeqsNum)
        newHeadNode = GetNthNode(ann->oldHead.load().node,successfulDeqsNum);
    else{
        newHeadNode = GetNthNode(ann->oldTail.load().node,successfulDeqsNum - oldQueueSize);
    }
    PtrCnt newHead {newHeadNode,ann->oldHead.load().cnt+successfulDeqsNum};
    PtrCntOrAnn ptrAnn = toPtr(ann);
    
    SQHead.CAS( ptrAnn, toPtr(newHead));

}

Node* GetNthNode(Node* node, uint n){
    for (int i = 0; i <  n; i++){
        node = node->next.load();
    }
    return node;
}

void PairFuturesWithResults(Node* oldHeadNode){
    Node* nextEnqNode = threadData.enqsHead;
    Node* currentHead = oldHeadNode;
    bool noMoreSuccessfulDeqs = false;
    
    while(!threadData.opsQ.empty()){
        FutureOp* op = threadData.opsQ.front();
        threadData.opsQ.pop();
        if (op->type == ENQ)
            nextEnqNode = nextEnqNode->next.load();
        
        else{
            
            if (noMoreSuccessfulDeqs ||
              
                currentHead->next == nextEnqNode)        
                op->future->result = NULL;
            
            else{
                
                Node* lastHead = currentHead;
                currentHead = currentHead->next.load();
                
                if (currentHead == threadData.enqsTail)
                    noMoreSuccessfulDeqs = true;
                
                op->future->result = currentHead->item;
                
            }
        }   
        op->future->isDone = true;
    }
}

UIntNode ExecuteDeqsBatch(){
    PtrCnt oldHeadAndCnt;
    uint successfulDeqsNum;
    while(true){

        oldHeadAndCnt = HelpAnnAndGetHead();
        Node* newHeadNode = oldHeadAndCnt.node;

        successfulDeqsNum = 0;

        for(int i = 0; i < threadData.deqsNum; i++){
            Node* headNextNode = newHeadNode->next;
            if (headNextNode == NULL)
                break;
            successfulDeqsNum++;
            newHeadNode = headNextNode;
        }

        if (successfulDeqsNum == 0)
            break;
        PtrCnt newHead {newHeadNode,oldHeadAndCnt.cnt + successfulDeqsNum};
        PtrCntOrAnn oldHead = toPtr(oldHeadAndCnt);
        if (SQHead.CAS(oldHead,toPtr(newHead)))
            break;     
    }

    UIntNode toReturn {successfulDeqsNum,oldHeadAndCnt.node};
    return toReturn;
    
}
void PairFutureDeqsWithResults(Node* oldHeadNode, uint successfulDeqsNum){
    Node* currentHead = oldHeadNode;
    for(int i = 0; i < successfulDeqsNum; i++){
        currentHead = currentHead->next;
        FutureOp* op = threadData.opsQ.front();
        threadData.opsQ.pop();

        op->future->result = currentHead->item;
        op->future->isDone = true;
    }
    for(int i = 0; i < threadData.deqsNum - successfulDeqsNum; i++){
        FutureOp* op = threadData.opsQ.front();
        threadData.opsQ.pop();
        op->future->result = NULL;
        op->future->isDone = true;
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

    PtrCntOrAnn newHead;
    PtrCnt newTail;

    newHead.ptrCnt.cnt = 0;
    newTail.cnt = 0;
    
    Node* sentinal = new Node(0,NULL);
    
    newTail.node = sentinal;
    newHead.ptrCnt.node = sentinal;

    SQHead.store(newHead);
    SQTail.store(newTail);
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
	std::uniform_real_distribution<double> dist(0,1);

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
        values[i] = new int((i+1) * 10);
    }
    for(int i = 0; i < 100; i++) {
        enqueue(values[rand()%10]);
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
    const int threadCounts[] = {1 , 2, 3, 4};
    const int numTests = 4; // must be length of above array
    double temp;
    int t, i;
    
//     std::cout << "testing on 2\n";
//     runTest(2, 75, 10);
//     
//     std::cout << "ending test on 2\n";
    
    std::cout << "Num Threads\t"<<std::fixed<<"Time Taken (microseconds)" << std::endl;
    std::cout << "-----------------------------------------" << std::endl;
    for (t = 0; t < numTests; t++) {
        temp = 0;
        for (i = 0; i < averageOver; i++)
            temp += runTest(threadCounts[t], 20000, 30) / (double)averageOver;
        std::cout << "          " << threadCounts[t] << ":\t" << std::fixed << temp << std::endl;
    }
}
