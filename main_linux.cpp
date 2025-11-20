#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <cmath>
#include <cstring>

using namespace std;

bool isPrime(int n) {
	if (n < 2) return false;
	for (int i = 2; i <= sqrt(n); ++i)
		if (n % i == 0) return false;
	return true;
}

int main() {
	const int TOTAL = 10000;
	const int NUM_PROCESSES = 10;
	const int INTERVAL = TOTAL / NUM_PROCESSES;

	int pipes[NUM_PROCESSES][2];

	for (int i = 0; i < NUM_PROCESSES; ++i) {
		if (pipe(pipes[i]) == -1) {
			perror("pipe");
			return 1;
		}
	}

	for (int i = 0; i < NUM_PROCESSES; ++i) {
		pid_t pid = fork();
		if (pid == -1) {
			perror("fork");
			return 1;
		}
		if (pid == 0) { // child process
			close(pipes[i][0]); // nu citim
			int start = i * INTERVAL + 1;
			int end = (i + 1) * INTERVAL;
			for (int num = start; num <= end; ++num) {
				if (isPrime(num)) {
					write(pipes[i][1], &num, sizeof(num)); // scriem numere prime
				}
			}
			close(pipes[i][1]);
			return 0;
		} else {
			close(pipes[i][1]); // parent nu scrie
		}
	}

	for (int i = 0; i < NUM_PROCESSES; ++i) {
		int num;
		while (read(pipes[i][0], &num, sizeof(num)) > 0) {
			cout << num << " ";
		}
		close(pipes[i][0]);
	}
	cout << endl;

	for (int i = 0; i < NUM_PROCESSES; ++i) {
		wait(nullptr);
	}

	return 0;
}

