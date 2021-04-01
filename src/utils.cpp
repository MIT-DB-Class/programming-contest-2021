#include "utils.h"

#include <iostream>

// Create a dummy column
static void createColumn(std::vector<uint64_t *> &columns,
                         uint64_t num_tuples) {
  auto col = new uint64_t[num_tuples];
  columns.push_back(col);
  for (unsigned i = 0; i < num_tuples; ++i) {
    col[i] = i;
  }
}

// Create a dummy relation
Relation Utils::createRelation(uint64_t size, uint64_t num_columns) {
  std::vector<uint64_t *> columns;
  for (unsigned i = 0; i < num_columns; ++i) {
    createColumn(columns, size);
  }
  return Relation(size, move(columns));
}

// Store a relation in all formats
void Utils::storeRelation(std::ofstream &out, Relation &r, unsigned i) {
  auto base_name = "r" + std::to_string(i);
  r.storeRelation(base_name);
  r.storeRelationCSV(base_name);
  r.dumpSQL(base_name, i);
  std::cout << base_name << "\n";
  out << base_name << "\n";
}

