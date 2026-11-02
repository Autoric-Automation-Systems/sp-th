#include "spth/power/DeepSleepController.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_sleep.h>

#include "spth/Logger.h"

namespace spth
{

void DeepSleepController::deepSleep(uint32_t durationMs)
{
  log::info("Entrando em deep sleep por %lu ms", static_cast<unsigned long>(durationMs));
  Serial.flush();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(durationMs) * 1000ULL);
  esp_deep_sleep_start();
}

bool DeepSleepController::wokeFromTimer()
{
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
}

void DeepSleepController::restart()
{
  log::info("Reiniciando para ativar o firmware novo");
  Serial.flush();
  ESP.restart();
}

const char *DeepSleepController::firmwareSlot()
{
  const esp_partition_t *running = esp_ota_get_running_partition();
  return running != nullptr ? running->label : "?";
}

const char *DeepSleepController::wakeCauseName()
{
  switch (esp_sleep_get_wakeup_cause())
  {
  case ESP_SLEEP_WAKEUP_UNDEFINED:
    return "POWERON";
  case ESP_SLEEP_WAKEUP_TIMER:
    return "TIMER";
  case ESP_SLEEP_WAKEUP_GPIO:
    return "GPIO";
  case ESP_SLEEP_WAKEUP_UART:
    return "UART";
  default:
    return "OUTRO";
  }
}

} // namespace spth
