/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 *  \file mkldnn_test.cc
 *  \brief test functions for mkldnn operators.
 *  \author Alex Zai
 */

#if MXNET_USE_MKLDNN == 1

#include <mkldnn_types.h>
#include <cmath>
#include <climits>
#include <set>
#include "gtest/gtest.h"
#include "mxnet/imperative.h"
#include "../../src/operator/nn/mkldnn/mkldnn_base-inl.h"
#include "../../src/operator/nn/mkldnn/mkldnn_ops-inl.h"
#include "../../src/operator/nn/mkldnn/mkldnn_pooling-inl.h"
#include "../../src/operator/nn/pooling-inl.h"
#include "../../src/operator/nn/convolution-inl.h"
#include "../../src/operator/nn/deconvolution-inl.h"
#include "../include/test_mkldnn.h"

using namespace mxnet;

OpAttrs GetCopyOp() {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("_copy");
  attrs.num_inputs = 1;
  attrs.num_outputs = 1;
  attrs.dispatches.resize(2);
  attrs.dispatches[0] = DispatchMode::kFCompute;
  attrs.dispatches[1] = DispatchMode::kFComputeEx;
  attrs.requests.insert(OpReqType::kWriteTo);
  attrs.requests.insert(OpReqType::kWriteInplace);
  attrs.requests.insert(OpReqType::kAddTo);
  return attrs;
}

OpAttrs GetCopyBackwardsOp() {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("_backward_copy");
  attrs.num_inputs = 1;
  attrs.num_outputs = 1;
  attrs.dispatches.resize(2);
  attrs.dispatches[0] = DispatchMode::kFCompute;
  attrs.dispatches[1] = DispatchMode::kFComputeEx;
  attrs.requests.insert(OpReqType::kWriteTo);
  attrs.requests.insert(OpReqType::kWriteInplace);
  attrs.requests.insert(OpReqType::kAddTo);
  return attrs;
}

OpAttrs GetReluOp() {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("Activation");
  attrs.attrs.dict.insert({"act_type", "relu"});
  attrs.attrs.op->attr_parser(&attrs.attrs);
  attrs.num_inputs = 1;
  attrs.num_outputs = 1;
  attrs.dispatches.resize(2);
  attrs.dispatches[0] = DispatchMode::kFCompute;
  attrs.dispatches[1] = DispatchMode::kFComputeEx;
  attrs.requests.insert(OpReqType::kWriteTo);
  attrs.requests.insert(OpReqType::kWriteInplace);
  attrs.requests.insert(OpReqType::kAddTo);
  return attrs;
}

OpAttrs GetReluBackwardsOp() {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("_backward_Activation");
  attrs.attrs.dict.insert({"act_type", "relu"});
  attrs.attrs.op->attr_parser(&attrs.attrs);
  attrs.num_inputs = 2;
  attrs.num_outputs = 1;
  attrs.dispatches.resize(2);
  attrs.dispatches[0] = DispatchMode::kFCompute;
  attrs.dispatches[1] = DispatchMode::kFComputeEx;
  attrs.requests.insert(OpReqType::kWriteTo);
  attrs.requests.insert(OpReqType::kWriteInplace);
  attrs.requests.insert(OpReqType::kAddTo);
  return attrs;
}

OpAttrs GetSumOp() {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("elemwise_add");
  attrs.num_inputs = 2;
  attrs.num_outputs = 1;
  attrs.dispatches.resize(2);
  attrs.dispatches[0] = DispatchMode::kFCompute;
  attrs.dispatches[1] = DispatchMode::kFComputeEx;
  attrs.requests.insert(OpReqType::kWriteTo);
  attrs.requests.insert(OpReqType::kWriteInplace);
  attrs.requests.insert(OpReqType::kAddTo);
  return attrs;
}

OpAttrs GetSumBackwardsOp() {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("_backward_add");
  attrs.num_inputs = 1;
  attrs.num_outputs = 2;
  attrs.dispatches.resize(2);
  attrs.dispatches[0] = DispatchMode::kFCompute;
  attrs.dispatches[1] = DispatchMode::kFComputeEx;
  attrs.requests.insert(OpReqType::kWriteTo);
  attrs.requests.insert(OpReqType::kWriteInplace);
  attrs.requests.insert(OpReqType::kAddTo);
  return attrs;
}

OpAttrs GetConcatOp(int num_args, int dim) {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("concat");
  attrs.num_inputs = num_args;
  attrs.num_outputs = 1;
  attrs.attrs.dict.insert({"num_args" , std::to_string(num_args)});
  attrs.attrs.dict.insert({"dim" , std::to_string(dim)});
  attrs.attrs.op->attr_parser(&attrs.attrs);
  attrs.dispatches.resize(2);
  attrs.dispatches[0] = DispatchMode::kFCompute;
  attrs.dispatches[1] = DispatchMode::kFComputeEx;
  return attrs;
}

OpAttrs GetConcatBackwardsOp(int num_args, int dim) {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("_backward_Concat");
  attrs.num_inputs = 2;
  attrs.num_outputs = num_args;
  attrs.attrs.dict.insert({"num_args" , std::to_string(num_args)});
  attrs.attrs.dict.insert({"dim" , std::to_string(dim)});
  attrs.attrs.op->attr_parser(&attrs.attrs);
  attrs.dispatches.resize(2);
  attrs.dispatches[0] = DispatchMode::kFCompute;
  attrs.dispatches[1] = DispatchMode::kFComputeEx;
  return attrs;
}

OpAttrs GetPoolingOp(int kernel, int dim, int stride, int pad) {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("Pooling");
  attrs.num_inputs = 1;
  attrs.num_outputs = dim == 2 ? 2 : 1;
  attrs.attrs.dict.insert({"kernel" , CreateShapeString(kernel, dim)});
  attrs.attrs.dict.insert({"stride" , CreateShapeString(stride, dim)});
  attrs.attrs.dict.insert({"pad" , CreateShapeString(pad, dim)});
  attrs.attrs.dict.insert({"pool_type" , "max"});
  attrs.attrs.op->attr_parser(&attrs.attrs);
  return attrs;
}

OpAttrs GetPoolingBackwardsOp(int kernel, int dim, int stride, int pad) {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("_backward_Pooling");
  attrs.num_inputs = dim == 2 ? 5 : 3;
  attrs.num_outputs = 1;
  attrs.attrs.dict.insert({"kernel", CreateShapeString(kernel, dim)});
  attrs.attrs.dict.insert({"stride", CreateShapeString(stride, dim)});
  attrs.attrs.dict.insert({"pad", CreateShapeString(pad, dim)});
  attrs.attrs.dict.insert({"pool_type", "max"});
  attrs.attrs.op->attr_parser(&attrs.attrs);
  return attrs;
}

OpAttrs GetLRNOp() {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("LRN");
  attrs.num_inputs = 1;
  attrs.num_outputs = 2;
  attrs.attrs.dict.insert({"nsize" , "3"});
  attrs.attrs.op->attr_parser(&attrs.attrs);
  attrs.dispatches.resize(2);
  attrs.requests.insert(OpReqType::kWriteTo);
  attrs.input_types = ArrayTypes::Normal |
      ArrayTypes::MKLDNN |
      ArrayTypes::NormalReshaped |
      ArrayTypes::MKLDNNReshaped;
  attrs.output_types = ArrayTypes::Normal |
      ArrayTypes::MKLDNN |
      ArrayTypes::NormalReshaped |
      ArrayTypes::MKLDNNReshaped;
  return attrs;
}

OpAttrs GetLRNBackwardsOp() {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("_backward_LRN");
  attrs.num_inputs = 3;
  attrs.num_outputs = 1;
  attrs.attrs.dict.insert({"nsize" , "3"});
  attrs.attrs.op->attr_parser(&attrs.attrs);
  attrs.dispatches.resize(2);
  attrs.requests.insert(OpReqType::kWriteTo);
  return attrs;
}

OpAttrs GetFullyConnectedOp() {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("FullyConnected");
  attrs.attrs.dict.insert({"num_hidden" , "20"});
  attrs.num_inputs = 3;
  attrs.num_outputs = 1;
  attrs.attrs.op->attr_parser(&attrs.attrs);
  attrs.requests.insert(OpReqType::kWriteTo);
  attrs.input_types = ArrayTypes::Normal |
      ArrayTypes::MKLDNN |
      ArrayTypes::NormalReshaped |
      ArrayTypes::MKLDNNReshaped;
  attrs.output_types = ArrayTypes::Normal |
      ArrayTypes::MKLDNN |
      ArrayTypes::NormalReshaped |
      ArrayTypes::MKLDNNReshaped;
  return attrs;
}

OpAttrs GetFullyConnectedBackwardsOp() {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("_backward_FullyConnected");
  attrs.attrs.dict.insert({"num_hidden" , "20"});
  attrs.num_inputs = 3;
  attrs.num_outputs = 3;
  attrs.attrs.op->attr_parser(&attrs.attrs);
  attrs.requests.insert(OpReqType::kWriteTo);
  return attrs;
}

OpAttrs GetConvOp(int kernel, int num_filters, int dim, int stride, int pad) {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("Convolution");
  attrs.num_inputs = 3;
  attrs.num_outputs = 1;
  attrs.attrs.dict.insert({"kernel" , CreateShapeString(kernel, dim)});
  attrs.attrs.dict.insert({"num_filter" , std::to_string(num_filters)});
  attrs.attrs.dict.insert({"stride" , CreateShapeString(stride, dim)});
  attrs.attrs.dict.insert({"pad" , CreateShapeString(pad, dim)});
  attrs.attrs.op->attr_parser(&attrs.attrs);
  attrs.input_types = ArrayTypes::Normal |
      ArrayTypes::MKLDNN |
      ArrayTypes::NormalReshaped |
      ArrayTypes::MKLDNNReshaped |
      ArrayTypes::NormalReused |
      ArrayTypes::MKLDNNReused |
      ArrayTypes::NormalReshapedReused;
  attrs.output_types = ArrayTypes::Normal |
      ArrayTypes::MKLDNN |
      ArrayTypes::NormalReshaped |
      ArrayTypes::MKLDNNReshaped |
      ArrayTypes::NormalReused |
      ArrayTypes::MKLDNNReused |
      ArrayTypes::NormalReshapedReused |
      ArrayTypes::NormalReusedDiffDtype;
  return attrs;
}

OpAttrs GetConvBackwardOp(int kernel, int num_filters, int dim, int stride, int pad) {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("_backward_Convolution");
  attrs.num_inputs = 4;
  attrs.num_outputs = 3;
  attrs.attrs.dict.insert({"kernel" , CreateShapeString(kernel, dim)});
  attrs.attrs.dict.insert({"num_filter" , std::to_string(num_filters)});
  attrs.attrs.dict.insert({"stride" , CreateShapeString(stride, dim)});
  attrs.attrs.dict.insert({"pad" , CreateShapeString(pad, dim)});
  attrs.attrs.op->attr_parser(&attrs.attrs);
  return attrs;
}

OpAttrs GetDeconvOp(int kernel, int num_filters, int dim, int stride, int pad) {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("Deconvolution");
  attrs.num_inputs = 2;
  attrs.num_outputs = 1;
  attrs.attrs.dict.insert({"kernel" , CreateShapeString(kernel, dim)});
  attrs.attrs.dict.insert({"num_filter" , std::to_string(num_filters)});
  attrs.attrs.dict.insert({"stride" , CreateShapeString(stride, dim)});
  attrs.attrs.dict.insert({"pad" , CreateShapeString(pad, dim)});
  attrs.attrs.op->attr_parser(&attrs.attrs);
  attrs.input_types = ArrayTypes::Normal |
      ArrayTypes::MKLDNN |
      ArrayTypes::NormalReshaped |
      ArrayTypes::MKLDNNReshaped |
      ArrayTypes::NormalReused |
      ArrayTypes::MKLDNNReused |
      ArrayTypes::NormalReshapedReused;
  attrs.output_types = ArrayTypes::Normal |
      ArrayTypes::MKLDNN |
      ArrayTypes::NormalReshaped |
      ArrayTypes::MKLDNNReshaped |
      ArrayTypes::NormalReused |
      ArrayTypes::MKLDNNReused |
      ArrayTypes::NormalReshapedReused |
      ArrayTypes::NormalReusedDiffDtype;
  return attrs;
}

OpAttrs GetDeconvBackwardOp(int kernel, int num_filters, int dim, int stride, int pad) {
  OpAttrs attrs;
  attrs.attrs.op = Op::Get("_backward_Deconvolution");
  attrs.num_inputs = 3;
  attrs.num_outputs = 2;
  attrs.attrs.dict.insert({"kernel" , CreateShapeString(kernel, dim)});
  attrs.attrs.dict.insert({"num_filter" , std::to_string(num_filters)});
  attrs.attrs.dict.insert({"stride" , CreateShapeString(stride, dim)});
  attrs.attrs.dict.insert({"pad" , CreateShapeString(pad, dim)});
  attrs.attrs.op->attr_parser(&attrs.attrs);
  return attrs;
}

void AssertEqual(const std::vector<NDArray *> &in_arrs,
                 const std::vector<NDArray *> &out_arrs,
                 float rtol = 1e-5, float atol = 1e-8) {
  NDArray tmp1 = in_arrs[0]->Reorder2Default();
  NDArray tmp2 = out_arrs[0]->Reorder2Default();
  EXPECT_EQ(tmp1.shape().Size(), tmp2.shape().Size());
  TBlob blob1 = tmp1.data();
  TBlob blob2 = tmp2.data();
  mshadow::default_real_t *d1 = static_cast<mshadow::default_real_t*>(blob1.dptr_);
  mshadow::default_real_t *d2 = static_cast<mshadow::default_real_t*>(blob2.dptr_);
  for (int i = 0; i < tmp1.shape().Size(); i++) {
    float abs_err = fabs((d1[i]) - (d2[i]));
    ASSERT_LE(abs_err, (atol + rtol * fabs(d2[i])));
  }
}

void VerifyActResult(const std::vector<NDArray *> &in_arrs,
                     const std::vector<NDArray *> &out_arrs) {
  NDArray tmp1 = in_arrs[0]->Reorder2Default();
  NDArray tmp2 = out_arrs[0]->Reorder2Default();
  TBlob blob1 = tmp1.data();
  TBlob blob2 = tmp2.data();
  mshadow::default_real_t *d1 = static_cast<mshadow::default_real_t*>(blob1.dptr_);
  mshadow::default_real_t *d2 = static_cast<mshadow::default_real_t*>(blob2.dptr_);
  EXPECT_EQ(tmp1.shape().Size(), tmp2.shape().Size());
  for (size_t i = 0; i < tmp1.shape().Size(); i++) {
    EXPECT_EQ(std::fmax(d1[i], 0), d2[i]);
  }
}

void VerifyActBackwardsResult(const std::vector<NDArray *> &in_arrs,
                              const std::vector<NDArray *> &out_arrs) {
  NDArray tmp1 = in_arrs[0]->Reorder2Default();  // out grads
  NDArray tmp2 = in_arrs[1]->Reorder2Default();  // input
  NDArray tmp3 = out_arrs[0]->Reorder2Default();  // input grads
  TBlob blob1 = tmp1.data();
  TBlob blob2 = tmp2.data();
  TBlob blob3 = tmp3.data();
  mshadow::default_real_t *d1 = static_cast<mshadow::default_real_t*>(blob1.dptr_);
  mshadow::default_real_t *d2 = static_cast<mshadow::default_real_t*>(blob2.dptr_);
  mshadow::default_real_t *d3 = static_cast<mshadow::default_real_t*>(blob3.dptr_);
  EXPECT_EQ(tmp1.shape().Size(), tmp2.shape().Size());
  for (size_t i = 0; i < tmp1.shape().Size(); i++) {
    ASSERT_EQ(d2[i] > 0 ? d1[i] : 0, d3[i]);
  }
}

void VerifySumBackwardsResult(const std::vector<NDArray *> &in_arrs,
                               const std::vector<NDArray *> &out_arrs) {
  NDArray out_grads = in_arrs[0]->Reorder2Default();  // out grads
  NDArray input_grads1 = out_arrs[0]->Reorder2Default();  // input grads
  NDArray input_grads2 = out_arrs[1]->Reorder2Default();  // input grads
  mshadow::default_real_t *og = out_grads.data().dptr<mshadow::default_real_t>();
  mshadow::default_real_t *ig1 = input_grads1.data().dptr<mshadow::default_real_t>();
  mshadow::default_real_t *ig2 = input_grads2.data().dptr<mshadow::default_real_t>();
  for (size_t i = 0; i < out_grads.shape().Size(); i++) {
    ASSERT_EQ(og[i], ig1[i]);
    ASSERT_EQ(og[i], ig2[i]);
  }
}

void VerifyConcatResult(const std::vector<NDArray *> &in_arrs,
                        const std::vector<NDArray *> &out_arrs) {
  int num_inputs = in_arrs.size();
  int input_size = in_arrs[0]->shape().Size();
  TShape input_shape = in_arrs[0]->shape();
  NDArray output = out_arrs[0]->Reorder2Default();
  size_t total_size = output.shape().Size();
  EXPECT_EQ(input_size * num_inputs, total_size);
  mshadow::default_real_t *out_data = output.data().dptr<mshadow::default_real_t>();

  int dim = GetDim(input_shape, output.shape());
  int block_size = GetBlockSize(input_shape, dim);
  int num_blocks = input_size / block_size;
  for (size_t input_num = 0; input_num < num_inputs; input_num++) {
    NDArray tmp = in_arrs[input_num]->Reorder2Default();
    mshadow::default_real_t* data = tmp.data().dptr<mshadow::default_real_t>();
    for (size_t block_num = 0; block_num < num_blocks; block_num++) {
      for (size_t i = 0; i < block_size; i++)
        ASSERT_EQ(data[block_num * block_size + i],
                  out_data[(block_num * num_inputs + input_num) * block_size + i]);
    }
  }
}

void VerifyConcatBackwardsResult(const std::vector<NDArray *> &in_arrs,
                        const std::vector<NDArray *> &out_arrs) {
  // in_arrs is larger array, out_arr is ammler
  int num_inputs = out_arrs.size();
  int input_size = out_arrs[0]->shape().Size();
  TShape input_shape = out_arrs[0]->shape();
  NDArray output = in_arrs[0]->Reorder2Default();
  size_t total_size = output.shape().Size();
  EXPECT_EQ(input_size * num_inputs, total_size);
  mshadow::default_real_t *out_data = output.data().dptr<mshadow::default_real_t>();

  int dim = GetDim(input_shape, output.shape());
  int block_size = GetBlockSize(input_shape, dim);
  int num_blocks = input_size / block_size;
  for (size_t input_num = 0; input_num < num_inputs; input_num++) {
    NDArray tmp = out_arrs[input_num]->Reorder2Default();
    mshadow::default_real_t* data = tmp.data().dptr<mshadow::default_real_t>();
    for (size_t block_num = 0; block_num < num_blocks; block_num++) {
      for (size_t i = 0; i < block_size; i++)
        ASSERT_EQ(data[block_num * block_size + i],
                  out_data[(block_num * num_inputs + input_num) * block_size + i]);
    }
  }
}

void TestOp(const OpAttrs &attrs, VerifyFunc verify_fn) {
  std::vector<NDArray*> inputs(attrs.num_inputs);
  std::vector<NDArray*> outputs(attrs.num_outputs);
  std::vector<OpReqType> req(attrs.num_outputs);
  std::vector<NDArrayAttrs> in_arrs;
  std::vector<std::vector<NDArrayAttrs>> out_arrs(attrs.num_outputs);
  std::vector<DispatchMode> dispatches = attrs.dispatches;

  TestArrayShapes tas = GetTestArrayShapes();
  std::vector<mkldnn::memory::primitive_desc> pds = tas.pds;

  if (attrs.requests.find(OpReqType::kWriteTo) != attrs.requests.end()) {
    std::vector<NDArrayAttrs> in_arrs = GetTestInputArrays();
    for (auto &in_arr : in_arrs) {
      for (auto &dispatch : dispatches) {
        std::vector<std::vector<NDArrayAttrs>> out_arrs(attrs.num_outputs);
        for (int i = 0; i < attrs.num_outputs; i++)
          out_arrs[i] = GetTestOutputArrays(in_arr.arr.shape(), pds);
        for (int i = 0; i < attrs.num_inputs; i++)
          inputs[i] = &in_arr.arr;
        for (size_t output_i = 0; output_i < out_arrs[0].size(); output_i++) {
          for (int i = 0; i < attrs.num_outputs; i++) {
            req[i] = kWriteTo;
            outputs[i] = &out_arrs[i][output_i].arr;
          }
          PrintVerifyMsg(in_arr, out_arrs[0][output_i]);
          Imperative::Get()->InvokeOp(Context(), attrs.attrs, inputs,
                                      outputs, req, dispatch, mxnet::OpStatePtr());
          Engine::Get()->WaitForAll();
          verify_fn(inputs, outputs);
        }
      }
    }
  }

  if (attrs.requests.find(OpReqType::kWriteInplace) != attrs.requests.end()) {
    for (auto &dispatch : dispatches) {
      in_arrs = GetTestInputArrays();
      for (auto &arr : in_arrs) {
        // If the array is a view, we shouldn't write data to it.
        if (arr.arr.IsView())
          continue;
        NDArrayAttrs orig(arr.arr.Copy(arr.arr.ctx()), "InPlace Copy");
        for (int i = 0; i < attrs.num_inputs; i++)
          inputs[i] = &arr.arr;
        for (int i = 0; i < attrs.num_outputs; i++) {
          req[i] = kWriteInplace;
          outputs[i] = &arr.arr;
        }
        PrintVerifyMsg(orig, arr);
        Imperative::Get()->InvokeOp(Context(), attrs.attrs, inputs, outputs, req,
                                    dispatch, mxnet::OpStatePtr());
        Engine::Get()->WaitForAll();
        std::vector<NDArray *> orig_inputs(attrs.num_inputs);
        for (int i = 0; i < attrs.num_inputs; i++)
          orig_inputs[i] = &orig.arr;
        verify_fn(orig_inputs, outputs);
      }
    }
  }

  if (attrs.requests.find(OpReqType::kAddTo) != attrs.requests.end()) {
    std::vector<NDArray*> original_outputs(attrs.num_outputs);
    in_arrs = GetTestInputArrays();
    for (auto &in_arr : in_arrs) {
      for (auto &dispatch : dispatches) {
        for (int i = 0; i < attrs.num_outputs; i++)
          out_arrs[i] = GetTestOutputArrays(in_arr.arr.shape(), pds);
        for (size_t i = 0; i < attrs.num_inputs; i++)
          inputs[i] = &in_arr.arr;
        for (size_t output_i = 0; output_i < out_arrs[0].size(); output_i++) {
          NDArray tmp;
          for (size_t i = 0; i < attrs.num_outputs; i++) {
            auto out_arr = out_arrs[i][output_i];
            tmp = out_arr.arr.Copy(out_arr.arr.ctx());
            original_outputs[i] =  &tmp;
            outputs[i] = &out_arrs[i][output_i].arr;
            req[i] = kAddTo;
          }
          PrintVerifyMsg(in_arr, out_arrs[0][output_i]);
          Imperative::Get()->InvokeOp(Context(), attrs.attrs, inputs,
                                      outputs, req, dispatch, mxnet::OpStatePtr());
          Engine::Get()->WaitForAll();
          VerifyAddRequest(inputs, original_outputs, outputs, verify_fn);
        }
      }
    }
  }
}

void TestConcatOp(const OpAttrs &attrs, VerifyFunc verify_fn,
            bool backwards = false) {
  std::vector<NDArray*> inputs(attrs.num_inputs);
  std::vector<NDArray*> outputs(attrs.num_outputs);
  std::vector<OpReqType> req(attrs.num_outputs);
  std::vector<DispatchMode> dispatches = attrs.dispatches;

  TestArrayShapes tas = GetTestArrayShapes();
  std::vector<mkldnn::memory::primitive_desc> pds = tas.pds;

  std::vector<NDArrayAttrs> in_arrs = GetTestInputArrays();

  // concat backwards uses scaled up inputs
  if (backwards) {
    std::string str_dim = const_cast<OpAttrs&>(attrs).attrs.dict["dim"];
    int dim = std::stoi(str_dim);
    std::vector<float> scale_vector(dim+1);
    for (size_t i = 0; i < dim+1; ++i)
      scale_vector[i] = 1;
    scale_vector[dim] = attrs.num_outputs;
    in_arrs = GetTestInputArrays(ArrayTypes::All, false, scale_vector);
  }

  for (auto &in_arr : in_arrs) {
    for (auto &dispatch : dispatches) {
      std::vector<std::vector<NDArrayAttrs>> out_arrs(attrs.num_outputs);

      std::string str_dim = const_cast<OpAttrs&>(attrs).attrs.dict["dim"];
      int dim = std::stoi(str_dim);
      if (dim >= in_arr.arr.shape().ndim())
        continue;
      float scale = backwards ? 1 / static_cast<float>(attrs.num_outputs) :
          static_cast<float>(attrs.num_inputs);

      std::vector<float> scale_vector(in_arr.arr.shape().ndim());
      for (int i = 0; i < in_arr.arr.shape().ndim(); i++)
        scale_vector[i] = 1;
      scale_vector[dim] = scale;
      for (int i = 0; i < attrs.num_outputs; i++)
        out_arrs[i] = GetTestOutputArrays(in_arr.arr.shape(), pds, scale_vector);

      for (int i = 0; i < attrs.num_inputs; i++)
        inputs[i] = &in_arr.arr;

      for (size_t output_i = 0; output_i < out_arrs[0].size(); output_i++) {
        for (int i = 0; i < attrs.num_outputs; i++) {
          req[i] = kWriteTo;
          outputs[i] = &out_arrs[i][output_i].arr;
        }
        PrintVerifyMsg(in_arr, out_arrs[0][output_i]);
        Imperative::Get()->InvokeOp(Context(), attrs.attrs, inputs,
                                    outputs, req, dispatch, mxnet::OpStatePtr());
        Engine::Get()->WaitForAll();
        verify_fn(inputs, outputs);
      }
    }
  }
}

// compares output of fcompute with fcomputex
void TestOpEx(const OpAttrs &forward_attrs, const OpAttrs &backwards_attrs) {
  std::vector<NDArray*> inputs(forward_attrs.num_inputs);
  std::vector<NDArray*> outputs(forward_attrs.num_outputs);
  std::vector<NDArray*> ex_outputs(forward_attrs.num_outputs);

  std::vector<NDArray*> backwards_input(backwards_attrs.num_inputs);
  std::vector<NDArray*> backwards_outputs(backwards_attrs.num_outputs);
  std::vector<NDArray*> backwards_ex_outputs(backwards_attrs.num_outputs);


  std::vector<OpReqType> req(forward_attrs.num_outputs);
  std::vector<OpReqType> back_req(backwards_attrs.num_outputs);

  TestArrayShapes tas = GetTestArrayShapes();
  std::vector<mkldnn::memory::primitive_desc> pds = tas.pds;

  std::vector<NDArrayAttrs> in_arrs = GetTestInputArrays(forward_attrs.input_types, true);
  std::vector<std::vector<NDArrayAttrs>> out_arrs(forward_attrs.num_outputs);
  std::vector<std::vector<NDArrayAttrs>> ex_out_arrs(forward_attrs.num_outputs);

  if (forward_attrs.requests.find(OpReqType::kWriteTo) != forward_attrs.requests.end()) {
    for (int i1 = 0; i1 < in_arrs.size(); i1++) {
      auto in_arr = in_arrs[i1];

      // TODO(alex): (MXNET-845) Remove when MKLDNN supports other dims
      if (in_arr.arr.shape().ndim() != 4)
        continue;

      for (int i = 0; i < forward_attrs.num_outputs; i++) {
        out_arrs[i] =
            GetTestOutputArrays(in_arr.arr.shape(), pds, {1}, forward_attrs.output_types);
        ex_out_arrs[i] =
            GetTestOutputArrays(in_arr.arr.shape(), pds, {1}, forward_attrs.output_types);
      }

      for (int i = 0; i < forward_attrs.num_inputs; i++)
        inputs[i] = &in_arr.arr;

      for (size_t output_i = 0; output_i < out_arrs[0].size(); output_i++) {
        if (out_arrs[0][output_i].arr.IsMKLDNNData())
          continue;

        for (int i = 0; i < forward_attrs.num_outputs; i++) {
          req[i] = kWriteTo;
          outputs[i] = &out_arrs[i][output_i].arr;
          ex_outputs[i] = &ex_out_arrs[i][output_i].arr;
        }
        Imperative::Get()->set_is_training(true);

        PrintVerifyMsg(in_arr, out_arrs[0][output_i]);
        Imperative::Get()->InvokeOp(
            Context(), forward_attrs.attrs, inputs, outputs, req,
            DispatchMode::kFCompute, mxnet::OpStatePtr());
        Imperative::Get()->InvokeOp(
            Context(), forward_attrs.attrs, inputs, ex_outputs, req,
            DispatchMode::kFComputeEx, mxnet::OpStatePtr());
        Engine::Get()->WaitForAll();
        AssertEqual(outputs, ex_outputs);

        // backwards test performed same time since output needed
        backwards_input[0] = outputs[0];  // output grad
        backwards_input[1] = inputs[0];  // input
        backwards_input[2] = outputs[1];  // out norm

        auto tmp_output = GetTestInputArrays(forward_attrs.input_types, true)[i1];
        backwards_outputs[0] = &tmp_output.arr;

        auto tmp_output2 = GetTestInputArrays(forward_attrs.input_types, true)[i1];
        backwards_ex_outputs[0] = &tmp_output2.arr;

        for (int i = 0; i < backwards_attrs.num_outputs; i++)
          back_req[i] = kWriteTo;

        std::cout << "Backwards: ";
        PrintVerifyMsg(out_arrs[0][output_i], tmp_output);
        Imperative::Get()->InvokeOp(
            Context(), backwards_attrs.attrs, backwards_input, backwards_outputs,
            back_req, DispatchMode::kFCompute, mxnet::OpStatePtr());
        Imperative::Get()->InvokeOp(
            Context(), backwards_attrs.attrs, backwards_input, backwards_ex_outputs,
            back_req, DispatchMode::kFComputeEx, mxnet::OpStatePtr());
        Engine::Get()->WaitForAll();
        AssertEqual(backwards_outputs, backwards_ex_outputs);
      }
    }
  }
}

// Computes second dimension of FC weight matrix based on input shape
uint32_t GetFCWeightDim2(const nnvm::TShape arr) {
  uint32_t dim = 1;
  for (int i = 1; i < arr.ndim(); i++) {
    dim *= arr[i];
  }
  return dim;
}

void TestFullyConnectedOp(const OpAttrs &forward_attrs, const OpAttrs &backwards_attrs) {
  std::vector<NDArray*> inputs(forward_attrs.num_inputs);
  std::vector<NDArray*> outputs(forward_attrs.num_outputs);
  std::vector<NDArray*> ex_outputs(forward_attrs.num_outputs);

  std::vector<NDArray*> backwards_input(backwards_attrs.num_inputs);
  std::vector<NDArray*> backwards_outputs(backwards_attrs.num_outputs);
  std::vector<NDArray*> backwards_ex_outputs(backwards_attrs.num_outputs);

  std::vector<OpReqType> req(forward_attrs.num_outputs);
  std::vector<OpReqType> back_req(backwards_attrs.num_outputs);

  TestArrayShapes tas = GetTestArrayShapes();
  std::vector<mkldnn::memory::primitive_desc> pds = tas.pds;

  std::vector<NDArrayAttrs> in_arrs = GetTestInputArrays(forward_attrs.input_types, true);
  std::vector<std::vector<NDArrayAttrs>> out_arrs(forward_attrs.num_outputs);
  std::vector<std::vector<NDArrayAttrs>> ex_out_arrs(forward_attrs.num_outputs);

  std::string str_hid = const_cast<OpAttrs&>(forward_attrs).attrs.dict["num_hidden"];
  int num_hid = std::stoi(str_hid);

  if (forward_attrs.requests.find(OpReqType::kWriteTo) != forward_attrs.requests.end()) {
    for (int i1 = 0; i1 < in_arrs.size(); i1++) {
      auto in_arr = in_arrs[i1];
      auto in_shape = in_arr.arr.shape();
      if (in_shape.ndim() < 2)
        continue;

      nnvm::TShape wt_shape(2);
      wt_shape[0] = num_hid;
      wt_shape[1] = GetFCWeightDim2(in_shape);
      NDArray weights(wt_shape, Context());
      InitDefaultArray(&weights, false);

      nnvm::TShape bias_shape(1);
      bias_shape[0] = num_hid;
      NDArray bias(bias_shape, Context());
      InitDefaultArray(&bias, false);

      inputs[0] = &in_arr.arr;
      inputs[1] = &weights;
      inputs[2] = &bias;

      nnvm::TShape out_shape(2);
      out_shape[0] = in_shape[0];
      out_shape[1] = num_hid;

      for (int i = 0; i < forward_attrs.num_outputs; i++) {
        out_arrs[i] =
            GetTestOutputArrays(out_shape, pds, {1}, forward_attrs.output_types);
        ex_out_arrs[i] =
            GetTestOutputArrays(out_shape, pds, {1}, forward_attrs.output_types);
      }

      for (size_t output_i = 0; output_i < out_arrs[0].size(); output_i++) {
        for (int i = 0; i < forward_attrs.num_outputs; i++) {
          req[i] = kWriteTo;
          outputs[i] = &out_arrs[i][output_i].arr;
          ex_outputs[i] = &ex_out_arrs[i][output_i].arr;
        }
        Imperative::Get()->set_is_training(true);

        PrintVerifyMsg(in_arr, out_arrs[0][output_i]);
        Imperative::Get()->InvokeOp(
            Context(), forward_attrs.attrs, inputs, outputs, req,
            DispatchMode::kFCompute, mxnet::OpStatePtr());
        Imperative::Get()->InvokeOp(
            Context(), forward_attrs.attrs, inputs, ex_outputs, req,
            DispatchMode::kFComputeEx, mxnet::OpStatePtr());
        Engine::Get()->WaitForAll();
        AssertEqual(outputs, ex_outputs);

        // backwards test performed same time since output needed
        backwards_input[0] = outputs[0];  // output grad
        backwards_input[1] = inputs[0];  // input
        backwards_input[2] = inputs[1];  // weights

        auto tmp_output = GetTestInputArrays(forward_attrs.input_types, true)[i1];
        NDArray back_weights(wt_shape, Context());
        NDArray back_bias(bias_shape, Context());
        backwards_outputs[0] = &tmp_output.arr;
        backwards_outputs[1] = &back_weights;
        backwards_outputs[2] = &back_bias;

        auto tmp_output2 = GetTestInputArrays(forward_attrs.input_types, true)[i1];
        NDArray back_ex_weights(wt_shape, Context());
        NDArray back_ex_bias(bias_shape, Context());
        backwards_ex_outputs[0] = &tmp_output2.arr;
        backwards_ex_outputs[1] = &back_ex_weights;
        backwards_ex_outputs[2] = &back_ex_bias;

        for (int i = 0; i < backwards_attrs.num_outputs; i++)
          back_req[i] = kWriteTo;

        std::cout << "Backwards: ";
        PrintVerifyMsg(out_arrs[0][output_i], tmp_output);
        Imperative::Get()->InvokeOp(
            Context(), backwards_attrs.attrs, backwards_input, backwards_outputs,
            back_req, DispatchMode::kFCompute, mxnet::OpStatePtr());
        Imperative::Get()->InvokeOp(
            Context(), backwards_attrs.attrs, backwards_input, backwards_ex_outputs,
            back_req, DispatchMode::kFComputeEx, mxnet::OpStatePtr());
        Engine::Get()->WaitForAll();
        AssertEqual(backwards_outputs, backwards_ex_outputs);
      }
    }
  }
}

template<typename P>
void TestConvOp(const OpAttrs &forward_attrs, const OpAttrs &backwards_attrs,
                bool is_deconv = false) {
  std::vector<NDArray*> inputs(forward_attrs.num_inputs);
  std::vector<NDArray*> outputs(forward_attrs.num_outputs);
  std::vector<NDArray*> ex_outputs(forward_attrs.num_outputs);

  std::vector<NDArray*> backwards_input(backwards_attrs.num_inputs);
  std::vector<NDArray*> backwards_outputs(backwards_attrs.num_outputs);
  std::vector<NDArray*> backwards_ex_outputs(backwards_attrs.num_outputs);


  std::vector<OpReqType> req(forward_attrs.num_outputs);
  std::vector<OpReqType> back_req(backwards_attrs.num_outputs);
  std::vector<DispatchMode> dispatches = forward_attrs.dispatches;

  TestArrayShapes tas = GetTestArrayShapes();
  std::vector<mkldnn::memory::primitive_desc> pds = tas.pds;

  P param;
  param.Init(forward_attrs.attrs.dict);
  TShape kernel = param.kernel;
  TShape padding = param.pad;
  TShape stride = param.stride;
  int num_filter = param.num_filter;

  std::vector<NDArrayAttrs> in_arrs = GetTestInputArrays(
      forward_attrs.input_types, true, {1}, true);
  std::vector<std::vector<NDArrayAttrs>> out_arrs(forward_attrs.num_outputs);
  std::vector<std::vector<NDArrayAttrs>> ex_out_arrs(forward_attrs.num_outputs);

  for (size_t i1 = 0; i1 < in_arrs.size(); ++i1) {
    auto in_arr = in_arrs[i1];

    // can only conv only 4D inputs
    TShape input_shape = in_arr.arr.shape();
    if (input_shape.ndim() != kernel.ndim() + 2)
      continue;

    float scale = CalculateWidthConvOutput(input_shape[2], kernel[0], padding[0], stride[0])
        / static_cast<float>(input_shape[2]);

    if (is_deconv) {
      scale = CalculateWidthDeconvOutput(input_shape[2], kernel[0], padding[0], stride[0])
        / static_cast<float>(input_shape[2]);
    }
    std::vector<float> scale_vector(in_arr.arr.shape().ndim());
    scale_vector[0] = 1;
    scale_vector[1] = static_cast<float>(num_filter) / input_shape[1];
    scale_vector[2] = scale;
    scale_vector[3] = scale;

    for (size_t i = 0; i < forward_attrs.num_outputs; ++i) {
      out_arrs[i] = GetTestOutputArrays(in_arr.arr.shape(), pds,
                                        scale_vector, true, forward_attrs.output_types);
      ex_out_arrs[i] = GetTestOutputArrays(in_arr.arr.shape(), pds,
                                           scale_vector, true, forward_attrs.output_types);
    }
    NDArray ndkernel = CreateKernelNDArray(kernel, num_filter, in_arr.arr.shape(), is_deconv);
    TShape bias_shape = {num_filter};
    NDArray ndbias = CreateBiasNDArray(bias_shape);
    inputs[0] = &in_arr.arr;
    inputs[1] = &ndkernel;

    if (!param.no_bias) {
      inputs[2] = &ndbias;
    }

    for (size_t output_i = 0; output_i < out_arrs[0].size(); output_i++) {
      for (size_t i = 0; i < forward_attrs.num_outputs; ++i) {
        req[i] = kWriteTo;
        outputs[i] = &out_arrs[i][output_i].arr;
        ex_outputs[i] = &ex_out_arrs[i][output_i].arr;
      }
      Imperative::Get()->set_is_training(true);

      PrintVerifyMsg(in_arr, out_arrs[0][output_i]);
      Imperative::Get()->InvokeOp(Context(), forward_attrs.attrs, inputs,
                                  outputs, req, DispatchMode::kFCompute, mxnet::OpStatePtr());
      Imperative::Get()->InvokeOp(Context(), forward_attrs.attrs, inputs,
                                  ex_outputs, req, DispatchMode::kFComputeEx, mxnet::OpStatePtr());
      Engine::Get()->WaitForAll();
      VerifyCopyResult(outputs, ex_outputs);

      // backwards test performed same time since output needed
      backwards_input[0] = outputs[0];  // output grad
      backwards_input[1] = inputs[0];  // input
      backwards_input[2] = inputs[1];  // kernel

      if (!param.no_bias) {
        backwards_input[3] = inputs[2];  // bias
      }

      auto tmp_output = GetTestInputArrays(forward_attrs.input_types, true, {1}, true)[i1];
      NDArray tmp_kernel = CreateKernelNDArray(kernel, num_filter, in_arr.arr.shape(), is_deconv);
      NDArray tmp_bias = CreateBiasNDArray(bias_shape);
      backwards_outputs[0] = &tmp_output.arr;
      backwards_outputs[1] = &tmp_kernel;
      if (!param.no_bias) {
        backwards_outputs[2] = &tmp_bias;
      }

      auto tmp_output2 = GetTestInputArrays(forward_attrs.input_types, true, {1}, true)[i1];
      NDArray tmp_kernel2 = CreateKernelNDArray(kernel, num_filter, in_arr.arr.shape(), is_deconv);
      NDArray tmp_bias2 = CreateBiasNDArray(bias_shape);
      backwards_ex_outputs[0] = &tmp_output2.arr;
      backwards_ex_outputs[1] = &tmp_kernel2;
      if (!param.no_bias) {
        backwards_ex_outputs[2] = &tmp_bias2;
      }

      for (size_t i = 0; i < backwards_attrs.num_outputs; ++i)
        back_req[i] = kWriteTo;

      std::cout << "Backwards: ";
      PrintVerifyMsg(out_arrs[0][output_i], tmp_output);
      Imperative::Get()->InvokeOp(
          Context(), backwards_attrs.attrs, backwards_input, backwards_outputs,
          back_req, DispatchMode::kFCompute, mxnet::OpStatePtr());
      Imperative::Get()->InvokeOp(
          Context(), backwards_attrs.attrs, backwards_input, backwards_ex_outputs,
          back_req, DispatchMode::kFComputeEx, mxnet::OpStatePtr());
      Engine::Get()->WaitForAll();
      VerifyCopyResult(backwards_outputs, backwards_ex_outputs);
    }
  }
}

void TestPoolingOp(const OpAttrs &forward_attrs, const OpAttrs &backwards_attrs) {
  std::vector<NDArray*> inputs(forward_attrs.num_inputs);
  std::vector<NDArray*> outputs(forward_attrs.num_outputs);
  std::vector<NDArray*> ex_outputs(forward_attrs.num_outputs);

  std::vector<NDArray*> backwards_input(backwards_attrs.num_inputs);
  std::vector<NDArray*> backwards_outputs(backwards_attrs.num_outputs);
  std::vector<NDArray*> backwards_ex_outputs(backwards_attrs.num_outputs);


  std::vector<OpReqType> req(forward_attrs.num_outputs);
  std::vector<OpReqType> back_req(backwards_attrs.num_outputs);
  std::vector<DispatchMode> dispatches = forward_attrs.dispatches;

  TestArrayShapes tas = GetTestArrayShapes();
  std::vector<mkldnn::memory::primitive_desc> pds = tas.pds;

  mxnet::op::PoolingParam param;
  param.Init(forward_attrs.attrs.dict);
  TShape kernel = param.kernel;
  TShape padding = param.pad;
  TShape stride = param.stride;

  std::vector<NDArrayAttrs> in_arrs = GetTestInputArrays();
  std::vector<std::vector<NDArrayAttrs>> out_arrs(forward_attrs.num_outputs);
  std::vector<std::vector<NDArrayAttrs>> ex_out_arrs(forward_attrs.num_outputs);

  for (int i1 = 0; i1 < in_arrs.size(); i1++) {
    auto in_arr = in_arrs[i1];

    // can only pool only 3D and 4D inputs
    TShape input_shape = in_arr.arr.shape();
    if (input_shape.ndim() != kernel.ndim() + 2)
      continue;
    // cannot pool if ndarray and mkldnn memory have different ndim
    if (in_arr.arr.IsView() || in_arr.arr.GetMKLDNNData()->get_primitive_desc().desc().data.ndims
        != in_arr.arr.shape().ndim())
      continue;
    std::vector<float> scale_vector(in_arr.arr.shape().ndim());
    for (int i = 0; i < in_arr.arr.shape().ndim(); i++) {
      if (i < 2)
        scale_vector[i] = 1;
      else
        scale_vector[i] = CalculateWidthPoolOutput(
            input_shape[i], kernel[i-2], padding[i-2], stride[i-2]) /
            static_cast<float>(input_shape[i]);
    }
    for (int i = 0; i < forward_attrs.num_outputs; i++) {
      out_arrs[i] = GetTestOutputArrays(in_arr.arr.shape(), pds, scale_vector);
      ex_out_arrs[i] = GetTestOutputArrays(in_arr.arr.shape(), pds, scale_vector);
    }

    for (int i = 0; i < forward_attrs.num_inputs; i++)
      inputs[i] = &in_arr.arr;

    for (size_t output_i = 0; output_i < out_arrs[0].size(); output_i++) {
      for (int i = 0; i < forward_attrs.num_outputs; i++) {
        req[i] = kWriteTo;
        outputs[i] = &out_arrs[i][output_i].arr;
        ex_outputs[i] = &ex_out_arrs[i][output_i].arr;
      }
      Imperative::Get()->set_is_training(true);

      PrintVerifyMsg(in_arr, out_arrs[0][output_i]);
      Imperative::Get()->InvokeOp(Context(), forward_attrs.attrs, inputs,
                                  outputs, req, DispatchMode::kFCompute, mxnet::OpStatePtr());
      Imperative::Get()->InvokeOp(Context(), forward_attrs.attrs, inputs,
                                  ex_outputs, req, DispatchMode::kFComputeEx, mxnet::OpStatePtr());
      Engine::Get()->WaitForAll();
      VerifyCopyResult(outputs, ex_outputs);


      // backwards test performed same time since output needed
      if (backwards_attrs.num_inputs == 3) {
        backwards_input[0] = outputs[0];  // output grad
        backwards_input[1] = inputs[0];  // input
        backwards_input[2] = outputs[0];  // output
      } else if (backwards_attrs.num_inputs == 5) {
        backwards_input[0] = outputs[0];  // output grad
        backwards_input[1] = outputs[0];  // workspace grad
        backwards_input[2] = inputs[0];  // input
        backwards_input[3] = outputs[0];  // output
        backwards_input[4] = ex_outputs[1];  // workspace
      }

      // needs copies of inputs since they be reused in next iteration
      // cannot use Copy method since we need to maintain MKLDNN format
      auto tmp_output = GetTestInputArrays()[i1];
      auto tmp_output2 = GetTestInputArrays()[i1];
      backwards_outputs[0] = &tmp_output.arr;
      backwards_ex_outputs[0] = &tmp_output2.arr;
      back_req[0] = kWriteTo;
      std::cout << "Backwards: ";
      PrintVerifyMsg(out_arrs[0][output_i], tmp_output);
      Imperative::Get()->InvokeOp(
          Context(), backwards_attrs.attrs, backwards_input, backwards_outputs,
          back_req, DispatchMode::kFCompute, mxnet::OpStatePtr());
      Imperative::Get()->InvokeOp(
          Context(), backwards_attrs.attrs, backwards_input, backwards_ex_outputs,
          back_req, DispatchMode::kFComputeEx, mxnet::OpStatePtr());
      Engine::Get()->WaitForAll();
      VerifyCopyResult(backwards_outputs, backwards_ex_outputs);
    }
  }
}

TEST(IMPERATIVE, CopyOp) {
  OpAttrs attrs = GetCopyOp();
  TestOp(attrs, VerifyCopyResult);
}

TEST(IMPERATIVE, CopyBackwardsOp) {
  OpAttrs attrs = GetCopyBackwardsOp();
  TestOp(attrs, VerifyCopyResult);
}

TEST(IMPERATIVE, ActOp) {
  OpAttrs attrs = GetReluOp();
  TestOp(attrs, VerifyActResult);
}

TEST(IMPERATIVE, ActBackwardsOp) {
  OpAttrs attrs = GetReluBackwardsOp();
  TestOp(attrs, VerifyActBackwardsResult);
}

TEST(IMPERATIVE, SumOp) {
  OpAttrs attrs = GetSumOp();
  TestOp(attrs, VerifySumResult);
}

TEST(IMPERATIVE, SumBackwardsOp) {
  OpAttrs attrs = GetSumBackwardsOp();
  TestOp(attrs, VerifySumBackwardsResult);
}

TEST(IMPERATIVE, ConcatOp) {
  for (int num_inputs = 2; num_inputs < 4; num_inputs++) {
    for (int dim = 0; dim < 5; dim++) {
      OpAttrs attrs = GetConcatOp(num_inputs, dim);
      TestConcatOp(attrs, VerifyConcatResult);
    }
  }
}

TEST(IMPERATIVE, ConcatBackwardsOp) {
  for (int num_inputs = 2; num_inputs < 4; num_inputs++) {
    for (int dim = 0; dim < 5; dim++) {
      OpAttrs attrs = GetConcatBackwardsOp(num_inputs, dim);
      TestConcatOp(attrs, VerifyConcatBackwardsResult, true);
    }
  }
}

TEST(IMPERATIVE, LRNOp) {
  OpAttrs forward_attrs = GetLRNOp();
  OpAttrs backwards_attrs = GetLRNBackwardsOp();
  TestOpEx(forward_attrs, backwards_attrs);
}

TEST(IMPERATIVE, FullyConnectedOp) {
  OpAttrs forward_attrs = GetFullyConnectedOp();
  OpAttrs backwards_attrs = GetFullyConnectedBackwardsOp();
  TestFullyConnectedOp(forward_attrs, backwards_attrs);
}

TEST(IMPERATIVE, PoolingOp) {
  for (int dim = 2; dim < 4; dim++) {
    for (int kernel = 1; kernel < 4; kernel++) {
      for (int stride = 1; stride < 3; stride++) {
        for (int pad = 0; pad < 2; pad++) {
          if (kernel / 2. < pad)
            continue;
          OpAttrs forward_attrs = GetPoolingOp(kernel, dim, stride, pad);
          OpAttrs backwards_attrs = GetPoolingBackwardsOp(kernel, dim, stride, pad);
          TestPoolingOp(forward_attrs, backwards_attrs);
        }
      }
    }
  }
}

TEST(IMPERATIVE, ConvOp) {
  int dim = 2;  // MKLDNN conv only supports 2d kernels
  for (size_t num_filters = 2; num_filters < 3; ++num_filters) {
    for (size_t kernel = 1; kernel < 4; ++kernel) {
      for (size_t stride = 1; stride < 3; ++stride) {
        for (size_t pad = 0; pad < 2; ++pad) {
          if (kernel / 2. < pad)
            continue;
          OpAttrs forward_attrs = GetConvOp(kernel, num_filters, dim, stride, pad);
          OpAttrs backwards_attrs = GetConvBackwardOp(kernel, num_filters, dim, stride, pad);
          TestConvOp<mxnet::op::ConvolutionParam>(forward_attrs, backwards_attrs);
        }
      }
    }
  }
}

TEST(IMPERATIVE, DeconvOp) {
  int dim = 2;  // MKLDNN deconv only supports 2d kernels
  for (size_t num_filters = 2; num_filters < 3; ++num_filters) {
    for (size_t kernel = 1; kernel < 3; ++kernel) {
      for (size_t stride = 1; stride < 3; ++stride) {
        for (size_t pad = 0; pad < 2; ++pad) {
          if (kernel / 2. < pad)
            continue;
          OpAttrs forward_attrs = GetDeconvOp(kernel, num_filters, dim, stride, pad);
          OpAttrs backwards_attrs = GetDeconvBackwardOp(kernel, num_filters, dim, stride, pad);
          TestConvOp<mxnet::op::DeconvolutionParam>(forward_attrs, backwards_attrs, true);
        }
      }
    }
  }
}

#endif