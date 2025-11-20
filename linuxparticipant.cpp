// linux/participant.cpp
#define _GNU_SOURCE
#include <iostream>
#include <random>
#include <thread>
#include <chrono>
#include <cstring>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

const char* SHM_NAME = "/my_counter_shm_v3";
const char* SEM_NAME = "/my_counter_sem_v3";
const int TARGET_NUMBER = 1000;

struct SharedData { int counter; };

class CrossPlatformSemaphore {
    sem_t* sem_handle;
public:
    CrossPlatformSemaphore() {
        // O_CREAT cu permisiuni decente
        sem_handle = sem_open(SEM_NAME, O_CREAT, 0644, 1);
        if (sem_handle == SEM_FAILED) {
            perror("sem_open");
            exit(1);
        }
    }
    ~CrossPlatformSemaphore() {
        sem_close(sem_handle); // nu eliminăm numele; curățare manuală dacă dorești
    }
    void acquire() { sem_wait(sem_handle); }
    void release() { sem_post(sem_handle); }
};

class SharedMemory {
    SharedData* data_ptr;
    int shm_fd;
    bool is_creator;
public:
    SharedMemory() : data_ptr(nullptr), shm_fd(-1), is_creator(false) {
        shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("shm_open");
            exit(1);
        }

        struct stat st;
        if (fstat(shm_fd, &st) == -1) {
            perror("fstat");
            exit(1);
        }

        if (st.st_size == 0) {
            // primul creator setează dimensiunea
            if (ftruncate(shm_fd, sizeof(SharedData)) == -1) {
                perror("ftruncate");
                exit(1);
            }
            is_creator = true;
        }

        data_ptr = (SharedData*)mmap(nullptr, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (data_ptr == MAP_FAILED) {
            perror("mmap");
            exit(1);
        }

        if (is_creator) {
            data_ptr->counter = 0;
            std::cout << "[System] Memory initialized to 0.\n";
        }
    }

    ~SharedMemory() {
        munmap(data_ptr, sizeof(SharedData));
        close(shm_fd);
    }

    SharedData* get() { return data_ptr; }
};

// logica comună proces
void run_process_logic(const std::string& name) {
    SharedMemory shm;
    CrossPlatformSemaphore sem;
    SharedData* data = shm.get();

    std::random_device rd;
    std::mt19937 gen(rd() ^ (unsigned) getpid());
    std::uniform_int_distribution<> coin_toss(1, 2);

    while (true) {
        sem.acquire();
        if (data->counter >= TARGET_NUMBER) {
            sem.release();
            break;
        }
        while (true) {
            if (data->counter >= TARGET_NUMBER) break;
            int roll = coin_toss(gen);
            if (roll == 2) {
                data->counter++;
                std::cout << "[" << name << "] Coin 2 -> Count: " << data->counter << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(20)); // mic sleep după scriere
            } else {
                // coin == 1 -> yield (nu scriem)
                std::cout << "[" << name << "] Coin 1 -> Yielding.\n";
                break;
            }
        }
        sem.release();
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // evităm busy-loop
    }
}

int main(int argc, char* argv[]) {
    // pornim două procese: parent + child (fork)
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        run_process_logic("Process 2 (Child)");
        return 0;
    } else {
        std::cout << "[Launcher] Forked child with PID " << pid << "\n";
        run_process_logic("Process 1 (Parent)");
        wait(NULL);
    }

    std::cout << "[System] Both processes finished.\n";
    // Pentru UX: nu bloca terminalul aici; doar iesim
    return 0;
}
