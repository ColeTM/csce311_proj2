// copyright ColeTM 2026

#include"proj2/lib/domain_socket.h"
#include"proj2/lib/file_reader.h"
#include"proj2/lib/sha_solver.h"
#include"proj2/lib/thread_log.h"
#include"proj2/lib/timings.h"
#include<iostream>
#include<pthread.h>
#include<string>
#include<csignal>
#include<cstdint>
#include<vector>
#include<cstring>
#include<queue>
#include<sys/sysinfo.h>
#include<semaphore.h>

using namespace proj2;

// struct to hold info about a single file in a datagram request
struct File {
  std::string path;
  std::uint32_t num_rows;
};

// struct to hold a datagram request's endpoint and all of its files' info
struct DatagramContent {
  std::string endpoint;
  std::vector<File> files;
};

volatile sig_atomic_t terminate = 0;

void handle_signal(int) {
  terminate = 1;
}

// initialize queue, mutex, and semaphore
std::queue<std::string> msg_queue;
pthread_mutex_t msg_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t msg_semaphore;

DatagramContent ParseMessage(const std::string msg) {
  DatagramContent ret;  // initialize datagram to return
  const char* m = msg.data();  // pointer m points to beginning of msg
  // parse length of endpoint name
  std::uint32_t endpoint_length;
  std::memcpy(&endpoint_length, m, 4);
  m += 4;
  // parse endpoint name
  ret.endpoint = std::string(m, endpoint_length);
  m += endpoint_length;
  // parse number of files to read
  std::uint32_t file_count;
  std::memcpy(&file_count, m, 4);
  m += 4;
  
  // loop through rest of the msg to get all files
  for (std::uint32_t i = 0; i < file_count; ++i) {
    File f; // initialize file to create
    // parse length of path for each file
    std::uint32_t path_length;
    std::memcpy(&path_length, m, 4);
    m += 4;
    // parse file path
    f.path = std::string(m, path_length);
    m += path_length;
    // parse number of rows in the file
    std::uint32_t row_count;
    std::memcpy(&row_count, m, 4);
    m += 4;
    f.num_rows = row_count;
    // add file to list of files in datagram
    ret.files.push_back(f);
  }
  return ret;
}

// function that each thread executes
void* StartRoutine(void* arg) {
  for(;;) {
    // decrement semaphore, or wait until value > 0
    sem_wait(&msg_semaphore);
    // check if process has been terminated
    if (terminate && msg_queue.empty())
      return nullptr;

    // lock mutex, grab next message from queue, unlock mutex, parse datagram
    pthread_mutex_lock(&msg_queue_mutex);
    std::string msg = msg_queue.front();
    msg_queue.pop();
    pthread_mutex_unlock(&msg_queue_mutex);
    DatagramContent dg = ParseMessage(msg);
    
    // find the maximum number of rows from the files in the datagram
    std::uint32_t max_rows = 0;
    for (std::uint32_t i = 0; i < dg.files.size(); ++i) {
      if (dg.files[i].num_rows > max_rows)
        max_rows = dg.files[i].num_rows;
    }

    // make copies of vectors of all path names & row counts for each datagram
    std::vector<std::string> paths;
    std::vector<std::uint32_t> row_counts;
    for (std::uint32_t i = 0; i < dg.files.size(); ++i) {
      paths.push_back(dg.files[i].path);
      row_counts.push_back(dg.files[i].num_rows);
    }

    // acquire solver and reader resources
    SolverHandle solvers = ShaSolvers::Checkout(max_rows);
    ReaderHandle readers = FileReaders::Checkout(dg.files.size(), &solvers);

    // initialize the hashings results table and perform hashes
    std::vector<std::vector<ReaderHandle::HashType>> hashes(dg.files.size());
    readers.Process(paths, row_counts, &hashes);

    // return reader and solver resources
    FileReaders::Checkin(std::move(readers));
    ShaSolvers::Checkin(std::move(solvers));

    // concatenate all hashes into one long string
    std::string hashes_concat;
    for (auto& file_hashes : hashes) {
      for (auto& hash : file_hashes)
        hashes_concat.append(hash.data(), 64);
    }
    // connect to client reply socket and stream the hashes
    UnixDomainStreamClient reply(dg.endpoint);
    reply.Init();
    reply.Write(hashes_concat.data(), hashes_concat.size());
  }
  return nullptr;
}

int main(int argc, char* argv[]) {
  // validate input, print usage message if incorrect
  if (argc < 4) {
    std::cout << "usage: bin/proj2-server <server_name> "
      << "<num_file_readers> <num_SHA_solvers>" << std::endl;
    return 1;
  }
  // parse command line
  std::string server_name = argv[1];
  int num_file_readers = std::stoi(argv[2]);
  int num_sha_solvers = std::stoi(argv[3]);
  // make sure there are readers and solvers
  if (num_file_readers < 1 || num_sha_solvers < 1) {
    std::cout << "need readers and solvers to execute" << std::endl;
    return 1;
  }

  // instruct signal handlers to execute handle_signal function
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  // intialize thread pools for file readers and SHA solvers
  FileReaders::Init(num_file_readers);
  ShaSolvers::Init(num_sha_solvers);
  // bind to a Unix domain datagram socket
  UnixDomainDatagramEndpoint dgram_server(server_name);
  dgram_server.Init();

  // initialize semaphore
  sem_init(&msg_semaphore, 0, 0);
  // spin up threads
  int num_threads = get_nprocs();
  pthread_t threads[num_threads];
  for (int i = 0; i < num_threads; ++i)
    pthread_create(&threads[i], nullptr, StartRoutine, nullptr);

  // server continues to run until instructed to terminate
  while (!terminate) {
    std::string whatever;
    // wait for datagrams to be sent
    std::string msg = dgram_server.RecvFrom(&whatever, 65000);

    pthread_mutex_lock(&msg_queue_mutex);    // lock mutex
    msg_queue.push(msg);                     // add datagram to queue
    pthread_mutex_unlock(&msg_queue_mutex);  // unlock mutex
    sem_post(&msg_semaphore);                // increment semaphore
  }

  // wake all threads back up before joining them
  for (int i = 0; i < num_threads; ++i)
    sem_post(&msg_semaphore);
  for (int i = 0; i < num_threads; ++i)
    pthread_join(threads[i], nullptr);
  // destroy semaphore and mutex
  sem_destroy(&msg_semaphore);
  pthread_mutex_destroy(&msg_queue_mutex);

  return 0;
}
