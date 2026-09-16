#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "kadoka_othello/ai.hpp"

namespace kadoka::othello {

struct ExternalAISessionConfig {
    std::string executable;
    std::vector<std::string> arguments;
    std::chrono::milliseconds response_timeout{5000};
};

// Long-lived external AI transport. One instance owns one child process and
// exchanges framed requests/responses through stdin/stdout.
class ExternalAISession {
public:
    explicit ExternalAISession(ExternalAISessionConfig config);
    ~ExternalAISession();

    ExternalAISession(ExternalAISession&&) noexcept;
    ExternalAISession& operator=(ExternalAISession&&) noexcept;

    ExternalAISession(const ExternalAISession&) = delete;
    ExternalAISession& operator=(const ExternalAISession&) = delete;

    [[nodiscard]] AIInspection inspect(const AdaptedAIInput& input);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::size_t next_request_id_{1};
};

}  // namespace kadoka::othello
