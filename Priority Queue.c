#include <stdio.h>
#include <stdlib.h>

// Define the HeapNode and PriorityQueue structures
typedef struct {
    int mandatory;  // Higher is better
    int delay;      // Higher is better
    int patient_id; // Lower is better
} HeapNode;

typedef struct {
    HeapNode *data; // Dynamically allocated array for heap nodes
    int size;       // Current number of elements in the priority queue
    int capacity;   // Maximum capacity of the priority queue
} PriorityQueue;

// Function prototypes
void initQueue(PriorityQueue *pq, int initialCapacity);
void insert(PriorityQueue *pq, HeapNode node);
HeapNode extractMax(PriorityQueue *pq);
HeapNode peekMax(PriorityQueue *pq);
int isEmpty(PriorityQueue *pq);
void heapifyUp(PriorityQueue *pq, int index);
void heapifyDown(PriorityQueue *pq, int index);
int compareNodes(HeapNode a, HeapNode b);
void resize(PriorityQueue *pq);
void freeQueue(PriorityQueue *pq);

// Initialize the priority queue
void initQueue(PriorityQueue *pq, int initialCapacity) {
    pq->size = 0;
    pq->capacity = initialCapacity;
    pq->data = (HeapNode *)malloc(initialCapacity * sizeof(HeapNode));
    if (!pq->data) {
        printf("Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
}

// Insert a new element into the priority queue
void insert(PriorityQueue *pq, HeapNode node) {
    if (pq->size == pq->capacity) {
        resize(pq); // Resize the array if it's full
    }
    pq->data[pq->size] = node; // Add the new node at the end
    heapifyUp(pq, pq->size);   // Restore the heap property
    pq->size++;
}

// Extract the element with the highest priority
HeapNode extractMax(PriorityQueue *pq) {
    if (isEmpty(pq)) {
        printf("Priority Queue Underflow\n");
        exit(EXIT_FAILURE);
    }
    HeapNode maxNode = pq->data[0];
    pq->data[0] = pq->data[pq->size - 1];
    pq->size--;
    heapifyDown(pq, 0);
    return maxNode;
}

// Peek at the element with the highest priority without removing it
HeapNode peekMax(PriorityQueue *pq) {
    if (isEmpty(pq)) {
        printf("Priority Queue is empty\n");
        exit(EXIT_FAILURE);
    }
    return pq->data[0];
}

// Check if the priority queue is empty
int isEmpty(PriorityQueue *pq) {
    return pq->size == 0;
}

// Compare two HeapNodes based on priority
int compareNodes(HeapNode a, HeapNode b) {
    if (a.mandatory != b.mandatory) {
        return a.mandatory - b.mandatory; // Higher mandatory is better
    }
    if (a.delay != b.delay) {
        return a.delay - b.delay;         // Higher delay is better
    }
    return b.patient_id - a.patient_id;   // Lower patient_id is better
}

// Restore the heap property by moving the node at index up
void heapifyUp(PriorityQueue *pq, int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (compareNodes(pq->data[index], pq->data[parent]) > 0) {
            // Swap child with parent
            HeapNode temp = pq->data[index];
            pq->data[index] = pq->data[parent];
            pq->data[parent] = temp;
        } else {
            break;
        }
        index = parent;
    }
}

// Restore the heap property by moving the node at index down
void heapifyDown(PriorityQueue *pq, int index) {
    while (2 * index + 1 < pq->size) {
        int leftChild = 2 * index + 1;
        int rightChild = 2 * index + 2;
        int largest = index;

        if (leftChild < pq->size && compareNodes(pq->data[leftChild], pq->data[largest]) > 0) {
            largest = leftChild;
        }
        if (rightChild < pq->size && compareNodes(pq->data[rightChild], pq->data[largest]) > 0) {
            largest = rightChild;
        }
        if (largest != index) {
            // Swap parent with the largest child
            HeapNode temp = pq->data[index];
            pq->data[index] = pq->data[largest];
            pq->data[largest] = temp;
            index = largest;
        } else {
            break;
        }
    }
}

// Resize the priority queue's data array
void resize(PriorityQueue *pq) {
    pq->capacity *= 2; // Double the capacity
    pq->data = (HeapNode *)realloc(pq->data, pq->capacity * sizeof(HeapNode));
    if (!pq->data) {
        printf("Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
}

// Free the allocated memory
void freeQueue(PriorityQueue *pq) {
    free(pq->data);
    pq->data = NULL;
    pq->size = 0;
    pq->capacity = 0;
}


HeapNode makeHeapNode(int mandatory, int delay, int p_id) {
    HeapNode node = {mandatory, delay, p_id};
    return node;
}

// Main function to demonstrate the priority queue
int main() {
    PriorityQueue *pq;
    pq = (PriorityQueue *) calloc (1, sizeof(PriorityQueue));
    initQueue(pq, 5);

    HeapNode node1 = {1, 5, 101};
    HeapNode node2 = {1, 10, 102};
    HeapNode node3 = {1, 7, 104};
    HeapNode node4 = {1, 5, 103};
    HeapNode node5 = {1, 10, 10};
    HeapNode node6 = {0, 3, 13};
    HeapNode node7 = {0, 10, 3};
    HeapNode node8 = {0, 5, 108};
    HeapNode node9 = {1, 7, 107};
    HeapNode node10 = {0, 1, 1010};

    insert(pq, node1);
    insert(pq, node2);
    insert(pq, node3);
    insert(pq, node4);
    insert(pq, node5);
    insert(pq, node6);
    insert(pq, node7);
    insert(pq, node8);
    insert(pq, node9);
    insert(pq, node10);
    
    printf("Highest priority node: mandatory=%d, delay=%d, patient_id=%d\n", 
        peekMax(pq).mandatory, peekMax(pq).delay, peekMax(pq).patient_id);

    while (!isEmpty(pq)) {
        HeapNode maxNode = extractMax(pq);
        printf("Extracted node: mandatory=%d, delay=%d, patient_id=%d\n",
            maxNode.mandatory, maxNode.delay, maxNode.patient_id);
    }
    printf("\ncurrent size of the PQ: %d\tCapacity: %d", pq->size, pq->capacity);

    freeQueue(pq);
    return 0;
}