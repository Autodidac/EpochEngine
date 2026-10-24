// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#include "ai.backend_controller_task_intent.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <sstream>
#include <system_error>

namespace epoch_ai::controller {
namespace {
[[nodiscard]] std::string lowercase(std::string_view input) {
    std::string result(input);
    std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}
[[nodiscard]] bool contains_any(std::string_view input, std::initializer_list<std::string_view> markers) {
    for (const auto marker : markers) if (input.find(marker) != std::string_view::npos) return true;
    return false;
}
[[nodiscard]] bool safe_simple(std::string_view text) {
    if (text.size() > 180 || text.find('\n') != std::string_view::npos) return false;
    // A greeting without a trailing space is still a complete utterance.
    // Match the entire trimmed phrase, not substrings such as "hi" in "this".
    auto greeting = text;
    while (!greeting.empty() && std::isspace(static_cast<unsigned char>(greeting.front()))) greeting.remove_prefix(1);
    while (!greeting.empty() && std::isspace(static_cast<unsigned char>(greeting.back()))) greeting.remove_suffix(1);
    while (!greeting.empty() && (greeting.back() == '!' || greeting.back() == '.' || greeting.back() == '?')) greeting.remove_suffix(1);
    if (greeting == "hi" || greeting == "hey" || greeting == "hello" || greeting == "yo" ||
        greeting == "good morning" || greeting == "good evening") return true;
    return contains_any(text, {
        "hello", "hey", "hi ", "thanks", "thank you", "what is ", "what's ",
        "define ", "who is ", "when was ", "how old ", "explain briefly",
        "short answer", "summarize in one", "what does ", "rewrite this sentence",
        "correct grammar", "draft a short", "write a short greeting"
    });
}
} // namespace

std::string_view to_string(TaskIntent intent) noexcept {
    switch (intent) {
    case TaskIntent::simple: return "simple";
    case TaskIntent::complex: return "complex";
    case TaskIntent::coding: return "coding";
    }
    return "complex";
}

TaskIntent classify_task(std::string_view text) {
    const auto lower = lowercase(text);
    if (contains_any(lower, {"c++", "cpp23", "cpp20", "cmake", "compile", "compiler", "debug", "source code",
        "codebase", "repository", "refactor", "implement", "patch", "fix the", "stack trace",
        "segfault", "shader", "vulkan", "code review", "write code", "typescript", "javascript",
        "python script", "unit test", "build error", "api endpoint", "git "})) return TaskIntent::coding;
    // A small model is not a reliable final authority for calculations, logic,
    // quantitative claims, health, legal, or other consequential answers.
    if (contains_any(lower, {"square root", "sqrt", "calculate", "compute", "equation",
        "algebra", "calculus", "derivative", "integral", "probability", "statistics",
        "percentage", "percent", "factorial", "multiply", "divide", "division",
        "fraction", "arithmetic", "geometry", "trigonometry", "matrix multiplication",
        "mathematics", "math", "times ", "plus ", "minus ", "squared", "cubed",
        "power of", "average speed", "round trip", "proof", "prove", "logic puzzle", "reasoning puzzle",
        "how many", "how much does", "tax", "medical", "diagnos", "legal advice",
        "finance", "financial", "mortgage", "interest rate", "security", "sandbox",
        "research", "investigate", "compare", "analyze", "analyse", "detailed",
        "architecture", "step by step", "plan a", "design a", "multiple",
        "use tools", "find files", "run tests", "create a file"})) return TaskIntent::complex;
    // Route symbolic arithmetic even without keywords, including percentage
    // and subtraction expressions that otherwise resemble simple questions.
    bool has_digit = false;
    bool has_operator = false;
    for (const unsigned char c : lower) {
        has_digit |= std::isdigit(c) != 0;
        has_operator |= c == '+' || c == '-' || c == '*' || c == '/' ||
            c == '^' || c == '=' || c == '%';
    }
    if (has_digit && has_operator) return TaskIntent::complex;
    if (safe_simple(lower)) return TaskIntent::simple;
    return TaskIntent::complex;
}

bool requires_foundation_reasoning(std::string_view text) {
    const auto lower = lowercase(text);
    if (contains_any(lower, {
        "math", "square root", "sqrt", "equation", "algebra", "calculus", "fraction",
        "calculate", "compute", "arithmetic", "numeric", "probability", "statistics",
        "percentage", "percent", "modulo", "\\mod", "integer", "exact value", "exact fraction",
        "round trip", "average speed", "mph", "proof", "theorem", "millions of",
        "medical", "diagnos", "legal", "finance", "financial", "tax", "mortgage",
        "investment", "insurance", "interest rate", "privacy", "malware",
        "cybersecurity", "security audit", "sandbox escape"})) return true;
    // C++23 is a language name, not the arithmetic expression C + +23.
    // Normalize this token before examining numeric operators; expressions
    // elsewhere in the request must still take the quantitative route.
    std::string arithmetic_text = lower;
    for (std::size_t pos = 0; (pos = arithmetic_text.find("c++", pos)) != std::string::npos;)
        arithmetic_text.replace(pos, 3, "cpp");
    // Treat TeX arithmetic and mathematical operators paired with numerals as
    // quantitative even when a user supplies only symbolic notation.
    const bool has_digit = std::ranges::any_of(arithmetic_text, [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
    if (!has_digit) return false;
    if (contains_any(lower, {"\\(", "\\[", "\\frac", "\\equiv", "\\pmod", "\\leftarrow"})) return true;
    return std::ranges::any_of(arithmetic_text, [](unsigned char c) {
        return c == '=' || c == '+' || c == '*' || c == '/' || c == '^' || c == '%';
    });
}

bool warrants_specialist_handoff(std::string_view text) {
    if (requires_foundation_reasoning(text)) return false;
    const auto lower = lowercase(text);
    // Explicitly prioritize direct answers for short coding examples. A
    // language name alone is not a reason to spend 2-3 extra model calls.
    if (contains_any(lower, {"basic example", "simple example", "hello world",
        "basic cpp23", "basic c++23", "show me a basic", "small snippet",
        "without writing files", "don't write any files"})) return false;
    // Meaningful multi-stage work: review, integration, architecture, complex
    // debugging, multi-file changes or hard performance/concurrency problems.
    if (contains_any(lower, {"refactor", "redesign", "architecture", "architectural",
        "multithread", "concurren", "thread safety", "thread-safe", "race condition",
        "deadlock", "optimi", "performance profil", "bottleneck", "integrat",
        "across files", "multiple files", "entire source", "entire project",
        "codebase", "repository", "dependency graph", "investigate the crash",
        "root cause", "complex bug", "security review", "migration",
        "build and test", "implement and test", "plan and implement"})) return true;
    return lower.size() > 420 &&
        contains_any(lower, {"implement", "debug", "design", "review", "fix", "plan"});
}

std::string_view to_string(ResponseTier tier) noexcept {
    switch (tier) {
    case ResponseTier::small: return "small";
    case ResponseTier::middle: return "middle";
    case ResponseTier::foundation: return "foundation";
    }
    return "foundation";
}

ResponseTier route_response(std::string_view text, bool small_ready,
    bool middle_ready, bool auto_route) {
    if (!auto_route || requires_foundation_reasoning(text)) return ResponseTier::foundation;
    const auto intent = classify_task(text);
    if (intent == TaskIntent::coding) return ResponseTier::foundation;
    // Only the deterministic, explicitly simple policy can send a final
    // response to the smallest model, and it must already be resident.
    if (intent == TaskIntent::simple)
        return small_ready ? ResponseTier::small : ResponseTier::foundation;
    if (!middle_ready || text.size() > 1200 || warrants_specialist_handoff(text))
        return ResponseTier::foundation;
    const auto lower = lowercase(text);
    // Moderate requests must be bounded, informational and tool-free. Words
    // indicating external lookup, files, agency or consequential work escalate.
    if (contains_any(lower, {"search", "look up", "lookup", "browse", "latest", "today",
        "current ", "real-time", "check online", "check the", "read ", "open ",
        "file", "folder", "repository", "repo", "source", "build", "execute",
        "run ", "install", "download", "create", "generate a file", "edit ",
        "change ", "modify ", "delete", "fix ", "implement", "patch", "test ",
        "diagnos", "medical", "health", "medicine", "legal", "security",
        "privacy", "finance", "financial", "investment", "trading", "tax ",
        "politic", "election", "calculate", "compute", "math", "logic",
        "analyze", "analyse", "compare", "research", "investigate", "verify",
        "exact ", "prove", "proof", "data", "statistics", "code", "script", "design "}))
        return ResponseTier::foundation;
    // Plain-language explanations, outlines, brainstorming and reformulations
    // can go to an already resident middle model, without tool permissions.
    constexpr std::string_view starters[]{
        "explain ", "describe ", "outline ", "summarize ", "give me an overview",
        "give an overview", "give me a summary", "what are ", "how does ",
        "how do ", "why does ", "why do ", "rewrite ", "rephrase ",
        "brainstorm ", "suggest ", "list ", "tell me about ", "give me ideas",
        "can you explain ", "could you explain "};
    const auto first = lower.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return ResponseTier::foundation;
    const auto trimmed = std::string_view{lower}.substr(first);
    return std::ranges::any_of(starters, [&](std::string_view prefix) {
        return trimmed.starts_with(prefix);
    }) ? ResponseTier::middle : ResponseTier::foundation;
}

PredictiveRouteShadow::Features PredictiveRouteShadow::features(std::string_view text) {
    const auto lower = lowercase(text);
    return {{1.0, std::min(1.0, static_cast<double>(text.size()) / 512.0),
        text.find('\n') != std::string_view::npos ? 1.0 : 0.0,
        contains_any(lower, {"c++", "cpp23", "cpp20", "cmake", "compile", "code", "vulkan", "git "}) ? 1.0 : 0.0,
        contains_any(lower, {"fix", "implement", "debug", "refactor", "patch"}) ? 1.0 : 0.0,
        contains_any(lower, {"research", "compare", "analyze", "plan", "security"}) ? 1.0 : 0.0,
        contains_any(lower, {"what is", "define ", "who is", "what does"}) ? 1.0 : 0.0,
        text.find('?') != std::string_view::npos ? 1.0 : 0.0,
        std::min(1.0, static_cast<double>(std::ranges::count(text, ' ')) / 80.0)}};
}

PredictiveRouteShadow::PredictiveRouteShadow(std::filesystem::path data_root) {
    if (data_root.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(data_root / "controller", ec);
    if (ec) return;
    journal_ = data_root / "controller" / "route-shadow-v1.samples";
    std::ifstream input(journal_);
    std::string line;
    while (examples_ < 200000 && std::getline(input, line)) {
        std::istringstream stream(line);
        int label = -1;
        Features values{};
        if (!(stream >> label) || label < 0 || label > 2) continue;
        bool valid = true;
        for (auto& feature : values) if (!(stream >> feature) || !std::isfinite(feature)) { valid = false; break; }
        if (valid) train(values, static_cast<TaskIntent>(label));
    }
}

void PredictiveRouteShadow::train(const Features& values, TaskIntent label) {
    // Tiny multiclass online linear predictor with bounded updates; training
    // target is the deterministic routing policy, not user satisfaction.
    const auto index = static_cast<std::size_t>(label);
    std::array<double, 3> logits{};
    for (std::size_t cls = 0; cls < 3; ++cls)
        for (std::size_t i = 0; i < values.size(); ++i) logits[cls] += weights_[cls][i] * values[i];
    const auto max_logit = *std::max_element(logits.begin(), logits.end());
    double sum = 0.0;
    for (auto& v : logits) { v = std::exp(std::clamp(v - max_logit, -50.0, 0.0)); sum += v; }
    constexpr double step = 0.025;
    for (std::size_t cls = 0; cls < 3; ++cls) {
        const auto error = (cls == index ? 1.0 : 0.0) - logits[cls] / sum;
        for (std::size_t i = 0; i < values.size(); ++i)
            weights_[cls][i] = std::clamp(weights_[cls][i] + step * error * values[i], -12.0, 12.0);
    }
    ++examples_;
}

void PredictiveRouteShadow::observe(std::string_view text, TaskIntent label) {
    const auto values = features(text);
    std::scoped_lock lock(mutex_);
    train(values, label);
    if (journal_.empty() || examples_ > 200000) return;
    std::ofstream output(journal_, std::ios::app);
    if (!output) return;
    output << static_cast<int>(label);
    for (const auto value : values) output << ' ' << value;
    output << '\n';
}

std::pair<TaskIntent, double> PredictiveRouteShadow::predict(std::string_view text) const {
    const auto values = features(text);
    std::scoped_lock lock(mutex_);
    std::array<double, 3> scores{};
    for (std::size_t cls = 0; cls < 3; ++cls)
        for (std::size_t i = 0; i < values.size(); ++i) scores[cls] += weights_[cls][i] * values[i];
    const auto best = static_cast<std::size_t>(std::distance(scores.begin(), std::max_element(scores.begin(), scores.end())));
    const auto maximum = scores[best];
    double denominator = 0;
    for (const auto score : scores) denominator += std::exp(std::clamp(score - maximum, -50.0, 0.0));
    return {static_cast<TaskIntent>(best), denominator > 0 ? 1.0 / denominator : 0.0};
}

std::size_t PredictiveRouteShadow::examples_seen() const noexcept {
    std::scoped_lock lock(mutex_);
    return examples_;
}
} // namespace epoch_ai::controller
