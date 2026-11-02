#pragma once

#include "spth/power/IPowerController.h"

namespace spth
{

class DeepSleepController : public IPowerController
{
public:
  void deepSleep(uint32_t durationMs) override;
  bool wokeFromTimer() override;
  const char *wakeCauseName() override;
  void restart() override;
  const char *firmwareSlot() override;
};

} // namespace spth
