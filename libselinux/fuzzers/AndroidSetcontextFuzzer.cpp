#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>
#include <string>

#include <selinux/android.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FuzzedDataProvider fdp(data, size);
  uid_t uid = fdp.ConsumeIntegral<int>();
  bool isSystemServer = fdp.ConsumeBool();
  std::string pkgname = fdp.ConsumeRandomLengthString();
  std::vector<char> seinfo = fdp.ConsumeRemainingBytes<char>();

  selinux_android_setcontext(uid, isSystemServer, seinfo.data(), pkgname.c_str());

  return 0;
}
