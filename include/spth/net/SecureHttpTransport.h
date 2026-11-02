#pragma once

#include "spth/net/ITransport.h"

namespace spth
{

// POST HTTPS com validação de certificado, timeouts e chave compartilhada.
class SecureHttpTransport : public ITransport
{
public:
  int post(const char *url, const char *body, char *response, size_t responseSize) override;
};

} // namespace spth
