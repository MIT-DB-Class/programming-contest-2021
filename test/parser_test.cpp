#include "gtest/gtest.h"

#include "parser.h"

TEST(Parser, ParseRelations) {
  QueryInfo i;
  const auto &relation_ids = i.relation_ids();
  std::string rel("0 1");
  i.parseRelationIds(rel);

  ASSERT_EQ(relation_ids.size(), 2u);
  ASSERT_EQ(relation_ids[0], 0u);
  ASSERT_EQ(relation_ids[1], 1u);
}

static void assertPredicateEqual(const PredicateInfo &p_info,
                                 unsigned left_rel,
                                 unsigned left_col,
                                 unsigned right_rel,
                                 unsigned right_col) {
  ASSERT_EQ(p_info.left.rel_id, left_rel);
  ASSERT_EQ(p_info.left.col_id, left_col);
  ASSERT_EQ(p_info.right.rel_id, right_rel);
  ASSERT_EQ(p_info.right.col_id, right_col);
}

static void assertPredicateBindingEqual(const PredicateInfo &p_info,
                                        unsigned leftBind,
                                        unsigned left_col,
                                        unsigned rightBind,
                                        unsigned right_col) {
  ASSERT_EQ(p_info.left.binding, leftBind);
  ASSERT_EQ(p_info.left.col_id, left_col);
  ASSERT_EQ(p_info.right.binding, rightBind);
  ASSERT_EQ(p_info.right.col_id, right_col);
}

static void assertSelectEqual(const SelectInfo &s_info,
                              unsigned rel,
                              unsigned col) {
  ASSERT_EQ(s_info.rel_id, rel);
  ASSERT_EQ(s_info.col_id, col);
}

static void assertSelectBindingEqual(const SelectInfo &s_info,
                                     unsigned binding,
                                     unsigned col) {
  ASSERT_EQ(s_info.binding, binding);
  ASSERT_EQ(s_info.col_id, col);
}

static void assertFilterBindingEqual(const FilterInfo &f_info,
                                     unsigned binding,
                                     unsigned col,
                                     uint64_t constant,
                                     FilterInfo::Comparison comparison) {
  assertSelectBindingEqual(f_info.filter_column, binding, col);
  ASSERT_EQ(f_info.constant, constant);
  ASSERT_EQ(f_info.comparison, comparison);
}

static void assertFilterEqual(const FilterInfo &f_info,
                              unsigned rel,
                              unsigned col,
                              uint64_t constant,
                              FilterInfo::Comparison comparison) {
  assertSelectEqual(f_info.filter_column, rel, col);
  ASSERT_EQ(f_info.constant, constant);
  ASSERT_EQ(f_info.comparison, comparison);
}

TEST(Parser, ParsePredicates) {
  std::string preds("0.2=1.3&2.2=3.3");
  QueryInfo i;
  const auto &predicates = i.predicates();
  const auto &filters = i.filters();
  i.parsePredicates(preds);

  ASSERT_EQ(predicates.size(), 2u);
  assertPredicateBindingEqual(predicates[0], 0, 2, 1, 3);
  assertPredicateBindingEqual(predicates[1], 2, 2, 3, 3);

  i.clear();
  std::string empty_preds("");
  i.parsePredicates(empty_preds);
  ASSERT_EQ(predicates.size(), 0u);

  i.clear();
  std::string only_filters("0.1=111&1.2<222&1.1>333");
  i.parsePredicates(only_filters);
  ASSERT_EQ(predicates.size(), 0u);
  ASSERT_EQ(filters.size(), 3u);
  assertFilterBindingEqual(filters[0],
                           0,
                           1,
                           111,
                           FilterInfo::Comparison::Equal);
  assertFilterBindingEqual(filters[1], 1, 2, 222, FilterInfo::Comparison::Less);
  assertFilterBindingEqual(filters[2],
                           1,
                           1,
                           333,
                           FilterInfo::Comparison::Greater);

  i.clear();
  std::string mixedPredicatesAndFilters("0.1=111&1.2=2.1&1.2<333");
  i.parsePredicates(mixedPredicatesAndFilters);
  ASSERT_EQ(predicates.size(), 1u);
  assertPredicateBindingEqual(predicates[0], 1, 2, 2, 1);
  ASSERT_EQ(filters.size(), 2u);
  assertFilterBindingEqual(filters[0],
                           0,
                           1,
                           111,
                           FilterInfo::Comparison::Equal);
  assertFilterBindingEqual(filters[1], 1, 2, 333, FilterInfo::Comparison::Less);
}

TEST(Parser, ParseSelections) {
  std::string raw_selections("0.1 0.2 1.2 4.4");
  QueryInfo i;
  const auto &selections = i.selections();
  i.parseSelections(raw_selections);

  ASSERT_EQ(selections.size(), 4u);
  assertSelectBindingEqual(selections[0], 0, 1);
  assertSelectBindingEqual(selections[1], 0, 2);
  assertSelectBindingEqual(selections[2], 1, 2);
  assertSelectBindingEqual(selections[3], 4, 4);

  i.clear();

  std::string empty_select;
  i.parseSelections(empty_select);
  ASSERT_EQ(selections.size(), 0u);
}

TEST(Parser, ParseQuery) {
  std::string raw_query("0 2 4|0.1=1.1&0.0=2.1&1.0=2.0&1.0>3|0.1 1.4 2.2");
  QueryInfo i(raw_query);

  ASSERT_EQ(i.relation_ids().size(), 3u);
  ASSERT_EQ(i.relation_ids()[0], 0u);
  ASSERT_EQ(i.relation_ids()[1], 2u);
  ASSERT_EQ(i.relation_ids()[2], 4u);

  ASSERT_EQ(i.predicates().size(), 3u);
  assertPredicateEqual(i.predicates()[0], 0, 1, 2, 1);
  assertPredicateEqual(i.predicates()[1], 0, 0, 4, 1);
  assertPredicateEqual(i.predicates()[2], 2, 0, 4, 0);

  assertFilterEqual(i.filters()[0], 2, 0, 3, FilterInfo::Comparison::Greater);

  ASSERT_EQ(i.selections().size(), 3u);
  assertSelectEqual(i.selections()[0], 0, 1);
  assertSelectEqual(i.selections()[1], 2, 4);
  assertSelectEqual(i.selections()[2], 4, 2);
}

TEST(Parser, DumpSQL) {
  std::string raw_query("0 2|0.1=1.1&0.0=1.0&0.1=5|0.1 1.4");
  QueryInfo i(raw_query);

  auto sql = i.dumpSQL();
  ASSERT_EQ(
      "SELECT SUM(\"0\".c1), SUM(\"1\".c4) FROM r0 \"0\", r2 \"1\" WHERE \"0\".c1=\"1\".c1 and \"0\".c0=\"1\".c0 and \"0\".c1=5;",
      sql);
}

TEST(Parser, DumpText) {
  std::string raw_query("0 2|0.1=1.1&0.0=1.0&1.2=3|0.1 1.4");
  QueryInfo i(raw_query);

  ASSERT_EQ(i.dumpText(), raw_query);
}
