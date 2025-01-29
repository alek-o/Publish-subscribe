#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
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

void* threadBusyWait(void*) {
    while (true) {}
}

void *test4(void *arg) {
    bool *success = malloc(sizeof(bool));
    *success = true;

    TQueue *queue = createQueue(10);
    pthread_t** thread = malloc(sizeof(pthread_t*) * 2);
    thread[0] = malloc(sizeof(pthread_t));
    thread[1] = malloc(sizeof(pthread_t));

    pthread_create(thread[0], NULL, threadBusyWait, NULL);
    pthread_create(thread[1], NULL, threadBusyWait, NULL);

    subscribe(queue, *thread[0]);
    subscribe(queue, *thread[1]);

    int *g1 = getMsg(queue, *thread[0]);

    int *msg1 = malloc(sizeof(int));
    *msg1 = 10;
    int *msg2 = malloc(sizeof(int));
    *msg2 = 20;

    addMsg(queue, msg1);
    addMsg(queue, msg2);

    if (*g1 == 10)
    {

    }
    else
    {
        success = false;
    }

    free(g1);

    pthread_join(*thread[0], NULL);
    pthread_join(*thread[1], NULL);

    free(thread[0]);
    free(thread[1]);

    destroyQueue(queue);

    return success;
}

typedef struct RoutineArgs {
    TQueue *queue;
    int routine_number;
} RoutineArgs;

void *routine_default(void *args) {
    RoutineArgs *r_args = (RoutineArgs *)args;

    printf("Thread %d subscribing...\n", r_args->routine_number);
    subscribe(r_args->queue, pthread_self());

    void *msg1 = getMsg(r_args->queue, pthread_self());
    if (msg1) {
        printf("Thread %d received message: %d\n", r_args->routine_number, *(int*)msg1);
    }

    void *msg2 = getMsg(r_args->queue, pthread_self());
    if (msg2) {
        printf("Thread %d received message: %d\n", r_args->routine_number, *(int*)msg2);
    }

    unsubscribe(r_args->queue, pthread_self());
    printf("Thread %d unsubscribed.\n", r_args->routine_number);

    free(r_args); // Free allocated memory for the routine args
    return NULL;
}

// void *routine2(void *args) {
//     RoutineArgs r_args = *(RoutineArgs *)args;
//     int *msg1 = getMsg(r_args.queue, pthread_self());
//     printf("routine %d received message: %s\n", r_args.routine_number, *msg2);
//     free(msg1);

//     int *msg2 = getMsg(r_args.queue, pthread_self());
//     printf("routine %d received message: %s\n", r_args.routine_number, *msg2);
//     free(msg2);
// }

void *simple_test(void *arg) {
    bool *success = malloc(sizeof(bool));
    *success = true;

    TQueue *queue = createQueue(10);
    pthread_t threads[2];

    RoutineArgs *r_args0 = malloc(sizeof(RoutineArgs));
    r_args0->queue = queue;
    r_args0->routine_number = 0;

    RoutineArgs *r_args1 = malloc(sizeof(RoutineArgs));
    r_args1->queue = queue;
    r_args1->routine_number = 1;

    printf("Right before action\n");

    pthread_create(&threads[0], NULL, routine_default, r_args0);
    pthread_create(&threads[1], NULL, routine_default, r_args1);

    // sleep(3); // Give subscribers time to wait on messages

    int *msg1 = malloc(sizeof(int));
    int *msg2 = malloc(sizeof(int));
    *msg1 = 10;
    *msg2 = 20;
    addMsg(queue, msg1);
    printf("Producer added message 1\n");
    addMsg(queue, msg2);
    printf("Producer added message 2\n");

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);

    destroyQueue(queue);
    return success;
}

int main() {
    // bool result1 = test_create_destroy_queue();
    // printf("test_create_destroy_queue: %s\n", result1 ? "success" : "failed");

    // bool result2 = test_subscribe_unsubscribe();
    // printf("test_subscribe_unsubscribe: %s\n", result2 ? "success" : "failed");

    // bool *result3 = test_1_publisher_1_subscriber(NULL);
    // printf("test_1_publisher_1_subscriber: %s\n", *result3 ? "success" : "failed");
    // free(result3);

    // bool *result4 = test4(NULL);
    // printf("test4: %s\n", *result4 ? "success" : "failed");
    // free(result4);

    bool *simple_result = simple_test(NULL);
    printf("simple_test: %s\n", *simple_result ? "success" : "failed");
    free(simple_result);

    return 0;
}