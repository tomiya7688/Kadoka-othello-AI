#include "kadoka_othello/package_loader.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "kadoka_othello/obake_kadoka.hpp"

namespace kadoka::othello {
namespace {

std::unique_ptr<IAIAdapter> make_adapter(const std::string& id) {
    if (id == "pass_through") return std::make_unique<PassThroughAdapter>();
    if (id == "drop_legal_moves") return std::make_unique<DropLegalMovesAdapter>();
    throw std::invalid_argument("unknown AI adapter: " + id);
}

std::string resolve_model_path(const AIPackageManifest& manifest) {
    if (manifest.model.empty()) return {};
    namespace fs = std::filesystem;
    fs::path path = fs::path(manifest.model);
    if (path.is_relative()) path = fs::path(manifest.source_directory) / path;
    return path.string();
}

std::unique_ptr<IAIEngine> make_native_engine(
    const AIPackageManifest& manifest,
    std::uint64_t seed) {
    if (manifest.id == "kadoka.random") {
        return std::make_unique<RandomAI>(seed);
    }
    if (manifest.id == "kadoka.obake_kadoka") {
        const ObakeKadokaConfig config = load_obake_kadoka_config(resolve_model_path(manifest));
        return std::make_unique<ObakeKadokaAI>(seed, config);
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

class ExternalProcessAI final : public IAIEngine {
public:
    ExternalProcessAI(AIPackageManifest manifest, bool python)
        : manifest_(std::move(manifest)), python_(python) {}

    std::string id() const override { return manifest_.id; }

    AIOutput think(const AdaptedAIInput& input) override {
        return inspect(input).output;
    }

    AIInspection inspect(const AdaptedAIInput& input) override {
        if (input.board == nullptr) throw std::invalid_argument("external AI requires board input");

        namespace fs = std::filesystem;
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        const fs::path temp_dir = fs::temp_directory_path();
        const fs::path request_path = temp_dir / ("kadoka_ai_request_" + std::to_string(stamp) + ".txt");
        const fs::path response_path = temp_dir / ("kadoka_ai_response_" + std::to_string(stamp) + ".txt");

        {
            std::ofstream request(request_path);
            if (!request) throw std::runtime_error("failed to create external AI request file");
            request << "KADOKA_AI_PROTOCOL 1\n";
            request << "size " << input.board->size() << '\n';
            for (std::size_t row = 0; row < input.board->size(); ++row) {
                for (std::size_t col = 0; col < input.board->size(); ++col) {
                    const Cell cell = input.board->at({row, col});
                    request << (cell == Cell::Black ? 'B' : cell == Cell::White ? 'W' : '.');
                }
                request << '\n';
            }
            const std::size_t legal_count = input.legal_moves == nullptr ? 0 : input.legal_moves->size();
            request << "legal_count " << legal_count << '\n';
            if (input.legal_moves != nullptr) {
                for (const auto move : *input.legal_moves) request << move.row << ' ' << move.col << '\n';
            }
        }

        fs::path entry = fs::path(manifest_.entry);
        if (entry.is_relative()) entry = fs::path(manifest_.source_directory) / entry;
        std::string command;
        if (python_) command = "python " + quote_arg(entry.string());
        else command = quote_arg(entry.string());
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
    bool python_{};
};

}  // namespace

LoadedAIPackage load_ai_package(
    const AIPackageManifest& manifest,
    std::uint64_t seed) {
    LoadedAIPackage loaded;
    loaded.manifest = manifest;
    loaded.adapter = make_adapter(manifest.adapter);

    switch (manifest.interface_type) {
        case AIPackageInterface::Native:
            loaded.engine = make_native_engine(manifest, seed);
            break;
        case AIPackageInterface::ExternalProcess:
            loaded.engine = std::make_unique<ExternalProcessAI>(manifest, false);
            break;
        case AIPackageInterface::Python:
            loaded.engine = std::make_unique<ExternalProcessAI>(manifest, true);
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
