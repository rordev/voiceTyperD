#pragma once

#include "postprocess/LlmChat.h"

#include <QObject>
#include <QPointer>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace vt {

// Optional stage between voice commands and the paste: sends the dictated
// text to an OpenAI-compatible chat-completions endpoint and hands back the
// model's answer, cut down by the configured regex. Asynchronous, on the GUI
// thread, one request at a time.
class LlmPostProcessor : public QObject {
    Q_OBJECT
public:
    // Exactly one of `answer` / `error` is non-empty.
    using Callback =
        std::function<void(const QString& answer, const QString& error)>;

    explicit LlmPostProcessor(QObject* parent = nullptr);
    ~LlmPostProcessor() override;

    // Calls `done` once, later, from the event loop. A request still in flight
    // is cancelled first.
    void process(const LlmRequestConfig& cfg, const QString& text, Callback done);

    // Aborts the request in flight without calling its callback.
    void cancel();

private:
    QNetworkAccessManager* nam_ = nullptr;
    QPointer<QNetworkReply> reply_;
};

} // namespace vt
