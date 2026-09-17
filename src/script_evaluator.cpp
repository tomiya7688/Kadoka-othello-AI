#include "kadoka_othello/script_evaluator.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "kadoka_othello/native_in_process_evaluator.hpp"

namespace kadoka::othello {
namespace {

std::string quote_arg(const std::string& value) {
    std::string result = "\"";
    for (char ch : value) {
        if (ch == '"') result += '\\';
        result += ch;
    }
    result += '"';
    return result;
}

std::string read_all(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("failed to open script evaluator config: " + path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string find_string(const std::string& text, const std::string& key, const std::string& fallback = {}) {
    const std::string token = "\"" + key + "\"";
    const std::size_t key_pos = text.find(token);
    if (key_pos == std::string::npos) return fallback;
    const std::size_t colon = text.find(':', key_pos + token.size());
    if (colon == std::string::npos) return fallback;
    const std::size_t first = text.find('"', colon + 1);
    if (first == std::string::npos) return fallback;
    const std::size_t second = text.find('"', first + 1);
    if (second == std::string::npos) return fallback;
    return text.substr(first + 1, second - first - 1);
}

ScriptEvaluatorRuntime parse_runtime(const std::string& value) {
    if (value == "python_process") return ScriptEvaluatorRuntime::PythonProcess;
    if (value == "native_process") return ScriptEvaluatorRuntime::NativeProcess;
    if (value == "native_in_process") return ScriptEvaluatorRuntime::NativeInProcess;
    if (value == "wasm") return ScriptEvaluatorRuntime::Wasm;
    throw std::runtime_error("unsupported script evaluator runtime: " + value);
}

}  // namespace

double ScriptEvaluatorResult::value_or(
    const std::string& key,
    double fallback) const noexcept {
    for (const auto& item : values) {
        if (item.key == key) return item.value;
    }
    return fallback;
}

const char* script_evaluator_runtime_name(
    ScriptEvaluatorRuntime runtime) noexcept {
    switch (runtime) {
        case ScriptEvaluatorRuntime::PythonProcess: return "python_process";
        case ScriptEvaluatorRuntime::NativeProcess: return "native_process";
        case ScriptEvaluatorRuntime::NativeInProcess: return "native_in_process";
        case ScriptEvaluatorRuntime::Wasm: return "wasm";
    }
    return "unknown";
}

ScriptEvaluator::ScriptEvaluator(ScriptEvaluatorConfig config)
    : config_(std::move(config)) {
    if (config_.entry_path.empty()) {
        throw std::invalid_argument("script evaluator requires entry_path");
    }
    if (config_.runtime == ScriptEvaluatorRuntime::Wasm) {
        throw std::runtime_error("WASM evaluator runtime is reserved but not implemented yet");
    }
    if (config_.runtime == ScriptEvaluatorRuntime::NativeInProcess) {
        native_in_process_ = std::make_shared<NativeInProcessEvaluator>(
            config_.entry_path,
            config_.symbol);
    }
}

const ScriptEvaluatorConfig& ScriptEvaluator::config() const noexcept {
    return config_;
}

std::string ScriptEvaluator::build_command(
    const std::string& input_path,
    const std::string& output_path) const {
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
        case ScriptEvaluatorRuntime::Wasm:
            throw std::runtime_error("WASM evaluator runtime is reserved but not implemented yet");
    }

    command += " --kadoka-eval-input " + quote_arg(input_path);
    command += " --kadoka-eval-output " + quote_arg(output_path);
#ifdef _WIN32
    // std::system() dispatches through cmd.exe /c on Windows. When the
    // executable itself is quoted, cmd.exe needs one additional outer quote
    // pair so it does not consume the executable quote as command syntax.
    return "\"" + command + "\"";
#else
    return command;
#endif
}

std::vector<ScriptEvaluatorResult> ScriptEvaluator::evaluate_batch(
    const std::vector<ScriptEvaluatorCase>& cases) const {
    if (cases.empty()) return {};

    if (config_.runtime == ScriptEvaluatorRuntime::NativeInProcess) {
        if (!native_in_process_) {
            throw std::logic_error("native in-process evaluator was not initialized");
        }
        return native_in_process_->evaluate_batch(cases);
    }

    namespace fs = std::filesystem;
    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const fs::path temp_dir = fs::temp_directory_path();
    const fs::path input_path = temp_dir / ("kadoka_script_eval_in_" + std::to_string(stamp) + ".txt");
    const fs::path output_path = temp_dir / ("kadoka_script_eval_out_" + std::to_string(stamp) + ".txt");

    {
        std::ofstream output(input_path);
        if (!output) throw std::runtime_error("failed to create script evaluator input");
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
        fs::remove(input_path);
        fs::remove(output_path);
        throw std::runtime_error(
            std::string("script evaluator process failed runtime=") +
            script_evaluator_runtime_name(config_.runtime) +
            " exit_code=" + std::to_string(exit_code));
    }

    std::ifstream input(output_path);
    if (!input) {
        fs::remove(input_path);
        throw std::runtime_error("script evaluator did not create output");
    }

    std::vector<ScriptEvaluatorResult> results;
    ScriptEvaluatorResult current;
    bool active = false;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream parser(line);
        std::string kind;
        parser >> kind;
        if (kind == "result") {
            if (active) results.push_back(std::move(current));
            current = ScriptEvaluatorResult{};
            parser >> current.id;
            active = true;
        } else if (kind == "value" && active) {
            ScriptEvaluatorValue value;
            parser >> value.key >> value.value;
            current.values.push_back(std::move(value));
        } else if (kind == "diag" && active) {
            std::string pair;
            parser >> pair;
            const std::size_t equals = pair.find('=');
            if (equals != std::string::npos) {
                current.diagnostics.push_back({pair.substr(0, equals), pair.substr(equals + 1)});
            }
        } else if (kind == "end" && active) {
            results.push_back(std::move(current));
            current = ScriptEvaluatorResult{};
            active = false;
        }
    }
    if (active) results.push_back(std::move(current));

    fs::remove(input_path);
    fs::remove(output_path);

    if (results.size() != cases.size()) {
        throw std::runtime_error("script evaluator returned unexpected result count");
    }
    return results;
}

ScriptEvaluatorConfig load_script_evaluator_config(
    const std::string& path,
    const std::string& default_entry_path) {
    ScriptEvaluatorConfig config;
    config.entry_path = default_entry_path;
    if (path.empty()) return config;

    const std::string text = read_all(path);
    config.runtime = parse_runtime(find_string(text, "runtime", "python_process"));
    config.executable = find_string(text, "executable", config.executable);
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
        config.entry_path = (std::filesystem::absolute(std::filesystem::path(path)).parent_path() /
                             config.entry_path).string();
    }
    return config;
}

ScriptEvaluator load_script_evaluator_asset(
    const ModelRootDescriptor& model,
    const std::string& asset_id) {
    for (const auto& asset : model.assets) {
        if (asset.id != asset_id) continue;
        if (asset.type != "kadoka.script_evaluator.v1") {
            throw std::runtime_error(
                "model asset is not kadoka.script_evaluator.v1: " + asset_id);
        }

        const std::string config_path = find_model_asset_path(model, asset_id);
        if (config_path.empty()) {
            throw std::runtime_error("script evaluator asset path is empty: " + asset_id);
        }
        return ScriptEvaluator(load_script_evaluator_config(config_path));
    }
    throw std::runtime_error("script evaluator asset not found: " + asset_id);
}

}  // namespace kadoka::othello
