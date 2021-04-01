#include "gtest/gtest.h"

#include "joiner.h"
#include "operators.h"
#include "utils.h"

namespace {

class OperatorTest : public testing::Test {
 protected:
  Relation r1 = Utils::createRelation(5, 3);
  Relation r2 = Utils::createRelation(10, 5);
};

TEST_F(OperatorTest, Scan) {
  unsigned rel_binding = 5;
  Scan scan(r1, rel_binding);
  scan.require(SelectInfo(rel_binding, 0));
  scan.require(SelectInfo(rel_binding, 2));
  scan.run();
  auto results = scan.getResults();
  ASSERT_EQ(results.size(), 2ull);
  auto col_id_1 = scan.resolve(SelectInfo{rel_binding, 0});
  auto col_id_2 = scan.resolve(SelectInfo{rel_binding, 2});
  ASSERT_EQ(results[col_id_1], r1.columns()[0]);
  ASSERT_EQ(results[col_id_2], r1.columns()[2]);
}

TEST_F(OperatorTest, ScanWithSelection) {
  unsigned rel_id = 0;
  unsigned rel_binding = 1;
  unsigned col_id = 2;
  SelectInfo s_info(rel_id, rel_binding, col_id);
  uint64_t constant = 2;
  {
    FilterInfo f_info(s_info, constant, FilterInfo::Comparison::Equal);
    FilterScan filter_scan(r1, f_info);
    filter_scan.run();
    auto results = filter_scan.getResults();
    ASSERT_EQ(results.size(), 0u);
  }
  {
    FilterInfo f_info(s_info, constant, FilterInfo::Comparison::Equal);
    FilterScan filter_scan(r1, f_info);
    filter_scan.require(SelectInfo(rel_binding, 0));
    filter_scan.require(SelectInfo(rel_binding, 2));
    filter_scan.run();

    ASSERT_EQ(filter_scan.result_size(), 1ull);
    auto results = filter_scan.getResults();
    ASSERT_EQ(results.size(), 2ull);
    auto filter_col_id = filter_scan.resolve(SelectInfo{rel_binding, col_id});
    ASSERT_EQ(results[filter_col_id][0], constant);
  }
  {
    FilterInfo f_info(s_info, constant, FilterInfo::Comparison::Greater);
    FilterScan filter_scan(r1, f_info);
    col_id = 1;
    filter_scan.require(SelectInfo(rel_binding, col_id));
    filter_scan.run();

    ASSERT_EQ(filter_scan.result_size(), 2ull);
    auto results = filter_scan.getResults();
    ASSERT_EQ(results.size(), 1ull);

    auto res_col_id = filter_scan.resolve(SelectInfo{rel_binding, col_id});
    auto filter_col = results[res_col_id];
    for (unsigned j = 0; j < filter_scan.result_size(); ++j) {
      ASSERT_TRUE(filter_col[j] > constant);
    }
  }
}

TEST_F(OperatorTest, Join) {
  unsigned l_rid = 0, r_rid = 1;
  unsigned r1_bind = 0, r2_bind = 1;
  unsigned l_col_id = 1, r_col_id = 3;

  Scan r1_scan(r1, r1_bind);
  Scan r2_scan(r2, r2_bind);

  {
    PredicateInfo p_info
        (SelectInfo(l_rid, r1_bind, l_col_id),
         SelectInfo(r_rid, r2_bind, r_col_id));
    auto r1_scan_ptr = std::make_unique<Scan>(r1_scan);
    auto r2_scan_ptr = std::make_unique<Scan>(r2_scan);
    Join join(move(r1_scan_ptr), move(r2_scan_ptr), p_info);
    join.run();
  }
  {
    // Self join
    PredicateInfo p_info(SelectInfo(0, 0, 1), SelectInfo(0, 1, 2));
    Scan r1_scan2(r1, 1);
    auto left_ptr = std::make_unique<Scan>(r1_scan);
    auto right_ptr = std::make_unique<Scan>(r1_scan2);
    Join join(move(left_ptr), move(right_ptr), p_info);
    join.require(SelectInfo(r1_bind, 0));
    join.run();

    ASSERT_EQ(join.result_size(), r1.size());

    auto res_col_id = join.resolve(SelectInfo{r1_bind, 0});
    auto results = join.getResults();
    ASSERT_EQ(results.size(), 1ull);
    auto result_col = results[res_col_id];
    for (unsigned j = 0; j < join.result_size(); ++j) {
      ASSERT_EQ(result_col[j], r1.columns()[0][j]);
    }
  }
  {
    // Join r1 and r2 (should have same result as r1 and r1)
    auto left_ptr = std::make_unique<Scan>(r2_scan);
    auto right_ptr = std::make_unique<Scan>(r1_scan);
    PredicateInfo p_info(SelectInfo(1, r2_bind, 1), SelectInfo(0, r1_bind, 2));
    Join join(move(left_ptr), move(right_ptr), p_info);
    join.require(SelectInfo(r1_bind, 1));
    join.require(SelectInfo(r2_bind, 3));
    // Request a column two times (should not have an effect)
    join.require(SelectInfo(r2_bind, 3));
    join.run();

    ASSERT_EQ(join.result_size(), r1.size());

    auto res_col_id = join.resolve(SelectInfo{r2_bind, 3});
    auto results = join.getResults();
    ASSERT_EQ(results.size(), 2ull);
    auto result_col = results[res_col_id];
    for (unsigned j = 0; j < join.result_size(); ++j) {
      ASSERT_EQ(result_col[j], r1.columns()[0][j]);
    }
  }
}

TEST_F(OperatorTest, Checksum) {
  unsigned rel_binding = 5;
  Scan r1_scan(r1, rel_binding);

  {
    auto r1_scan_ptr = std::make_unique<Scan>(r1_scan);
    std::vector<SelectInfo> checksum_columns;
    Checksum checkSum(move(r1_scan_ptr), checksum_columns);
    checkSum.run();
    ASSERT_EQ(checkSum.check_sums().size(), 0ull);
  }
  {
    auto r1_scan_ptr = std::make_unique<Scan>(r1_scan);
    std::vector<SelectInfo> checksum_columns;
    checksum_columns.emplace_back(0, rel_binding, 0);
    checksum_columns.emplace_back(0, rel_binding, 2);
    Checksum checksum(move(r1_scan_ptr), checksum_columns);
    checksum.run();

    ASSERT_EQ(checksum.check_sums().size(), 2ull);
    uint64_t expected_sum = 0;
    for (unsigned i = 0; i < r1.size(); ++i) {
      expected_sum += r1.columns()[0][i];
    }
    ASSERT_EQ(checksum.check_sums()[0], expected_sum);
    ASSERT_EQ(checksum.check_sums()[1], expected_sum);
  }
  {
    SelectInfo s_info(0, rel_binding, 2);
    uint64_t constant = 3;
    FilterInfo f_info(s_info, constant, FilterInfo::Comparison::Equal);
    FilterScan r1_scan_filter(r1, f_info);
    auto filter_scan_ptr = std::make_unique<FilterScan>(r1_scan_filter);
    std::vector<SelectInfo> checksum_columns;
    checksum_columns.emplace_back(0, rel_binding, 2);
    Checksum checksum(move(filter_scan_ptr), checksum_columns);
    checksum.run();
    ASSERT_EQ(checksum.check_sums().size(), 1ull);
    ASSERT_EQ(checksum.check_sums()[0], constant);
  }
}

TEST_F(OperatorTest, SelfJoin) {
  unsigned rel_binding = 5;
  Scan r1_scan(r1, rel_binding);
  {
    PredicateInfo
        p_info(SelectInfo(1, rel_binding, 1), SelectInfo(1, rel_binding, 2));
    SelfJoin selfjoin(std::make_unique<Scan>(r1_scan), p_info);
    selfjoin.run();
    ASSERT_EQ(selfjoin.result_size(), r1.size());
    ASSERT_EQ(selfjoin.getResults().size(), 0ull);
  }
  {
    PredicateInfo
        p_info(SelectInfo(1, rel_binding, 1), SelectInfo(1, rel_binding, 2));
    SelfJoin selfjoin(std::make_unique<Scan>(r1_scan), p_info);
    selfjoin.require(SelectInfo(rel_binding, 0));
    selfjoin.run();
    selfjoin.resolve(SelectInfo(rel_binding, 0));
    ASSERT_EQ(selfjoin.result_size(), r1.size());
    auto results = selfjoin.getResults();
    ASSERT_EQ(results.size(), 1ull);
  }
}

TEST_F(OperatorTest, Joiner) {
  Joiner joiner;
  unsigned num_tuples = 10;
  for (unsigned i = 0; i < 5; i++) {
    joiner.addRelation(Utils::createRelation(num_tuples, 3));
  }

  uint64_t sum = 0;
  for (unsigned i = 0; i < num_tuples; ++i) {
    sum += joiner.relations()[0].columns()[0][i];
  }
  std::string expSumWithoutFilters = std::to_string(sum);
  {
    // Binary join without selections_
    auto query = "1 2|0.0=1.1|1.2";
    QueryInfo i(query);
    auto result = joiner.join(i);
    ASSERT_EQ(result, expSumWithoutFilters + "\n");
  }
  {
    // Query without selections_
    auto query = "0 2 3|0.0=1.1&1.2=2.0|2.2";
    QueryInfo i(query);
    auto result = joiner.join(i);
    ASSERT_EQ(result, expSumWithoutFilters + "\n");
  }
  {
    // Query with selection (=4)
    auto query = "0 1 4|0.0=1.1&1.2=2.0&1.1=4|1.0 2.2";
    QueryInfo i(query);
    auto result = joiner.join(i);
    ASSERT_EQ(result, "4 4\n");
  }
  {
    // Query without result
    auto query = "0 1 2|0.0=1.1&1.2=2.0&1.1=100|1.0 2.2";
    QueryInfo i(query);
    auto result = joiner.join(i);
    ASSERT_EQ(result, "NULL NULL\n");
  }
  {
    // Self join
    auto query = "0 0|0.0=1.1|1.0";
    QueryInfo i(query);
    auto result = joiner.join(i);
    ASSERT_EQ(result, expSumWithoutFilters + "\n");
  }
  {
    // Cyclic query graph
    auto query = "0 1 2|0.0=1.1&1.1=2.0&2.2=0.1|1.0";
    QueryInfo i(query);
    auto result = joiner.join(i);
    ASSERT_EQ(result, expSumWithoutFilters + "\n");
  }
  {
    // 4 Relations
    auto query = "0 1 2 3|0.0=1.1&1.1=2.0&2.2=3.1|1.0";
    QueryInfo i(query);
    auto result = joiner.join(i);
    ASSERT_EQ(result, expSumWithoutFilters + "\n");
  }
  {
    // 4 Relations (Permuted)
    auto query = "0 1 2 3|0.0=1.1&2.1=3.0&0.2=2.1|1.0";
    QueryInfo i(query);
    auto result = joiner.join(i);
    ASSERT_EQ(result, expSumWithoutFilters + "\n");
  }
  {
    // 2 Filters (Equal)
    auto query = "0 1|0.0=1.1&0.0=3&1.0=3|1.0";
    QueryInfo i(query);
    auto result = joiner.join(i);
    ASSERT_EQ(result, "3\n");
  }
  {
    // 2 Filters (Distinct)
    auto query = "0 1|0.0=1.1&0.0<3&1.0>3|1.0";
    QueryInfo i(query);
    auto result = joiner.join(i);
    ASSERT_EQ(result, "NULL\n");
  }
  {
    // Multiple Filters per relation_
    auto query = "0 1|0.0=1.1&0.0>1&0.0<3|1.0";
    QueryInfo i(query);
    auto result = joiner.join(i);
    ASSERT_EQ(result, "2\n");
  }
}

}
