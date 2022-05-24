// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once
#include <memory>
#include "core/framework/iexecutor.h"
#include "core/framework/stream_handles.h"
#include "core/graph/basic_types.h"
#include <unordered_map>

namespace onnxruntime {

class SessionState;
struct AllocPlanPerValue;
struct ParallelExecutionPlanImpl;

// Specify how many logic streams for each provider type
using ProviderStreamMap = std::unordered_map<std::string, int>;
// Each set contains ops which should be grouped in an independent logic stream
using OpStreamMap = std::vector<std::unordered_set<std::string>>;

struct ParallelExecutionPlan : public IExecutor {
  ParallelExecutionPlan(const SessionState& session_state,
                        const ProviderStreamMap& provider_stream_map,
                        const OpStreamMap& op_stream_map = {});
  ~ParallelExecutionPlan();
  common::Status Execute(const SessionState& session_state, const std::vector<int>& feed_mlvalue_idxs,
                         const std::vector<OrtValue>& feeds, const std::vector<int>& fetch_mlvalue_idxs,
                         std::vector<OrtValue>& fetches,
                         const std::unordered_map<size_t, CustomAllocator>& fetch_allocators,
                         const logging::Logger& logger) override;
  const std::vector<int>& ParallelExecutionPlan::GetRefCounts() const;
  Stream* GetComputeStreamForNode(NodeIndex index) const;
  const std::vector<AllocPlanPerValue>& GetAllocPlanPerValue() const;
  std::unique_ptr<ParallelExecutionPlanImpl> impl_;
  //ParallelExecutionPlanImpl* impl_;
};

}  // namespace onnxruntime