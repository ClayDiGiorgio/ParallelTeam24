# ParallelTeam24


-----------------------------
=== A Note on Our Testing ===    
-----------------------------

The included report is based on the old method of tests
(which can be seen in BQLockFree_OldTests.cpp).
The current versions of BQLockFree.cpp, BQLocked.cpp, 
and BQSequential.cpp are under development and do not
currently work. 

The old methods of testing can be compiled with the same
commands as the in-development ones, though replacing, 
for example, `BQLockFree.cpp` with `BQLockFree_OldTests.cpp`


     =============================
     |-~-~-~-~-~-~-~-~-~-~-~-~-~-|
     |                           |
     |   Running and Compiling   |
     |    The Benchmark Tests    |
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

