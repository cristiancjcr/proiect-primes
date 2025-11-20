// windows/participant.cpp
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <iostream>
#include <random>
#include <thread>
#include <chrono>
#include <string>

const char* MAPPING_NAME = "Global\\MyCounterShm_v3";
const char* SEM_NAME = "Global\\MyCounterSem_v3";
const int TARGET_NUMBER = 1000;

struct SharedData { int counter; };

class CrossPlatformSemaphore {
    HANDLE sem_handle;
public:
    CrossPlatformSemaphore() {
        // CreateSemaphore returns handle and GetLastError tells if existed
        sem_handle = CreateSemaphoreA(NULL, 1, 1, SEM_NAME);
        if (sem_handle == NULL) {
            std::cerr << "CreateSemaphore failed: " << GetLastError() << "\n";
            exit(1);
        }
    }
    ~CrossPlatformSemaphore() {
        CloseHandle(sem_handle); // nu unlink automat
    }
    void acquire() {
        WaitForSingleObject(sem_handle, INFINITE);
    }
    void release() {
        ReleaseSemaphore(sem_handle, 1, NULL);
    }
};

class SharedMemory {
    SharedData* data_ptr;
    HANDLE hMap;
    bool is_creator;
public:
    SharedMemory() : data_ptr(nullptr), hMap(NULL), is_creator(false) {
        // CreateFileMapping with INVALID_HANDLE_VALUE -> backing in pagefile
        hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedData), MAPPING_NAME);
        if (hMap == NULL) {
            std::cerr << "CreateFileMapping failed: " << GetLastError() << "\n";
            exit(1);
        }
        // daca nu exista anterior, GetLastError() != ERROR_ALREADY_EXISTS
        if (GetLastError() != ERROR_ALREADY_EXISTS) is_creator = true;

        data_ptr = (SharedData*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
        if (data_ptr == NULL) {
            std::cerr << "MapViewOfFile failed: " << GetLastError() << "\n";
            CloseHandle(hMap);
            exit(1);
        }

        if (is_creator) {
            data_ptr->counter = 0;
            std::cout << "[System] Memory initialized to 0.\n";
        }
    }

    ~SharedMemory() {
        UnmapViewOfFile(data_ptr);
        CloseHandle(hMap);
    }

    SharedData* get() { return data_ptr; }
};

void run_process_logic(const std::string& name) {
    SharedMemory shm;
    CrossPlatformSemaphore sem;
    SharedData* data = shm.get();

    std::random_device rd;
    std::mt19937 gen((unsigned) GetCurrentProcessId() ^ rd());
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
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            } else {
                std::cout << "[" << name << "] Coin 1 -> Yielding.\n";
                break;
            }
        }
        sem.release();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main(int argc, char* argv[]) {
    // pe Windows lansez un child prin CreateProcess; child-ul ruleaza logica
    if (argc > 1 && std::string(argv[1]) == "child_worker") {
        run_process_logic("Process 2 (Child)");
        return 0;
    }

    std::cout << "[Launcher] Starting Process 1 and spawning Process 2...\n";

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::string cmdLine = std::string(argv[0]) + " child_worker";
    // buffer pentru CreateProcess (mutabil)
    char cmdLineMutable[1024];
    strcpy_s(cmdLineMutable, sizeof(cmdLineMutable), cmdLine.c_str()); // folosit pentru CreateProcess

    if (!CreateProcessA(NULL, cmdLineMutable, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "Failed to create child process: " << GetLastError() << "\n";
        return 1;
    }

    run_process_logic("Process 1 (Parent)");

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    std::cout << "[System] Both processes finished.\n";
    // pe Windows uneori e util să aștepți un ENTER; se poate elimina
    std::cin.ignore();
    return 0;
}
