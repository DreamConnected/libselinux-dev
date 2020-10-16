#include <android-base/file.h>
#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>
#include <string>

#include <selinux/android.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FuzzedDataProvider fdp(data, size);
  std::string description = fdp.ConsumeRandomLengthString();
  std::vector<uint8_t> remaining_data = fdp.ConsumeRemainingBytes<uint8_t>();

  TemporaryFile temp_file;
  write(temp_file.fd, remaining_data.data(), remaining_data.size());
  lseek(temp_file.fd, 0, SEEK_SET);

  selinux_android_load_policy_from_fd(temp_file.fd, description.c_str());

  return 0;
}
