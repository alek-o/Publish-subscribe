---
title:    Publish-subscribe
subtitle: Programowanie systemowe i współbieżne
author:   alek-o
date:     v1.0, 2025-01-28
lang:     pl-PL
---


Projekt jest dostępny w repozytorium pod adresem:  
<https://git.cs.put.poznan.pl/voytek/project1>.

Rysunek wizualizujący struktury danych i ich wykorzystanie w projekcie:   
<https://excalidraw.com/#json=dkBOFgaMQIhEkLvhfZgcf,M79FTniQeictq8Wf4TpoFA>.


# Struktury danych

1. Struktura `Message` reprezentuje wiadomość w kolejce:

   ```C
   typedef struct Message {
       void *data;
       struct Message *next;
       int ref_count; // Reference count for subscribers
   } Message;
   ```

2. Struktura `Subscriber` reprezentuje subskrybenta kolejki:

   ```C
   typedef struct Subscriber {
       pthread_t thread;
       struct Subscriber *next;
       Message *last_read;
   } Subscriber;
   ```

3. Struktura `TQueue` reprezentuje kolejkę wiadomości:

   ```C
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
   ```

# Funkcje

1. `TQueue* createQueue(int size)` -- tworzy nową kolejkę o podanym rozmiarze `size`.
1. `void destroyQueue(TQueue *queue)` -- niszczy kolejkę i zwalnia pamięć.
1. `void subscribe(TQueue *queue, pthread_t thread)` -- dodaje subskrybenta do kolejki.
1. `void unsubscribe(TQueue *queue, pthread_t thread)` -- usuwa subskrybenta z kolejki.
1. `void addMsg(TQueue *queue, void *msg)` -- dodaje wiadomość do kolejki.
1. `void* getMsg(TQueue *queue, pthread_t thread)` -- pobiera wiadomość z kolejki dla danego subskrybenta.
1. `int getAvailable(TQueue *queue, pthread_t thread)` -- zwraca liczbę dostępnych wiadomości dla danego subskrybenta.
1. `void removeMsg(TQueue *queue, void *msg)` -- usuwa wiadomość z kolejki.
1. `void setSize(TQueue *queue, int size)` -- ustawia nowy rozmiar kolejki.


# Algorytm / dodatkowy opis

Kolejka wiadomości implementuje wzorzec publish-subscribe, gdzie subskrybenci mogą rejestrować się do kolejki i otrzymywać wiadomości dodane przez nadawców. Kolejka jest zabezpieczona za pomocą mutexów i zmiennych warunkowych, aby zapewnić bezpieczny dostęp w środowisku wielowątkowym.

* Sytuacje skrajne:
  * Pusta kolejka: subskrybenci czekają na wiadomości.
  * Pełna kolejka: nadawcy czekają na wolne miejsce w kolejce.

* Odporność algorytmu na typowe problemy przetwarzania współbieżnego:
  * Zakleszczenie: użycie mutexów i zmiennych warunkowych zapobiega zakleszczeniom.
  * Aktywne oczekiwanie: subskrybenci i nadawcy są blokowani do momentu spełnienia warunków.
  * Głodzenie: każdy subskrybent otrzymuje wiadomości w kolejności ich dodania.


# Przykład użycia

razem z dołączonym plikiem main.c

```
gcc -o main main.c queue.c -lpthread  
./main
```

oczekiwany rezultat:

```
test_create_destroy_queue: success  
test_subscribe_unsubscribe: success 
test_1_publisher_1_subscriber: success  
```