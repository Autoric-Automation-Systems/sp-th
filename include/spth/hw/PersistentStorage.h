#pragma once

#include <cstdint>

#include "spth/core/EventQueue.h"

namespace spth
{

// Estado que precisa sobreviver ao deep sleep (o chip reinicia a cada ciclo).
struct DeviceState
{
  uint32_t magic;
  uint32_t bootCount;
  uint32_t wifiFailures;
  int32_t lastBatteryPercent; // -1 = nunca medido
  int32_t lastRssi;           // 0 = nunca medido (0 não é RSSI válido)
  uint32_t checksum;
};

// Referência crua ao bloco de RTC (sem inicializar nada).
DeviceState &persistentDeviceState();

// Valida o bloco (checksum) e conta o boot.
void beginPersistentState();

// Fila de eventos em RTC: sobrevive ao deep sleep, a panic e a watchdog.
QueueStorage &persistentQueueStorage();

} // namespace spth
