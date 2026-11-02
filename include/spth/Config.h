#pragma once

#include <cstdint>

#include "spth/core/IntervalLimits.h"

namespace spth
{
namespace config
{

// A versão pode ser sobrescrita no build para ensaiar o OTA:
//   -DSPTH_FIRMWARE_VERSION='"1.3"'
#ifndef SPTH_FIRMWARE_VERSION
#define SPTH_FIRMWARE_VERSION "1.3"
#endif

constexpr const char *kFirmwareVersion = SPTH_FIRMWARE_VERSION;
constexpr const char *kDeviceType = "SP-TH";
constexpr const char *kPortalSsid = "Smart Plant APP - SP-TH";

// 1 = servidor local de desenvolvimento
#ifndef SPTH_LOCAL_SERVER
#define SPTH_LOCAL_SERVER 0
#endif

#if SPTH_LOCAL_SERVER
constexpr const char *kBaseUrl = "http://192.168.100.103:3000";
#else
constexpr const char *kBaseUrl = "https://smartplant.app.br";
#endif

// ================= Hardware =================
constexpr uint8_t kBatteryPin = 1;

// Placa medidora 0-25 VDC (divisor 30k/7.5k -> 5:1). Vbat = Vpin * 5.
constexpr float kBatteryDivider = 5.0f;
constexpr uint8_t kBatterySamples = 8;

constexpr uint8_t kI2cSdaPin = 8;
constexpr uint8_t kI2cSclPin = 9;
constexpr uint32_t kI2cClockHz = 100000;
constexpr uint8_t kClimateInitAttempts = 10;
constexpr uint16_t kClimateInitDelayMs = 300;

// ================= Fila de eventos =================
constexpr uint8_t kEventsPerBatch = 20;

// ================= Ciclo / deep sleep =================
// O servidor devolve o intervalo (normalmente 600000 ms = 10 min) e ele é
// gravado em NVS, sobrevivendo a reboot e regravação de firmware.
#ifndef DEFAULT_SLEEP_MS
#define DEFAULT_SLEEP_MS 10000UL
#endif
#ifndef MIN_SLEEP_MS
#define MIN_SLEEP_MS 5000UL
#endif
#ifndef MAX_SLEEP_MS
#define MAX_SLEEP_MS 3600000UL
#endif

constexpr uint32_t kDefaultSleepMs = DEFAULT_SLEEP_MS;
constexpr uint32_t kMinSleepMs = MIN_SLEEP_MS;
constexpr uint32_t kMaxSleepMs = MAX_SLEEP_MS;
constexpr IntervalLimits kSleepLimits{kMinSleepMs, kMaxSleepMs, kDefaultSleepMs};

// ================= Rede =================
constexpr uint16_t kWifiConnectRetries = 40; // 40 x 500 ms = 20 s
constexpr uint16_t kPortalTimeoutSeconds = 120;
constexpr uint16_t kNtpTimeoutMs = 5000;
constexpr int kRssiReportDeltaDbm = 10;
constexpr int kBatteryReportDeltaPercent = 5;

// Portal só depois de N ciclos seguidos sem WiFi: nunca bloqueia o aparelho
// dentro do mesmo boot (o firmware anterior ficava em loop de portal).
constexpr uint8_t kPortalAfterWifiFailures = 3;

// ================= HTTP / TLS =================
constexpr uint32_t kHttpConnectTimeoutMs = 10000;
constexpr uint32_t kHttpReadTimeoutMs = 15000;

// 1 = valida o certificado do servidor (include/spth/roots_ca.h)
#ifndef TLS_VERIFY
#define TLS_VERIFY 1
#endif

// Chave compartilhada do servidor (DEVICE_API_KEY). Vazia = header não enviado.
// Defina no build: -DDEVICE_API_KEY='"chave"' (nunca commite o segredo)
#ifndef DEVICE_API_KEY
#define DEVICE_API_KEY ""
#endif
constexpr const char *kDeviceApiKey = DEVICE_API_KEY;

// ================= OTA =================
// A atualização é aplicada na partição inativa e só com bateria suficiente:
// uma queda no meio do download não afeta o firmware em execução.
constexpr int kMinBatteryPercentForUpdate = 20;
constexpr uint32_t kOtaConnectTimeoutMs = 15000;
constexpr uint32_t kOtaReadTimeoutMs = 30000;

// Somente bancada: força a atualização a partir deste URL no próximo ciclo,
// ignorando o que o servidor informa. Vazio = comportamento normal.
#ifndef SPTH_OTA_FORCE_URL
#define SPTH_OTA_FORCE_URL ""
#endif
constexpr const char *kForcedUpdateUrl = SPTH_OTA_FORCE_URL;

} // namespace config
} // namespace spth
