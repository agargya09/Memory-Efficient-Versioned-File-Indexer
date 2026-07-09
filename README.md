# Versioned File Indexer

This is a memory-efficient command-line tool that builds a word frequency index over large text files. The file is read in fixed-size chunks so that only one buffer's worth of data sits in memory at a time, keeping memory usage constant regardless of file size. Multiple files can be indexed as separate versions and queried against each other.

## Compilation Instructions
To compile the source code into an executable named `analyzer`, run the following command in your terminal:

```bash
g++ -std=c++17 -O2 -o analyzer 240234_Avtansh.cpp
```
## Word count query
```bash
./analyzer --file verbose_logs.txt --version v1 --buffer 512 --query word --word error
```
## Top 10 query
```bash
./analyzer --file verbose_logs.txt --version v1 --buffer 512 --query top --top 10
```
## Difference query
```bash
./analyzer --file1 verbose_logs.txt --version1 v1 --file2 test_logs.txt --version2 v2 --buffer 512 --query diff --word error
```
