#pragma once

#include "spth/net/IFirmwareUpdater.h"

namespace spth
{

// OTA por HTTP(S) usando as duas partições app (ota_0/ota_1).
class HttpOtaUpdater : public IFirmwareUpdater
{
public:
  bool install(const char *url) override;
};

} // namespace spth
