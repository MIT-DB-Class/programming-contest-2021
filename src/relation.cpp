#include "relation.h"

#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <sys/mman.h>
#include <sys/stat.h>

// Stores a relation into a binary file
void Relation::storeRelation(const std::string &file_name) {
  std::ofstream out_file;
  out_file.open(file_name, std::ios::out | std::ios::binary);
  out_file.write((char *) &size_, sizeof(size_));
  auto numColumns = columns_.size();
  out_file.write((char *) &numColumns, sizeof(size_t));
  for (auto c : columns_) {
    out_file.write((char *) c, size_ * sizeof(uint64_t));
  }
  out_file.close();
}

// Stores a relation into a file (csv), e.g., for loading/testing it with a DBMS
void Relation::storeRelationCSV(const std::string &file_name) {
  std::ofstream out_file;
  out_file.open(file_name + ".tbl", std::ios::out);
  for (uint64_t i = 0; i < size_; ++i) {
    for (auto &c : columns_) {
      out_file << c[i] << '|';
    }
    out_file << "\n";
  }
}

// Dump SQL: Create and load table (PostgreSQL)
void Relation::dumpSQL(const std::string &file_name, unsigned relation_id) {
  std::ofstream out_file;
  out_file.open(file_name + ".sql", std::ios::out);
  // Create table statement
  out_file << "CREATE TABLE r" << relation_id << " (";
  for (unsigned cId = 0; cId < columns_.size(); ++cId) {
    out_file << "c" << cId << " bigint"
             << (cId < columns_.size() - 1 ? "," : "");
  }
  out_file << ");\n";
  // Load from csv statement
  out_file << "copy r" << relation_id << " from 'r" << relation_id
           << ".tbl' delimiter '|';\n";
}

void Relation::loadRelation(const char *file_name) {
  int fd = open(file_name, O_RDONLY);
  if (fd == -1) {
    std::cerr << "cannot open " << file_name << std::endl;
    throw;
  }

  // Obtain file size_
  struct stat sb{};
  if (fstat(fd, &sb) == -1)
    std::cerr << "fstat\n";

  auto length = sb.st_size;

  char *addr = static_cast<char *>(mmap(nullptr,
                                        length,
                                        PROT_READ,
                                        MAP_PRIVATE,
                                        fd,
                                        0u));
  if (addr == MAP_FAILED) {
    std::cerr << "cannot mmap " << file_name << " of length " << length
              << std::endl;
    throw;
  }

  if (length < 16) {
    std::cerr << "relation_ file " << file_name
              << " does not contain a valid header"
              << std::endl;
    throw;
  }

  this->size_ = *reinterpret_cast<uint64_t *>(addr);
  addr += sizeof(size_);
  auto numColumns = *reinterpret_cast<size_t *>(addr);
  addr += sizeof(size_t);
  for (unsigned i = 0; i < numColumns; ++i) {
    this->columns_.push_back(reinterpret_cast<uint64_t *>(addr));
    addr += size_ * sizeof(uint64_t);
  }
}

// Constructor that loads relation_ from disk
Relation::Relation(const char *file_name) : owns_memory_(false), size_(0) {
  loadRelation(file_name);
}

// Destructor
Relation::~Relation() {
  if (owns_memory_) {
    for (auto c : columns_)
      delete[] c;
  }
}

