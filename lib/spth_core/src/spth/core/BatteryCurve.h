#pragma once

namespace spth
{
namespace battery
{

// Converte a tensão da bateria (V) em percentual usando a curva de descarga
// típica de Li-ion, com interpolação linear entre os pontos da tabela.
int percentFromVoltage(float volts);

} // namespace battery
} // namespace spth
