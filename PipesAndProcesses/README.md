
# README - Process and Pipe Management Program

This repository contains a C program designed to demonstrate the use of pipes and process management, wrote as a part of operating systems course. The program simulates a production line with multiple "brigades" (processes) that handle data through pipes. It focuses on process creation, inter-process communication (IPC) using pipes, and handling concurrent processes.

## Program Overview

The program involves three groups of workers (referred to as brigades), each performing a specific task in the production pipeline:

1. **First Brigade (Workers w1)**: Reads data from a FIFO (named pipe) and passes it down the pipeline.
2. **Second Brigade (Workers w2)**: Reads data from the first brigade, processes it, and writes it to the next stage in the pipeline.
3. **Third Brigade (Workers w3)**: Reads data from the second brigade, processes it further, and prints the result when a complete "word" is formed.

The "boss" process supervises the entire workflow and ensures proper cleanup after the workers are done.

## Key Concepts and Features

- **Pipes**: The program uses unnamed pipes for communication between different brigades and between the workers and the boss. Each brigade is represented by a set of processes that use pipes to communicate.
- **Process Management**: The program forks multiple processes for each brigade. Each process runs concurrently, performing its task independently, which simulates a production line.
- **Error Handling**: The program includes robust error handling with custom macros for managing errors and handling signal interruptions (e.g., `SIGPIPE`, `SIGINT`).
- **FIFO (Named Pipe)**: The program also interacts with a named pipe (`warehouse`), which is used by the first brigade to read data.
- **Random Delays**: Each worker has random sleep intervals to simulate the variability in production rates and timing.

## Usage

To build and run the program, you can use the provided Makefile. Follow these steps:

### Step 1: Build the Program
Run the following command to compile the program using the Makefile:

```bash
make
```

This will use the flags specified in the Makefile to compile the program with debugging and optimization options.

### Step 2: Run the Program
Once compiled, execute the program with three arguments representing the number of workers in each brigade. The arguments specify the number of workers for the first, second, and third brigades.

```bash
./sop-factory <w1> <w2> <w3>
```

- `<w1>`: Number of workers in the first brigade (1-10).
- `<w2>`: Number of workers in the second brigade (1-10).
- `<w3>`: Number of workers in the third brigade (1-10).

For example:

```bash
./sop-factory 3 3 2
```

This will start the program with 3 workers in the first and second brigades, and 2 workers in the third brigade.

## Example Output

The output consists of messages printed by each worker and the boss, indicating their process ID and the number of file descriptors they are using, to ensure correct pipe handling. For instance:

```
Worker 1234 from the first brigade: descriptors: 5
Worker 1235 from the second brigade: descriptors: 6
Worker 1236 from the third brigade: descriptors: 5
Boss: descriptors: 8
```

Each brigade will process and pass letters through the pipes, forming words and printing them to the console.

## Cleanup

The program performs cleanup after execution, closing all pipes and unlinking the FIFO.

## Notes

- Ensure that the FIFO path (`warehouse`) is accessible and permissions are correctly set.
- The program demonstrates basic pipe handling, process management, and the use of signals for process coordination in Unix-like operating systems.
