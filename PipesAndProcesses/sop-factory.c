
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)             \
    (__extension__({                               \
        long int __result;                         \
        do                                         \
            __result = (long int)(expression);     \
        while (__result == -1L && errno == EINTR); \
        __result;                                  \
    }))
#endif

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void msleep(int millisec)
{
    struct timespec tt;
    tt.tv_sec = millisec / 1000;
    tt.tv_nsec = (millisec % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}

int count_descriptors()
{
    int count = 0;
    DIR* dir;
    struct dirent* entry;
    struct stat stats;
    if ((dir = opendir("/proc/self/fd")) == NULL)
        ERR("opendir");
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    chdir("/proc/self/fd");
    do
    {
        errno = 0;
        if ((entry = readdir(dir)) != NULL)
        {
            if (lstat(entry->d_name, &stats))
                ERR("lstat");
            if (!S_ISDIR(stats.st_mode))
                count++;
        }
    } while (entry != NULL);
    if (chdir(path))
        ERR("chdir");
    if (closedir(dir))
        ERR("closedir");
    return count - 1;  // one descriptor for open directory
}

void usage(char* program)
{
    printf("%s w1 w2 w3\n", program);
    exit(EXIT_FAILURE);
}

void first_brigade_work(int production_pipe_write, int boss_pipe, char* fifoPath)
{
    srand(getpid());

    int sleep;
    int res;
    char letter[1];

    int fd_read = open(fifoPath, O_RDONLY);
    if (fd_read == -1)
    {
        perror("open fifo");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        sleep = rand() % 10;
        sleep++;
        msleep(sleep);
        // char randomletter = 'A' + (rand() % 26);

        if (read(fd_read, letter, 1) == -1)
            ERR("read");

        if (letter[0] < 'a' || letter[0] > 'z')
            continue;
        if ((res = write(production_pipe_write, &letter[0], 1)) == -1)
            ERR("write");
        res = write(boss_pipe, &letter[0], 1);
        if (res == -1 && errno == EAGAIN)
        {
            continue;
        }
        else if (res == -1)
            ERR("write");
    }
    printf("Worker %d from the first brigade: descriptors: %d\n", getpid(), count_descriptors());
    close(fd_read);
}
void second_brigade_work(int production_pipe_write, int production_pipe_read, int boss_pipe)
{
    srand(getpid());
    int sleep;
    int res;
    char buf;
    int iterationNum;
    char letter[1];
    letter[0] = 'q';
    while (1)
    {
        sleep = rand() % 10;
        sleep++;

        res = read(production_pipe_read, &buf, 1);
        if (res == 0)
            break;

        if (res == -1)
            ERR("read");

        if (res == 1)
        {
            msleep(sleep);
            iterationNum = rand() % 4;
            iterationNum++;
            for (int i = 0; i < iterationNum; i++)
            {
                if ((res = write(production_pipe_write, &buf, 1)) == -1)
                    ERR("write");
            }
            res = write(boss_pipe, &letter[0], 1);
            if (res == -1 && errno == EAGAIN)
            {
                continue;
            }
            else if (res == -1)
                ERR("write");
        }
    }
    printf("Worker %d from the second brigade: descriptors: %d\n", getpid(), count_descriptors());
}
void third_brigade_work(int production_pipe_read, int boss_pipe)
{
    srand(getpid());
    int sleep;
    int res;
    char buf;
    char word[6];
    int iterationNum = 0;

    char letter[1];
    letter[0] = 'q';

    while (1)
    {
        sleep = rand() % 3;
        sleep++;
        msleep(sleep);
        res = read(production_pipe_read, &buf, 1);
        if (res == 0)
            break;
        if (res == -1)
            ERR("read");
        if (res == 1)
        {
            word[iterationNum] = buf;
            iterationNum++;
            if (iterationNum == 5)
            {
                iterationNum = 0;
                printf("%s\n", word);
            }
            res = write(boss_pipe, &letter[0], 1);
            if (res == -1 && errno == EAGAIN)
            {
                continue;
            }
            else if (res == -1)
                ERR("write");
        }
    }
    printf("Worker %d from the third brigade: descriptors: %d\n", getpid(), count_descriptors());
}

int main(int argc, char* argv[])
{
    if (argc != 4)
        usage(argv[0]);
    int w1 = atoi(argv[1]), w2 = atoi(argv[2]), w3 = atoi(argv[3]);
    if (w1 < 1 || w1 > 10 || w2 < 1 || w2 > 10 || w3 < 1 || w3 > 10)
        usage(argv[0]);
    // between brigades
    int pipe12[2];
    int pipe23[2];
    // workers - boss
    int* pipes1;
    int* pipes2;
    int* pipes3;

    set_handler(SIG_IGN, SIGPIPE);
    set_handler(SIG_IGN, SIGINT);

    char* fifoPath = "warehouse";
    unlink(fifoPath);
    if (mkfifo(fifoPath, 0666) == -1)
        ERR("mkfifo");

    if ((pipes1 = calloc(w1 * 2, sizeof(int))) == NULL)
        ERR("calloc");
    if ((pipes2 = calloc(w2 * 2, sizeof(int))) == NULL)
        ERR("calloc");
    if ((pipes3 = calloc(w3 * 2, sizeof(int))) == NULL)
        ERR("calloc");

    // pipe inicialization
    if (pipe(pipe12) == -1)
        ERR("pipe");
    if (pipe(pipe23) == -1)
        ERR("pipe");

    for (int i = 0; i < w1; i++)
    {
        if (pipe(&pipes1[2 * i]) == -1)
            ERR("pipe");
        fcntl(2 * i, F_SETFL, O_NONBLOCK);
    }
    for (int i = 0; i < w2; i++)
    {
        if (pipe(&pipes2[2 * i]) == -1)
            ERR("pipe");
        fcntl(2 * i, F_SETFL, O_NONBLOCK);
    }
    for (int i = 0; i < w3; i++)
    {
        if (pipe(&pipes3[2 * i]) == -1)
            ERR("pipe");
        fcntl(2 * i, F_SETFL, O_NONBLOCK);
    }

    // forks:

    int res;

    for (int j = 0; j < w1; j++)
    {
        res = fork();
        if (res == 0)
        {
            close(pipe12[0]);
            for (int i = 0; i < 2; i++)
            {
                close(pipe23[i]);
            }

            for (int i = 0; i < w1; i++)
            {
                if (i != j)
                    close(pipes1[2 * i + 1]);
                close(pipes1[2 * i]);
            }
            for (int i = 0; i < w2; i++)
            {
                close(pipes2[2 * i + 1]);
                close(pipes2[2 * i]);
            }
            for (int i = 0; i < w3; i++)
            {
                close(pipes3[2 * i + 1]);
                close(pipes3[2 * i]);
            }

            first_brigade_work(pipe12[1], pipes1[2 * j + 1], fifoPath);

            close(pipe12[1]);
            close(pipes1[2 * j + 1]);

            free(pipes1);
            free(pipes2);
            free(pipes3);
            exit(EXIT_SUCCESS);
        }
        if (res == -1)
            ERR("fork");
    }
    for (int j = 0; j < w2; j++)
    {
        res = fork();
        if (res == 0)
        {
            close(pipe12[1]);

            close(pipe23[0]);

            for (int i = 0; i < w1; i++)
            {
                close(pipes1[2 * i + 1]);
                close(pipes1[2 * i]);
            }
            for (int i = 0; i < w2; i++)
            {
                if (i != j)
                    close(pipes2[2 * i + 1]);
                close(pipes2[2 * i]);
            }
            for (int i = 0; i < w3; i++)
            {
                close(pipes3[2 * i + 1]);
                close(pipes3[2 * i]);
            }
            second_brigade_work(pipe23[1], pipe12[0], pipes2[2 * j + 1]);

            close(pipe12[0]);
            close(pipe23[1]);
            close(pipes2[2 * j + 1]);

            free(pipes1);
            free(pipes2);
            free(pipes3);
            exit(EXIT_SUCCESS);
        }
        if (res == -1)
            ERR("fork");
    }
    for (int j = 0; j < w3; j++)
    {
        res = fork();
        if (res == 0)
        {
            for (int i = 0; i < 2; i++)
            {
                close(pipe12[i]);
            }

            close(pipe23[1]);

            for (int i = 0; i < w1; i++)
            {
                close(pipes1[2 * i + 1]);
                close(pipes1[2 * i]);
            }
            for (int i = 0; i < w2; i++)
            {
                close(pipes2[2 * i + 1]);
                close(pipes2[2 * i]);
            }
            for (int i = 0; i < w3; i++)
            {
                if (j != i)
                    close(pipes3[2 * i + 1]);
                close(pipes3[2 * i]);
            }

            third_brigade_work(pipe23[0], pipes3[2 * j + 1]);

            close(pipe23[0]);
            close(pipes3[2 * j + 1]);

            free(pipes1);
            free(pipes2);
            free(pipes3);
            exit(EXIT_SUCCESS);
        }
        if (res == -1)
            ERR("fork");
    }

    // boss_rouine:

    for (int i = 0; i < 2; i++)
    {
        close(pipe12[i]);
    }
    for (int i = 0; i < 2; i++)
    {
        close(pipe23[i]);
    }

    for (int i = 0; i < w1; i++)
    {
        close(pipes1[2 * i + 1]);
    }
    for (int i = 0; i < w2; i++)
    {
        close(pipes2[2 * i + 1]);
    }
    for (int i = 0; i < w3; i++)
    {
        close(pipes3[2 * i + 1]);
    }
    printf("Boss: descriptors: %d\n", count_descriptors());
    while (wait(NULL) > 0)
        ;

    // cleanup
    for (int i = 0; i < w1; i++)
    {
        close(pipes1[2 * i]);
    }
    for (int i = 0; i < w2; i++)
    {
        close(pipes2[2 * i]);
    }
    for (int i = 0; i < w3; i++)
    {
        close(pipes3[2 * i]);
    }

    free(pipes1);
    free(pipes2);
    free(pipes3);
    printf("Boss: descriptors: %d\n", count_descriptors());
    unlink(fifoPath);
    return EXIT_SUCCESS;
}
