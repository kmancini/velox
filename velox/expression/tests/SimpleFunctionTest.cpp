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

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "velox/functions/Udf.h"
#include "velox/functions/common/tests/FunctionBaseTest.h"

namespace {

using namespace facebook::velox;

class SimpleFunctionTest : public functions::test::FunctionBaseTest {};

// Some input data.
static std::vector<std::vector<int64_t>> arrayData = {
    {0, 1, 2, 4},
    {99, 98},
    {101, 42},
    {10001, 12345676},
};

// Function that returns an array of bigints.
VELOX_UDF_BEGIN(array_writer_func)
FOLLY_ALWAYS_INLINE bool call(
    out_type<Array<int64_t>>& out,
    const arg_type<int64_t>& input) {
  const size_t size = arrayData[input].size();
  out.reserve(out.size() + size);
  for (const auto i : arrayData[input]) {
    out.append(i);
  }
  return true;
}
VELOX_UDF_END();

TEST_F(SimpleFunctionTest, arrayWriter) {
  registerFunction<udf_array_writer_func, Array<int64_t>, int64_t>(
      {}, ARRAY(BIGINT()));

  const size_t rows = arrayData.size();
  auto flatVector = makeFlatVector<int64_t>(rows, [](auto row) { return row; });
  auto result = evaluate<ArrayVector>(
      "array_writer_func(c0)", makeRowVector({flatVector}));

  auto expected = vectorMaker_.arrayVector(arrayData);
  assertEqualVectors(expected, result);
}

// Funciton that takes an array as input.
VELOX_UDF_BEGIN(array_reader_func)
FOLLY_ALWAYS_INLINE bool call(
    int64_t& out,
    const arg_type<Array<int64_t>>& input) {
  out = input.size();
  return true;
}
VELOX_UDF_END();

TEST_F(SimpleFunctionTest, arrayReader) {
  registerFunction<udf_array_reader_func, int64_t, Array<int64_t>>();

  const size_t rows = arrayData.size();
  auto arrayVector = makeArrayVector(arrayData);
  auto result = evaluate<FlatVector<int64_t>>(
      "array_reader_func(c0)", makeRowVector({arrayVector}));

  auto arrayDataLocal = arrayData;
  auto expected = makeFlatVector<int64_t>(
      rows, [&arrayDataLocal](auto row) { return arrayDataLocal[row].size(); });
  assertEqualVectors(expected, result);
}

// Some input data for the rowVector.
static std::vector<int64_t> rowVectorCol1 = {0, 22, 44, 55, 99, 101, 9, 0};
static std::vector<double> rowVectorCol2 =
    {9.1, 22.4, 44.55, 99.9, 1.01, 9.8, 10001.1, 0.1};

// Function that returns a tuple.
VELOX_UDF_BEGIN(row_writer_func)
FOLLY_ALWAYS_INLINE bool call(
    out_type<Row<int64_t, double>>& out,
    const arg_type<int64_t>& input) {
  out = std::make_tuple(rowVectorCol1[input], rowVectorCol2[input]);
  return true;
}
VELOX_UDF_END();

TEST_F(SimpleFunctionTest, rowWriter) {
  registerFunction<udf_row_writer_func, Row<int64_t, double>, int64_t>(
      {}, ROW({BIGINT(), DOUBLE()}));

  const size_t rows = rowVectorCol1.size();
  auto flatVector = makeFlatVector<int64_t>(rows, [](auto row) { return row; });
  auto result =
      evaluate<RowVector>("row_writer_func(c0)", makeRowVector({flatVector}));

  auto vector1 = vectorMaker_.flatVector(rowVectorCol1);
  auto vector2 = vectorMaker_.flatVector(rowVectorCol2);
  auto expected = makeRowVector({vector1, vector2});
  assertEqualVectors(expected, result);
}

// Function that takes a tuple as a parameter.
VELOX_UDF_BEGIN(row_reader_func)
FOLLY_ALWAYS_INLINE bool call(
    int64_t& out,
    const arg_type<Row<int64_t, double>>& input) {
  out = *input.template at<0>();
  return true;
}
VELOX_UDF_END();

TEST_F(SimpleFunctionTest, rowReader) {
  registerFunction<udf_row_reader_func, int64_t, Row<int64_t, double>>();

  const size_t rows = rowVectorCol1.size();
  auto vector1 = vectorMaker_.flatVector(rowVectorCol1);
  auto vector2 = vectorMaker_.flatVector(rowVectorCol2);
  auto internalRowVector = makeRowVector({vector1, vector2});
  auto result = evaluate<FlatVector<int64_t>>(
      "row_reader_func(c0)", makeRowVector({internalRowVector}));

  auto expected = vectorMaker_.flatVector(rowVectorCol1);
  assertEqualVectors(expected, result);
}

// Function that returns an array of rows.
VELOX_UDF_BEGIN(array_row_writer_func)
FOLLY_ALWAYS_INLINE bool call(
    out_type<Array<Row<int64_t, double>>>& out,
    const arg_type<int32_t>& input) {
  // Appends each row tree times.
  auto tuple = std::make_tuple(rowVectorCol1[input], rowVectorCol2[input]);
  out.append(tuple);
  out.append(tuple);
  out.append(tuple);
  return true;
}
VELOX_UDF_END();

TEST_F(SimpleFunctionTest, arrayRowWriter) {
  registerFunction<
      udf_array_row_writer_func,
      Array<Row<int64_t, double>>,
      int32_t>({}, ARRAY(ROW({BIGINT(), DOUBLE()})));

  const size_t rows = rowVectorCol1.size();
  auto flatVector = makeFlatVector<int32_t>(rows, [](auto row) { return row; });
  auto result = evaluate<ArrayVector>(
      "array_row_writer_func(c0)", makeRowVector({flatVector}));

  std::vector<std::vector<variant>> data;
  for (int64_t i = 0; i < rows; ++i) {
    data.push_back({
        variant::row({rowVectorCol1[i], rowVectorCol2[i]}),
        variant::row({rowVectorCol1[i], rowVectorCol2[i]}),
        variant::row({rowVectorCol1[i], rowVectorCol2[i]}),
    });
  }
  auto expected =
      vectorMaker_.arrayOfRowVector(ROW({BIGINT(), DOUBLE()}), data);
  assertEqualVectors(expected, result);
}

// Function that takes an array of rows as an argument..
VELOX_UDF_BEGIN(array_row_reader_func)
FOLLY_ALWAYS_INLINE bool call(
    int64_t& out,
    const arg_type<Array<Row<int64_t, double>>>& input) {
  out = 0;
  for (size_t i = 0; i < input.size(); i++) {
    out += *input.at(i)->template at<0>();
  }
  return true;
}
VELOX_UDF_END();

TEST_F(SimpleFunctionTest, arrayRowReader) {
  registerFunction<
      udf_array_row_reader_func,
      int64_t,
      Array<Row<int64_t, double>>>();

  const size_t rows = rowVectorCol1.size();
  std::vector<std::vector<variant>> data;

  for (int64_t i = 0; i < rows; ++i) {
    data.push_back({
        variant::row({rowVectorCol1[i], rowVectorCol2[i]}),
        variant::row({rowVectorCol1[i], rowVectorCol2[i]}),
        variant::row({rowVectorCol1[i], rowVectorCol2[i]}),
    });
  }
  auto arrayVector =
      vectorMaker_.arrayOfRowVector(ROW({BIGINT(), DOUBLE()}), data);
  auto result = evaluate<FlatVector<int64_t>>(
      "array_row_reader_func(c0)", makeRowVector({arrayVector}));

  auto localData = rowVectorCol1;
  auto expected = makeFlatVector<int64_t>(
      rows, [&localData](auto row) { return localData[row] * 3; });
  assertEqualVectors(expected, result);
}

} // namespace
