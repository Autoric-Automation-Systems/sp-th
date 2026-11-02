#include <Arduino.h>

#include "spth/Config.h"
#include "spth/app/Device.h"
#include "spth/app/SettingsStore.h"
#include "spth/core/EventQueue.h"
#include "spth/hw/BatteryGauge.h"
#include "spth/hw/ClimateSensor.h"
#include "spth/hw/PersistentStorage.h"
#include "spth/core/SignalMonitor.h"
#include "spth/net/ApiClient.h"
#include "spth/net/HttpOtaUpdater.h"
#include "spth/net/SecureHttpTransport.h"
#include "spth/net/SystemClock.h"
#include "spth/net/WifiLink.h"
#include "spth/power/DeepSleepController.h"

namespace
{

using namespace spth;

// Composition root: o único lugar que conhece todas as implementações.
SystemClock g_clock("pool.ntp.org", -3 * 3600, 0);
WifiLink g_network(config::kPortalSsid);
SecureHttpTransport g_transport;
ApiClient g_api(g_transport, config::kBaseUrl);
EventQueue g_queue(persistentQueueStorage());
ClimateSensor g_climate(config::kI2cSdaPin, config::kI2cSclPin, config::kI2cClockHz,
                        config::kClimateInitAttempts, config::kClimateInitDelayMs);
BatteryGauge g_battery(config::kBatteryPin, config::kBatteryDivider, config::kBatterySamples);
SignalMonitor g_signal(config::kRssiReportDeltaDbm);
SettingsStore g_settings(config::kSleepLimits, "spth");
DeepSleepController g_power;
HttpOtaUpdater g_updater;

Device g_device(Device::Dependencies{g_clock, g_network, g_api, g_queue, g_climate,
                                     g_battery, g_signal, g_settings, g_power,
                                     g_updater, persistentDeviceState()});

} // namespace

void setup()
{
  g_device.begin();
  g_device.runCycle();
}

void loop()
{
}
