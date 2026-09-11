#include "kadoka_othello/model_descriptor.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace kadoka::othello {
namespace {

std::string read_all(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("failed to open model descriptor: " + path);
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

bool find_bool(const std::string& text, const std::string& key, bool fallback) {
    const std::string token = "\"" + key + "\"";
    const std::size_t key_pos = text.find(token);
    if (key_pos == std::string::npos) return fallback;
    const std::size_t colon = text.find(':', key_pos + token.size());
    if (colon == std::string::npos) return fallback;
    std::size_t pos = colon + 1;
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    if (text.compare(pos, 4, "true") == 0) return true;
    if (text.compare(pos, 5, "false") == 0) return false;
    return fallback;
}

std::vector<ModelAssetDescriptor> parse_assets(const std::string& text) {
    std::vector<ModelAssetDescriptor> assets;
    const std::size_t key = text.find("\"assets\"");
    if (key == std::string::npos) return assets;
    const std::size_t open = text.find('[', key);
    if (open == std::string::npos) return assets;

    std::size_t pos = open + 1;
    while (pos < text.size()) {
        const std::size_t object_open = text.find('{', pos);
        const std::size_t array_close = text.find(']', pos);
        if (array_close != std::string::npos &&
            (object_open == std::string::npos || array_close < object_open)) {
            break;
        }
        if (object_open == std::string::npos) break;

        int depth = 1;
        std::size_t cursor = object_open + 1;
        for (; cursor < text.size() && depth > 0; ++cursor) {
            if (text[cursor] == '{') ++depth;
            else if (text[cursor] == '}') --depth;
        }
        if (depth != 0) throw std::runtime_error("invalid model asset object");

        const std::string object = text.substr(object_open, cursor - object_open);
        ModelAssetDescriptor asset;
        asset.id = find_string(object, "id");
        asset.type = find_string(object, "type");
        asset.path = find_string(object, "path");
        asset.required = find_bool(object, "required", true);
        if (asset.id.empty() || asset.type.empty() || asset.path.empty()) {
            throw std::runtime_error("model asset requires id, type and path");
        }
        assets.push_back(std::move(asset));
        pos = cursor;
    }
    return assets;
}

}  // namespace

ModelRootDescriptor load_model_root_descriptor(const std::string& path) {
    const std::string text = read_all(path);
    ModelRootDescriptor descriptor;
    descriptor.format = find_string(text, "format", descriptor.format);
    descriptor.model_id = find_string(text, "model_id");
    descriptor.model_kind = find_string(text, "model_kind");
    descriptor.engine = find_string(text, "engine");
    descriptor.assets = parse_assets(text);
    descriptor.source_directory = std::filesystem::absolute(std::filesystem::path(path)).parent_path().string();

    if (descriptor.model_id.empty() || descriptor.model_kind.empty() || descriptor.engine.empty()) {
        throw std::runtime_error("model descriptor requires model_id, model_kind and engine");
    }
    return descriptor;
}

std::string find_model_asset_path(
    const ModelRootDescriptor& descriptor,
    const std::string& asset_id) {
    namespace fs = std::filesystem;
    for (const auto& asset : descriptor.assets) {
        if (asset.id != asset_id) continue;
        fs::path path(asset.path);
        if (path.is_relative()) path = fs::path(descriptor.source_directory) / path;
        return path.string();
    }
    return {};
}

}  // namespace kadoka::othello
