#include "spth/hw/ClimateSensor.h"

#include <Arduino.h>
#include <Wire.h>

#include "spth/Logger.h"

namespace spth
{
namespace
{

constexpr float kMinTemperatureC = -40.0f;
constexpr float kMaxTemperatureC = 85.0f;
constexpr float kMinHumidityPercent = 0.0f;
constexpr float kMaxHumidityPercent = 100.0f;

} // namespace

ClimateSensor::ClimateSensor(uint8_t sdaPin, uint8_t sclPin, uint32_t clockHz,
                             uint8_t attempts, uint16_t attemptDelayMs)
    : sdaPin_(sdaPin),
      sclPin_(sclPin),
      clockHz_(clockHz),
      attempts_(attempts == 0 ? 1 : attempts),
      attemptDelayMs_(attemptDelayMs)
{
}

void ClimateSensor::begin()
{
  Wire.begin(sdaPin_, sclPin_);
  Wire.setClock(clockHz_);

  Serial.print("Iniciando AHT");
  for (uint8_t attempt = 0; attempt < attempts_; attempt++)
  {
    if (sensor_.begin())
    {
      ready_ = true;
      break;
    }
    Serial.print(".");
    delay(attemptDelayMs_);
  }
  Serial.println();

  log::info(ready_ ? "AHT pronto" : "AHT não respondeu");
}

ClimateReading ClimateSensor::read()
{
  ClimateReading reading{false, 0.0f, 0.0f};
  if (!ready_)
  {
    return reading;
  }

  sensors_event_t humidity;
  sensors_event_t temperature;
  if (!sensor_.getEvent(&humidity, &temperature))
  {
    return reading;
  }

  if (temperature.temperature < kMinTemperatureC || temperature.temperature > kMaxTemperatureC ||
      humidity.relative_humidity < kMinHumidityPercent || humidity.relative_humidity > kMaxHumidityPercent)
  {
    return reading;
  }

  reading.valid = true;
  reading.temperatureC = temperature.temperature;
  reading.humidityPercent = humidity.relative_humidity;
  return reading;
}

} // namespace spth
