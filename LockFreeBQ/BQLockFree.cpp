#include "BQStructs.h"
#include <iostream>
#include <atomic>

#define CAS compare_exchange_weak

thread_local ThreadData threadData;
std::atomic <PtrCntOrAnn> SQHead;
std::atomic <PtrCntOrAnn> SQTail;

Node* EMPTY = NULL;

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

//Initializers
void init();
void resetThread();

//Helpers for the garbage that is C++ Unions and Strict Typing
PtrCntOrAnn toPtr(Ann*& ann){
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
    
    Node* oldHead = SQHead.load().ptrCnt.node;
    
    BatchRequest bq {threadData.enqsHead,
	 threadData.enqsTail,
	 threadData.deqsNum,
	 threadData.excessDeqsNum};
    
    ExecuteBatch(bq);
    PairFuturesWithResults(oldHead);
    
    resetThread();

}

void EnqueueToShared(void* item){
    
    Node* newNode = new Node(item,NULL);
    while(true){
        PtrCnt tailAndCnt = SQTail.load().ptrCnt;
        PtrCntOrAnn oldTail = toPtr(tailAndCnt);
        if (tailAndCnt.node->next.CAS(EMPTY,newNode)){
            PtrCnt newTail {newNode,tailAndCnt.cnt + 1};
            SQTail.CAS(oldTail,toPtr(newTail));
            break;
        }
        PtrCntOrAnn head = SQHead;
        if (head.container.tag & 1 == 1)
            ExecuteAnn(head.container.ann);
        else{
            PtrCnt newTail {tailAndCnt.node->next.load(),tailAndCnt.cnt + 1};
            SQTail.CAS(oldTail,toPtr(newTail));
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
    
    Ann* ann = new Ann(batchRequest,SQTail.load().ptrCnt);
    PtrCnt oldHeadAndCnt;

    while(true){
        oldHeadAndCnt = HelpAnnAndGetHead();
        ann->oldHead = oldHeadAndCnt;
        PtrCntOrAnn oldPtr = toPtr(oldHeadAndCnt);
        if (SQHead.CAS(oldPtr,toPtr(ann)))
            break;
    }
    ExecuteAnn(ann);
    return oldHeadAndCnt.node;
     
}

void ExecuteAnn(Ann* ann){
    PtrCnt tailAndCnt, annOldTailAndCnt;
    while (true){
        tailAndCnt = SQTail.load().ptrCnt;
        annOldTailAndCnt = ann->oldTail;

        if (annOldTailAndCnt.node != NULL)
            break;

        tailAndCnt.node->next.CAS(EMPTY,ann->batchReq.firstEnq);

        if (tailAndCnt.node->next == ann->batchReq.firstEnq){
            ann->oldTail = annOldTailAndCnt = tailAndCnt;
            break;
        }
        else{
            PtrCnt newTail {tailAndCnt.node->next,tailAndCnt.cnt + 1};
            PtrCntOrAnn oldTail = toPtr(annOldTailAndCnt);
            SQTail.CAS(oldTail,toPtr(newTail));
        }
    }
    PtrCnt newTailAndCnt {ann->batchReq.lastEnq,annOldTailAndCnt.cnt + ann->batchReq.enqsNum};
    PtrCntOrAnn oldTail = toPtr(annOldTailAndCnt);
    SQTail.CAS(oldTail,toPtr(newTailAndCnt));
    UpdateHead(ann);
}

void UpdateHead(Ann* ann){
   
    uint oldQueueSize = ann->oldTail.cnt - ann->oldHead.cnt;
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
        newHeadNode = GetNthNode(ann->oldHead.node,successfulDeqsNum);
    else{
        newHeadNode = GetNthNode(ann->oldTail.node,successfulDeqsNum - oldQueueSize);
    }
    PtrCnt newHead {newHeadNode,ann->oldHead.cnt+successfulDeqsNum};
    PtrCntOrAnn ptrAnn = toPtr(ann);

    SQHead.CAS( ptrAnn, toPtr(newHead));    
}

Node* GetNthNode(Node* node, uint n){
    for (int i = 0; i <  n; i++){
        //std::cout << "ann " << node << std::endl;
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

    PtrCntOrAnn newHead, newTail;

    newHead.ptrCnt.cnt = 0;
    newTail.ptrCnt.cnt = 0;
    
    Node* sentinal = new Node(0,NULL);
    
    newTail.ptrCnt.node = sentinal;
    newHead.ptrCnt.node = sentinal;

    SQHead.store(newHead);
    SQTail.store(newTail);
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
    
    Future* f[6];
    
    f[0] = futureDeq();
    f[1] = futureDeq();
    f[2] = futureDeq();
    futureEnq(a);
    f[3] = futureDeq();
    f[4] = futureDeq();
    futureEnq(b);
    f[5] = futureDeq();
    /*
    futureEnq(b);
    f[5] = futureDeq();
    f[6] = futureDeq();
    f[7] = futureDeq();
    */
    execute();
    
    std::cout << std::endl;
    
     for (Future* i : f)
         std::cout << i->result << std::endl;   
    
}
