#include "kadoka_othello/script_evaluator.hpp"

#include "kadoka_othello/native_in_process_evaluator.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace kadoka::othello {
namespace {

std::string quote_arg(const std::string& value) {
    std::string result = "\"";
    for (char ch : value) {
        if (ch == '"')
            result += '\\';
        result += ch;
    }
    result += '"';
    return result;
}

std::vector<ScriptEvaluatorResult> read_process_results(const std::string& path,
                                                        const std::vector<ScriptEvaluatorCase>& cases) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("script evaluator did not create output");

    std::vector<ScriptEvaluatorResult> results;
    ScriptEvaluatorResult current;
    bool active = false;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        std::istringstream parser(line);
        std::string kind;
        if (!(parser >> kind))
            continue;
        if (kind == "result") {
            if (active || !(parser >> current.id)) {
                throw std::runtime_error("script evaluator returned malformed result header");
            }
            std::string extra;
            if (parser >> extra)
                throw std::runtime_error("script evaluator returned malformed result header");
            current.values.clear();
            current.diagnostics.clear();
            active = true;
        } else if (kind == "value" && active) {
            ScriptEvaluatorValue value;
            if (!(parser >> value.key >> value.value) || value.key.empty() || !std::isfinite(value.value)) {
                throw std::runtime_error("script evaluator returned malformed numeric value");
            }
            std::string extra;
            if (parser >> extra)
                throw std::runtime_error("script evaluator returned malformed numeric value");
            current.values.push_back(std::move(value));
        } else if (kind == "diag" && active) {
            std::string pair;
            std::string extra;
            if (!(parser >> pair) || (parser >> extra)) {
                throw std::runtime_error("script evaluator returned malformed diagnostic");
            }
            const std::size_t equals = pair.find('=');
            if (equals == std::string::npos || equals == 0) {
                throw std::runtime_error("script evaluator returned malformed diagnostic");
            }
            current.diagnostics.push_back({pair.substr(0, equals), pair.substr(equals + 1)});
        } else if (kind == "end" && active) {
            std::string extra;
            if (parser >> extra)
                throw std::runtime_error("script evaluator returned malformed end marker");
            results.push_back(std::move(current));
            current = ScriptEvaluatorResult{};
            active = false;
        } else {
            throw std::runtime_error("script evaluator returned invalid output at line " + std::to_string(line_number));
        }
    }
    if (active)
        throw std::runtime_error("script evaluator returned unterminated result");
    if (results.size() != cases.size()) {
        throw std::runtime_error("script evaluator returned unexpected result count");
    }

    std::vector<std::size_t> expected;
    std::vector<std::size_t> actual;
    expected.reserve(cases.size());
    actual.reserve(results.size());
    for (const auto& item : cases)
        expected.push_back(item.id);
    for (const auto& item : results)
        actual.push_back(item.id);
    std::sort(expected.begin(), expected.end());
    std::sort(actual.begin(), actual.end());
    if (expected != actual) {
        throw std::runtime_error("script evaluator returned mismatched result ids");
    }
    return results;
}

std::string read_all(const std::string& path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("failed to open script evaluator config: " + path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string find_string(const std::string& text, const std::string& key, const std::string& fallback = {}) {
    const std::string token = "\"" + key + "\"";
    const std::size_t key_pos = text.find(token);
    if (key_pos == std::string::npos)
        return fallback;
    const std::size_t colon = text.find(':', key_pos + token.size());
    if (colon == std::string::npos)
        return fallback;
    const std::size_t first = text.find('"', colon + 1);
    if (first == std::string::npos)
        return fallback;
    const std::size_t second = text.find('"', first + 1);
    if (second == std::string::npos)
        return fallback;
    return text.substr(first + 1, second - first - 1);
}

ScriptEvaluatorRuntime parse_runtime(const std::string& value) {
    if (value == "python_process")
        return ScriptEvaluatorRuntime::PythonProcess;
    if (value == "native_process")
        return ScriptEvaluatorRuntime::NativeProcess;
    if (value == "native_in_process")
        return ScriptEvaluatorRuntime::NativeInProcess;
    if (value == "wasm")
        return ScriptEvaluatorRuntime::Wasm;
    throw std::runtime_error("unsupported script evaluator runtime: " + value);
}

} // namespace

double ScriptEvaluatorResult::value_or(const std::string& key, double fallback) const noexcept {
    for (const auto& item : values) {
        if (item.key == key)
            return item.value;
    }
    return fallback;
}

const char* script_evaluator_runtime_name(ScriptEvaluatorRuntime runtime) noexcept {
    switch (runtime) {
    case ScriptEvaluatorRuntime::PythonProcess:
        return "python_process";
    case ScriptEvaluatorRuntime::NativeProcess:
        return "native_process";
    case ScriptEvaluatorRuntime::NativeInProcess:
        return "native_in_process";
    case ScriptEvaluatorRuntime::Wasm:
        return "wasm";
    }
    return "unknown";
}

ScriptEvaluator::ScriptEvaluator(ScriptEvaluatorConfig config) : config_(std::move(config)) {
    if (config_.entry_path.empty()) {
        throw std::invalid_argument("script evaluator requires entry_path");
    }
    if (config_.runtime == ScriptEvaluatorRuntime::NativeInProcess) {
        native_in_process_ = std::make_shared<NativeInProcessEvaluator>(config_.entry_path, config_.symbol);
    }
}

const ScriptEvaluatorConfig& ScriptEvaluator::config() const noexcept {
    return config_;
}

std::string ScriptEvaluator::build_command(const std::string& input_path, const std::string& output_path) const {
    namespace fs = std::filesystem;
    const fs::path entry = fs::absolute(config_.entry_path);

    std::string command;
    switch (config_.runtime) {
    case ScriptEvaluatorRuntime::PythonProcess:
        command = quote_arg(config_.executable) + " " + quote_arg(entry.string());
        break;
    case ScriptEvaluatorRuntime::NativeProcess:
        command = quote_arg(entry.string());
        break;
    case ScriptEvaluatorRuntime::NativeInProcess:
        throw std::logic_error("native_in_process evaluator does not build a process command");
    case ScriptEvaluatorRuntime::Wasm: {
        const fs::path input(input_path);
        const fs::path output(output_path);
        if (input.parent_path() != output.parent_path()) {
            throw std::logic_error("WASM evaluator files must share a directory");
        }
        const fs::path host_dir = fs::absolute(input.parent_path());
        const std::string guest_input = "/" + input.filename().string();
        const std::string guest_output = "/" + output.filename().string();
        command = quote_arg(config_.executable) + " run";
        if (!config_.wasm_cache_config.empty()) {
            command += " -C " + quote_arg("cache-config=" + fs::absolute(config_.wasm_cache_config).string());
        }
        command += " --dir " + quote_arg(host_dir.string() + "::/") + " " + quote_arg(entry.string());
        command += " --kadoka-eval-input " + quote_arg(guest_input);
        command += " --kadoka-eval-output " + quote_arg(guest_output);
        break;
    }
    }

    if (config_.runtime != ScriptEvaluatorRuntime::Wasm) {
        command += " --kadoka-eval-input " + quote_arg(input_path);
        command += " --kadoka-eval-output " + quote_arg(output_path);
    }
#ifdef _WIN32
    // std::system() dispatches through cmd.exe /c on Windows. When the
    // executable itself is quoted, cmd.exe needs one additional outer quote
    // pair so it does not consume the executable quote as command syntax.
    return "\"" + command + "\"";
#else
    return command;
#endif
}

std::vector<ScriptEvaluatorResult>
ScriptEvaluator::evaluate_batch(const std::vector<ScriptEvaluatorCase>& cases) const {
    if (cases.empty())
        return {};

    if (config_.runtime == ScriptEvaluatorRuntime::NativeInProcess) {
        if (!native_in_process_) {
            throw std::logic_error("native in-process evaluator was not initialized");
        }
        return native_in_process_->evaluate_batch(cases);
    }

    namespace fs = std::filesystem;
    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    static std::atomic<std::uint64_t> sequence{0};
    const fs::path temp_dir =
        fs::temp_directory_path() / ("kadoka_script_eval_" + std::to_string(stamp) + "_" +
                                     std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)));
    if (!fs::create_directory(temp_dir)) {
        throw std::runtime_error("failed to create script evaluator temporary directory");
    }
    const fs::path input_path = temp_dir / "input.txt";
    const fs::path output_path = temp_dir / "output.txt";
    const auto cleanup = [&]() {
        std::error_code ignored;
        fs::remove_all(temp_dir, ignored);
    };

    {
        std::ofstream output(input_path);
        if (!output) {
            cleanup();
            throw std::runtime_error("failed to create script evaluator input");
        }
        output << "KADOKA_SCRIPT_EVALUATOR 1\n";
        for (const auto& item : cases) {
            output << "case " << item.id << '\n';
            for (const auto& feature : item.features) {
                output << "feature " << feature.key << ' ' << feature.value << '\n';
            }
            output << "end\n";
        }
    }

    const std::string command = build_command(input_path.string(), output_path.string());
    const int exit_code = std::system(command.c_str());
    if (exit_code != 0) {
        cleanup();
        throw std::runtime_error(std::string("script evaluator process failed runtime=") +
                                 script_evaluator_runtime_name(config_.runtime) +
                                 " exit_code=" + std::to_string(exit_code));
    }

    try {
        auto results = read_process_results(output_path.string(), cases);
        cleanup();
        return results;
    } catch (...) {
        cleanup();
        throw;
    }
}

ScriptEvaluatorConfig load_script_evaluator_config(const std::string& path, const std::string& default_entry_path) {
    ScriptEvaluatorConfig config;
    config.entry_path = default_entry_path;
    if (path.empty())
        return config;

    const std::string text = read_all(path);
    config.runtime = parse_runtime(find_string(text, "runtime", "python_process"));
    config.executable = find_string(text, "executable",
                                    config.runtime == ScriptEvaluatorRuntime::Wasm ? "wasmtime" : config.executable);
    config.symbol = find_string(text, "symbol", config.symbol);

    std::string entry;
    if (config.runtime == ScriptEvaluatorRuntime::PythonProcess) {
        entry = find_string(text, "script", config.entry_path);
    } else if (config.runtime == ScriptEvaluatorRuntime::NativeProcess) {
        entry = find_string(text, "program", config.entry_path);
    } else if (config.runtime == ScriptEvaluatorRuntime::NativeInProcess) {
        entry = find_string(text, "library", config.entry_path);
    } else {
        entry = find_string(text, "module", config.entry_path);
    }
    config.entry_path = entry;

    if (!config.entry_path.empty() && std::filesystem::path(config.entry_path).is_relative()) {
        config.entry_path =
            (std::filesystem::absolute(std::filesystem::path(path)).parent_path() / config.entry_path).string();
    }
    if (config.runtime == ScriptEvaluatorRuntime::Wasm) {
        config.wasm_cache_config = find_string(text, "cache_config", "");
        if (!config.wasm_cache_config.empty() && std::filesystem::path(config.wasm_cache_config).is_relative()) {
            config.wasm_cache_config =
                (std::filesystem::absolute(std::filesystem::path(path)).parent_path() / config.wasm_cache_config)
                    .string();
        }
    }
    return config;
}

ScriptEvaluator load_script_evaluator_asset(const ModelRootDescriptor& model, const std::string& asset_id) {
    for (const auto& asset : model.assets) {
        if (asset.id != asset_id)
            continue;
        if (asset.type != "kadoka.script_evaluator.v1") {
            throw std::runtime_error("model asset is not kadoka.script_evaluator.v1: " + asset_id);
        }

        const std::string config_path = find_model_asset_path(model, asset_id);
        if (config_path.empty()) {
            throw std::runtime_error("script evaluator asset path is empty: " + asset_id);
        }
        return ScriptEvaluator(load_script_evaluator_config(config_path));
    }
    throw std::runtime_error("script evaluator asset not found: " + asset_id);
}

} // namespace kadoka::othello
