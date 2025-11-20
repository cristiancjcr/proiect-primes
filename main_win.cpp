#include <windows.h>
#include <iostream>
#include <cmath>

using namespace std;

bool isPrime(int n) {
    if (n < 2) return false;
    for (int i = 2; i <= sqrt(n); ++i)
        if (n % i == 0) return false;
    return true;
}

void ChildProcess(int start, int end, HANDLE writePipe) {
    for (int num = start; num <= end; ++num) {
        if (isPrime(num)) {
            DWORD written;
            WriteFile(writePipe, &num, sizeof(num), &written, nullptr); // write prime
        }
    }
    CloseHandle(writePipe); // inchidem pipe
}

int main() {
    const int TOTAL = 10000;
    const int NUM_PROCESSES = 10;
    const int INTERVAL = TOTAL / NUM_PROCESSES;

    HANDLE readPipes[NUM_PROCESSES];
    HANDLE writePipes[NUM_PROCESSES];

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        if (!CreatePipe(&readPipes[i], &writePipes[i], &sa, 0)) {
            cerr << "pipe creation failed" << endl;
            return 1;
        }
    }

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        // launch child in same process for simplicity
        ChildProcess(i * INTERVAL + 1, (i + 1) * INTERVAL, writePipes[i]);
    }

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        int num;
        DWORD readBytes;
        while (ReadFile(readPipes[i], &num, sizeof(num), &readBytes, nullptr) && readBytes > 0) {
            cout << num << " ";
        }
        CloseHandle(readPipes[i]);
    }
    cout << endl;

    return 0;
}
