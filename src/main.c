#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"
#include "sys/mman.h"
#include "string.h"
#include "signal.h"
#include "fcntl.h"

#define check(VALUE, MSG, BADVAL) if (VALUE == BADVAL) { perror(MSG); exit(1); }

int main() {
	size_t pagesize = sysconf(_SC_PAGESIZE);

	int pid = getpid();

	char fn1[256];
	char fn2[256];
	if (scanf("%s", fn1) <= 0) {
		perror("scanf error");
		return -1;
	}
	if (scanf("%s", fn2) <= 0) {
		perror("scanf error");
		return -1;
	}
	getchar();

	int file1 = open(fn1, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	check(file1, "open 1 error", 0)
	int file2 = open(fn2, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
	check(file2, "open 2 error", 0)

	char mfilename1[] = "mmap1";
	char mfilename2[] = "mmap2";
	int mfile1 = open(mfilename1, O_RDWR | O_CREAT, S_IRWXU);
	check(mfile1, "open 1 error", 0)
	int mfile2 = open(mfilename2, O_RDWR | O_CREAT, S_IRWXU);
	check(mfile2, "open 2 error", 0)
	size_t i1 = 0, i2 = 0;

	ftruncate(mfile1, pagesize);
	ftruncate(mfile2, pagesize);

	int id1 = fork();  

	if (id1 == -1)  {
		perror("fork 1 error");
		return -1;
		
	} else if (id1 == 0) {
		close(file2);
		close(mfile2);
		check(dup2(file1, fileno(stdout)), "dup2 error", -1)
		close(file1);

		char spid[10];
		snprintf(spid, 10, "%d", pid);
		char* const args[] = {"child", mfilename1, spid, (char *)NULL};
		check(execv("child", args), "execv child 1 error", -1)

	} else {
		int id2 = fork();

		if (id2 == -1)  {
			perror("fork 2 error");
			return -1;

		} else if (id2 == 0) {
			close(file1);
			close(mfile1);
			check(dup2(file2, fileno(stdout)), "dup2 error", -1)
			close(file2);

			char spid[10];
			snprintf(spid, 10, "%d", pid);
			char* const args[] = {"child", mfilename2, spid, (char *)NULL};
			check(execv("child", args), "execv child 2 error", -1)

		} else {
			close(file1);
			close(file2);

			char *fmap1 = (char *)mmap(NULL, pagesize, PROT_WRITE | PROT_READ, MAP_SHARED, mfile1, 0);
			check(fmap1, "mmap 1 error", MAP_FAILED)
			char *fmap2 = (char *)mmap(NULL, pagesize, PROT_WRITE | PROT_READ, MAP_SHARED, mfile2, 0);
			check(fmap2, "mmap 2 error", MAP_FAILED)

			sigset_t set;
			check(sigemptyset(&set), "sigemptyset error", -1)
			check(sigaddset(&set, SIGUSR1), "sigaddset error", -1)
			check(sigprocmask(SIG_BLOCK, &set, NULL), "sigprocmask error", -1)
			int sig;

			char c;
			char str[10];
			str[0] = '\0';
			int n = 0;
			while (scanf("%c", &c) > 0) {
				if (c != '\n') {
					if (n < 10) {
						str[n] = c;
					} else if (str[0] != '\0') {
						for (int i = 0; i < 10; ++i) {
							fmap2[i2] = str[i];
							if (++i2 == pagesize) {
								i2 = 0;
								check(kill(id2, SIGUSR1), "send signal to child 2 error", -1)
								check(sigwait(&set, &sig), "sigwait error", -1)
							}
							str[i] = '\0';
						}
						fmap2[i2] = c;
						if (++i2 == pagesize) {
							i2 = 0;
							check(kill(id2, SIGUSR1), "send signal to child 2 error", -1)
							check(sigwait(&set, &sig), "sigwait error", -1)
						}
					} else {
						fmap2[i2] = c;
						if (++i2 == pagesize) {
							i2 = 0;
							check(kill(id2, SIGUSR1), "send signal to child 2 error", -1)
							check(sigwait(&set, &sig), "sigwait error", -1)
						}
					}
					++n;
				} else {
					if (str[0] != '\0') {
						for (int i = 0; i < n; ++i) {
							fmap1[i1] = str[i];
							if (++i1 == pagesize) {
								i1 = 0;
								check(kill(id1, SIGUSR1), "send signal to child 1 error", -1)
								check(sigwait(&set, &sig), "sigwait error", -1)
							}
							str[i] = '\0';
						}
						fmap1[i1] = c;
						i1 = 0;
						check(kill(id1, SIGUSR1), "send signal to child 1 error", -1)
						check(sigwait(&set, &sig), "sigwait error", -1)
					} else {
						fmap2[i2] = c;
						i2 = 0;
						check(kill(id2, SIGUSR1), "send signal to child 2 error", -1)
						check(sigwait(&set, &sig), "sigwait error", -1)
					}
					n = 0;
				}
			}
			c = '\0';
			fmap1[0] = c;
			fmap2[0] = c;
			check(kill(id1, SIGUSR1), "send signal to child 1 error", -1)
			check(kill(id2, SIGUSR1), "send signal to child 2 error", -1)
			check(munmap(fmap1, pagesize), "munmap error", -1);
			check(munmap(fmap2, pagesize), "munmap error", -1);
			close(mfile1);
			close(mfile2);
			check(remove(mfilename1), "remove 1 error", -1)
			check(remove(mfilename2), "remove 2 error", -1)
		}
	}
	return 0;
}
