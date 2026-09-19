#include "kadoka_othello/package_import.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace kadoka::othello {
namespace {

std::string escape_json(const std::string& value) {
    std::string result;
    for (char ch : value) {
        if (ch == '\\' || ch == '"') result.push_back('\\');
        result.push_back(ch);
    }
    return result;
}

}  // namespace

std::string import_ai_package(const AIPackageImportRequest& request) {
    if (request.source_path.empty() || request.destination_directory.empty() ||
        request.id.empty() || request.name.empty()) {
        throw std::invalid_argument("AI import requires source, destination, id and name");
    }

    namespace fs = std::filesystem;
    const fs::path source = fs::absolute(request.source_path);
    if (!fs::exists(source)) {
        throw std::runtime_error("AI import source does not exist: " + source.string());
    }

    const fs::path package_dir = fs::absolute(request.destination_directory);
    fs::create_directories(package_dir);

    fs::path entry = source;
    if (request.copy_source) {
        entry = package_dir / source.filename();
        fs::copy_file(source, entry, fs::copy_options::overwrite_existing);
    }

    const fs::path manifest_path = package_dir / "manifest.json";
    std::ofstream output(manifest_path);
    if (!output) {
        throw std::runtime_error("failed to create manifest: " + manifest_path.string());
    }

    output << "{\n";
    output << "  \"id\": \"" << escape_json(request.id) << "\",\n";
    output << "  \"name\": \"" << escape_json(request.name) << "\",\n";
    output << "  \"version\": \"" << escape_json(request.version) << "\",\n";
    output << "  \"protocol_version\": 1,\n";
    output << "  \"board_sizes\": [";
    for (std::size_t i = 0; i < request.board_sizes.size(); ++i) {
        if (i != 0) output << ", ";
        output << request.board_sizes[i];
    }
    output << "],\n";
    output << "  \"interface\": \"" << package_interface_name(request.interface_type) << "\",\n";
    output << "  \"entry\": \"" << escape_json(entry.filename().string()) << "\",\n";
    output << "  \"capabilities\": [\"move\", \"inspection\"]\n";
    output << "}\n";

    return manifest_path.string();
}

}  // namespace kadoka::othello
