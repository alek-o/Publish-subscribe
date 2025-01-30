#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "queue.h"

TQueue* createQueue(int size) {
    if (size <= 0) {
        return NULL;
    }

    TQueue *queue = (TQueue*)malloc(sizeof(TQueue));
    if (!queue) {
        return NULL;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->capacity = size;
    queue->subscribers = NULL;

    if (pthread_mutex_init(&queue->lock, NULL) != 0) {
        free(queue);
        return NULL;
    }

    if (pthread_cond_init(&queue->not_full, NULL) != 0) {
        pthread_mutex_destroy(&queue->lock);
        free(queue);
        return NULL;
    }

    if (pthread_cond_init(&queue->not_empty, NULL) != 0) {
        pthread_cond_destroy(&queue->not_full);
        pthread_mutex_destroy(&queue->lock);
        free(queue);
        return NULL;
    }

    return queue;
}

//--------------------------------------------------------------------------------

void destroyQueue(TQueue *queue) {
    if (!queue) {
        return;
    }

    pthread_mutex_lock(&queue->lock);

    // Destroy messages
    Message *current = queue->head;
    while (current) {
        Message *next = current->next;
        free(current);
        current = next;
    }

    // Destroy subscribers
    Subscriber *sub_current = queue->subscribers;
    while (sub_current) {
        Subscriber *sub_next = sub_current->next;
        free(sub_current);
        sub_current = sub_next;
    }

    pthread_mutex_unlock(&queue->lock);

    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    pthread_mutex_destroy(&queue->lock);

    free(queue);
}

//--------------------------------------------------------------------------------

void subscribe(TQueue *queue, pthread_t thread) {
    pthread_mutex_lock(&queue->lock);

    // Check if the thread is already subscribed
    Subscriber *current = queue->subscribers;
    while (current) {
        if (pthread_equal(current->thread, thread)) {
            pthread_mutex_unlock(&queue->lock);
            return;
        }
        current = current->next;
    }

    // Add the new subscriber
    Subscriber *new_subscriber = (Subscriber*)malloc(sizeof(Subscriber));
    if (!new_subscriber) {
        pthread_mutex_unlock(&queue->lock);
        return;
    }
    new_subscriber->thread = thread;
    new_subscriber->next = queue->subscribers;
    new_subscriber->last_read = queue->tail;
    queue->subscribers = new_subscriber;

    pthread_mutex_unlock(&queue->lock);
}

//--------------------------------------------------------------------------------

void unsubscribe(TQueue *queue, pthread_t thread) {
    pthread_mutex_lock(&queue->lock);

    Subscriber *current = queue->subscribers;
    Subscriber *prev = NULL;

    while (current) {
        if (pthread_equal(current->thread, thread)) {
            // Mark any unreceived messages as processed
            Message *msg = current->last_read ? current->last_read->next : queue->head;
            while (msg) {
                Message *next_msg = msg->next;
                msg->ref_count--;
                if (msg->ref_count == 0) {
                    removeMsg(queue, msg);
                }
                msg = next_msg;
            }

            // Remove the subscriber from the list
            if (prev) {
                prev->next = current->next;
            } else {
                queue->subscribers = current->next;
            }

            free(current);
            pthread_mutex_unlock(&queue->lock);
            return;
        }
        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&queue->lock);
}

//--------------------------------------------------------------------------------

void addMsg(TQueue *queue, void *msg_data) {
    pthread_mutex_lock(&queue->lock);

    // Exit if there are no subscribers
    if (queue->subscribers == NULL) {
        pthread_mutex_unlock(&queue->lock);
        return;
    }

    // Block if the queue is full
    while (queue->size >= queue->capacity) {
        pthread_cond_wait(&queue->not_full, &queue->lock);
    }

    // Create a new message
    Message *msg = (Message*)malloc(sizeof(Message));
    if (!msg) {
        pthread_mutex_unlock(&queue->lock);
        return;
    }
    msg->next = NULL;
    msg->ref_count = 0;
    msg->data = msg_data;


    // Add the message to the end of the queue
    if (queue->tail) {
        queue->tail->next = msg;
    } else {
        queue->head = msg;
    }
    queue->tail = msg;
    queue->size++;

    // Increment the reference count for each subscriber
    Subscriber *current = queue->subscribers;
    while (current) {
        msg->ref_count++;
        current = current->next;
    }

    // Signal that the queue is not empty
    pthread_cond_broadcast(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
}

//--------------------------------------------------------------------------------

void* getMsg(TQueue *queue, pthread_t thread) {
    pthread_mutex_lock(&queue->lock);

    // Find the subscriber
    Subscriber *current = queue->subscribers;
    while (current) {
        if (pthread_equal(current->thread, thread)) {
            // Block if there are no new messages available
            while (current->last_read == queue->tail) {
                pthread_cond_wait(&queue->not_empty, &queue->lock);
            }

            // Retrieve the next unread message
            Message *msg = current->last_read ? current->last_read->next : queue->head;
            if (msg) {
                current->last_read = msg;
                msg->ref_count--;

                // If the reference count reaches zero, remove the message
                if (msg->ref_count == 0) {
                    removeMsg(queue, msg);
                }

                pthread_mutex_unlock(&queue->lock);
                return msg->data;
            }
        }
        current = current->next;
    }

    pthread_mutex_unlock(&queue->lock);
    return NULL; // Thread is not subscribed
}

//--------------------------------------------------------------------------------

int getAvailable(TQueue *queue, pthread_t thread) {
    pthread_mutex_lock(&queue->lock);

    // Find the subscriber
    Subscriber *current = queue->subscribers;
    while (current) {
        if (pthread_equal(current->thread, thread)) {
            // Calculate the number of unread messages
            
            int unread_count = 0;
            Message *msg = current->last_read ? current->last_read->next : queue->head;
            while (msg) {
                unread_count++;
                msg = msg->next;
            }
            pthread_mutex_unlock(&queue->lock);
            return unread_count;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&queue->lock);
    return 0; // Thread is not subscribed
}

//--------------------------------------------------------------------------------

void removeMsg(TQueue *queue, void *msg) {
    if (!queue || !msg) {
        return;
    }

    // Cast the void pointer to a Message pointer
    Message *target_msg = (Message *)msg;

    // Find the message in the queue
    Message *current = queue->head;
    Message *prev = NULL;
    while (current) {
        if (current == target_msg) {
            break;
        }
        prev = current;
        current = current->next;
    }

    // If the message is not found, unlock and return
    if (!current) {
        pthread_mutex_unlock(&queue->lock);
        return;
    }

    // Remove the message from the queue
    if (queue->head == current) {
        queue->head = current->next;
        if (queue->tail == current) {
            queue->tail = NULL;
        }
    } else {
        if (prev) {
            prev->next = current->next;
            if (queue->tail == current) {
                queue->tail = prev;
            }
        }
    }

    // Update the last_read pointers of affected subscribers
    Subscriber *subscriber = queue->subscribers;
    while (subscriber) {
        if (subscriber->last_read == current) {
            subscriber->last_read = prev; // Update to the previous message
        }
        subscriber = subscriber->next;
    }

    queue->size--;

    pthread_cond_signal(&queue->not_full);
}

//--------------------------------------------------------------------------------

void setSize(TQueue *queue, int size) {
    pthread_mutex_lock(&queue->lock);

    while (queue->size > size) {
        // Remove the oldest message
        Message *msg = queue->head;
        if (msg) {
            queue->head = msg->next;
            if (queue->tail == msg) {
                queue->tail = NULL;
            }

            // Adjust last_read for each subscriber
            Subscriber *current = queue->subscribers;
            while (current) {
                if (current->last_read == msg) {
                    current->last_read = NULL;
                }
                current = current->next;
            }

            free(msg->data); 
            free(msg);
            queue->size--;
        }
    }

    queue->capacity = size;

    // Signal that the queue might not be full
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->lock);
}