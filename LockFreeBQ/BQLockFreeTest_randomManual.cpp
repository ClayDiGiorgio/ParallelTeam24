#include "BQLockFree.cpp"

#include <thread> 
#include <stdio.h> 
#include <stdlib.h>   
#include <time.h> 

#include <cstddef>

#include <iostream>
#include <vector>

#define TESTING_VALUE_RANGE 10

void doBatch(int numOps) {
    for(j = 0; j < numOpsPerBatch; j++) {
        if(rand() % 2 == 0)
            testFutureEnq(rand() % TESTING_VALUE_RANGE);
        else
            deqs.push_back(testFutureDeq());
    }
    
    execute();
}

void test(int numBatches, int numOpsPerBatch) {
    int i, j;
    
    for(i = 0; i < numBatches; i++) {
        switch(rand() % 3) {
            case 0: enqueue(rand() % TESTING_VALUE_RANGE); break;
            case 1: dequeue(); break;
            case 2: doBatch(numOpsPerBatch); break;
        }
    }
}

void threadTest(int numThreads, int numBatchesPerThread, int numOpsPerBatch){
    int i;
    Vector<thread> threads(numThreads, NULL);
    srand(time(NULL));
    
    auto start = std::chrono::high_resolution_clock::now();
    
    init();
    resetThread();
    
    for(i = 0; i < numThreads; i++)
        threads.push_back(thread(test, numBatchesPerThread, numOpsPerBatch));
    
    for(i = 0; i < numThreads; i++)
        threads.get(i).join();
    
    auto end = std::chrono::high_resolution_clock::now();

    double totalTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::cout << "Time taken for " << numThreads 
              << " threads with " << numBatchesPerThread 
              << " operations or batches each, over " << numBatchesPerThread
              << " batches: " << std::fixed << time_taken
              << " microsec" << std::endl;
}

void main() {
    int i = 0;
    for (i = 1; i <= 4; i++)
        threadTest(i, 75, 10);
}
