# ParallelTeam24

     =============================
     |-~-~-~-~-~-~-~-~-~-~-~-~-~-|
     |                           |
     |   Running and Compiling   |
     |    The Benchmark Test     |
     |                           |
     |-~-~-~-~-~-~-~-~-~-~-~-~-~-|
     =============================
 
---------------------
=== Automatically ===    
---------------------

Depending on which version you want to run,
compile with this command in the  appropriate directory:

`bash compileAndRun.sh`
 
 
----------------
=== Manually ===    
----------------

Depending on which version you want to run,
compile with one of these commands in the 
appropriate directory:

`g++ BQSequential.cpp -std=c++11`
`g++ -pthread BQLocked.cpp -std=c++11`
`g++ -pthread BQLockFree.cpp -std=c++11 -latomic`


See the included publication for further information.

