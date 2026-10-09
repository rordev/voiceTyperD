#include "postprocess/LlmEndpointSecurity.h"

#include <QHostAddress>
#include <QUrl>

namespace vt {

bool validatePrivateLlmEndpoint(const QUrl& url, QString* error) {
    const auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return false;
    };

    if (!url.isValid() || url.host().isEmpty() ||
        (url.scheme() != QLatin1String("http") &&
         url.scheme() != QLatin1String("https"))) {
        return fail(QStringLiteral("Enter a valid HTTP(S) endpoint URL."));
    }

    // Do not allow credentials in URLs, query tokens, or fragments. The API
    // token is sent separately via the Authorization header.
    if (!url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment())
        return fail(QStringLiteral("The endpoint URL must not contain credentials, query parameters, or fragments."));

    if (url.path() != QLatin1String("/v1/chat/completions"))
        return fail(QStringLiteral("Use the /v1/chat/completions endpoint."));

    // DNS names are intentionally not accepted: DNS configuration, proxies,
    // and rebindings can otherwise redirect potentially sensitive dictation
    // to an unexpected address. Tailscale IP addresses must be checked against
    // the machine's actual `tailscale ip -4` output before configuration.
    const QHostAddress host(url.host());
    if (host.isNull())
        return fail(QStringLiteral("Use a numeric localhost or Tailscale IP address, not a hostname."));

    const QHostAddress tailnetBase(QStringLiteral("100.64.0.0"));
    if (!host.isLoopback() && !host.isInSubnet(tailnetBase, 10))
        return fail(QStringLiteral("Only localhost and Tailscale IPv4 addresses (100.64.0.0/10) are allowed."));

    if (url.port() <= 0 || url.port() > 65535)
        return fail(QStringLiteral("Specify an explicit server port (for example, 1234)."));

    if (error)
        error->clear();
    return true;
}

} // namespace vt
