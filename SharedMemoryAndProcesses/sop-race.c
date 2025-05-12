#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define LEADERBOARD_FILENAME "./leaderboard"
#define LEADERBOARD_ENTRY_LEN 32

#define MIN_TRACK_LEN 16
#define MAX_TRACK_LEN 256
#define MIN_DOG_COUNT 2
#define MAX_DOG_COUNT 6

#define MIN_MOVEMENT 1
#define MAX_MOVEMENT 6
#define MIN_SLEEP 250
#define MAX_SLEEP 1500

#define ERR(source)                                                            \
  do {                                                                         \
    fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);                            \
    perror(source);                                                            \
    kill(0, SIGKILL);                                                          \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

void usage(char *program_name) {
  fprintf(stderr, "Usage: \n");
  fprintf(stderr, "  %s L N\n", program_name);
  fprintf(stderr, "    L - race track length, %d <= L <= %d\n", MIN_TRACK_LEN,
          MAX_TRACK_LEN);
  fprintf(stderr, "    N - number of dogs, %d <= N <= %d\n", MIN_DOG_COUNT,
          MAX_DOG_COUNT);
  exit(EXIT_FAILURE);
}

void msleep(unsigned int milli) {
  time_t sec = (int)(milli / 1000);
  milli = milli - (sec * 1000);
  struct timespec ts = {0};
  ts.tv_sec = sec;
  ts.tv_nsec = milli * 1000000L;
  if (nanosleep(&ts, &ts))
    ERR("nanosleep");
}

typedef struct sharedData {
  int racetrack[MAX_TRACK_LEN];
  pthread_mutex_t mutexArrDirection[MAX_DOG_COUNT];
  int dogsDirection[MAX_DOG_COUNT];
  int dogsPIDs[MAX_DOG_COUNT];

  pthread_mutex_t finishedMutex;
  int dogsFinished;

  pthread_mutex_t mutexArr[MAX_TRACK_LEN];
  pthread_barrier_t barrier_dogs;

  pthread_mutex_t leaderboardMutex;
  char *leaderboard;
} sharedData_t;

void childWork(int racetrackLength, sharedData_t *sharedData, int dogID) {
  srand(getpid());
  sharedData->dogsPIDs[dogID] = getpid();

  pthread_barrier_wait(&sharedData->barrier_dogs);
  printf("[%d] waf waf (started race) \n", getpid());

  int direction = 1;
  int sleepTime = 0;
  int movemementDistance = 0;
  int pos = -1;
  int oldpos = -1;
  int newpos;

  while (1) {
    sleepTime = rand() % 1001 + 250;
    msleep(sleepTime);
    movemementDistance = rand() % 6 + 1;

    pthread_mutex_lock(&sharedData->mutexArrDirection[dogID]);
    sharedData->dogsDirection[dogID] = direction;
    pthread_mutex_unlock(&sharedData->mutexArrDirection[dogID]);

    oldpos = pos;
    newpos = pos + movemementDistance * direction;

    if (newpos >= racetrackLength) // checking if we exceeded the barrier
    {
      newpos = racetrackLength - 1;
    }
    if (newpos < 0 && direction == -1) {
      printf("[%d] waf waf (finished race) \n", getpid());

      pthread_mutex_lock(&sharedData->mutexArr[oldpos]);
      sharedData->racetrack[oldpos] = 0;
      pthread_mutex_unlock(&sharedData->mutexArr[oldpos]);
      int placement;

      pthread_mutex_lock(&sharedData->finishedMutex);
      placement = ++sharedData->dogsFinished;
      pthread_mutex_unlock(&sharedData->finishedMutex);

      // Write to leaderboard
      pthread_mutex_lock(&sharedData->leaderboardMutex);

      char entry[LEADERBOARD_ENTRY_LEN];
      snprintf(entry, sizeof(entry), "%d. Dog %d\n", placement, getpid());
      int offset = (placement - 1) * LEADERBOARD_ENTRY_LEN;
      memset(sharedData->leaderboard + offset, ' ', LEADERBOARD_ENTRY_LEN);
      memcpy(sharedData->leaderboard + offset, entry, strlen(entry));

      if (msync(sharedData->leaderboard, MAX_DOG_COUNT * LEADERBOARD_ENTRY_LEN,
                MS_SYNC) < 0)
        ERR("msync leaderboard");

      pthread_mutex_unlock(&sharedData->leaderboardMutex);
      break;
    }

    pthread_mutex_lock(&sharedData->mutexArr[newpos]);

    if (sharedData->racetrack[newpos] != 0) // field occupied
    {
      printf("[%d] waf waf (the field [%d] is occupied) \n", getpid(), newpos);
      pthread_mutex_unlock(&sharedData->mutexArr[newpos]);
    } else // movement
    {
      printf("[%d] waf waf (new position = [%d]) \n", getpid(), newpos);

      sharedData->racetrack[newpos] = getpid();
      pthread_mutex_unlock(&sharedData->mutexArr[newpos]);
      pos = newpos;
      if (pos == racetrackLength - 1) {
        direction = -1;
        sharedData->dogsDirection[dogID] = -1;
      }

      if (oldpos != -1)
        pthread_mutex_lock(&sharedData->mutexArr[oldpos]);
      if (oldpos > -1 && oldpos < racetrackLength) {
        sharedData->racetrack[oldpos] = 0;
      }
      if (oldpos != -1)
        pthread_mutex_unlock(&sharedData->mutexArr[oldpos]);
    }
  }
}

void commentatorWork(int racetrackLength, sharedData_t *sharedData,
                     int dogsCount) {
  pthread_barrier_wait(&sharedData->barrier_dogs);
  while (1) {
    msleep(1000);
    if (sharedData->dogsFinished >= dogsCount)
      break;
    int dogID;
    for (int i = 0; i < MAX_TRACK_LEN; i++) {
      pthread_mutex_lock(&sharedData->mutexArr[i]);
    }
    for (int i = 0; i < racetrackLength; i++) {
      if (sharedData->racetrack[i] == 0) {
        printf(" ---");
      } else {
        for (int j = 0; j < dogsCount; j++) {
          if (sharedData->racetrack[i] == sharedData->dogsPIDs[j]) {
            dogID = j;
            break;
          }
        }
        pthread_mutex_lock(&sharedData->mutexArrDirection[dogID]);
        if (1 == sharedData->dogsDirection[dogID])
          printf(" %d >", sharedData->racetrack[i]);
        else
          printf(" < %d", sharedData->racetrack[i]);
        pthread_mutex_unlock(&sharedData->mutexArrDirection[dogID]);
      }
    }
    printf("\n");
    for (int i = 0; i < MAX_TRACK_LEN; i++) {
      pthread_mutex_unlock(&sharedData->mutexArr[i]);
    }
  }
}

void createChildren(int dogsCount, int racetrackLength,
                    sharedData_t *sharedData) {
  int ret;

  for (int i = 0; i < dogsCount; i++) {
    if (-1 == (ret = fork()))
      ERR("fork");

    if (ret == 0) // Child
    {
      printf("[%d] dog arrived\n", getpid());
      childWork(racetrackLength, sharedData, i);
      // Cleanup
      munmap(sharedData, sizeof(sharedData_t));
      exit(EXIT_SUCCESS);
    }
    // Parent
  }
  if (-1 == (ret = fork()))
    ERR("fork");
  if (ret == 0) // Child
  {
    commentatorWork(racetrackLength, sharedData, dogsCount);
    munmap(sharedData, sizeof(sharedData_t));
    exit(EXIT_SUCCESS);
  }
}

int main(int argc, char **argv) {
  if (argc != 3)
    usage(argv[0]);
  int racetrackLength, dogsCount;

  racetrackLength = atoi(argv[1]);
  dogsCount = atoi(argv[2]);

  if (racetrackLength < MIN_TRACK_LEN || racetrackLength > MAX_TRACK_LEN)
    usage(argv[0]);

  if (dogsCount < MIN_DOG_COUNT || dogsCount > MAX_DOG_COUNT)
    usage(argv[0]);

  // shared memory maping
  sharedData_t *sharedData =
      mmap(NULL, sizeof(sharedData_t), PROT_READ | PROT_WRITE,
           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (sharedData == MAP_FAILED)
    ERR("mmap");

  // barrier attributes
  pthread_barrierattr_t attr;
  if (pthread_barrierattr_init(&attr) != 0) {
    ERR("pthread_barrierattr_init");
    exit(EXIT_FAILURE);
  }
  if (pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
    ERR("pthread_barrierattr_setpshared");
    exit(EXIT_FAILURE);
  }

  // barrier init
  if (pthread_barrier_init(&sharedData->barrier_dogs, &attr, dogsCount + 1) !=
      0) {
    perror("pthread_barrier_init");
    exit(EXIT_FAILURE);
  }
  pthread_barrierattr_destroy(&attr);

  // mutexes
  pthread_mutexattr_t mutexAttr;
  pthread_mutexattr_init(&mutexAttr);
  pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);

  for (int i = 0; i < MAX_TRACK_LEN; i++) {
    pthread_mutex_init(&sharedData->mutexArr[i], &mutexAttr);
    sharedData->racetrack[i] = 0;
  }
  for (int i = 0; i < MAX_DOG_COUNT; i++) {
    pthread_mutex_init(&sharedData->mutexArrDirection[i], &mutexAttr);
  }
  pthread_mutex_init(&sharedData->finishedMutex, &mutexAttr);
  pthread_mutexattr_destroy(&mutexAttr);

  for (int i = 0; i < MAX_DOG_COUNT; i++) {
    sharedData->dogsDirection[i] = 1;
  }
  pthread_mutex_init(&sharedData->leaderboardMutex, &mutexAttr);
  sharedData->dogsFinished = 0;
  // laderboard
  int leaderboard_fd =
      open(LEADERBOARD_FILENAME, O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (leaderboard_fd < 0)
    ERR("open");

  if (ftruncate(leaderboard_fd, MAX_DOG_COUNT * LEADERBOARD_ENTRY_LEN) < 0)
    ERR("ftruncate");

  sharedData->leaderboard =
      mmap(NULL, MAX_DOG_COUNT * LEADERBOARD_ENTRY_LEN, PROT_READ | PROT_WRITE,
           MAP_SHARED, leaderboard_fd, 0);

  if (sharedData->leaderboard == MAP_FAILED)
    ERR("mmap leaderboard");

  close(leaderboard_fd);

  createChildren(dogsCount, racetrackLength, sharedData);

  while (wait(NULL) > 0)
    ;

  // PRINT LEADERBOARD
  printf("\n=== LEADERBOARD ===\n");
  for (int i = 0; i < dogsCount; i++) {
    char entry[LEADERBOARD_ENTRY_LEN + 1];
    memcpy(entry, sharedData->leaderboard + i * LEADERBOARD_ENTRY_LEN,
           LEADERBOARD_ENTRY_LEN);
    entry[LEADERBOARD_ENTRY_LEN] = '\0';
    printf("%s\n", entry);
  }

  // cleanup
  pthread_barrier_destroy(&sharedData->barrier_dogs);

  for (int i = 0; i < MAX_TRACK_LEN; i++) {
    pthread_mutex_destroy(&sharedData->mutexArr[i]);
  }
  for (int i = 0; i < MAX_DOG_COUNT; i++) {
    pthread_mutex_destroy(&sharedData->mutexArrDirection[i]);
  }

  pthread_mutex_destroy(&sharedData->leaderboardMutex);
  munmap(sharedData->leaderboard, MAX_DOG_COUNT * LEADERBOARD_ENTRY_LEN);

  pthread_mutex_destroy(&sharedData->finishedMutex);
  munmap(sharedData, sizeof(sharedData_t));

  return EXIT_SUCCESS;
}
