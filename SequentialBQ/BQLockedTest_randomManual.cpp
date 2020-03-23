#include "BQLocked.h"

#include <thread> 
#include <stdio.h> 
#include <stdlib.h>   
#include <time.h> 

#include <cstddef>

#include <iostream>
#include <vector>


Future *testFutureDeq() {
    std::cout << "deq(), ";
    return futureDeq();
}

Future *testFutureEnq(void *data) {
    std::cout << "enq(" << data << "), ";
    return futureEnq(data);
}

void test(int threadNum, int numOps, Vector<Future*> deqs) {
    int i;
    
    lock();
    std::cout << "Thread " << threadNum << " - before: " << std::endl;
    printQ();
    
    for(i = 0; i < numOps; i++) {
        if(rand() % 2 == 0)
            testFutureEnq(rand() % 10);
        else
            deqs.push_back(testFutureDeq());
    }
    
    execute();
    std::cout << std::endl;
    
    std::cout << "results: ";
    for (Future *f : deqs)
        std::cout << f->result << ", ";
    std::cout << std::endl;
    
    
    std::cout << "Thread " << threadNum << " - after: " << std::endl;
    printQ();
    
    std::cout << std::endl;
    unlock();
}

void printQ(){
    Node n = SQHead.node;
    while(n != NULL) {
        n = n->next;
        std::cout << n << " -> "
    }
    
    std::cout << "NULL" << std::endl;
    return node;
}

void main(){
    int i;
    int numThreads = 5;
    int numOpsPerThread = 10;
    Vector<Vector<Future*>> deqs(numThreads, NULL);
    Vector<thread> threads(numThreads, NULL);
    srand(time(NULL));
    
    
    for(i = 0; i < numThreads; i++)
        deqs.push_back(Vector<Future*>());
    
    for(i = 0; i < numThreads; i++)
        threads.push_back(thread(test, i, numOpsPerThread));
    
    for(i = 0; i < numThreads; i++)
        threads.get(i).join();
}
