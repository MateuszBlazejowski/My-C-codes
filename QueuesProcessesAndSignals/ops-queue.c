#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PARENT_QUEUE_NAME "/parent"
#define WORK_QUEUE_NAME "/work"
#define CHILD_COUNT 4
#define MAX_MSG_COUNT 4
#define MAX_RAND_INT 128
#define ROUNDS 512
#define SLEEP_TIME 20
#define FLOAT_ACCURACY RAND_MAX

typedef struct timespec timespec_t;
typedef struct point
{
    float x;
    float y;
} point_t;

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

typedef struct
{
    int in_circle;
    int total;
    mqd_t mqd;

} info;

void sethandler(void (*f)(int, siginfo_t *, void *), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = f;
    act.sa_flags = SA_SIGINFO;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void receivehandler(int sig, siginfo_t *si, void *ucontext)
{
    info *infoptr = (info *)si->si_value.sival_ptr;

    mqd_t parent_queue_fd = infoptr->mqd;

    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = infoptr;

    if (mq_notify(parent_queue_fd, &sev) == -1)
    {
        if (errno != EBUSY && errno != EBADF)
            ERR("mq_notify");
        return;
    }

    char result;
    unsigned int P;
    while (1)
    {
        if (TEMP_FAILURE_RETRY(mq_receive(parent_queue_fd, &result, sizeof(char), &P)) < 0)
        {
            if (errno = EAGAIN)
            {
                break;
            }
            else
            {
                ERR("mq receive");
            }
        }
        if (result == '0')
        {
            infoptr->total++;
        }
        else if (result == '1')
        {
            infoptr->total++;
            infoptr->in_circle++;
        }
        else
            printf("Unknown result\n");

        printf("Pi approximation: %f\n", (float)(4.0f * (float)infoptr->in_circle / (float)infoptr->total));
    }
}

void msleep(unsigned int milisec)
{
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    timespec_t req = {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if (nanosleep(&req, &req))
        ERR("nanosleep");
}

float rand_float()
{
    int rand_int = rand() % FLOAT_ACCURACY + 1;
    float rand_float = (float)rand_int / (float)FLOAT_ACCURACY;

    return rand_float * 2.0f - 1.0f;
}

void child_work(mqd_t _parent_queue_fd, mqd_t _work_queue_fd)
{
    mqd_t parent_queue_fd = _parent_queue_fd;

    mq_close(parent_queue_fd);
    if (-1 == (parent_queue_fd = mq_open(PARENT_QUEUE_NAME, O_EXCL | O_RDWR)))
        ERR("mq_open");

    point_t point = {};
    unsigned int P;

    float _x, _y;
    char result;
    while (1)
    {
        if (-1 == mq_receive(_work_queue_fd, (char *)&point, 2 * sizeof(float), &P))
        {
            if (errno == EAGAIN)
            {
                break;
            }
            ERR("mq_receive");
        }
        _x = point.x;
        _y = point.y;

        msleep(SLEEP_TIME);

        // printf("[%d] Coordinates: X: %f,Y: %f, ", getpid(), _x, _y);

        if ((double)(_x * _x + _y * _y) <= 1)
        {
            result = '1';
            // printf("Within the circle\n");
        }
        else
        {
            result = '0';
            // printf("Outside the circle the circle\n");
        }

        if (-1 == mq_send(parent_queue_fd, &result, sizeof(char), 1))
        {
            ERR("mq_send");
        }
    }
}

void create_children(mqd_t parent_queue_fd, mqd_t work_queue_fd)
{
    int res;

    for (int i = 0; i < CHILD_COUNT; i++)
    {
        if (-1 == (res = fork()))
            ERR("fork");

        if (res == 0)  // Case child
        {
            child_work(parent_queue_fd, work_queue_fd);
            printf("[%d] Exiting…\n", getpid());
            exit(EXIT_SUCCESS);
        }
    }
}

int main(void)
{
    mq_unlink(PARENT_QUEUE_NAME);
    mq_unlink(WORK_QUEUE_NAME);

    mqd_t parent_queue_fd, work_queue_fd;

    struct mq_attr parent_queue_attributes = {};
    parent_queue_attributes.mq_maxmsg = MAX_MSG_COUNT;
    parent_queue_attributes.mq_msgsize = sizeof(char);
    if (-1 == (parent_queue_fd =
                   mq_open(PARENT_QUEUE_NAME, O_CREAT | O_EXCL | O_RDWR | O_NONBLOCK, 0600, &parent_queue_attributes)))
        ERR("mq_open");

    struct mq_attr work_queue_attributes = {};
    work_queue_attributes.mq_maxmsg = MAX_MSG_COUNT;
    work_queue_attributes.mq_msgsize = 2 * sizeof(float);
    if (-1 == (work_queue_fd = mq_open(WORK_QUEUE_NAME, O_CREAT | O_EXCL | O_RDWR, 0600, &work_queue_attributes)))
        ERR("mq_open");

    // signals

    sethandler(receivehandler, SIGRTMIN);
    info *infoptr = malloc(sizeof(info));

    if (!infoptr)
    {
        ERR("malloc");
    }

    infoptr->mqd = parent_queue_fd;
    infoptr->in_circle = 0;
    infoptr->total = 0;
    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = infoptr;

    if (mq_notify(parent_queue_fd, &sev) == -1)
    {
        ERR("mq_notify");
    }

    create_children(parent_queue_fd, work_queue_fd);

    point_t point = {};

    for (int i = 0; i < ROUNDS; i++)
    {
        point.x = rand_float();
        point.y = rand_float();
        if (-1 == mq_send(work_queue_fd, (const char *)&point, 2 * sizeof(float), 1))
        {
            if (errno != EAGAIN || errno != EINTR)
                ERR("mq_send");
        }
    }

    parent_queue_attributes.mq_flags = O_EXCL | O_NONBLOCK;

    mq_setattr(work_queue_fd, &parent_queue_attributes, NULL);

    while (wait(NULL) > 0)
        ;

    printf("END\n");
    free(infoptr);
    mq_close(parent_queue_fd);
    mq_close(work_queue_fd);
    mq_unlink(PARENT_QUEUE_NAME);
    mq_unlink(WORK_QUEUE_NAME);

    return EXIT_SUCCESS;
}
