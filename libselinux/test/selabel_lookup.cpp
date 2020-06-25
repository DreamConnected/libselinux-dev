/*
 * Copyright 2020, The Android Open Source Project
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

#include <getopt.h>
#include <gtest/gtest.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <selinux/selinux.h>
#include <selinux/label.h>

class SelabelLookupTest : public ::testing::Test {};

TEST_F(SelabelLookupTest, Test) {
    char *context = NULL;
    struct selabel_handle *hnd;
	struct selinux_opt selabel_option[] = {
		{ SELABEL_OPT_PATH, NULL },
		{ SELABEL_OPT_VALIDATE, NULL }
	};
    hnd = selabel_open(SELABEL_CTX_ANDROID_KEYSTORE_KEY, selabel_option, 2);
    int error = selabel_lookup(hnd, &context, "key", SELABEL_CTX_ANDROID_KEYSTORE_KEY);
    ASSERT_EQ(error, 0);
}
