#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace vt {

// Wire format of an OpenAI-compatible chat-completions call, kept free of
// networking so it can be tested on its own (tests/llm_chat_test.cpp).
//
// The request is a single user message: the prompt template with the dictated
// text in place of {{text}}. The answer is choices[0].message.content.

// Placeholder in the prompt template that the dictated text replaces.
inline constexpr auto kLlmTextPlaceholder = "{{text}}";

struct LlmRequestConfig {
    QString endpoint;        // full URL, e.g. https://host/v1/chat/completions
    QString apiKey;          // optional: "Authorization: Bearer <key>"
    QString model;           // optional: left out of the request when empty
    QString promptTemplate;  // user message; {{text}} = the dictated text
    QJsonObject extraParams; // merged into the request body (temperature, ...)
    QString stripPattern;    // optional regex; every match is cut from the answer
    int timeoutMs = 30000;
};

// The user message for `text`: every {{text}} in `tmpl` replaced by it. A
// template without the placeholder gets the text appended after a blank line;
// an empty template sends the text alone.
QString expandPromptTemplate(const QString& tmpl, const QString& text);

// JSON body of the chat-completions request. `extraParams` go in first, so
// they cannot override "messages", "stream" (always false) or a set model.
QByteArray buildChatCompletionBody(const LlmRequestConfig& cfg,
                                   const QString& text);

// The assistant's answer from a successful reply, or a null QString with
// *error set (not JSON, an "error" object, no choices, an empty answer).
QString parseChatCompletionReply(const QByteArray& body, QString* error);

// The server's own error message from an error reply ({"error": {"message":
// ...}} and the common variants), or empty when there is none.
QString chatCompletionErrorMessage(const QByteArray& body);

// Removes every match of `pattern` from `answer` and trims the result. An
// empty pattern returns the trimmed answer unchanged.
QString stripAnswer(const QString& answer, const QString& pattern);

// Parses the "extra request fields" setting. Blank => an empty object. Returns
// false and sets *error unless it is a JSON object.
bool parseExtraParams(const QString& json, QJsonObject* out, QString* error);

} // namespace vt
