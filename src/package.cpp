#include "kadoka_othello/package.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace kadoka::othello {
namespace {

std::string read_all(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open AI manifest: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string find_string(const std::string& json, const std::string& key, const std::string& fallback = {}) {
    const std::string token = "\"" + key + "\"";
    const std::size_t key_pos = json.find(token);
    if (key_pos == std::string::npos) return fallback;
    const std::size_t colon = json.find(':', key_pos + token.size());
    const std::size_t first_quote = json.find('"', colon + 1);
    if (colon == std::string::npos || first_quote == std::string::npos) return fallback;
    const std::size_t second_quote = json.find('"', first_quote + 1);
    if (second_quote == std::string::npos) return fallback;
    return json.substr(first_quote + 1, second_quote - first_quote - 1);
}

std::size_t find_number(const std::string& json, const std::string& key, std::size_t fallback) {
    const std::string token = "\"" + key + "\"";
    const std::size_t key_pos = json.find(token);
    if (key_pos == std::string::npos) return fallback;
    const std::size_t colon = json.find(':', key_pos + token.size());
    if (colon == std::string::npos) return fallback;
    std::size_t start = colon + 1;
    while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) ++start;
    std::size_t end = start;
    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end]))) ++end;
    return end == start ? fallback : static_cast<std::size_t>(std::stoull(json.substr(start, end - start)));
}

std::vector<std::string> find_string_array(const std::string& json, const std::string& key) {
    std::vector<std::string> values;
    const std::string token = "\"" + key + "\"";
    const std::size_t key_pos = json.find(token);
    if (key_pos == std::string::npos) return values;
    const std::size_t open = json.find('[', key_pos + token.size());
    const std::size_t close = json.find(']', open + 1);
    if (open == std::string::npos || close == std::string::npos) return values;
    std::size_t pos = open + 1;
    while (pos < close) {
        const std::size_t first_quote = json.find('"', pos);
        if (first_quote == std::string::npos || first_quote >= close) break;
        const std::size_t second_quote = json.find('"', first_quote + 1);
        if (second_quote == std::string::npos || second_quote > close) break;
        values.push_back(json.substr(first_quote + 1, second_quote - first_quote - 1));
        pos = second_quote + 1;
    }
    return values;
}

std::vector<std::size_t> find_number_array(const std::string& json, const std::string& key) {
    std::vector<std::size_t> values;
    const std::string token = "\"" + key + "\"";
    const std::size_t key_pos = json.find(token);
    if (key_pos == std::string::npos) return values;
    const std::size_t open = json.find('[', key_pos + token.size());
    const std::size_t close = json.find(']', open + 1);
    if (open == std::string::npos || close == std::string::npos) return values;
    std::size_t pos = open + 1;
    while (pos < close) {
        while (pos < close && !std::isdigit(static_cast<unsigned char>(json[pos]))) ++pos;
        if (pos >= close) break;
        std::size_t end = pos;
        while (end < close && std::isdigit(static_cast<unsigned char>(json[end]))) ++end;
        values.push_back(static_cast<std::size_t>(std::stoull(json.substr(pos, end - pos))));
        pos = end;
    }
    return values;
}

AIPackageInterface parse_interface(const std::string& value) {
    if (value == "native") return AIPackageInterface::Native;
    if (value == "dynamic_library") return AIPackageInterface::DynamicLibrary;
    if (value == "external_process") return AIPackageInterface::ExternalProcess;
    if (value == "python") return AIPackageInterface::Python;
    if (value == "network") return AIPackageInterface::Network;
    throw std::invalid_argument("unknown AI package interface: " + value);
}

}  // namespace

AIPackageManifest load_ai_manifest(const std::string& path) {
    const std::string json = read_all(path);
    AIPackageManifest manifest;
    manifest.id = find_string(json, "id");
    manifest.name = find_string(json, "name");
    manifest.version = find_string(json, "version");
    manifest.protocol_version = find_number(json, "protocol_version", 1);
    manifest.board_sizes = find_number_array(json, "board_sizes");
    manifest.interface_type = parse_interface(find_string(json, "interface", "native"));
    manifest.adapter = find_string(json, "adapter", "pass_through");
    manifest.entry = find_string(json, "entry");
    manifest.model = find_string(json, "model");
    manifest.metadata = find_string(json, "metadata");
    manifest.capabilities = find_string_array(json, "capabilities");
    manifest.source_directory = std::filesystem::path(path).parent_path().string();

    if (manifest.id.empty() || manifest.name.empty() || manifest.version.empty()) {
        throw std::invalid_argument("AI manifest requires id, name and version");
    }
    return manifest;
}

std::string package_interface_name(AIPackageInterface type) {
    switch (type) {
        case AIPackageInterface::Native: return "native";
        case AIPackageInterface::DynamicLibrary: return "dynamic_library";
        case AIPackageInterface::ExternalProcess: return "external_process";
        case AIPackageInterface::Python: return "python";
        case AIPackageInterface::Network: return "network";
    }
    return "unknown";
}

}  // namespace kadoka::othello
