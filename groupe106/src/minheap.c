#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "minheap.h"
#include "log.h"

int n = 0;

int* createMinHeap(int size){
  int *MinHeap = (int*)malloc(sizeof(int)*size);
  return MinHeap;
}
void exch(int* MinHeap, int i, int j){
  int a = MinHeap[i];
  int b = MinHeap[j];
  MinHeap[i] = b;
  MinHeap[j] = a;
}
void swim(int* MinHeap, int k){
  while(k>1 && MinHeap[k]<MinHeap[k/2]){
    exch(MinHeap,k/2,k);
    k=k/2;
  }
}
void sink(int * MinHeap, int k){
  while(2*k<=n){
    int j = 2*k;
    if(j<n && MinHeap[j]>MinHeap[j+1]) j++;
    if(MinHeap[j]>MinHeap[k]) break;
    exch(MinHeap, k,j);
    k=j;
  }
}
//minHeap[0]==NULL
void insert(int* MinHeap, int key){
  n=n+1;
  MinHeap[n] = key;
  swim(MinHeap, n);
}
int delMin(int* MinHeap){
  if(n>0){
    int min = MinHeap[1];
    exch(MinHeap,1,n--);
    MinHeap[n+1]=300;
    sink(MinHeap,1);
    return min;
  }
  else{return-1;}

}

int peek(int* MinHeap){
  if(n>0){
  return MinHeap[1];
  }
  else{return-1;}
}






/*int main() {

  int * Heap = createMinHeap(5);
  insert(Heap, 5);
  printf("Plus petit elem: %d\n", peek(Heap));
  insert(Heap,3);
  printf("Plus petit elem: %d\n", peek(Heap));
  insert(Heap,7);
  printf("Plus petit elem: %d\n", peek(Heap));
  insert(Heap,1);
  printf("Plus petit elem: %d\n", peek(Heap));
  int a = delMin(Heap);
  printf("Plus petit elem: %d\n", peek(Heap));
  insert(Heap,1);
  printf("Plus petit elem: %d\n", peek(Heap));
  a = delMin(Heap);
  printf("Plus petit elem: %d\n", a);
  a = delMin(Heap);
  printf("Plus petit elem: %d\n",a);
  a = delMin(Heap);
  printf("Plus petit elem: %d\n", a);
  a = delMin(Heap);
  printf("Plus petit elem: %d\n", a);
  a = delMin(Heap);
  printf("Plus petit elem: %d\n", a);
  a = delMin(Heap);
  printf("Plus petit elem: %d\n", a);
  return 0;
}*/
