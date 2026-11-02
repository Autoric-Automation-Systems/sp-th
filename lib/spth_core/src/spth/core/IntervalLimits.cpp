#include "spth/core/IntervalLimits.h"

namespace spth
{

bool isIntervalAllowed(uint32_t value, const IntervalLimits &limits)
{
  return value >= limits.minMs && value <= limits.maxMs;
}

uint32_t sanitizeInterval(uint32_t value, const IntervalLimits &limits)
{
  return isIntervalAllowed(value, limits) ? value : limits.fallbackMs;
}

} // namespace spth
