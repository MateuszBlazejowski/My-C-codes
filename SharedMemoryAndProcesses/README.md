
# README - Dog Race simulation using POSIX processes and mmap

This program implements a simulation of a dog race using multiple processes and inter-process synchronization through POSIX shared memory, mutexes, and barriers. The goal is to demonstrate synchronization between processes accessing and modifying shared data structures, and to simulate race dynamics with randomly moving dogs and live commentary.

## Program Overview

The simulation models a race where multiple "dogs" (represented by processes) move back and forth along a track of configurable length. Each dog moves at random intervals and distances. A "commentator" process monitors the track and displays the current state. Once a dog finishes the race, it records its result to a leaderboard file.

### Key Components:

1. **Dog Processes**: Each dog moves along the racetrack and tries to reach the end and return. Upon finishing, it writes its result to the shared leaderboard.
2. **Commentator Process**: Periodically prints the current state of the racetrack, showing dog positions and directions.
3. **Shared Memory**: All processes access a shared data structure (`sharedData_t`) that holds the track state, dog directions, positions, and the leaderboard.
4. **Synchronization Primitives**:
   - **Mutexes** to guard individual racetrack fields and shared data.
   - **Barrier** to synchronize the start of the race.

## Features and Concepts

- **Process Synchronization**: Demonstrates coordination using `pthread_mutex_t` and `pthread_barrier_t` shared between processes via `mmap()`.
- **Randomized Behavior**: Each dog moves with randomized sleep times and distances, simulating realistic racing unpredictability.
- **Direction Management**: Dogs reverse direction after reaching the end of the track and finish the race when they return to the start.
- **Shared Leaderboard**: Upon finishing, each dog updates a shared leaderboard stored in a memory-mapped file.

## Usage

### Build

```bash
make
```

### Run

```bash
./sop-race L N
```

Where:
- `L` is the racetrack length (`16 <= L <= 256`)
- `N` is the number of dogs (`2 <= N <= 6`)

Example:

```bash
./sop-race 50 4
```

### Example Output

```
[12345] dog arrived
[12346] waf waf (started race)
[12347] waf waf (started race)
[12346] waf waf (new position = [5])
[12347] waf waf (the field [5] is occupied)
<12346 --- --- >12347 ...
```

After the race, a leaderboard is printed:

```
=== LEADERBOARD ===
1. Dog 12346
2. Dog 12347
...
```

## Cleanup

The program ensures proper cleanup:
- All child processes terminate cleanly.
- All mutexes and barriers are destroyed.
- Shared memory regions are unmapped.
- The leaderboard file is properly truncated and closed.

## Notes

- Mutexes and barriers are initialized with `PTHREAD_PROCESS_SHARED` to allow usage between processes.
- Race visualization is text-based and updated every second by the commentator.
- Each dog prints its status and position to the console for traceability.