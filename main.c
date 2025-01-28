#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include "queue.h"

bool test_create_destroy_queue() {
    TQueue *queue = createQueue(10);
    if (!queue) {
        printf("Failed to create queue\n");
        return false;
    }

    if (queue->size != 0 || queue->head != NULL || queue->tail != NULL || queue->capacity != 10) {
        printf("Queue initial state is incorrect\n");
        destroyQueue(queue);
        return false;
    }

    destroyQueue(queue);

    return true;
}

bool test_subscribe_unsubscribe() {
    TQueue *queue = createQueue(10);
    if (!queue) {
        printf("Failed to create queue\n");
        return false;
    }

    pthread_t thread;
    pthread_create(&thread, NULL, (void*(*)(void*))pthread_self, NULL);

    subscribe(queue, thread);

    bool found = false;
    Subscriber *current = queue->subscribers;
    while (current) {
        if (pthread_equal(current->thread, thread)) {
            found = true;
            break;
        }
        current = current->next;
    }
    if (!found) {
        printf("Thread not found in subscriber list after subscribing\n");
        destroyQueue(queue);
        return false;
    }

    unsubscribe(queue, thread);

    found = false;
    current = queue->subscribers;
    while (current) {
        if (pthread_equal(current->thread, thread)) {
            found = true;
            break;
        }
        current = current->next;
    }
    if (found) {
        printf("Thread found in subscriber list after unsubscribing\n");
        destroyQueue(queue);
        return false;
    }

    destroyQueue(queue);

    return true;
}

#define NUM_MESSAGES 5

void* publisher(void* arg) {
    TQueue *queue = (TQueue*)arg;
    for (int i = 0; i < NUM_MESSAGES; i++) {
        int *data = (int*)malloc(sizeof(int));
        *data = i;
        addMsg(queue, data);
    }
    return NULL;
}

void* subscriber(void* arg) {
    TQueue *queue = (TQueue*)arg;
    bool *result = (bool*)malloc(sizeof(bool));
    *result = true;

    for (int i = 0; i < NUM_MESSAGES; i++) {
        const int *msg = getMsg(queue, pthread_self());
        if (msg == NULL || *msg != i) {
            printf("Subscriber received incorrect message: expected %d, got %d\n", i, msg ? *msg : -1);
            *result = false;
        }
        if (msg) {
            free((void*)msg);
        }
    }

    return result;
}

void* test_1_publisher_1_subscriber(void* arg) {
    bool *success = (bool*)malloc(sizeof(bool));
    *success = true;

    TQueue *queue = createQueue(10);
    if (!queue) {
        printf("Failed to create queue\n");
        *success = false;
        return success;
    }

    pthread_t sub_thread;
    pthread_create(&sub_thread, NULL, subscriber, queue);
    subscribe(queue, sub_thread);

    pthread_t pub_thread;
    pthread_create(&pub_thread, NULL, publisher, queue);

    void *sub_result;
    pthread_join(pub_thread, NULL);
    pthread_join(sub_thread, &sub_result);

    bool *result = (bool*)sub_result;
    if (!*result) {
        *success = false;
    }
    free(result);

    destroyQueue(queue);

    return success;
}

int main() {
    bool result1 = test_create_destroy_queue();
    printf("test_create_destroy_queue: %s\n", result1 ? "success" : "failed");

    bool result2 = test_subscribe_unsubscribe();
    printf("test_subscribe_unsubscribe: %s\n", result2 ? "success" : "failed");

    bool *result3 = test_1_publisher_1_subscriber(NULL);
    printf("test_1_publisher_1_subscriber: %s\n", *result3 ? "success" : "failed");
    free(result3);

    return 0;
}