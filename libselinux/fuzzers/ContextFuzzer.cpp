#include <stddef.h>
#include <stdint.h>

#include <selinux/context.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, [[maybe_unused]] size_t size) {
  context_t context = context_new((char*) data);
  context_free(context);

  return 0;
}
