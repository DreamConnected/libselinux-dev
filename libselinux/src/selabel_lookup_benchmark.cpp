/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <benchmark/benchmark.h>
#include <android-base/macros.h>
#include <selinux/android.h>
#include <selinux/label.h>

#include "selabel_lookup_test_golden.h"

static void SeLabelLookupSys(benchmark::State& state) {
  struct selabel_handle* fc = selinux_android_file_context_handle();
  while (state.KeepRunning()) {
    for (size_t i = 0; i < arraysize(golden_sys_values); ++i) {
      char* context = nullptr;
      const GoldenValue* golden_value = &golden_sys_values[i];
      selabel_lookup(fc, &context, golden_value->path, golden_value->mode);
      free(context);
    }
  }
}
BENCHMARK(SeLabelLookupSys);

static void SeLabelLookupData(benchmark::State& state) {
  struct selabel_handle* fc = selinux_android_file_context_handle();
  while (state.KeepRunning()) {
    for (size_t i = 0; i < arraysize(golden_data_values); ++i) {
      char* context = nullptr;
      const GoldenValue* golden_value = &golden_data_values[i];
      selabel_lookup(fc, &context, golden_value->path, golden_value->mode);
      free(context);
    }
  }
}
BENCHMARK(SeLabelLookupData);

BENCHMARK_MAIN()
