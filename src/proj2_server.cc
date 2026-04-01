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

using namespace proj2;

struct File {
  std::string path;
  uint32_t num_rows;
};

struct DatagramContent {
  std::string endpoint;
  std::vector<File> files;
};

volatile sig_atomic_t terminate = 0;

void handle_signal(int) {
  terminate = 1;
}

int main(int argc, char* argv[]) {

  if (argc < 4) {
    std::cout << "usage: bin/proj2-server [server name] "
      << "[num file readers] [num SHA solvers]" << std::endl;
  }
  
  std::string server_name = argv[1];
  int num_file_readers = std::stoi(argv[2]);
  int num_sha_solvers = std::stoi(argv[3]);
  
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  
  // intialize thread pools for file readers and SHA solvers
  FileReaders::Init(num_file_readers);
  ShaSolvers::Init(num_sha_solvers);
  // bind to a Unix domain datagram socket
  UnixDomainDatagramEndpoint dgram_server(server_name);
  dgram_server.Init();
  
  while (!terminate) {

    std::string whatever;
    std::string msg = dgram_server.RecvFrom(&whatever, 65000);

  }
  
  
  
  
  // 3. wait in a blocking loop for datagrams
  
  // 4. dispatch a thread to parse each request
  
  // 5. acquire required solver and reader resources
  
  // 6. use reader to process client request in hashes
  
  // 7. concatenate sets into one bye string (ordered by file oreder)
  
  // 8. connect to the client's reply socket (stream)
  
  // 9. stream concatenated hashes' string

  return 0;
}
