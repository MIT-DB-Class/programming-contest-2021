# 6.830 Programming Competition 2021

## Task Details

The task is to evaluate batches of join queries on a set of pre-defined
relations. Each join query specifies a set of relations, (equality) join
predicates, and selections (aggregations). The challenge is to execute
the queries as fast as possible.

Input to your program will be provided on the standard input, and the
output must appear on the standard output.

## Testing Protocol

Our test harness will first feed the set of relations to your program's
standard input. That means, your program will receive multiple lines
(separated by the new line character '\n') where each one contains a
string which represents the file name of the given relation. The relation
files are already in a binary format and thus do not require parsing.
All fields are unsigned 64 bit integers.
Our starter package already contains sample code that mmaps() a
relations into main memory.
The binary format of a relation consists of
a header and a data section. The header contains the number of tuples
and the number of columns. The data section follows the header and stores
all tuples using a column store. All of the values of a column
are stored sequentially, followed by the values of the next column,
and so on. The overall binary format is as follows (T0C0 stands for
tuple 0 of column 0; pipe symbol '|' is not part of the binary format):

```
uint64_t numTuples|uint64_t numColumns|uint64_t T0C0|uint64_t T1C0|..|uint64_t TnC0|uint64_t T0C1|..|uint64_t TnC1|..|uint64_t TnCm
```

After sending the set of relations, our test harness will send a line
containing the string "Done".

Next, our test harness will wait for **60s** until it starts sending
queries. This gives you time to prepare for the workload, e.g., 
collecting statistics, creating indexes, etc. The test harness sends
the workload in batches:
A workload batch contains a set of join queries (each line represents a
query). A join query consists of three consecutive parts (separated
by the pipe symbol '|'):

- **Relations**: A list of relations that will be joined. We will pass
the ids of the relation here separated by spaces (' '). The relation ids
are implicitly mapped to the relations by the order the relations were
passed in the first phase. For instance, id 0 refers to the first relation.

- **Predicates**: Each predicate is separated by a '&'. We have two types
of predicates: filter predicates and join predicates. Filter predicates are
of the form: filter column + comparison type (greater '>' less '<'
equal '=') + integer constant. Join predicates specify on which columns the
relations should be joined. A join predicate is composed out of two
relation-column pairs connected with an equality ('=') operator. Here,
a relation is identified by its offset in the list of relations to be
joined (i.e., we implicitly bind the first relation of a join query to
the identifier 0, the second one to 1, etc.).

- **Projections**: A list of columns that are needed to compute the final
check sum that we use to verify that the join was done properly. Similar 
to the join predicates, columns are denoted as relation-column pairs.
Each selection is delimited by a space character (' ').

Example:
```
0 2 4|0.1=1.2&1.0=2.1&0.1>3000|0.0 1.1
```

Translation to SQL:
```
SELECT SUM("0".c0), SUM("1".c1)
FROM r0 "0", r2 "1", r4 "2"
WHERE 0.c1=1.c2 and 1.c0=2.c1 and 0.c1>3000
```

The end of a batch is indicated by a line containing the character 'F'.
Our test harness will then wait for the results to be written to your
program's standard output. For each join query, your program is required
to output a line containing the check sums of the individual projections
separated by spaces (e.g., "42 4711"). If there is no qualifying tuple,
each check sum should return "NULL" like in SQL. Once the results have
been received, we will start delivering the next workload batch.

For your check sums, use SUM aggregation.
You do not have to worry about numeric overflows as long as you are using
64 bit unsigned integers.

Your solution will be evaluated for correctness and execution time.
Execution time measurement starts immediately after the 60s waiting
period. You are free to fully utilize the waiting period for any kind of
pre-processing.

All join graphs are connected. There are no cross products. Cyclic queries and
self joins are possible. We will provide more constraints soon.

## Starter code

We provide a starter package in C++. It is in the format required for
submission. It includes a query parser and a relation loader.
It implements a basic query execution model featuring full
materialization. It does not implement any query optimization. It only
uses standard STL containers (like unordered_map) for the join
processing. Its query processing capabilities are limited to the
demands of this contest.

DISCLAIMER: Although we have tested the package
intensively, we cannot guarantee that it is free of bugs. Thus, we test
your submissions against the results computed by a real DBMS.

This project is only meant to give you a quick start into the project and
to dig right into the fun (coding) part. It is not required to use the
provided code. You can create a submittable submission.tar.gz file using
the included package.sh script.

For testing, we provide a small workload in the required format.
We also provide CSV versions (.tbl files) of each relation + SQL
files to load the relations into a DBMS and a file
(small.work.sql) that contains SQL versions of all queries in
small.work.

To build the starter code base run the following commands:
```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

To build in debug mode run 
`cmake -DCMAKE_BUILD_TYPE=Debug ..`

To not build the tests run 
`cmake -DCMAKE_BUILD_TYPE=Release -DFORCE_TESTS=OFF ..`

This creates the binaries `driver`, `harness`, and `query2SQL` in `build`
directory and `tester` in `build/test` directory. `driver` is the binary that
interacts with our test harness `harness` according to the protocol described
above. You can use `query2SQL` to transform our query format to SQL.

To test the small workload using our test harness run

```
bash run_test_harness.sh workloads/small
```

To execute all tests run 

```
./tester
```

To execute a specific test run 

```
./tester --gtest_filter=<test_name>
```
For example,
```
./tester --gtest_filter=OperatorTest.ScanWithSelection
```

We encourage you to write your own tests to as you develop your code base.
We have provided some examples using the
[GoogleTest](https://github.com/google/googletest) framework in `test`
folder.

## Evaluation

Our testing infrastructure will evaluate each submission by unpacking the
submission file, compiling the submitted code (using your submitted
compile.sh script), and then running a series of tests (using your
submitted run.sh script). Extraction time is limited to 10 seconds,
and compilation time is limited to 60 seconds. Submissions that exceed
these limits will not be tested at all.

Each test uses the test harness to supply an initial dataset and a
workload to your submitted program. The total time for each test is
limited to 4-5 minutes (different tests may have slightly different
limits). The total per-test time includes the time to ingest the dataset
and the time to process the workload. 

Submissions will be unpacked, compiled and tested in a network-isolated
container. We will provide system specifications of the testing
environment soon. 

A submission is considered to be rankable if it correctly processes all
of the test workloads within their time limits. As discussed in the
testing protocol description, initial import of relations is not included
in a submission's score. The leaderboard will show the best rankable
submission for each team that has at least one such submission.

The submission portal and leaderboard will be up soon.

## Submission

Submit a single compressed tar file called submission.tar.gz.
You can use package.sh to do that.
Submission files must be no larger than 5 MB - larger files will be
rejected. Each submission must include the following files/directories:
- **run.sh**:
This script is responsible for running your executable, which should
interact with our test harness according to the testing protocol,
reading from standard input and writing results to standard output.
- **readme.txt**:
This file must contain information about all team members, a brief
description of the solution, a list of the third party code used and
their licenses.
- **Source Files**:
The package must contain all of the source code.
- **compile.sh**:
This shell script must be able to compile the source contained in the
submission directory. The produced executable file(s) must be locate
within the submission directory. The benchmark environment is isolated
and without internet access. You will have to provide all files required
for successful compilation.

You can use the starter package, which has the required format, as a
starting point.

## Rules

- All submissions must consist only of code written by the team or open source
licensed software (i.e., using an OSI-approved license). For source code from
books or public articles, clear reference and attribution must be made.
- Please keep your solution private and not make it publicly available.

## Updates
When we release more details about the competition, we will add them under this
section. 

- A larger dataset is now available for testing. You can download it
[here](http://dsg.csail.mit.edu/data/public.zip).
- System specifications of the testing environment:

| | |
| ----------- | ----------- |
| Processor       | Intel(R) Xeon(R) CPU E5-2630 v4 @ 2.20GHz       |
| Configuration   | 12 cores / 24 hyperthreads        |
| Main memory     | 96 GB DIMM RAM        |
| OS              | Ubuntu 18.04        |
| Software        | autoconf=2.69-11, automake=1:1.15.1-3ubuntu2, cmake=3.10.2-1ubuntu2.18.04.1, gcc=4:7.4.0-1ubuntu2.3, libjemalloc-dev=3.6.0-11, libboost-dev=1.65.1.0ubuntu1, clang-5.0=1:5.0.1-4, libtbb-dev=2017~U7-8        |

- Dockerfile for the testing environment is available for download
[here](https://github.com/MIT-DB-Class/programming-contest-2021/blob/main/Dockerfile).

## Acknowledgements

This contest is adapted from SIGMOD 2018 programming contest. The starter code
is a modified version of the quick start package provided with the contest.