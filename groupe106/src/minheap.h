
#include<stdio.h>
#include<stdlib.h>

int n;
int* createMinHeap(int size);
void exch(int* MinHeap, int i, int j);
void swim(int* MinHeap, int k);
void sink(int * MinHeap, int k);
void insert(int* MinHeap, int key);
int delMin(int* MinHeap);
int peek(int* MinHeap);
