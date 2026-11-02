#include "spth/app/Device.h"

#include <Arduino.h>
#include <cstdlib>

#include "spth/Config.h"
#include "spth/Logger.h"
#include "spth/core/Event.h"
#include "spth/core/FirmwareVersion.h"

namespace spth
{
namespace
{

constexpr const char *kTemperatureEvent = "temperature";
constexpr const char *kHumidityEvent = "humidity";
constexpr const char *kBatteryEvent = "battery";
constexpr const char *kWifiEvent = "wifi";
constexpr const char *kSensorErrorEvent = "aht_error";

Event makeEvent(const char *name, float value, uint32_t ts)
{
  Event event{};
  setEventName(event, name);
  event.value = roundEventValue(value);
  event.ts = ts;
  return event;
}

} // namespace

Device::Device(const Dependencies &dependencies) : dependencies_(dependencies)
{
}

void Device::begin()
{
  log::begin(115200);
  beginPersistentState();
  dependencies_.queue.restore();
  loadInterval();

  mac_ = dependencies_.network.macAddress();
  dependencies_.signal.seed(dependencies_.state.lastRssi);

  log::blank();
  log::info("Boot Smart Plant %s | %s | boot #%lu | wake: %s | slot: %s",
            config::kFirmwareVersion, config::kDeviceType,
            static_cast<unsigned long>(dependencies_.state.bootCount),
            dependencies_.power.wakeCauseName(),
            dependencies_.power.firmwareSlot());
  log::info("MAC: %s | intervalo: %lu ms | fila: %u evento(s) | falhas WiFi: %u",
            mac_, static_cast<unsigned long>(intervalMs_),
            static_cast<unsigned>(dependencies_.queue.size()),
            static_cast<unsigned>(dependencies_.state.wifiFailures));

  dependencies_.climate.begin();
}

void Device::runCycle()
{
  publishClimateEvents();
  const int batteryPercent = publishBatteryEvent();

  if (dependencies_.network.connect(config::kWifiConnectRetries))
  {
    dependencies_.state.wifiFailures = 0;
    dependencies_.clock.ensureSynced(config::kNtpTimeoutMs);
    publishSignalEvent();
    publishQueuedEvents();

    HeartbeatReply reply{};
    if (sendHeartbeat(reply) && applyFirmwareUpdate(reply, batteryPercent))
    {
      // A imagem já está validada na partição inativa: o reboot a ativa.
      dependencies_.power.restart();
      return;
    }

    dependencies_.network.disconnect();
  }
  else
  {
    if (dependencies_.state.wifiFailures < UINT8_MAX)
    {
      dependencies_.state.wifiFailures++;
    }
    log::info("Sem WiFi (%u ciclos seguidos), nada enviado",
              static_cast<unsigned>(dependencies_.state.wifiFailures));

    // Nunca bloqueia o aparelho no mesmo ciclo: o portal abre uma vez, com
    // timeout, e o ciclo seguinte usa as credenciais salvas.
    if (dependencies_.state.wifiFailures >= config::kPortalAfterWifiFailures)
    {
      dependencies_.network.openPortal(config::kPortalTimeoutSeconds);
      dependencies_.state.wifiFailures = 0;
    }
  }

  dependencies_.power.deepSleep(intervalMs_);
}

void Device::loadInterval()
{
  intervalMs_ = sanitizeInterval(dependencies_.settings.heartbeatInterval(), config::kSleepLimits);
}

void Device::publishClimateEvents()
{
  const ClimateReading reading = dependencies_.climate.read();
  const uint32_t now = dependencies_.clock.epochNow();

  if (!reading.valid)
  {
    log::info("Sem leitura do AHT (sensor ausente ou fora de faixa)");
    dependencies_.queue.push(makeEvent(kSensorErrorEvent, 1.0f, now));
    return;
  }

  log::info("Clima: %.2f C | %.2f %%", reading.temperatureC, reading.humidityPercent);
  dependencies_.queue.push(makeEvent(kTemperatureEvent, reading.temperatureC, now));
  dependencies_.queue.push(makeEvent(kHumidityEvent, reading.humidityPercent, now));
}

int Device::publishBatteryEvent()
{
  const BatteryReading reading = dependencies_.battery.read();
  log::info("Bateria: %.2f V (%d%%)", reading.volts, reading.percent);

  const int32_t previous = dependencies_.state.lastBatteryPercent;
  if (previous >= 0 && abs(reading.percent - static_cast<int>(previous)) < config::kBatteryReportDeltaPercent)
  {
    return reading.percent;
  }

  dependencies_.state.lastBatteryPercent = reading.percent;
  dependencies_.queue.push(
      makeEvent(kBatteryEvent, static_cast<float>(reading.percent), dependencies_.clock.epochNow()));
  return reading.percent;
}

void Device::publishSignalEvent()
{
  const int rssi = dependencies_.network.rssi();
  if (!SignalMonitor::isValid(rssi))
  {
    log::info("RSSI inválido: %d", rssi);
    return;
  }
  if (!dependencies_.signal.shouldReport(rssi))
  {
    return;
  }

  dependencies_.state.lastRssi = rssi;
  dependencies_.queue.push(makeEvent(kWifiEvent, static_cast<float>(rssi), dependencies_.clock.epochNow()));
}

void Device::publishQueuedEvents()
{
  if (dependencies_.queue.empty())
  {
    return;
  }

  const uint32_t now = dependencies_.clock.epochNow();
  if (now == 0)
  {
    log::info("Sem hora válida: eventos continuam na fila");
    return;
  }

  const size_t delivered = dependencies_.api.sendEvents(
      mac_, dependencies_.queue.data(), dependencies_.queue.size(),
      config::kEventsPerBatch, now);

  if (delivered > 0)
  {
    dependencies_.queue.dropFront(delivered);
  }
  if (!dependencies_.queue.empty())
  {
    log::info("Envio incompleto: %u evento(s) mantidos na fila",
              static_cast<unsigned>(dependencies_.queue.size()));
  }
}

bool Device::sendHeartbeat(HeartbeatReply &reply)
{
  HeartbeatStatus status{};
  status.mac = mac_;
  status.deviceType = config::kDeviceType;
  status.version = config::kFirmwareVersion;
  status.rssi = dependencies_.network.rssi();
  status.ip = dependencies_.network.localIp();
  status.uptimeMs = millis();
  status.boots = dependencies_.state.bootCount;
  status.freeHeap = ESP.getFreeHeap();

  const bool ok = dependencies_.api.sendHeartbeat(status, reply);
  applyIntervalHint(reply.intervalMs);

  log::info("Heap livre: %lu | fila: %u evento(s) | descartados: %lu",
            static_cast<unsigned long>(ESP.getFreeHeap()),
            static_cast<unsigned>(dependencies_.queue.size()),
            static_cast<unsigned long>(dependencies_.queue.dropped()));
  return ok;
}

bool Device::applyFirmwareUpdate(const HeartbeatReply &reply, int batteryPercent)
{
  const char *url = reply.firmwareUrl;
  bool skipVersionCheck = false;

  // Atalho de bancada: força o download sem depender da resposta do servidor.
  if (config::kForcedUpdateUrl[0] != '\0')
  {
    url = config::kForcedUpdateUrl;
    skipVersionCheck = true;
  }

  if (url == nullptr || url[0] == '\0')
  {
    return false;
  }

  if (batteryPercent >= 0 && batteryPercent < config::kMinBatteryPercentForUpdate)
  {
    log::info("OTA adiado: bateria em %d%% (mínimo %d%%)", batteryPercent,
              config::kMinBatteryPercentForUpdate);
    return false;
  }

  if (!skipVersionCheck && !isNewerVersion(reply.firmwareVersion, config::kFirmwareVersion))
  {
    log::info("OTA: versão %s não é mais nova que %s", reply.firmwareVersion,
              config::kFirmwareVersion);
    return false;
  }

  log::info("OTA: %s -> %s", config::kFirmwareVersion,
            skipVersionCheck ? "forçada (bancada)" : reply.firmwareVersion);
  return dependencies_.updater.install(url);
}

void Device::applyIntervalHint(uint32_t newIntervalMs)
{
  if (newIntervalMs == 0)
  {
    log::info("heartbeatInterval ausente na resposta");
    return;
  }
  if (!isIntervalAllowed(newIntervalMs, config::kSleepLimits))
  {
    log::info("heartbeatInterval fora da faixa, ignorado: %lu",
              static_cast<unsigned long>(newIntervalMs));
    return;
  }
  if (newIntervalMs == intervalMs_)
  {
    log::info("heartbeatInterval sem alteração");
    return;
  }

  dependencies_.settings.saveHeartbeatInterval(newIntervalMs);
  intervalMs_ = newIntervalMs;
  log::info("Novo intervalo salvo: %lu ms", static_cast<unsigned long>(intervalMs_));
}

} // namespace spth
