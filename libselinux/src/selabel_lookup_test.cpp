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

#include <fts.h>

#include <iostream>

#include <android-base/macros.h>
#include <gtest/gtest.h>
#include <selinux/android.h>
#include <selinux/label.h>

#include "selabel_lookup_test_golden.h"

/*
TEST(selabel_lookup, dump_sys) {
    const char* const paths[] = { "/sys", "/data", nullptr };

    struct selabel_handle* fc = selinux_android_file_context_handle();

    FTS* fts = fts_open(const_cast<char* const*>(paths), FTS_NOCHDIR | FTS_PHYSICAL | FTS_XDEV, nullptr);

    FTSENT* ftsent;
    while ((ftsent = fts_read(fts)) != nullptr) {
        switch (ftsent->fts_info) {
            case FTS_D:
            case FTS_F:
                char* context = nullptr;
                selabel_lookup(fc, &context, ftsent->fts_path, ftsent->fts_statp->st_mode);
                if (ftsent->fts_path && ftsent->fts_statp && context) {
                    std::cout << "{ \"" << ftsent->fts_path << "\", " << ftsent->fts_statp->st_mode
                              << ", \"" << context << "\" }," << std::endl;
                }
                free(context);
                context = nullptr;
        }
    }
    fts_close(fts);
}
*/

TEST(selabel_lookup, sysfs_compare_to_golden) {
  struct selabel_handle* fc = selinux_android_file_context_handle();
  for (size_t i = 0; i < arraysize(golden_sys_values); ++i) {
    char* context = nullptr;
    const GoldenValue* golden_value = &golden_sys_values[i];
    EXPECT_EQ(0, selabel_lookup(fc, &context, golden_value->path, golden_value->mode)) << golden_value->path << " " << golden_value->mode;
    EXPECT_STREQ(context, golden_value->label);
    free(context);
  }
}

TEST(selabel_lookup, data_compare_to_golden) {
  struct selabel_handle* fc = selinux_android_file_context_handle();
  for (size_t i = 0; i < arraysize(golden_data_values); ++i) {
    char* context = nullptr;
    const GoldenValue* golden_value = &golden_data_values[i];
    EXPECT_EQ(0, selabel_lookup(fc, &context, golden_value->path, golden_value->mode));
    EXPECT_STREQ(context, golden_value->label);
    free(context);
  }
}
