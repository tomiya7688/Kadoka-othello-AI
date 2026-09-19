#include "kadoka_othello/package_loader.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "kadoka_othello/evaluator_ai.hpp"
#include "kadoka_othello/external_ai_session.hpp"
#include "kadoka_othello/obake_kadoka.hpp"
#include "kadoka_othello/obake_maru.hpp"

namespace kadoka::othello {
namespace {

std::string resolve_model_path(const AIPackageManifest& manifest) {
    if (manifest.model.empty()) return {};
    namespace fs = std::filesystem;
    fs::path path = fs::path(manifest.model);
    if (path.is_relative()) path = fs::path(manifest.source_directory) / path;
    return path.string();
}

std::filesystem::path resolve_entry_path(const AIPackageManifest& manifest) {
    namespace fs = std::filesystem;
    fs::path entry = fs::path(manifest.entry);
    if (entry.is_relative()) entry = fs::path(manifest.source_directory) / entry;
    return entry;
}

std::unique_ptr<IAIEngine> make_native_engine(
    const AIPackageManifest& manifest,
    std::uint64_t seed) {
    if (manifest.id == "kadoka.random") {
        return std::make_unique<RandomAI>(seed);
    }
    if (manifest.id == "kadoka.obake_kadoka") {
        const ObakeKadokaConfig config = load_obake_kadoka_model(resolve_model_path(manifest));
        return std::make_unique<ObakeKadokaAI>(seed, config);
    }
    if (manifest.id == "kadoka.obake_maru") {
        const ObakeMaruConfig config = load_obake_maru_model(resolve_model_path(manifest));
        return std::make_unique<ObakeMaruAI>(seed, config);
    }

    const std::string model_path = resolve_model_path(manifest);
    if (!model_path.empty()) {
        const ModelRootDescriptor model = load_model_root_descriptor(model_path);
        if (model.engine == "kadoka.evaluator_ai.v1") {
            return std::make_unique<EvaluatorAI>(manifest.id, model);
        }
    }

    throw std::invalid_argument("unknown built-in native AI: " + manifest.id);
}

std::string quote_arg(const std::string& value) {
    std::string result = "\"";
    for (char ch : value) {
        if (ch == '"') result += '\\';
        result += ch;
    }
    result += '"';
    return result;
}

class PersistentExternalProcessAI final : public IAIEngine {
public:
    PersistentExternalProcessAI(AIPackageManifest manifest, bool script)
        : manifest_(std::move(manifest)),
          session_(make_session_config(manifest_, script)) {}

    std::string id() const override { return manifest_.id; }

    AIOutput think(const AIInput& input) override {
        return session_.inspect(input).output;
    }

    AIInspection inspect(const AIInput& input) override {
        return session_.inspect(input);
    }

private:
    static ExternalAISessionConfig make_session_config(
        const AIPackageManifest& manifest,
        bool script) {
        const std::filesystem::path entry = resolve_entry_path(manifest);
        ExternalAISessionConfig config;
        config.response_timeout = std::chrono::milliseconds(manifest.timeout_ms);

        if (script) {
            config.executable = manifest.executable.empty() ? "python" : manifest.executable;
            config.arguments = {entry.string(), "--kadoka-session"};
        } else if (!manifest.executable.empty()) {
            config.executable = manifest.executable;
            config.arguments = {entry.string(), "--kadoka-session"};
        } else {
            config.executable = entry.string();
            config.arguments = {"--kadoka-session"};
        }
        return config;
    }

    AIPackageManifest manifest_;
    ExternalAISession session_;
};

// Compatibility transport for old external packages. New packages use
// transport=persistent and never create request/response files per move.
class LegacyOneshotExternalProcessAI final : public IAIEngine {
public:
    LegacyOneshotExternalProcessAI(AIPackageManifest manifest, bool script)
        : manifest_(std::move(manifest)), script_(script) {}

    std::string id() const override { return manifest_.id; }

    AIOutput think(const AIInput& input) override {
        return inspect(input).output;
    }

    AIInspection inspect(const AIInput& input) override {
        if (input.board == nullptr) throw std::invalid_argument("external AI requires board input");

        namespace fs = std::filesystem;
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        const fs::path temp_dir = fs::temp_directory_path();
        const fs::path request_path = temp_dir / ("kadoka_ai_request_" + std::to_string(stamp) + ".txt");
        const fs::path response_path = temp_dir / ("kadoka_ai_response_" + std::to_string(stamp) + ".txt");

        {
            std::ofstream request(request_path);
            if (!request) throw std::runtime_error("failed to create external AI request file");
            request << core_state_to_json(input) << '\n';
        }

        const fs::path entry = resolve_entry_path(manifest_);
        std::string command;
        if (script_) {
            const std::string executable = manifest_.executable.empty() ? "python" : manifest_.executable;
            command = quote_arg(executable) + " " + quote_arg(entry.string());
        } else if (!manifest_.executable.empty()) {
            command = quote_arg(manifest_.executable) + " " + quote_arg(entry.string());
        } else {
            command = quote_arg(entry.string());
        }
        command += " --kadoka-input " + quote_arg(request_path.string());
        command += " --kadoka-output " + quote_arg(response_path.string());

        const int exit_code = std::system(command.c_str());
        if (exit_code != 0) {
            fs::remove(request_path);
            fs::remove(response_path);
            throw std::runtime_error("external AI process failed with exit code " + std::to_string(exit_code));
        }

        std::ifstream response(response_path);
        if (!response) {
            fs::remove(request_path);
            throw std::runtime_error("external AI did not create response file");
        }

        AIInspection inspection;
        bool has_move = false;
        std::string line;
        while (std::getline(response, line)) {
            std::istringstream parser(line);
            std::string kind;
            parser >> kind;
            if (kind == "move") {
                parser >> inspection.output.move.row >> inspection.output.move.col;
                has_move = true;
            } else if (kind == "diag") {
                std::string pair;
                parser >> pair;
                const std::size_t equals = pair.find('=');
                if (equals != std::string::npos) {
                    inspection.diagnostics.push_back({pair.substr(0, equals), pair.substr(equals + 1)});
                }
            } else if (kind == "candidate") {
                AICandidate candidate;
                parser >> candidate.move.row >> candidate.move.col >> candidate.value >> candidate.policy;
                inspection.candidates.push_back(candidate);
            }
        }

        fs::remove(request_path);
        fs::remove(response_path);
        if (!has_move) throw std::runtime_error("external AI response does not contain move");
        return inspection;
    }

private:
    AIPackageManifest manifest_;
    bool script_{};
};

std::unique_ptr<IAIEngine> make_external_engine(
    const AIPackageManifest& manifest,
    bool script) {
    if (manifest.transport == "legacy_oneshot") {
        return std::make_unique<LegacyOneshotExternalProcessAI>(manifest, script);
    }
    return std::make_unique<PersistentExternalProcessAI>(manifest, script);
}

}  // namespace

LoadedAIPackage load_ai_package(
    const AIPackageManifest& manifest,
    std::uint64_t seed) {
    LoadedAIPackage loaded;
    loaded.manifest = manifest;
    switch (manifest.interface_type) {
        case AIPackageInterface::Native:
            loaded.engine = make_native_engine(manifest, seed);
            break;
        case AIPackageInterface::ExternalProcess:
            loaded.engine = make_external_engine(manifest, false);
            break;
        case AIPackageInterface::Python:
            loaded.engine = make_external_engine(manifest, true);
            break;
        case AIPackageInterface::DynamicLibrary:
        case AIPackageInterface::Network:
            throw std::runtime_error(
                "AI package interface is recognized but runtime loading is not implemented yet: " +
                package_interface_name(manifest.interface_type));
    }

    return loaded;
}

}  // namespace kadoka::othello
