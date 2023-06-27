#include "core/framework/tensor_shape.h"
#include "core/framework/tensor_shape2.h"

namespace onnxruntime {
TensorShape2::TensorShape2(std::vector<int64_t> dims) {
  tensor_shape_ = std::make_unique<TensorShape>(std::move(dims));
  //tensor_shape_ = new TensorShape(std::move(dims));
}

TensorShape2::~TensorShape2() {
//  delete tensor_shape_;
}
int64_t TensorShape2::operator[](size_t idx) const { return (*tensor_shape_)[idx]; }
int64_t& TensorShape2::operator[](size_t idx) { return (*tensor_shape_)[idx]; }

size_t TensorShape2::NumDimensions() const noexcept {
  return tensor_shape_->NumDimensions();
//return 1L;
}

std::string TensorShape2::ToString() const {
  return tensor_shape_->ToString();
//return "toString()";
}

bool TensorShape2::IsScalar() const {
  return tensor_shape_->IsScalar();
//return true;
}

TensorShape3::TensorShape3() {
}

size_t TensorShape3::NumDimensions() {
  return 4L;
}

int cube(int x) {
  return x * x * x;
}
}
