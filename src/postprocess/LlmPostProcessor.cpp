#include "postprocess/LlmPostProcessor.h"

#include "core/Logging.h"

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace vt {

LlmPostProcessor::LlmPostProcessor(QObject* parent)
    : QObject(parent), nam_(new QNetworkAccessManager(this)) {}

LlmPostProcessor::~LlmPostProcessor() { cancel(); }

void LlmPostProcessor::cancel() {
    if (!reply_)
        return;
    QNetworkReply* reply = reply_;
    reply_.clear();
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
}

void LlmPostProcessor::process(const LlmRequestConfig& cfg, const QString& text,
                               Callback done) {
    cancel();

    const QUrl url(cfg.endpoint.trimmed(), QUrl::StrictMode);
    if (!url.isValid() ||
        (url.scheme() != QLatin1String("https") &&
         url.scheme() != QLatin1String("http"))) {
        const QString error =
            cfg.endpoint.trimmed().isEmpty()
                ? tr("No endpoint URL is set.")
                : tr("Invalid endpoint URL '%1'.").arg(cfg.endpoint.trimmed());
        QMetaObject::invokeMethod(
            this, [done, error]() { done(QString(), error); },
            Qt::QueuedConnection);
        return;
    }

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QByteArrayLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");
    const QString key = cfg.apiKey.trimmed();
    if (!key.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + key.toUtf8());
    // The answer arrives in one piece once generated, so this bounds the whole
    // wait, not just a stalled transfer.
    req.setTransferTimeout(cfg.timeoutMs);

    // Endpoint URLs may carry tokens in query parameters. Do not log them.
    qCInfo(vtLlm) << "Sending optional LLM request";

    QElapsedTimer elapsed;
    elapsed.start();
    QNetworkReply* reply = nam_->post(req, buildChatCompletionBody(cfg, text));
    reply_ = reply;

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, elapsed, done, stripPattern = cfg.stripPattern,
             timeoutMs = cfg.timeoutMs]() {
                reply_.clear();
                reply->deleteLater();

                const QNetworkReply::NetworkError netError = reply->error();
                const int status =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const double seconds = elapsed.elapsed() / 1000.0;

                QString error;
                QString answer;
                if (netError == QNetworkReply::TimeoutError ||
                    netError == QNetworkReply::OperationCanceledError) {
                    // The transfer timeout (cancel() disconnects before it
                    // aborts). The reply is closed by now: nothing to read.
                    error = tr("No answer within %1 s.").arg(timeoutMs / 1000);
                } else if (netError != QNetworkReply::NoError) {
                    QString detail = chatCompletionErrorMessage(reply->readAll());
                    if (detail.isEmpty())
                        detail = reply->attribute(
                                          QNetworkRequest::HttpReasonPhraseAttribute)
                                     .toString();
                    error = status != 0 && !detail.isEmpty()
                                ? tr("HTTP %1: %2").arg(status).arg(detail)
                                : reply->errorString();
                } else {
                    const QByteArray body = reply->readAll();
                    answer = parseChatCompletionReply(body, &error);
                    if (!answer.isNull()) {
                        answer = stripAnswer(answer, stripPattern);
                        if (answer.isEmpty())
                            error = tr("Nothing is left of the answer after "
                                       "the removal regex.");
                    }
                }

                if (error.isEmpty()) {
                    qCInfo(vtLlm) << "LLM response received in" << seconds << "s";
                    done(answer, QString());
                } else {
                    // Server error details may echo dictated text or credentials.
                    qCWarning(vtLlm) << "Request failed after" << seconds
                                     << "s, HTTP status" << status;
                    done(QString(), error);
                }
            });
}

} // namespace vt
