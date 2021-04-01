#include "operators.h"

#include <cassert>

// Get materialized results
std::vector<uint64_t *> Operator::getResults() {
  std::vector<uint64_t *> result_vector;
  for (auto &c : tmp_results_) {
    result_vector.push_back(c.data());
  }
  return result_vector;
}

// Require a column and add it to results
bool Scan::require(SelectInfo info) {
  if (info.binding != relation_binding_)
    return false;
  assert(info.col_id < relation_.columns().size());
  result_columns_.push_back(relation_.columns()[info.col_id]);
  select_to_result_col_id_[info] = result_columns_.size() - 1;
  return true;
}

// Run
void Scan::run() {
  // Nothing to do
  result_size_ = relation_.size();
}

// Get materialized results
std::vector<uint64_t *> Scan::getResults() {
  return result_columns_;
}

// Require a column and add it to results
bool FilterScan::require(SelectInfo info) {
  if (info.binding != relation_binding_)
    return false;
  assert(info.col_id < relation_.columns().size());
  if (select_to_result_col_id_.find(info) == select_to_result_col_id_.end()) {
    // Add to results
    input_data_.push_back(relation_.columns()[info.col_id]);
    tmp_results_.emplace_back();
    unsigned colId = tmp_results_.size() - 1;
    select_to_result_col_id_[info] = colId;
  }
  return true;
}

// Copy to result
void FilterScan::copy2Result(uint64_t id) {
  for (unsigned cId = 0; cId < input_data_.size(); ++cId)
    tmp_results_[cId].push_back(input_data_[cId][id]);
  ++result_size_;
}

// Apply filter
bool FilterScan::applyFilter(uint64_t i, FilterInfo &f) {
  auto compare_col = relation_.columns()[f.filter_column.col_id];
  auto constant = f.constant;
  switch (f.comparison) {
    case FilterInfo::Comparison::Equal:return compare_col[i] == constant;
    case FilterInfo::Comparison::Greater:return compare_col[i] > constant;
    case FilterInfo::Comparison::Less:return compare_col[i] < constant;
  };
  return false;
}

// Run
void FilterScan::run() {
  for (uint64_t i = 0; i < relation_.size(); ++i) {
    bool pass = true;
    for (auto &f : filters_) {
      pass &= applyFilter(i, f);
    }
    if (pass)
      copy2Result(i);
  }
}

// Require a column and add it to results
bool Join::require(SelectInfo info) {
  if (requested_columns_.count(info) == 0) {
    bool success = false;
    if (left_->require(info)) {
      requested_columns_left_.emplace_back(info);
      success = true;
    } else if (right_->require(info)) {
      success = true;
      requested_columns_right_.emplace_back(info);
    }
    if (!success)
      return false;

    tmp_results_.emplace_back();
    requested_columns_.emplace(info);
  }
  return true;
}

// Copy to result
void Join::copy2Result(uint64_t left_id, uint64_t right_id) {
  unsigned rel_col_id = 0;
  for (unsigned cId = 0; cId < copy_left_data_.size(); ++cId)
    tmp_results_[rel_col_id++].push_back(copy_left_data_[cId][left_id]);

  for (unsigned cId = 0; cId < copy_right_data_.size(); ++cId)
    tmp_results_[rel_col_id++].push_back(copy_right_data_[cId][right_id]);
  ++result_size_;
}

// Run
void Join::run() {
  left_->require(p_info_.left);
  right_->require(p_info_.right);
  left_->run();
  right_->run();


  // Use smaller input_ for build
  if (left_->result_size() > right_->result_size()) {
    std::swap(left_, right_);
    std::swap(p_info_.left, p_info_.right);
    std::swap(requested_columns_left_, requested_columns_right_);
  }

  auto left_input_data = left_->getResults();
  auto right_input_data = right_->getResults();

  // Resolve the input_ columns_
  unsigned res_col_id = 0;
  for (auto &info : requested_columns_left_) {
    copy_left_data_.push_back(left_input_data[left_->resolve(info)]);
    select_to_result_col_id_[info] = res_col_id++;
  }
  for (auto &info : requested_columns_right_) {
    copy_right_data_.push_back(right_input_data[right_->resolve(info)]);
    select_to_result_col_id_[info] = res_col_id++;
  }

  auto left_col_id = left_->resolve(p_info_.left);
  auto right_col_id = right_->resolve(p_info_.right);

  // Build phase
  auto left_key_column = left_input_data[left_col_id];
  hash_table_.reserve(left_->result_size() * 2);
  for (uint64_t i = 0, limit = i + left_->result_size(); i != limit; ++i) {
    hash_table_.emplace(left_key_column[i], i);
  }
  // Probe phase
  auto right_key_column = right_input_data[right_col_id];
  for (uint64_t i = 0, limit = i + right_->result_size(); i != limit; ++i) {
    auto rightKey = right_key_column[i];
    auto range = hash_table_.equal_range(rightKey);
    for (auto iter = range.first; iter != range.second; ++iter) {
      copy2Result(iter->second, i);
    }
  }
}

// Copy to result
void SelfJoin::copy2Result(uint64_t id) {
  for (unsigned cId = 0; cId < copy_data_.size(); ++cId)
    tmp_results_[cId].push_back(copy_data_[cId][id]);
  ++result_size_;
}

// Require a column and add it to results
bool SelfJoin::require(SelectInfo info) {
  if (required_IUs_.count(info))
    return true;
  if (input_->require(info)) {
    tmp_results_.emplace_back();
    required_IUs_.emplace(info);
    return true;
  }
  return false;
}

// Run
void SelfJoin::run() {
  input_->require(p_info_.left);
  input_->require(p_info_.right);
  input_->run();
  input_data_ = input_->getResults();

  for (auto &iu : required_IUs_) {
    auto id = input_->resolve(iu);
    copy_data_.emplace_back(input_data_[id]);
    select_to_result_col_id_.emplace(iu, copy_data_.size() - 1);
  }

  auto left_col_id = input_->resolve(p_info_.left);
  auto right_col_id = input_->resolve(p_info_.right);

  auto left_col = input_data_[left_col_id];
  auto right_col = input_data_[right_col_id];
  for (uint64_t i = 0; i < input_->result_size(); ++i) {
    if (left_col[i] == right_col[i])
      copy2Result(i);
  }
}

// Run
void Checksum::run() {
  for (auto &sInfo : col_info_) {
    input_->require(sInfo);
  }
  input_->run();
  auto results = input_->getResults();

  for (auto &sInfo : col_info_) {
    auto col_id = input_->resolve(sInfo);
    auto result_col = results[col_id];
    uint64_t sum = 0;
    result_size_ = input_->result_size();
    for (auto iter = result_col, limit = iter + input_->result_size();
         iter != limit;
         ++iter)
      sum += *iter;
    check_sums_.push_back(sum);
  }
}

