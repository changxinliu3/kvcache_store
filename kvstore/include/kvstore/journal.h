#pragma once

#include "kvstore/status.h"

#include <vector>

namespace kvstore {

struct KvExtent;
struct KvKey;
struct KvRecord;

// V1 stub: process restart invalidates namespace. Structure reserved for future journal.
class Journal {
 public:
  Status append_put(const KvKey& key, const KvRecord& record);
  Status append_erase(const KvKey& key);
  Status checkpoint();
};

}  // namespace kvstore
