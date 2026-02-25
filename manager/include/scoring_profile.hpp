#pragma once

namespace monitor {

enum class ScoringProfile {
  BALANCED = 0,
  HIGH_CONCURRENCY = 1,
  IO_INTENSIVE = 2,
  MEMORY_SENSITIVE = 3,
};

}  // namespace monitor
