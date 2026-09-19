#include "kadoka_othello/external_ai_session.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <ctime>
#include <poll.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace kadoka::othello {
namespace {

std::string build_request(std::size_t request_id, const AIInput& input) {
    if (input.board == nullptr) {
        throw std::invalid_argument("external AI session requires board input");
    }

    std::ostringstream out;
    out << "request " << request_id << '\n';
    out << "size " << input.board->size() << '\n';
    for (std::size_t row = 0; row < input.board->size(); ++row) {
        out << "row ";
        for (std::size_t col = 0; col < input.board->size(); ++col) {
            const Cell cell = input.board->at({row, col});
            out << (cell == Cell::Black ? 'B' : cell == Cell::White ? 'W' : '.');
        }
        out << '\n';
    }

    const std::size_t legal_count = input.legal_moves == nullptr ? 0 : input.legal_moves->size();
    out << "legal_count " << legal_count << '\n';
    if (input.legal_moves != nullptr) {
        for (const Position move : *input.legal_moves) {
            out << "legal " << move.row << ' ' << move.col << '\n';
        }
    }
    out << "end\n";
    return out.str();
}

AIInspection parse_response(
    std::size_t expected_request_id,
    const std::vector<std::string>& lines) {
    if (lines.empty()) {
        throw std::runtime_error("external AI returned an empty response");
    }

    {
        std::istringstream first(lines.front());
        std::string kind;
        std::size_t request_id = 0;
        first >> kind >> request_id;
        if (kind != "result" || request_id != expected_request_id) {
            throw std::runtime_error("external AI response has invalid request id");
        }
    }

    AIInspection inspection;
    bool has_move = false;
    for (std::size_t index = 1; index < lines.size(); ++index) {
        std::istringstream parser(lines[index]);
        std::string kind;
        parser >> kind;
        if (kind.empty()) continue;

        if (kind == "move") {
            if (!(parser >> inspection.output.move.row >> inspection.output.move.col)) {
                throw std::runtime_error("external AI response contains malformed move");
            }
            has_move = true;
        } else if (kind == "diag") {
            std::string pair;
            parser >> pair;
            const std::size_t equals = pair.find('=');
            if (equals == std::string::npos) {
                throw std::runtime_error("external AI response contains malformed diagnostic");
            }
            inspection.diagnostics.push_back({pair.substr(0, equals), pair.substr(equals + 1)});
        } else if (kind == "candidate") {
            AICandidate candidate;
            if (!(parser >> candidate.move.row >> candidate.move.col >> candidate.value >> candidate.policy)) {
                throw std::runtime_error("external AI response contains malformed candidate");
            }
            inspection.candidates.push_back(candidate);
        } else {
            throw std::runtime_error("external AI response contains unknown record: " + kind);
        }
    }

    if (!has_move) {
        throw std::runtime_error("external AI response does not contain move");
    }
    return inspection;
}

#ifdef _WIN32

std::string quote_windows_arg(const std::string& value) {
    if (value.find_first_of(" \t\"") == std::string::npos) return value;
    std::string result = "\"";
    std::size_t backslashes = 0;
    for (const char ch : value) {
        if (ch == '\\') {
            ++backslashes;
            continue;
        }
        if (ch == '"') {
            result.append(backslashes * 2 + 1, '\\');
            result.push_back('"');
            backslashes = 0;
            continue;
        }
        result.append(backslashes, '\\');
        backslashes = 0;
        result.push_back(ch);
    }
    result.append(backslashes * 2, '\\');
    result.push_back('"');
    return result;
}

#else

ssize_t write_without_sigpipe(int fd, const void* data, std::size_t size) {
    sigset_t blocked{};
    sigset_t old_mask{};
    sigset_t pending{};
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGPIPE);

    const int mask_result = pthread_sigmask(SIG_BLOCK, &blocked, &old_mask);
    bool sigpipe_was_pending = false;
    if (mask_result == 0 && sigpending(&pending) == 0) {
        sigpipe_was_pending = sigismember(&pending, SIGPIPE) == 1;
    }

    const ssize_t result = ::write(fd, data, size);
    const int saved_errno = errno;

    if (result < 0 && saved_errno == EPIPE && mask_result == 0 && !sigpipe_was_pending) {
        timespec no_wait{};
        while (::sigtimedwait(&blocked, nullptr, &no_wait) < 0 && errno == EINTR) {
        }
    }

    if (mask_result == 0) {
        (void)pthread_sigmask(SIG_SETMASK, &old_mask, nullptr);
    }
    errno = saved_errno;
    return result;
}

#endif

}  // namespace

class ExternalAISession::Impl {
public:
    explicit Impl(ExternalAISessionConfig config)
        : config_(std::move(config)) {
        if (config_.executable.empty()) {
            throw std::invalid_argument("external AI executable must not be empty");
        }
        start();
    }

    ~Impl() {
        stop();
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    void write_request(const std::string& request) {
        write_all(request);
    }

    std::vector<std::string> read_response(std::size_t request_id) {
        std::vector<std::string> lines;
        const std::string expected = "result " + std::to_string(request_id);
        const std::string first = read_line(config_.response_timeout);
        if (first != expected) {
            throw std::runtime_error("external AI response does not begin with expected result frame");
        }
        lines.push_back(first);

        while (true) {
            const std::string line = read_line(config_.response_timeout);
            if (line == "end") break;
            lines.push_back(line);
        }
        return lines;
    }

private:
    void start() {
#ifdef _WIN32
        SECURITY_ATTRIBUTES security{};
        security.nLength = sizeof(security);
        security.bInheritHandle = TRUE;

        HANDLE child_stdin_read = nullptr;
        HANDLE child_stdout_write = nullptr;
        if (!CreatePipe(&child_stdin_read, &stdin_write_, &security, 0)) {
            throw std::runtime_error("failed to create external AI stdin pipe");
        }
        if (!SetHandleInformation(stdin_write_, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(child_stdin_read);
            CloseHandle(stdin_write_);
            throw std::runtime_error("failed to configure external AI stdin pipe");
        }
        if (!CreatePipe(&stdout_read_, &child_stdout_write, &security, 0)) {
            CloseHandle(child_stdin_read);
            CloseHandle(stdin_write_);
            throw std::runtime_error("failed to create external AI stdout pipe");
        }
        if (!SetHandleInformation(stdout_read_, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(child_stdin_read);
            CloseHandle(stdin_write_);
            CloseHandle(stdout_read_);
            CloseHandle(child_stdout_write);
            throw std::runtime_error("failed to configure external AI stdout pipe");
        }

        std::string command = quote_windows_arg(config_.executable);
        for (const std::string& argument : config_.arguments) {
            command += ' ';
            command += quote_windows_arg(argument);
        }
        std::vector<char> command_buffer(command.begin(), command.end());
        command_buffer.push_back('\0');

        STARTUPINFOA startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = child_stdin_read;
        startup.hStdOutput = child_stdout_write;
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION process_info{};
        const BOOL created = CreateProcessA(
            nullptr,
            command_buffer.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup,
            &process_info);

        CloseHandle(child_stdin_read);
        CloseHandle(child_stdout_write);
        if (!created) {
            CloseHandle(stdin_write_);
            CloseHandle(stdout_read_);
            stdin_write_ = nullptr;
            stdout_read_ = nullptr;
            throw std::runtime_error("failed to start external AI process");
        }

        process_ = process_info.hProcess;
        CloseHandle(process_info.hThread);
#else
        int child_stdin[2]{};
        int child_stdout[2]{};
        if (::pipe(child_stdin) != 0) {
            throw std::runtime_error("failed to create external AI stdin pipe");
        }
        if (::pipe(child_stdout) != 0) {
            ::close(child_stdin[0]);
            ::close(child_stdin[1]);
            throw std::runtime_error("failed to create external AI stdout pipe");
        }

        const pid_t child = ::fork();
        if (child < 0) {
            ::close(child_stdin[0]);
            ::close(child_stdin[1]);
            ::close(child_stdout[0]);
            ::close(child_stdout[1]);
            throw std::runtime_error("failed to fork external AI process");
        }

        if (child == 0) {
            ::dup2(child_stdin[0], STDIN_FILENO);
            ::dup2(child_stdout[1], STDOUT_FILENO);
            ::close(child_stdin[0]);
            ::close(child_stdin[1]);
            ::close(child_stdout[0]);
            ::close(child_stdout[1]);

            std::vector<std::string> storage;
            storage.reserve(config_.arguments.size() + 1);
            storage.push_back(config_.executable);
            storage.insert(storage.end(), config_.arguments.begin(), config_.arguments.end());
            std::vector<char*> argv;
            argv.reserve(storage.size() + 1);
            for (std::string& item : storage) argv.push_back(item.data());
            argv.push_back(nullptr);
            ::execvp(config_.executable.c_str(), argv.data());
            ::_exit(127);
        }

        pid_ = child;
        stdin_fd_ = child_stdin[1];
        stdout_fd_ = child_stdout[0];
        ::close(child_stdin[0]);
        ::close(child_stdout[1]);
#endif
    }

    void stop() noexcept {
#ifdef _WIN32
        if (stdin_write_ != nullptr) {
            CloseHandle(stdin_write_);
            stdin_write_ = nullptr;
        }
        if (process_ != nullptr) {
            if (WaitForSingleObject(process_, 100) == WAIT_TIMEOUT) {
                TerminateProcess(process_, 1);
                WaitForSingleObject(process_, 1000);
            }
            CloseHandle(process_);
            process_ = nullptr;
        }
        if (stdout_read_ != nullptr) {
            CloseHandle(stdout_read_);
            stdout_read_ = nullptr;
        }
#else
        if (stdin_fd_ >= 0) {
            ::close(stdin_fd_);
            stdin_fd_ = -1;
        }
        if (pid_ > 0) {
            int status = 0;
            for (int attempt = 0; attempt < 10; ++attempt) {
                const pid_t result = ::waitpid(pid_, &status, WNOHANG);
                if (result == pid_ || (result < 0 && errno == ECHILD)) {
                    pid_ = -1;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (pid_ > 0) {
                ::kill(pid_, SIGTERM);
                (void)::waitpid(pid_, &status, 0);
                pid_ = -1;
            }
        }
        if (stdout_fd_ >= 0) {
            ::close(stdout_fd_);
            stdout_fd_ = -1;
        }
#endif
    }

    void write_all(const std::string& data) {
#ifdef _WIN32
        std::size_t offset = 0;
        while (offset < data.size()) {
            DWORD written = 0;
            const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(data.size() - offset, 1U << 20));
            if (!WriteFile(stdin_write_, data.data() + offset, chunk, &written, nullptr) || written == 0) {
                throw std::runtime_error("failed to write external AI request");
            }
            offset += written;
        }
#else
        std::size_t offset = 0;
        while (offset < data.size()) {
            const ssize_t written = write_without_sigpipe(stdin_fd_, data.data() + offset, data.size() - offset);
            if (written < 0) {
                if (errno == EINTR) continue;
                if (errno == EPIPE) throw std::runtime_error("external AI process closed stdin");
                throw std::runtime_error("failed to write external AI request");
            }
            if (written == 0) throw std::runtime_error("external AI stdin closed");
            offset += static_cast<std::size_t>(written);
        }
#endif
    }

    std::string read_line(std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (true) {
            const std::size_t newline = read_buffer_.find('\n');
            if (newline != std::string::npos) {
                std::string line = read_buffer_.substr(0, newline);
                read_buffer_.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                return line;
            }

            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                throw std::runtime_error("external AI response timed out");
            }

#ifdef _WIN32
            DWORD available = 0;
            if (!PeekNamedPipe(stdout_read_, nullptr, 0, nullptr, &available, nullptr)) {
                throw std::runtime_error("external AI stdout pipe closed");
            }
            if (available == 0) {
                if (WaitForSingleObject(process_, 0) == WAIT_OBJECT_0) {
                    throw std::runtime_error("external AI process exited before response completed");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            char buffer[4096];
            const DWORD wanted = std::min<DWORD>(available, static_cast<DWORD>(sizeof(buffer)));
            DWORD read = 0;
            if (!ReadFile(stdout_read_, buffer, wanted, &read, nullptr) || read == 0) {
                throw std::runtime_error("failed to read external AI response");
            }
            read_buffer_.append(buffer, buffer + read);
#else
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            pollfd descriptor{};
            descriptor.fd = stdout_fd_;
            descriptor.events = POLLIN | POLLHUP;
            const int wait_ms = static_cast<int>(std::max<std::int64_t>(1, remaining.count()));
            const int result = ::poll(&descriptor, 1, wait_ms);
            if (result < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error("failed while waiting for external AI response");
            }
            if (result == 0) continue;

            char buffer[4096];
            const ssize_t read = ::read(stdout_fd_, buffer, sizeof(buffer));
            if (read < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error("failed to read external AI response");
            }
            if (read == 0) {
                throw std::runtime_error("external AI process exited before response completed");
            }
            read_buffer_.append(buffer, static_cast<std::size_t>(read));
#endif
        }
    }

    ExternalAISessionConfig config_;
    std::string read_buffer_;
#ifdef _WIN32
    HANDLE process_{nullptr};
    HANDLE stdin_write_{nullptr};
    HANDLE stdout_read_{nullptr};
#else
    pid_t pid_{-1};
    int stdin_fd_{-1};
    int stdout_fd_{-1};
#endif
};

ExternalAISession::ExternalAISession(ExternalAISessionConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

ExternalAISession::~ExternalAISession() = default;
ExternalAISession::ExternalAISession(ExternalAISession&&) noexcept = default;
ExternalAISession& ExternalAISession::operator=(ExternalAISession&&) noexcept = default;

AIInspection ExternalAISession::inspect(const AIInput& input) {
    const std::size_t request_id = next_request_id_++;
    impl_->write_request(build_request(request_id, input));
    return parse_response(request_id, impl_->read_response(request_id));
}

}  // namespace kadoka::othello
