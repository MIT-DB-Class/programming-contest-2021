#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <vector>

const unsigned long MAX_FAILED_QUERIES = 100;

// Time to wait between initializing and issuing queries
const unsigned long WAITING_TIME_SECS = 60;

static void usage() {
  std::cerr
      << "Usage: "
         "harness <init-file> <workload-file> <result-file> <test-executable>"
      << std::endl;
}

// Set a file descriptor to be non-blocking
static int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) return flags;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Read a given number of bytes to the specified file descriptor
static ssize_t read_bytes(int fd, void *buffer, size_t num_bytes) {
  char *p = (char *) buffer;
  char *end = p + num_bytes;
  while (p != end) {
    ssize_t res = read(fd, p, end - p);
    if (res < 0) {
      if (errno == EINTR) continue;
      return res;
    }
    p += res;
  }

  return num_bytes;
}

// Write a given number of bytes to the specified file descriptor
static ssize_t write_bytes(int fd, const void *buffer, size_t num_bytes) {
  const char *p = (const char *) buffer;
  const char *end = p + num_bytes;
  while (p != end) {
    ssize_t res = write(fd, p, end - p);
    if (res < 0) {
      if (errno == EINTR) continue;
      return res;
    }
    p += res;
  }

  return num_bytes;
}

int main(int argc, char *argv[]) {
  // Check for the correct number of arguments
  if (argc != 5) {
    usage();
    exit(EXIT_FAILURE);
  }

  std::vector<std::string> input_batches;
  std::vector<std::vector<std::string>> result_batches;

  // Load the workload and result files and parse them into batches
  {
    std::ifstream work_file(argv[2]);
    if (!work_file) {
      std::cerr << "Cannot open workload file" << std::endl;
      exit(EXIT_FAILURE);
    }

    std::ifstream result_file(argv[3]);
    if (!result_file) {
      std::cerr << "Cannot open result file" << std::endl;
      exit(EXIT_FAILURE);
    }

    std::string input_chunk;
    input_chunk.reserve(100000);

    std::vector<std::string> result_chunk;
    result_chunk.reserve(150);

    std::string line;
    while (getline(work_file, line)) {
      input_chunk += line;
      input_chunk += '\n';

      if (line.length() > 0 && (line[0] != 'F')) {
        // Add result
        std::string result;
        getline(result_file, result);
        result_chunk.emplace_back(std::move(result));
      } else {
        // End of batch
        // Copy input_ and results
        input_batches.push_back(input_chunk);
        result_batches.push_back(result_chunk);
        input_chunk = "";
        result_chunk.clear();
      }
    }
  }

  // Create pipes for child communication
  int stdin_pipe[2];
  int stdout_pipe[2];
  if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  // Start the test executable
  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    dup2(stdin_pipe[0], STDIN_FILENO);
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    execlp(argv[4], argv[4], (char *) nullptr);
    perror("execlp");
    exit(EXIT_FAILURE);
  }
  close(stdin_pipe[0]);
  close(stdout_pipe[1]);

  // Open the file and feed the initial relations_
  int init_file = open(argv[1], O_RDONLY);
  if (init_file == -1) {
    std::cerr << "Cannot open init file" << std::endl;
    exit(EXIT_FAILURE);
  }

  while (1) {
    char buffer[4096];
    ssize_t bytes = read(init_file, buffer, sizeof(buffer));
    if (bytes < 0) {
      if (errno == EINTR) continue;
      perror("read");
      exit(EXIT_FAILURE);
    }
    if (bytes == 0) break;
    ssize_t written = write_bytes(stdin_pipe[1], buffer, bytes);
    if (written < 0) {
      perror("write");
      exit(EXIT_FAILURE);
    }
  }

  close(init_file);

  // Signal the end of the initial phase
  ssize_t status_bytes = write_bytes(stdin_pipe[1], "Done\n", 5);
  if (status_bytes < 0) {
    perror("write");
    exit(EXIT_FAILURE);
  }

  // Wait for WAITING_TIME_SECS
  std::cout << "Waiting for " << WAITING_TIME_SECS << " seconds" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(WAITING_TIME_SECS));
  std::cout << "Issuing queries ..." << std::endl;

  // Use select with non-blocking files to read and write from the child
  // process, avoiding deadlocks
  if (set_nonblocking(stdout_pipe[0]) == -1) {
    perror("fcntl");
    exit(EXIT_FAILURE);
  }

  if (set_nonblocking(stdin_pipe[1]) == -1) {
    perror("fcntl");
    exit(EXIT_FAILURE);
  }

  // Start the stopwatch
  struct timeval start{};
  gettimeofday(&start, nullptr);

  unsigned long query_no = 0;
  unsigned long failure_cnt = 0;

  // Loop over all batches
  for (unsigned long batch = 0;
       batch != input_batches.size() && failure_cnt < MAX_FAILED_QUERIES;
       ++batch) {
    std::string output;  // raw output is collected here
    output.reserve(1000000);

    size_t input_ofs = 0;    // byte position in the input_ batch
    size_t output_read = 0;  // number of lines read from the child output

    while (input_ofs != input_batches[batch].length()
        || output_read < result_batches[batch].size()) {
      fd_set read_fd, write_fd;
      FD_ZERO(&read_fd);
      FD_ZERO(&write_fd);

      if (input_ofs != input_batches[batch].length())
        FD_SET(stdin_pipe[1],
               &write_fd);

      if (output_read != result_batches[batch].size())
        FD_SET(stdout_pipe[0],
               &read_fd);

      int retval = select(std::max(stdin_pipe[1], stdout_pipe[0]) + 1,
                          &read_fd,
                          &write_fd,
                          nullptr,
                          nullptr);
      if (retval == -1) {
        perror("select");
        exit(EXIT_FAILURE);
      }

      // Read output from the test program
      if (FD_ISSET(stdout_pipe[0], &read_fd)) {
        char buffer[4096];
        int bytes = read(stdout_pipe[0], buffer, sizeof(buffer));
        if (bytes < 0) {
          if (errno == EINTR) continue;
          perror("read");
          exit(1);
        }
        // Count how many lines were returned
        for (size_t j = 0; j != size_t(bytes); ++j) {
          if (buffer[j] == '\n') ++output_read;
        }
        output.append(buffer, bytes);
      }

      // Feed another chunk of data from this batch to the test program
      if (FD_ISSET(stdin_pipe[1], &write_fd)) {
        int bytes =
            write(stdin_pipe[1],
                  input_batches[batch].data() + input_ofs,
                  input_batches[batch].length() - input_ofs);
        if (bytes < 0) {
          if (errno == EINTR) continue;
          perror("write");
          exit(EXIT_FAILURE);
        }
        input_ofs += bytes;
      }
    }

    // Parse and compare the batch result
    std::stringstream result(output);

    for (unsigned i = 0;
         i != result_batches[batch].size() && failure_cnt < MAX_FAILED_QUERIES;
         ++i) {
      std::string val;

      // result >> val;
      getline(result, val);
      if (!result) {
        std::cerr << "Incomplete batch output for batch " << batch << std::endl;
        exit(EXIT_FAILURE);
      }

      bool matched = val == result_batches[batch][i];
      if (!matched) {
        std::cerr << "Result mismatch for query " << query_no << ", expected: "
                  << result_batches[batch][i]
                  << ", actual: " << val << std::endl;
        ++failure_cnt;
      }
      /*if (matched)
      {
          std::cout << std::endl << val << std::endl
                    << std::endl << result_batches[batch][i];
      }*/
      ++query_no;
    }
  }

  struct timeval end{};
  gettimeofday(&end, nullptr);

  if (failure_cnt == 0) {
    // Output the elapsed time in milliseconds
    double elapsed_sec =
        static_cast<double>(end.tv_sec - start.tv_sec)
            + (end.tv_usec - start.tv_usec) / 1000000.0;
    std::cout << (long) (elapsed_sec * 1000) << std::endl;
    return EXIT_SUCCESS;
  }

  return EXIT_FAILURE;
}

