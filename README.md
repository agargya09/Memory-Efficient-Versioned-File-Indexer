# Versioned File Indexer

**Name:** Avtansh  
**Roll Number:** 240234  

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
