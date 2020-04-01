# ParallelTeam24

Depending on which version you want to run,
compile with one of these commands:

g++ BQSequential.cpp -std=c++11
g++ -pthread BQLocked.cpp -std=c++11
g++ -pthread BQLockFree.cpp -std=c++11 -latomic


See the included publication for further information.
