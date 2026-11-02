#pragma once

#include <cstdint>

namespace spth
{

// Abstração do modo de baixo consumo: permite testar a aplicação sem dormir.
class IPowerController
{
public:
  virtual ~IPowerController() = default;

  // Dorme pelo tempo indicado; não retorna (o chip reinicia no próximo wake).
  virtual void deepSleep(uint32_t durationMs) = 0;

  virtual bool wokeFromTimer() = 0;

  // Motivo do boot atual (deep sleep, power-on, panic...), para diagnóstico.
  virtual const char *wakeCauseName() = 0;

  // Reinicia para ativar uma imagem OTA recém-gravada.
  virtual void restart() = 0;

  // Slot de firmware em execução ("ota_0"/"ota_1"), para confirmar o OTA.
  virtual const char *firmwareSlot() = 0;
};

} // namespace spth
