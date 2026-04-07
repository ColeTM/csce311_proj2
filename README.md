# CSCE 311 Project 2 -- Local IPC and Resource Management

This project's goal is to integrate POSIX threads, Unix domain sockets, binary protocol framing, controlled resource allocation, and structured concurrent computation. Given a functional client, the task was to write a server that could accept datagrams sent from a client containing text files, perform hashing operations on its contents, and then stream the hashing results back to the client.

## Source code file contents

All of the source code for the server is written in `src/proj2_server.cc`. Its contents include:

- `File` struct: stores the necessary information about each file passed in a datagram request (its name and the number of rows it has)
- `DatagramContent` struct: stores the necessary information about a datagram received by the server (its endpoint and a vector of all of its files)
- `ParseMessage()` function: takes in a string (the message passed by the datagram) and parses its contents to build the relevant File and DatagramContent objects
- `StartRoutine()` function: the work done by a thread, including getting a message from the queue, calling ParseMessage(), acquiring the necessary resources and hashing all of the contents of the message, and then concatenating the hashes into one long string and streaming that result back to the client
- `main()` function: reads the allocated number of reading and solving resources from the command line, initializing them along with the server socket, dispatching threads to wait for work, and then waits in a loop for a datagram to be sent. When it is signalled to terminate, it exits that loop, joins the threads, and destroys its resources before returning

## Additional design information

### Safe concurrency

Because several threads are running concurrently, this program must avoid race conditions. `msg_queue_mutex` controls access to the message queue, ensuring that only one thread can access this queue at a time. It also uses a producer-consumer style semaphore that signals whether any work is available by incrementing whenever a message is added to the queue and decrementing whenever a thread pops a message.

### Resource allocation

Two types of resources are allocated during this program to do the necessary work: file readers and SHA solvers. To prevent deadlock, SHA solvers are always allocated to threads before file readers. Then, when the thread is done, it releases the file readers before the SHA solvers. Enforcing this order eliminates circular wait, preventing deadlock from occurring.

### Signal Handling

This program includes handles for SIGINT and SIGTERM. A global variable, `terminate`, is initially set to 0 (false). While `terminate` is false, the server waits in a loop to receive a datagram. Using a `timeval` and `setsockopt()`, it checks the `terminate` condition every second. Once this condition becomes true, either through ctrl+C or the OS/another process, it exits this waiting loop and proceeds with a graceful shutdown.

## Running the program

The project folder comes with a makefile (the working files are already included in this repo, but this allows you to easily call make if you choose to edit the project and need to build it again).

To start the server, run a command of this format from the project root:
`bin/proj2-server <server_name> <num_file_readers> <num_SHA_solvers>`

In another terminal, run the client with a command of this format:
`bin/proj2-client <reply_name> <server_name> <file1> [file2 ...]`

Example command line calls below.

Server terminal:
```bash
bin/proj2-server datagram_host 100 200
```

Client terminal:
```bash
bin/proj2-client host_n datagram_host dat/*
```
