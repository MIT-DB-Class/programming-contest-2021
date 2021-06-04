#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "relation.h"

struct SelectInfo {
  /// Relation id
  RelationId rel_id;
  /// Binding for the relation
  unsigned binding;
  /// Column id
  unsigned col_id;

  /// The constructor
  SelectInfo(RelationId rel_id, unsigned b, unsigned col_id)
      : rel_id(rel_id), binding(b), col_id(col_id) {};
  /// The constructor if relation id does not matter
  SelectInfo(unsigned b, unsigned colId) : SelectInfo(-1, b, colId) {};

  /// Equality operator
  inline bool operator==(const SelectInfo &o) const {
    return o.rel_id == rel_id && o.binding == binding && o.col_id == col_id;
  }
  /// Less Operator
  inline bool operator<(const SelectInfo &o) const {
    return (binding < o.binding) || (binding == o.binding && col_id < o.col_id);
  }

  /// Dump text format
  std::string dumpText();
  /// Dump SQL
  std::string dumpSQL(bool add_sum = false);

  /// The delimiter used in our text format
  static const char delimiter = ' ';
  /// The delimiter used in SQL
  constexpr static const char delimiterSQL[] = ", ";
};

struct FilterInfo {
  enum Comparison : char { Less = '<', Greater = '>', Equal = '=' };
  /// Filter Column
  SelectInfo filter_column;
  /// Constant
  uint64_t constant;
  /// Comparison type
  Comparison comparison;

  /// The constructor
  FilterInfo(SelectInfo filter_column, uint64_t constant, Comparison comparison)
      : filter_column(filter_column),
        constant(constant),
        comparison(comparison) {};

  /// Dump SQL
  std::string dumpSQL();
  /// Dump text format
  std::string dumpText();

  /// The delimiter used in our text format
  static const char delimiter = '&';
  /// The delimiter used in SQL
  constexpr static const char delimiterSQL[] = " and ";
};

static const std::vector<FilterInfo::Comparison> comparisonTypes
    {FilterInfo::Comparison::Less, FilterInfo::Comparison::Greater,
     FilterInfo::Comparison::Equal};

struct PredicateInfo {
  /// Left
  SelectInfo left;
  /// Right
  SelectInfo right;

  /// The constructor
  PredicateInfo(SelectInfo left, SelectInfo right)
      : left(left), right(right) {};

  /// Dump text format
  std::string dumpText();
  /// Dump SQL
  std::string dumpSQL();

  /// The delimiter used in our text format
  static const char delimiter = '&';
  /// The delimiter used in SQL
  constexpr static const char delimiterSQL[] = " and ";
};

class QueryInfo {
 private:
  /// The relation ids
  std::vector<RelationId> relation_ids_;
  /// The predicates
  std::vector<PredicateInfo> predicates_;
  /// The filters
  std::vector<FilterInfo> filters_;
  /// The selections
  std::vector<SelectInfo> selections_;

 public:
  /// The empty constructor
  QueryInfo() = default;
  /// The constructor that parses a query
  explicit QueryInfo(std::string raw_query);

  /// Parse relation ids <r1> <r2> ...
  void parseRelationIds(std::string &raw_relations);
  /// Parse predicates r1.a=r2.b&r1.b=r3.c...
  void parsePredicates(std::string &raw_predicates);
  /// Parse selections r1.a r1.b r3.c...
  void parseSelections(std::string &raw_selections);
  /// Parse selections [RELATIONS]|[PREDICATES]|[SELECTS]
  void parseQuery(std::string &raw_query);

  /// Dump text format
  std::string dumpText();
  /// Dump SQL
  std::string dumpSQL();

  /// Reset query info
  void clear();

  /// The relation ids
  const std::vector<RelationId> &relation_ids() const { return relation_ids_; }
  /// The predicates
  const std::vector<PredicateInfo> &predicates() const { return predicates_; }
  /// The filters
  const std::vector<FilterInfo> &filters() const { return filters_; }
  /// The selections
  const std::vector<SelectInfo> &selections() const { return selections_; }

 private:
  /// Parse a single predicate
  void parsePredicate(std::string &raw_predicate);
  /// Resolve bindings of relation ids
  void resolveRelationIds();

};

