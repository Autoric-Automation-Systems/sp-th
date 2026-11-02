#pragma once

#include <Adafruit_AHTX0.h>
#include <cstdint>

namespace spth
{

struct ClimateReading
{
  bool valid;
  float temperatureC;
  float humidityPercent;
};

// AHT10/AHT20 por I2C, com inicialização tolerante a falha: o ciclo segue
// sem leitura quando o sensor não responde (em vez de enviar valor inválido).
class ClimateSensor
{
public:
  ClimateSensor(uint8_t sdaPin, uint8_t sclPin, uint32_t clockHz,
                uint8_t attempts, uint16_t attemptDelayMs);

  void begin();
  bool ready() const { return ready_; }
  ClimateReading read();

private:
  Adafruit_AHTX0 sensor_;
  uint8_t sdaPin_;
  uint8_t sclPin_;
  uint32_t clockHz_;
  uint8_t attempts_;
  uint16_t attemptDelayMs_;
  bool ready_ = false;
};

} // namespace spth
