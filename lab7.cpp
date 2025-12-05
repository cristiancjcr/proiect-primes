#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Platform specific includes and definitions
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define THREAD_RETURN unsigned __stdcall
#define THREAD_TYPE HANDLE

// Wrapper for Windows Mutex
typedef HANDLE Mutex;
void create_mutex(Mutex* m) { *m = CreateMutex(NULL, FALSE, NULL); }
void lock_mutex(Mutex* m) { WaitForSingleObject(*m, INFINITE); }
void unlock_mutex(Mutex* m) { ReleaseMutex(*m); }
void destroy_mutex(Mutex* m) { CloseHandle(*m); }
void sleep_ms(int ms) { Sleep(ms); }
void wait_thread(THREAD_TYPE t) { WaitForSingleObject(t, INFINITE); }

#else
#include <pthread.h>
#include <unistd.h>
#define THREAD_RETURN void*
#define THREAD_TYPE pthread_t

// Wrapper for POSIX Mutex (Linux)
typedef pthread_mutex_t Mutex;
void create_mutex(Mutex* m) { pthread_mutex_init(m, NULL); }
void lock_mutex(Mutex* m) { pthread_mutex_lock(m); }
void unlock_mutex(Mutex* m) { pthread_mutex_unlock(m); }
void destroy_mutex(Mutex* m) { pthread_mutex_destroy(m); }
void sleep_ms(int ms) { usleep(ms * 1000); }
void wait_thread(THREAD_TYPE t) { pthread_join(t, NULL); }
#endif

// --- SHARED DATA AND SYNCHRONIZATION OBJECTS ---

// Mutex to protect the resource from simultaneous Black/White access
Mutex resourceMutex;

// Mutex acting as a FIFO queue/turnstile to prevent starvation
Mutex turnstileMutex;

// Mutexes to protect the counter variables
Mutex whiteCountMutex;
Mutex blackCountMutex;

// Counters for threads currently using the resource
int whiteCount = 0;
int blackCount = 0;

// Shared Logic for White Threads
void white_enter() {
    // 1. Pass through the turnstile. This enforces order and prevents starvation.
    lock_mutex(&turnstileMutex);

    lock_mutex(&whiteCountMutex);
    whiteCount++;
    // If this is the first white thread, lock the resource for Whites
    if (whiteCount == 1) {
        lock_mutex(&resourceMutex);
    }
    unlock_mutex(&whiteCountMutex);

    // 2. Release turnstile immediately so other threads (same or diff type) can arrive
    // If a Black thread was waiting, it will grab this and block new Whites 
    // from entering until the resource is free.
    unlock_mutex(&turnstileMutex);
}

void white_exit() {
    lock_mutex(&whiteCountMutex);
    whiteCount--;
    // If this is the last white thread, release the resource
    if (whiteCount == 0) {
        unlock_mutex(&resourceMutex);
    }
    unlock_mutex(&whiteCountMutex);
}

// Shared Logic for Black Threads (Symmetric to White)
void black_enter() {
    lock_mutex(&turnstileMutex);

    lock_mutex(&blackCountMutex);
    blackCount++;
    if (blackCount == 1) {
        lock_mutex(&resourceMutex);
    }
    unlock_mutex(&blackCountMutex);

    unlock_mutex(&turnstileMutex);
}

void black_exit() {
    lock_mutex(&blackCountMutex);
    blackCount--;
    if (blackCount == 0) {
        unlock_mutex(&resourceMutex);
    }
    unlock_mutex(&blackCountMutex);
}

// --- THREAD WORKER FUNCTIONS ---

THREAD_RETURN white_thread_func(void* arg) {
    int id = *((int*)arg);
    free(arg);

    // Simulate some work before requesting access
    sleep_ms(rand() % 100);

    printf("White Thread %d is requesting access.\n", id);

    white_enter();

    // CRITICAL SECTION (Using the resource)
    printf(" -> White Thread %d is USING the resource. (Total Whites: %d)\n", id, whiteCount);
    sleep_ms(200 + (rand() % 300)); // Simulate usage time

    white_exit();

    printf("White Thread %d finished.\n", id);
    return 0;
}

THREAD_RETURN black_thread_func(void* arg) {
    int id = *((int*)arg);
    free(arg);

    sleep_ms(rand() % 100);

    printf("Black Thread %d is requesting access.\n", id);

    black_enter();

    // CRITICAL SECTION
    printf(" -> Black Thread %d is USING the resource. (Total Blacks: %d)\n", id, blackCount);
    sleep_ms(200 + (rand() % 300));

    black_exit();

    printf("Black Thread %d finished.\n", id);
    return 0;
}

// --- MAIN ---

int main() {
    srand(time(NULL));

    // Initialize synchronization primitives
    create_mutex(&resourceMutex);
    create_mutex(&turnstileMutex);
    create_mutex(&whiteCountMutex);
    create_mutex(&blackCountMutex);

    const int NUM_WHITE = 5;
    const int NUM_BLACK = 5;
    const int TOTAL_THREADS = NUM_WHITE + NUM_BLACK;

    THREAD_TYPE threads[10]; // Cannot use variable size array for initialization in standard C++ without vectors

    // Create threads in mixed order
    for (int i = 0; i < TOTAL_THREADS; i++) {
        int* id = (int*)malloc(sizeof(int));
        *id = i;

        // Alternate creation to simulate random arrival, 
        // though sleep in thread func adds more randomness.
        if (i % 2 == 0) {
#ifdef _WIN32
            threads[i] = (HANDLE)_beginthreadex(NULL, 0, white_thread_func, id, 0, NULL);
#else
            pthread_create(&threads[i], NULL, white_thread_func, id);
#endif
        }
        else {
#ifdef _WIN32
            threads[i] = (HANDLE)_beginthreadex(NULL, 0, black_thread_func, id, 0, NULL);
#else
            pthread_create(&threads[i], NULL, black_thread_func, id);
#endif
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < TOTAL_THREADS; i++) {
        wait_thread(threads[i]);
    }

    // Cleanup
    destroy_mutex(&resourceMutex);
    destroy_mutex(&turnstileMutex);
    destroy_mutex(&whiteCountMutex);
    destroy_mutex(&blackCountMutex);

    printf("All threads completed execution.\n");
    return 0;
}

