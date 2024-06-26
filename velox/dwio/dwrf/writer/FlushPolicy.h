/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>
#include "velox/dwio/common/FlushPolicy.h"
#include "velox/dwio/dwrf/writer/WriterContext.h"

namespace facebook::velox::dwrf {
enum class FlushDecision {
  SKIP,
  EVALUATE_DICTIONARY,
  FLUSH_DICTIONARY,
  ABANDON_DICTIONARY,
};

class DWRFFlushPolicy : virtual public dwio::common::FlushPolicy {
 public:
  virtual ~DWRFFlushPolicy() override = default;

  virtual bool shouldFlush(
      const dwio::common::StripeProgress& stripeProgress) override = 0;

  /// Checks additional flush criteria based on dictionary encoding.
  /// Different actions can also be taken based on the additional checks.
  /// e.g. abandon or evaluate dictionary encodings.
  virtual FlushDecision shouldFlushDictionary(
      bool flushStripe,
      bool overMemoryBudget,
      const dwio::common::StripeProgress& stripeProgress,
      const WriterContext& context) = 0;

  /// This method needs to be safe to call *after* WriterBase::close().
  virtual void onClose() override = 0;
};

class DefaultFlushPolicy : public DWRFFlushPolicy {
 public:
  DefaultFlushPolicy(
      uint64_t stripeSizeThreshold,
      uint64_t dictionarySizeThreshold);

  virtual ~DefaultFlushPolicy() override = default;

  bool shouldFlush(
      const dwio::common::StripeProgress& stripeProgress) override {
    return stripeProgress.stripeSizeEstimate >= stripeSizeThreshold_;
  }

  FlushDecision shouldFlushDictionary(
      bool flushStripe,
      bool overMemoryBudget,
      const dwio::common::StripeProgress& stripeProgress,
      int64_t dictionaryMemoryUsage);

  FlushDecision shouldFlushDictionary(
      bool flushStripe,
      bool overMemoryBudget,
      const dwio::common::StripeProgress& stripeProgress,
      const WriterContext& context) override;

  void onClose() override {}

 private:
  uint64_t getDictionaryAssessmentIncrement() const;

  const uint64_t stripeSizeThreshold_;
  const uint64_t dictionarySizeThreshold_;
  uint64_t dictionaryAssessmentThreshold_;
};

class RowsPerStripeFlushPolicy : public DWRFFlushPolicy {
 public:
  explicit RowsPerStripeFlushPolicy(std::vector<uint64_t> rowsPerStripe);

  virtual ~RowsPerStripeFlushPolicy() override = default;

  bool shouldFlush(const dwio::common::StripeProgress& stripeProgress) override;

  FlushDecision shouldFlushDictionary(
      bool /* flushStripe */,
      bool /* overMemoryBudget */,
      const dwio::common::StripeProgress& /* stripeProgress */,
      const WriterContext& /* context */) override {
    return FlushDecision::SKIP;
  }

  void onClose() override {}

 private:
  const std::vector<uint64_t> rowsPerStripe_;
};

class RowThresholdFlushPolicy : public DWRFFlushPolicy {
 public:
  explicit RowThresholdFlushPolicy(uint64_t rowCountThreshold)
      : rowCountThreshold_{rowCountThreshold} {}

  virtual ~RowThresholdFlushPolicy() override = default;

  bool shouldFlush(
      const dwio::common::StripeProgress& stripeProgress) override {
    return stripeProgress.stripeRowCount >= rowCountThreshold_;
  }

  FlushDecision shouldFlushDictionary(
      bool /* flushStripe */,
      bool /* overMemoryBudget */,
      const dwio::common::StripeProgress& /* stripeProgress */,
      const WriterContext& /* context */) override {
    return FlushDecision::SKIP;
  }

  void onClose() override {}

 private:
  const uint64_t rowCountThreshold_;
};

class LambdaFlushPolicy : public DWRFFlushPolicy {
 public:
  explicit LambdaFlushPolicy(std::function<bool()> lambda) : lambda_{lambda} {}

  virtual ~LambdaFlushPolicy() override = default;

  bool shouldFlush(const dwio::common::StripeProgress& /* ununsed */) override {
    return lambda_();
  }

  FlushDecision shouldFlushDictionary(
      bool /* flushStripe */,
      bool /* overMemoryBudget */,
      const dwio::common::StripeProgress& /* stripeProgress */,
      const WriterContext& /* unused */) override {
    return FlushDecision::SKIP;
  }

  void onClose() override {}

 private:
  const std::function<bool()> lambda_;
};

} // namespace facebook::velox::dwrf
