#pragma once

#include <QString>

class QUrl;

namespace vt {

// Defense in depth for private LM Studio use. Deliberately accepts only
// numeric loopback and Tailscale (100.64.0.0/10) IP addresses; MagicDNS
// hostnames and ordinary LAN addresses are not accepted.
bool validatePrivateLlmEndpoint(const QUrl& url, QString* error = nullptr);

} // namespace vt
