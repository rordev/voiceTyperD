#include "postprocess/LlmChat.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>

namespace vt {

namespace {

// message.content is usually a string; some servers send an array of parts
// ([{"type": "text", "text": ...}, ...]) instead.
QString contentText(const QJsonValue& content) {
    if (content.isString())
        return content.toString();
    QString text;
    for (const QJsonValue& part : content.toArray()) {
        const QJsonObject p = part.toObject();
        if (p.value(QStringLiteral("type")).toString() == QLatin1String("text"))
            text += p.value(QStringLiteral("text")).toString();
    }
    return text;
}

QString errorText(const QJsonValue& error) {
    if (error.isString())
        return error.toString();
    const QJsonObject obj = error.toObject();
    const QString message = obj.value(QStringLiteral("message")).toString();
    if (!message.isEmpty())
        return message;
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

} // namespace

QString expandPromptTemplate(const QString& tmpl, const QString& text) {
    if (tmpl.trimmed().isEmpty())
        return text;
    const QString placeholder = QLatin1String(kLlmTextPlaceholder);
    if (!tmpl.contains(placeholder))
        return tmpl + QStringLiteral("\n\n") + text;
    QString out = tmpl;
    out.replace(placeholder, text);
    return out;
}

QByteArray buildChatCompletionBody(const LlmRequestConfig& cfg,
                                   const QString& text) {
    QJsonObject body = cfg.extraParams;
    if (!cfg.model.trimmed().isEmpty())
        body.insert(QStringLiteral("model"), cfg.model.trimmed());

    const QJsonObject message{
        {QStringLiteral("role"), QStringLiteral("user")},
        {QStringLiteral("content"), expandPromptTemplate(cfg.promptTemplate, text)},
    };
    body.insert(QStringLiteral("messages"), QJsonArray{message});
    // The reply is read in one piece; a streamed (SSE) one would not parse.
    body.insert(QStringLiteral("stream"), false);
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QString parseChatCompletionReply(const QByteArray& body, QString* error) {
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = QStringLiteral("The reply is not a JSON object: %1")
                         .arg(QString::fromUtf8(body.left(200)));
        return QString();
    }

    const QJsonObject root = doc.object();
    if (root.contains(QStringLiteral("error"))) {
        if (error)
            *error = errorText(root.value(QStringLiteral("error")));
        return QString();
    }

    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        if (error)
            *error = QStringLiteral("The reply has no choices.");
        return QString();
    }

    const QJsonObject choice = choices.first().toObject();
    const QString answer = contentText(
        choice.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")));
    if (answer.trimmed().isEmpty()) {
        if (error) {
            // Reasoning models spend the token budget on reasoning_content
            // first; running out leaves content empty.
            *error = choice.value(QStringLiteral("finish_reason")).toString() ==
                             QLatin1String("length")
                         ? QStringLiteral("The model ran out of tokens before "
                                          "answering (raise max_tokens).")
                         : QStringLiteral("The model returned an empty answer.");
        }
        return QString();
    }
    return answer;
}

QString chatCompletionErrorMessage(const QByteArray& body) {
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject())
        return QString();
    const QJsonObject root = doc.object();
    for (const char* key : {"error", "message", "detail"}) {
        const QJsonValue v = root.value(QLatin1String(key));
        if (!v.isUndefined() && !v.isNull())
            return errorText(v);
    }
    return QString();
}

QString stripAnswer(const QString& answer, const QString& pattern) {
    QString out = answer;
    if (!pattern.isEmpty())
        out.remove(QRegularExpression(pattern));
    return out.trimmed();
}

bool parseExtraParams(const QString& json, QJsonObject* out, QString* error) {
    if (json.trimmed().isEmpty()) {
        *out = QJsonObject();
        return true;
    }
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError) {
        if (error)
            *error = QStringLiteral("JSON parse error at offset %1: %2")
                         .arg(perr.offset)
                         .arg(perr.errorString());
        return false;
    }
    if (!doc.isObject()) {
        if (error)
            *error = QStringLiteral("Must be a JSON object, e.g. "
                                    "{\"temperature\": 0.2}.");
        return false;
    }
    *out = doc.object();
    return true;
}

} // namespace vt
