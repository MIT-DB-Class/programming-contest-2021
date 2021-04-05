#pragma once

#include <vector>
#include <cstdint>
#include <set>

#include "operators.h"
#include "relation.h"
#include "parser.h"

class Joiner {
 private:
  /// The relations that might be joined
  std::vector<Relation> relations_;

 public:
  /// Add relation
  void addRelation(const char *file_name);
  void addRelation(Relation &&relation);
  /// Get relation
  const Relation &getRelation(unsigned relation_id);
  /// Joins a given set of relations
  std::string join(QueryInfo &i);

  const std::vector<Relation> &relations() const { return relations_; }

 private:
  /// Add scan to query
  std::unique_ptr<Operator> addScan(std::set<unsigned> &used_relations,
                                    const SelectInfo &info,
                                    QueryInfo &query);
};

