#pragma once

#include <cstdint>

#include "spth/app/SettingsStore.h"
#include "spth/core/EventQueue.h"
#include "spth/hw/BatteryGauge.h"
#include "spth/hw/ClimateSensor.h"
#include "spth/hw/PersistentStorage.h"
#include "spth/core/SignalMonitor.h"
#include "spth/net/ApiClient.h"
#include "spth/net/IClock.h"
#include "spth/net/IFirmwareUpdater.h"
#include "spth/net/INetworkLink.h"
#include "spth/power/IPowerController.h"

namespace spth
{

// Ciclo único de um dispositivo que dorme: acorda, mede, envia e volta a dormir.
// Conhece apenas abstrações (IClock, INetworkLink, ApiClient, IPowerController).
class Device
{
public:
  struct Dependencies
  {
    IClock &clock;
    INetworkLink &network;
    ApiClient &api;
    EventQueue &queue;
    ClimateSensor &climate;
    BatteryGauge &battery;
    SignalMonitor &signal;
    SettingsStore &settings;
    IPowerController &power;
    IFirmwareUpdater &updater;
    DeviceState &state;
  };

  explicit Device(const Dependencies &dependencies);

  // Loga o boot e restaura o estado persistido (uma vez por wake).
  void begin();

  // Executa o ciclo completo e entra em deep sleep (não retorna).
  void runCycle();

private:
  void loadInterval();
  void publishClimateEvents();
  int publishBatteryEvent();
  void publishSignalEvent();
  void publishQueuedEvents();
  bool sendHeartbeat(HeartbeatReply &reply);
  void applyIntervalHint(uint32_t newIntervalMs);
  bool applyFirmwareUpdate(const HeartbeatReply &reply, int batteryPercent);

  Dependencies dependencies_;
  const char *mac_ = "";
  uint32_t intervalMs_ = 0;
};

} // namespace spth
