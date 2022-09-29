// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <utility>

#include "core/framework/op_kernel.h"
#include "core/graph/indexed_sub_graph.h"
#include "core/providers/common.h"
#include "core/providers/shared/node_unit/node_unit.h"

#include "xnnpack.h"

namespace onnxruntime {
class GraphViewer;
class NodeUnit;
namespace xnnpack {
constexpr const char* kDynamicDomainByCreate = "xnnpack";

enum OpComputeType : uint8_t {
  op_compute_type_invalid = 0,
  op_compute_type_fp32,
  op_compute_type_fp16,
  op_compute_type_qs8_per_channel,
  op_compute_type_qs8,
  op_compute_type_qu8,
};

enum TensorQuantType : uint8_t {
  TensorTypeInvalid = 0,
  TensorTypeFp32,
  TensorTypeInt8,
  TensorTypeUint8,
  TensorTypeInt8_Per_Channel,
  TensorTypeInt32,
  TensorTypeInt32_Per_Channel,
  TensorTypeFp16,
};

using OpQuantParam = std::vector<std::pair<std::vector<float>, uint8_t>>;

enum class QuantizedOpType : uint8_t {
  QLinearConv,
  QLinearMaxPool,
  QlinearAvgPool,
  QLinearAdd,
  QLinearMul,
  QLinearSub,
  // QDQ operator
  QDQConv,
  QDQMaxPool,
  QDQAvgPool,
  QDQSoftmax,
  QDQAdd,
  QDQMul,
  QDQSub,
  Unknown,
};

QuantizedOpType GetQuantizedOpType(const NodeUnit& node_unit);

// forward declaration for this EP's namespace.
template <typename T>
KernelCreateInfo BuildKernelCreateInfo();

struct XnnpackOperatorDeleter {
  void operator()(struct xnn_operator* p) const {
    if (p != nullptr) {
      // Ignore returned value because it fails only when xnnpack wasn't initialized
      xnn_delete_operator(p);
    }
  }
};

struct XnnpackSubgraphDeleter {
  void operator()(struct xnn_subgraph* p) const {
    if (p != nullptr) {
      // Ignore returned value because it fails only when xnnpack wasn't initialized
      xnn_delete_subgraph(p);
    }
  }
};

struct XnnpackRuntimeDeleter {
  void operator()(struct xnn_runtime* p) const {
    if (p != nullptr) {
      // Ignore returned value because it fails only when xnnpack wasn't initialized
      xnn_delete_runtime(p);
    }
  }
};

struct XnnpackWorkspaceDeleter {
  void operator()(struct xnn_workspace* p) const {
    if (p != nullptr) {
      // Ignore returned value because it fails only when xnnpack wasn't initialized
      xnn_release_workspace(p);
    }
  }
};

bool IsPaddingTypeSupported(AutoPadType auto_pad);

using XnnpackOperator = std::unique_ptr<struct xnn_operator, XnnpackOperatorDeleter>;
using XnnpackSubgraph = std::unique_ptr<struct xnn_subgraph, XnnpackSubgraphDeleter>;
using XnnpackRuntime = std::unique_ptr<struct xnn_runtime, XnnpackRuntimeDeleter>;
using XnnpackWorkspace = std::unique_ptr<struct xnn_workspace, XnnpackWorkspaceDeleter>;

std::unique_ptr<IndexedSubGraph::MetaDef> FuseActivation(const NodeUnit& conv_unit, const NodeUnit& activation,
                                                         const GraphViewer& graph);
std::unique_ptr<IndexedSubGraph::MetaDef> FuseQDQGroup(const NodeUnit& unit_node);

bool GetType(const NodeArg& node_arg, int32_t& type);

TensorQuantType GetTensorQuantType(const onnxruntime::NodeUnit& node_unit, int32_t io_index,
                                   bool is_output, const onnxruntime::GraphViewer& graph_viewer);

OpQuantParam ParseQuantParamForOp(const OpKernelInfo& info, int32_t x_dtype, size_t howManyInputScaleAndZp);
const char* TensorQtypeToString(enum TensorQuantType type);
const char* OpTypeToString(OpComputeType opCtype);

template <typename T>
auto xnn_u8s8_quantize(float val, float scale, T zero_point) {
  auto typed_min = static_cast<float>(std::numeric_limits<T>::min());
  auto typed_max = static_cast<float>(std::numeric_limits<T>::max());
  auto zp = static_cast<float>(zero_point);
  return static_cast<T>(lrintf(fminf(fmaxf(val / scale + zp, typed_min), typed_max)));
}
}  // namespace xnnpack
}  // namespace onnxruntime
