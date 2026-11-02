#include "spth/hw/PersistentStorage.h"

#include <Arduino.h>
#include <cstddef>

#include "spth/core/Checksum.h"

namespace spth
{
namespace
{

constexpr uint32_t kStateMagic = 0x53505448UL; // "SPTH"
constexpr uint32_t kMaxPlausibleBoots = 1000000UL;

RTC_NOINIT_ATTR DeviceState g_deviceState;
RTC_NOINIT_ATTR QueueStorage g_queueStorage;

uint32_t computeStateChecksum()
{
  return checksumOf(kChecksumSeed, &g_deviceState, offsetof(DeviceState, checksum));
}

void updateStateChecksum()
{
  g_deviceState.checksum = computeStateChecksum();
}

void resetState()
{
  g_deviceState.magic = kStateMagic;
  g_deviceState.bootCount = 0;
  g_deviceState.wifiFailures = 0;
  g_deviceState.lastBatteryPercent = -1;
  g_deviceState.lastRssi = 0;
  updateStateChecksum();
}

} // namespace

DeviceState &persistentDeviceState()
{
  return g_deviceState;
}

void beginPersistentState()
{
  if (g_deviceState.magic != kStateMagic || g_deviceState.bootCount > kMaxPlausibleBoots ||
      g_deviceState.lastBatteryPercent < -1 || g_deviceState.lastBatteryPercent > 100 ||
      g_deviceState.checksum != computeStateChecksum())
  {
    resetState();
  }

  g_deviceState.bootCount++;
  updateStateChecksum();
}

QueueStorage &persistentQueueStorage()
{
  return g_queueStorage;
}

} // namespace spth
