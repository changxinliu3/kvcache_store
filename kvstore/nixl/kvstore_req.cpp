#include "kvstore_req.h"

// Lifecycle mapping (spec §25-§29):
//   prepXfer  -> parse MD -> KvKey -> index/LBA lookup -> build KvstoreReq (no IO)
//   postXfer  -> IoEngine / KvRequest::submit
//   checkXfer -> KvRequest::query
//   releaseReqH -> free KvstoreReq / IoRequest metadata
