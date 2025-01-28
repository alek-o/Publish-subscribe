#ifndef LCL_QUEUE_H
#define LCL_QUEUE_H

#include <pthread.h>
#include <stdbool.h>

typedef struct Message {
    void *data;
    struct Message *next;
    int ref_count; // Reference count for subscribers
} Message;

typedef struct Subscriber {
    pthread_t thread;
    struct Subscriber *next;
    Message *last_read;
} Subscriber;

typedef struct TQueue {
    Message *head;
    Message *tail;
    int size;
    int capacity;
    Subscriber *subscribers;
    pthread_mutex_t lock;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} TQueue;

TQueue* createQueue(int size);

void destroyQueue(TQueue *queue);

void subscribe(TQueue *queue, pthread_t thread);

void unsubscribe(TQueue *queue, pthread_t thread);

void addMsg(TQueue *queue, void *msg);

void* getMsg(TQueue *queue, pthread_t thread);

int getAvailable(TQueue *queue, pthread_t thread);

void removeMsg(TQueue *queue, void *msg);

void setSize(TQueue *queue, int size);

#endif //LCL_QUEUE_H
