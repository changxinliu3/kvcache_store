#include "kvstore/journal.h"

namespace kvstore {

Status Journal::append_put(const KvKey&, const KvRecord&) {
  // V1: metadata is memory-only. Journal is a structural placeholder.
  return Status::OK;
}

Status Journal::append_erase(const KvKey&) {
  return Status::OK;
}

Status Journal::checkpoint() {
  return Status::OK;
}

}  // namespace kvstore
