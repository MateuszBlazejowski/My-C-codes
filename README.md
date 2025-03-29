
# README - My C codes

This repository contains C programs designed to demonstrate the knowledge.
This repository contains C programs designed to demonstrate my knowledge of operating systems. The programs were written during my Operating Systems course and cover various concepts such as process management, inter-process communication (IPC), file handling, and more. Each program is placed in its own directory, allowing for focused exploration of specific topics.

## Repo Overview

#Pipes 

The program involves three groups of workers(processes) (referred to as brigades), each performing a specific task in the production pipeline:

1. **First Brigade (Workers w1)**: Reads data from a FIFO (named pipe) and passes it down the pipeline.
2. **Second Brigade (Workers w2)**: Reads data from the first brigade, processes it, and writes it to the next stage in the pipeline.
3. **Third Brigade (Workers w3)**: Reads data from the second brigade, processes it further, and prints the result when a complete "word" is formed.

The "boss" process supervises the entire workflow and ensures proper cleanup after the workers are 
done.

Each worker has its own pipe to the boss and shares a pipe to the next brigade with fellow workers. 
<img src="PipesAndProcesses/PipeSchema.png" alt="Alt text"  />  

Open PipesAndProcesses directory to see more. 

