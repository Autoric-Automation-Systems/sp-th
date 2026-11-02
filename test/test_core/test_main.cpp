#include <unity.h>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>

#include "spth/core/BatteryCurve.h"
#include "spth/core/EventQueue.h"
#include "spth/core/FirmwareVersion.h"
#include "spth/core/IntervalLimits.h"
#include "spth/core/SignalMonitor.h"

using namespace spth;

namespace
{

Event makeEvent(const char *name, float value, uint32_t ts)
{
  Event event{};
  setEventName(event, name);
  event.value = value;
  event.ts = ts;
  return event;
}

QueueStorage g_storage;

} // namespace

void test_event_name_is_truncated()
{
  Event event{};
  setEventName(event, "humidity_percent");
  TEST_ASSERT_EQUAL_STRING("humidity_pe", event.name);
}

void test_event_queue_keeps_order()
{
  QueueStorage storage{};
  EventQueue queue(storage);
  queue.restore();

  queue.push(makeEvent("temperature", 23.5f, 100));
  queue.push(makeEvent("humidity", 61.2f, 200));

  TEST_ASSERT_EQUAL_UINT(2, queue.size());
  TEST_ASSERT_EQUAL_STRING("temperature", queue.at(0).name);
  TEST_ASSERT_EQUAL_FLOAT(61.2f, queue.at(1).value);
}

void test_event_queue_survives_deep_sleep_reboot()
{
  QueueStorage storage{};
  EventQueue first(storage);
  first.restore();
  first.push(makeEvent("temperature", 21.0f, 100));
  first.push(makeEvent("humidity", 50.0f, 100));

  // Outro boot: o mesmo bloco de RTC é revalidado, sem perder o conteudo
  EventQueue second(storage);
  second.restore();

  TEST_ASSERT_EQUAL_UINT(2, second.size());
  TEST_ASSERT_EQUAL_STRING("temperature", second.at(0).name);
}

void test_event_queue_drops_oldest_when_full()
{
  EventQueue queue(g_storage);
  queue.restore();

  for (uint16_t i = 0; i < QueueStorage::kCapacity; i++)
  {
    queue.push(makeEvent("temperature", static_cast<float>(i), 0));
  }

  const bool kept = queue.push(makeEvent("ultimo", 99.0f, 0));

  TEST_ASSERT_FALSE(kept);
  TEST_ASSERT_EQUAL_UINT(QueueStorage::kCapacity, queue.size());
  TEST_ASSERT_EQUAL_FLOAT(1.0f, queue.at(0).value);
  TEST_ASSERT_EQUAL_STRING("ultimo", queue.at(queue.size() - 1).name);
}

void test_event_queue_restore_rejects_unknown_memory()
{
  QueueStorage storage{};
  storage.count = 7;
  storage.magic = 0xDEADBEEF;

  EventQueue queue(storage);
  queue.restore();

  TEST_ASSERT_EQUAL_UINT(0, queue.size());
  TEST_ASSERT_EQUAL_UINT32(QueueStorage::kValidMagic, storage.magic);
}

void test_battery_curve_percentages()
{
  TEST_ASSERT_EQUAL_INT(100, battery::percentFromVoltage(4.30f));
  TEST_ASSERT_EQUAL_INT(60, battery::percentFromVoltage(3.87f));
  TEST_ASSERT_EQUAL_INT(0, battery::percentFromVoltage(3.00f));
  TEST_ASSERT_INT_WITHIN(1, 53, battery::percentFromVoltage(3.835f));
}

void test_sleep_limits_sanitize()
{
  const IntervalLimits limits{5000, 3600000, 10000};

  TEST_ASSERT_TRUE(isIntervalAllowed(600000, limits));
  TEST_ASSERT_FALSE(isIntervalAllowed(0, limits));

  // Intervalo zero nunca pode virar deep sleep imediato (loop de WiFi)
  TEST_ASSERT_EQUAL_UINT32(10000, sanitizeInterval(0, limits));
  TEST_ASSERT_EQUAL_UINT32(10000, sanitizeInterval(99999999UL, limits));
  TEST_ASSERT_EQUAL_UINT32(600000, sanitizeInterval(600000, limits));
}

void test_signal_monitor_uses_hysteresis()
{
  SignalMonitor monitor(10);

  // Primeira leitura depois do boot sempre reporta
  TEST_ASSERT_TRUE(monitor.shouldReport(-60));
  TEST_ASSERT_FALSE(monitor.shouldReport(-65));

  // Wake seguinte: semeado com o valor persistido, sem repetir o evento
  SignalMonitor nextBoot(10);
  nextBoot.seed(-60);
  TEST_ASSERT_FALSE(nextBoot.shouldReport(-64));
  TEST_ASSERT_TRUE(nextBoot.shouldReport(-45));

  // Semente inválida é ignorada (0 = nunca medido)
  SignalMonitor empty(10);
  empty.seed(0);
  TEST_ASSERT_TRUE(empty.shouldReport(-70));
}

void test_event_value_is_rounded_to_two_decimals()
{
  TEST_ASSERT_EQUAL_FLOAT(29.64f, roundEventValue(29.63562012f));
  TEST_ASSERT_EQUAL_FLOAT(50.79f, roundEventValue(50.78725815f));
  TEST_ASSERT_EQUAL_FLOAT(-37.0f, roundEventValue(-37.0f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, roundEventValue(1.0f));
}

void test_event_queue_rejects_garbage_with_valid_magic()
{
  QueueStorage storage{};
  {
    EventQueue first(storage);
    first.restore();
    first.push(makeEvent("temperature", 20.0f, 100));
  }

  // Lixo deixado por outro firmware (flash/teste): magic e contagem plausíveis
  storage.slots[0].name[0] = '\x01';
  storage.slots[0].value = 0.0f;
  storage.slots[0].ts = 0;

  EventQueue restored(storage);
  restored.restore();

  TEST_ASSERT_EQUAL_UINT(0, restored.size());
}

void test_payload_has_at_most_two_decimals()
{
  StaticJsonDocument<96> document;
  char text[16];
  char buffer[48];

  formatEventValue(29.63562012f, text, sizeof(text));
  document["value"] = serialized(text);
  serializeJson(document, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING_MESSAGE("{\"value\":29.64}", buffer, buffer);

  formatEventValue(50.78725815f, text, sizeof(text));
  document["value"] = serialized(text);
  serializeJson(document, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING_MESSAGE("{\"value\":50.79}", buffer, buffer);

  // Inteiros continuam inteiros no JSON (sem .00)
  formatEventValue(-37.0f, text, sizeof(text));
  document["value"] = serialized(text);
  serializeJson(document, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING_MESSAGE("{\"value\":-37}", buffer, buffer);

  formatEventValue(100.0f, text, sizeof(text));
  document["value"] = serialized(text);
  serializeJson(document, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING_MESSAGE("{\"value\":100}", buffer, buffer);

  // Valor não finito nunca vira JSON inválido
  formatEventValue(NAN, text, sizeof(text));
  document["value"] = serialized(text);
  serializeJson(document, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING_MESSAGE("{\"value\":0}", buffer, buffer);
}

void test_firmware_version_parsing()
{
  const FirmwareVersion full = parseFirmwareVersion("1.2.3");
  TEST_ASSERT_EQUAL_UINT16(1, full.major);
  TEST_ASSERT_EQUAL_UINT16(2, full.minor);
  TEST_ASSERT_EQUAL_UINT16(3, full.patch);

  const FirmwareVersion shortForm = parseFirmwareVersion("2.10");
  TEST_ASSERT_EQUAL_UINT16(2, shortForm.major);
  TEST_ASSERT_EQUAL_UINT16(10, shortForm.minor);
  TEST_ASSERT_EQUAL_UINT16(0, shortForm.patch);

  // Prefixo de texto e sufixo não numérico não quebram o parser
  const FirmwareVersion prefixed = parseFirmwareVersion("v1.4-beta");
  TEST_ASSERT_EQUAL_UINT16(1, prefixed.major);
  TEST_ASSERT_EQUAL_UINT16(4, prefixed.minor);

  const FirmwareVersion invalid = parseFirmwareVersion("");
  TEST_ASSERT_EQUAL_UINT16(0, invalid.major);
}

void test_firmware_version_ordering()
{
  // Comparação numérica: texto puro diria que "1.10" < "1.9"
  TEST_ASSERT_TRUE(isNewerVersion("1.10", "1.9"));
  TEST_ASSERT_FALSE(isNewerVersion("1.9", "1.10"));

  TEST_ASSERT_TRUE(isNewerVersion("1.2", "1.1"));
  TEST_ASSERT_FALSE(isNewerVersion("1.1", "1.2"));

  TEST_ASSERT_TRUE(isNewerVersion("2.0", "1.99"));
  TEST_ASSERT_TRUE(isNewerVersion("1.2.1", "1.2"));
  TEST_ASSERT_FALSE(isNewerVersion("1.2.0", "1.2"));

  // Mesma versão ou ausência de versão nunca dispara atualização
  TEST_ASSERT_FALSE(isNewerVersion("1.2", "1.2"));
  TEST_ASSERT_FALSE(isNewerVersion("", "1.2"));
  TEST_ASSERT_FALSE(isNewerVersion(nullptr, "1.2"));
}

void setup()
{
  // Espera o monitor serial anexar depois do reset do gravador.
  delay(5000);
  UNITY_BEGIN();
  RUN_TEST(test_event_name_is_truncated);
  RUN_TEST(test_event_value_is_rounded_to_two_decimals);
  RUN_TEST(test_payload_has_at_most_two_decimals);
  RUN_TEST(test_event_queue_keeps_order);
  RUN_TEST(test_event_queue_survives_deep_sleep_reboot);
  RUN_TEST(test_event_queue_drops_oldest_when_full);
  RUN_TEST(test_event_queue_restore_rejects_unknown_memory);
  RUN_TEST(test_battery_curve_percentages);
  RUN_TEST(test_sleep_limits_sanitize);
  RUN_TEST(test_signal_monitor_uses_hysteresis);
  RUN_TEST(test_firmware_version_parsing);
  RUN_TEST(test_firmware_version_ordering);
  UNITY_END();
}

void loop()
{
}
