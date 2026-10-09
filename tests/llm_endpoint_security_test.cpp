// Lightweight Qt-only security policy test: no microphone or Whisper model.
// Configure with -DBUILD_TESTING=ON and run: ctest --test-dir build --output-on-failure.
#include "postprocess/LlmEndpointSecurity.h"

#include <QUrl>

#include <cstdio>

int main() {
    struct Case { const char* name; const char* endpoint; bool expected; };
    const Case cases[] = {
        {"localhost_ipv4", "http://127.0.0.1:1234/v1/chat/completions", true},
        {"localhost_ipv6", "http://[::1]:1234/v1/chat/completions", true},
        {"tailnet_lower", "http://100.64.0.1:1234/v1/chat/completions", true},
        {"tailnet_upper", "http://100.127.255.254:1234/v1/chat/completions", true},
        {"tailnet_https", "https://100.101.12.2:1234/v1/chat/completions", true},
        {"public_ipv4", "https://8.8.8.8:1234/v1/chat/completions", false},
        {"lan_ipv4", "http://192.168.1.22:1234/v1/chat/completions", false},
        {"carrier_outside_range", "http://100.128.0.1:1234/v1/chat/completions", false},
        {"hostname", "http://localhost:1234/v1/chat/completions", false},
        {"magic_dns", "http://workstation.example.ts.net:1234/v1/chat/completions", false},
        {"embedded_credentials", "http://username:secret@100.101.12.2:1234/v1/chat/completions", false},
        {"query_string", "http://100.101.12.2:1234/v1/chat/completions?api_key=abc", false},
        {"fragment", "http://100.101.12.2:1234/v1/chat/completions#secret", false},
        {"no_port", "http://100.101.12.2/v1/chat/completions", false},
        {"unexpected_path", "http://100.101.12.2:1234/api/v1/chat", false},
        {"invalid_scheme", "file:///v1/chat/completions", false},
    };
    int failures = 0;
    for (const auto& test : cases) {
        QString error;
        const bool accepted = vt::validatePrivateLlmEndpoint(
            QUrl(QString::fromLatin1(test.endpoint), QUrl::StrictMode), &error);
        if (accepted != test.expected) {
            std::fprintf(stderr, "FAIL %s: %s\n", test.name, error.toUtf8().constData());
            ++failures;
        } else {
            std::printf("PASS %s\n", test.name);
        }
    }
    return failures == 0 ? 0 : 1;
}
