// Standalone correctness check for the chat-completions wire format used by
// LLM processing (postprocess/LlmChat). Not part of the app build; requires
// only Qt6Core. Build with:
//   g++ -std=c++17 -fPIC -Isrc tests/llm_chat_test.cpp src/postprocess/LlmChat.cpp
//       $(pkg-config --cflags --libs Qt6Core) -o llm_chat_test

#include "postprocess/LlmChat.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstdio>
#include <string>

using namespace vt;

static int g_failures = 0;

static void check(const std::string& name, const QString& got,
                  const QString& want) {
    const bool ok = got == want && got.isNull() == want.isNull();
    if (!ok)
        ++g_failures;
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name.c_str());
    if (!ok) {
        std::printf("   got : %s%s\n", got.toUtf8().constData(),
                    got.isNull() ? " (null)" : "");
        std::printf("   want: %s%s\n", want.toUtf8().constData(),
                    want.isNull() ? " (null)" : "");
    }
}

static void checkBool(const std::string& name, bool got, bool want) {
    const bool ok = got == want;
    if (!ok)
        ++g_failures;
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name.c_str());
}

static QJsonObject bodyOf(const LlmRequestConfig& cfg, const QString& text) {
    return QJsonDocument::fromJson(buildChatCompletionBody(cfg, text)).object();
}

static QString userMessage(const QJsonObject& body) {
    return body.value("messages").toArray().first().toObject().value("content").toString();
}

int main() {
    // --- Prompt template -------------------------------------------------
    check("placeholder_replaced",
          expandPromptTemplate("Fix: {{text}}. Only the text.", "hello world"),
          "Fix: hello world. Only the text.");
    check("every_placeholder_replaced",
          expandPromptTemplate("{{text}} / {{text}}", "a"), "a / a");
    check("no_placeholder_appends",
          expandPromptTemplate("Fix this text.", "hello"), "Fix this text.\n\nhello");
    check("empty_template_sends_text", expandPromptTemplate("  ", "hello"), "hello");
    check("placeholder_in_text_left_alone",
          expandPromptTemplate("Fix: {{text}}", "say {{text}}"), "Fix: say {{text}}");

    // --- Request body ----------------------------------------------------
    LlmRequestConfig cfg;
    cfg.model = "test-model";
    cfg.promptTemplate = "Fix: {{text}}";
    QJsonObject body = bodyOf(cfg, "привет мир");
    check("model_sent", body.value("model").toString(), cfg.model);
    check("one_user_message",
          body.value("messages").toArray().first().toObject().value("role").toString(),
          "user");
    checkBool("only_one_message", body.value("messages").toArray().size() == 1, true);
    check("user_message_expanded", userMessage(body), "Fix: привет мир");
    checkBool("stream_false", body.value("stream").toBool(true), false);

    cfg.model.clear();
    checkBool("empty_model_omitted", bodyOf(cfg, "x").contains("model"), false);

    cfg.extraParams = QJsonObject{{"temperature", 0.2}, {"max_tokens", 4096},
                                  {"stream", true}, {"model", "from-extra"},
                                  {"messages", QJsonArray()}};
    body = bodyOf(cfg, "x");
    checkBool("extra_param_merged", body.value("temperature").toDouble() == 0.2, true);
    checkBool("extra_int_merged", body.value("max_tokens").toInt() == 4096, true);
    checkBool("extra_cannot_enable_stream", body.value("stream").toBool(true), false);
    check("extra_model_used_when_model_empty", body.value("model").toString(),
          "from-extra");
    check("extra_cannot_replace_messages", userMessage(body), "Fix: x");
    cfg.model = "real";
    check("model_wins_over_extra", bodyOf(cfg, "x").value("model").toString(), "real");

    // --- Reply parsing ---------------------------------------------------
    QString err;
    check("string_content",
          parseChatCompletionReply(
              R"({"choices":[{"message":{"role":"assistant","content":"Hello, world."},"finish_reason":"stop"}]})",
              &err),
          "Hello, world.");
    check("parts_content",
          parseChatCompletionReply(
              R"({"choices":[{"message":{"content":[{"type":"text","text":"Hel"},{"type":"image_url"},{"type":"text","text":"lo"}]}}]})",
              &err),
          "Hello");
    check("reasoning_field_ignored",
          parseChatCompletionReply(
              R"({"choices":[{"message":{"content":"Answer","reasoning_content":"thinking..."}}]})",
              &err),
          "Answer");

    check("error_object_null",
          parseChatCompletionReply(R"({"error":{"message":"Invalid API key","code":401}})", &err),
          QString());
    check("error_object_message", err, "Invalid API key");
    check("not_json_null", parseChatCompletionReply("<html>502</html>", &err), QString());
    checkBool("not_json_error_set", err.contains("not a JSON"), true);
    check("no_choices_null", parseChatCompletionReply(R"({"choices":[]})", &err), QString());
    check("no_choices_error", err, "The reply has no choices.");
    check("empty_content_null",
          parseChatCompletionReply(R"({"choices":[{"message":{"content":"  "},"finish_reason":"stop"}]})", &err),
          QString());
    check("empty_content_error", err, "The model returned an empty answer.");
    parseChatCompletionReply(
        R"({"choices":[{"message":{"content":null,"reasoning_content":"..."},"finish_reason":"length"}]})",
        &err);
    checkBool("token_limit_explained", err.contains("max_tokens"), true);

    check("server_error_object", chatCompletionErrorMessage(R"({"error":{"message":"Model not found"}})"),
          "Model not found");
    check("server_error_string", chatCompletionErrorMessage(R"({"error":"Unauthorized"})"),
          "Unauthorized");
    check("server_detail", chatCompletionErrorMessage(R"({"detail":"Not Found"})"), "Not Found");
    check("server_no_message", chatCompletionErrorMessage("Bad Gateway"), QString());

    // --- Answer regex ----------------------------------------------------
    check("no_regex_whole_answer", stripAnswer("  Hello, world.\n", QString()),
          "Hello, world.");
    check("think_block_removed",
          stripAnswer("<think>\nThe user wants...\n</think>\n\nHello, world.",
                      R"(<think>[\s\S]*?</think>)"),
          "Hello, world.");
    check("every_match_removed",
          stripAnswer("Note: a\nHello\nNote: b", R"((?m)^Note:.*$\n?)"), "Hello");
    check("no_match_unchanged", stripAnswer("Hello", "<think>.*</think>"), "Hello");

    // --- Extra params ----------------------------------------------------
    QJsonObject extra{{"stale", 1}};
    checkBool("blank_params_ok", parseExtraParams("  ", &extra, &err), true);
    checkBool("blank_params_empty", extra.isEmpty(), true);
    checkBool("object_params_ok",
              parseExtraParams(R"({"temperature": 0.2})", &extra, &err), true);
    checkBool("object_params_value", extra.value("temperature").toDouble() == 0.2, true);
    checkBool("array_params_rejected", parseExtraParams("[1, 2]", &extra, &err), false);
    checkBool("broken_params_rejected", parseExtraParams("{temperature:", &extra, &err), false);

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
