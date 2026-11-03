#include "kadoka_othello/model_descriptor.hpp"

#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace kadoka::othello {
namespace {

std::string read_all(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("failed to open model descriptor: " + path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

class ModelJsonParser {
  public:
    explicit ModelJsonParser(const std::string& text) : text_(text) {}

    ModelRootDescriptor parse() {
        ModelRootDescriptor descriptor;
        bool format_found = false;
        bool model_id_found = false;
        bool model_kind_found = false;
        bool engine_found = false;
        bool assets_found = false;

        expect('{');
        std::unordered_set<std::string> keys;
        if (!consume('}')) {
            do {
                const std::string key = parse_string();
                if (!keys.emplace(key).second)
                    fail("duplicate object key");
                expect(':');
                if (key == "format") {
                    descriptor.format = parse_string();
                    format_found = true;
                } else if (key == "model_id") {
                    descriptor.model_id = parse_string();
                    model_id_found = true;
                } else if (key == "model_kind") {
                    descriptor.model_kind = parse_string();
                    model_kind_found = true;
                } else if (key == "engine") {
                    descriptor.engine = parse_string();
                    engine_found = true;
                } else if (key == "assets") {
                    descriptor.assets = parse_assets();
                    assets_found = true;
                } else {
                    skip_value(0);
                }
            } while (consume(','));
            expect('}');
        }
        skip_whitespace();
        if (position_ != text_.size())
            fail("unexpected content after root object");

        if (!format_found || descriptor.format != "kadoka.model.v1") {
            throw std::runtime_error("model descriptor requires format kadoka.model.v1");
        }
        if (!model_id_found || descriptor.model_id.empty() || !model_kind_found || descriptor.model_kind.empty() ||
            !engine_found || descriptor.engine.empty()) {
            throw std::runtime_error("model descriptor requires model_id, model_kind and engine");
        }
        if (!assets_found)
            throw std::runtime_error("model descriptor requires an assets array");
        return descriptor;
    }

  private:
    static constexpr unsigned int kMaxNestingDepth = 64;

    [[noreturn]] void fail(const char* message) const {
        throw std::runtime_error("invalid model descriptor JSON at byte " + std::to_string(position_) + ": " + message);
    }

    void skip_whitespace() {
        while (position_ < text_.size() && (text_[position_] == ' ' || text_[position_] == '\t' ||
                                            text_[position_] == '\n' || text_[position_] == '\r')) {
            ++position_;
        }
    }

    bool consume(char expected) {
        skip_whitespace();
        if (position_ >= text_.size() || text_[position_] != expected)
            return false;
        ++position_;
        return true;
    }

    void expect(char expected) {
        if (!consume(expected))
            fail("unexpected token");
    }

    static void append_utf8(std::string& output, unsigned int code_point) {
        if (code_point <= 0x7fU) {
            output += static_cast<char>(code_point);
        } else if (code_point <= 0x7ffU) {
            output += static_cast<char>(0xc0U | (code_point >> 6U));
            output += static_cast<char>(0x80U | (code_point & 0x3fU));
        } else if (code_point <= 0xffffU) {
            output += static_cast<char>(0xe0U | (code_point >> 12U));
            output += static_cast<char>(0x80U | ((code_point >> 6U) & 0x3fU));
            output += static_cast<char>(0x80U | (code_point & 0x3fU));
        } else {
            output += static_cast<char>(0xf0U | (code_point >> 18U));
            output += static_cast<char>(0x80U | ((code_point >> 12U) & 0x3fU));
            output += static_cast<char>(0x80U | ((code_point >> 6U) & 0x3fU));
            output += static_cast<char>(0x80U | (code_point & 0x3fU));
        }
    }

    unsigned int parse_hex_quad() {
        if (text_.size() - position_ < 4U)
            fail("incomplete unicode escape");
        unsigned int value = 0;
        for (unsigned int index = 0; index < 4U; ++index) {
            const char ch = text_[position_++];
            value <<= 4U;
            if (ch >= '0' && ch <= '9') {
                value |= static_cast<unsigned int>(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                value |= static_cast<unsigned int>(ch - 'a' + 10);
            } else if (ch >= 'A' && ch <= 'F') {
                value |= static_cast<unsigned int>(ch - 'A' + 10);
            } else {
                fail("invalid unicode escape");
            }
        }
        return value;
    }

    void append_unescaped_utf8(std::string& output, unsigned char first_byte) {
        const std::size_t sequence_start = position_ - 1U;
        unsigned int continuation_count = 0;
        unsigned int code_point = 0;
        unsigned int minimum_code_point = 0;
        if (first_byte >= 0xc2U && first_byte <= 0xdfU) {
            continuation_count = 1U;
            code_point = first_byte & 0x1fU;
            minimum_code_point = 0x80U;
        } else if (first_byte >= 0xe0U && first_byte <= 0xefU) {
            continuation_count = 2U;
            code_point = first_byte & 0x0fU;
            minimum_code_point = 0x800U;
        } else if (first_byte >= 0xf0U && first_byte <= 0xf4U) {
            continuation_count = 3U;
            code_point = first_byte & 0x07U;
            minimum_code_point = 0x10000U;
        } else {
            fail("invalid UTF-8 leading byte");
        }

        if (text_.size() - position_ < continuation_count) {
            fail("incomplete UTF-8 sequence");
        }
        for (unsigned int index = 0; index < continuation_count; ++index) {
            const unsigned char continuation = static_cast<unsigned char>(text_[position_++]);
            if ((continuation & 0xc0U) != 0x80U) {
                fail("invalid UTF-8 continuation byte");
            }
            code_point = (code_point << 6U) | (continuation & 0x3fU);
        }
        if (code_point < minimum_code_point || code_point > 0x10ffffU ||
            (code_point >= 0xd800U && code_point <= 0xdfffU)) {
            fail("invalid UTF-8 code point");
        }
        output.append(text_, sequence_start, continuation_count + 1U);
    }

    std::string parse_string() {
        expect('"');
        std::string value;
        while (position_ < text_.size()) {
            const unsigned char ch = static_cast<unsigned char>(text_[position_++]);
            if (ch == '"')
                return value;
            if (ch < 0x20U)
                fail("unescaped control character in string");
            if (ch >= 0x80U) {
                append_unescaped_utf8(value, ch);
                continue;
            }
            if (ch != '\\') {
                value += static_cast<char>(ch);
                continue;
            }
            if (position_ >= text_.size())
                fail("incomplete string escape");
            const char escaped = text_[position_++];
            switch (escaped) {
            case '"':
                value += '"';
                break;
            case '\\':
                value += '\\';
                break;
            case '/':
                value += '/';
                break;
            case 'b':
                value += '\b';
                break;
            case 'f':
                value += '\f';
                break;
            case 'n':
                value += '\n';
                break;
            case 'r':
                value += '\r';
                break;
            case 't':
                value += '\t';
                break;
            case 'u': {
                unsigned int code_point = parse_hex_quad();
                if (code_point >= 0xd800U && code_point <= 0xdbffU) {
                    if (text_.size() - position_ < 6U || text_[position_] != '\\' || text_[position_ + 1U] != 'u') {
                        fail("unpaired high surrogate");
                    }
                    position_ += 2U;
                    const unsigned int low_surrogate = parse_hex_quad();
                    if (low_surrogate < 0xdc00U || low_surrogate > 0xdfffU) {
                        fail("invalid low surrogate");
                    }
                    code_point = 0x10000U + ((code_point - 0xd800U) << 10U) + (low_surrogate - 0xdc00U);
                } else if (code_point >= 0xdc00U && code_point <= 0xdfffU) {
                    fail("unpaired low surrogate");
                }
                append_utf8(value, code_point);
                break;
            }
            default:
                fail("invalid string escape");
            }
        }
        fail("unterminated string");
    }

    bool parse_boolean() {
        skip_whitespace();
        if (text_.compare(position_, 4, "true") == 0) {
            position_ += 4U;
            return true;
        }
        if (text_.compare(position_, 5, "false") == 0) {
            position_ += 5U;
            return false;
        }
        fail("expected boolean");
    }

    void parse_number() {
        skip_whitespace();
        if (consume('-') && position_ >= text_.size())
            fail("incomplete number");
        if (position_ >= text_.size())
            fail("expected value");
        if (text_[position_] == '0') {
            ++position_;
            if (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) {
                fail("leading zero in number");
            }
        } else {
            if (text_[position_] < '1' || text_[position_] > '9')
                fail("expected number");
            while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) {
                ++position_;
            }
        }
        if (position_ < text_.size() && text_[position_] == '.') {
            ++position_;
            const std::size_t fraction_start = position_;
            while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) {
                ++position_;
            }
            if (position_ == fraction_start)
                fail("incomplete fraction");
        }
        if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
            ++position_;
            if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) {
                ++position_;
            }
            const std::size_t exponent_start = position_;
            while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) {
                ++position_;
            }
            if (position_ == exponent_start)
                fail("incomplete exponent");
        }
    }

    void skip_literal(std::string_view literal) {
        skip_whitespace();
        if (text_.compare(position_, literal.size(), literal) != 0)
            fail("invalid literal");
        position_ += literal.size();
    }

    void skip_object(unsigned int depth) {
        if (depth > kMaxNestingDepth)
            fail("maximum nesting depth exceeded");
        expect('{');
        std::unordered_set<std::string> keys;
        if (consume('}'))
            return;
        do {
            const std::string key = parse_string();
            if (!keys.emplace(key).second)
                fail("duplicate object key");
            expect(':');
            skip_value(depth);
        } while (consume(','));
        expect('}');
    }

    void skip_array(unsigned int depth) {
        if (depth > kMaxNestingDepth)
            fail("maximum nesting depth exceeded");
        expect('[');
        if (consume(']'))
            return;
        do {
            skip_value(depth);
        } while (consume(','));
        expect(']');
    }

    void skip_value(unsigned int depth) {
        skip_whitespace();
        if (position_ >= text_.size())
            fail("expected value");
        switch (text_[position_]) {
        case '"':
            static_cast<void>(parse_string());
            return;
        case '{':
            skip_object(depth + 1U);
            return;
        case '[':
            skip_array(depth + 1U);
            return;
        case 't':
            skip_literal("true");
            return;
        case 'f':
            skip_literal("false");
            return;
        case 'n':
            skip_literal("null");
            return;
        default:
            parse_number();
            return;
        }
    }

    ModelAssetDescriptor parse_asset() {
        ModelAssetDescriptor asset;
        bool id_found = false;
        bool type_found = false;
        bool path_found = false;
        bool required_found = false;

        expect('{');
        std::unordered_set<std::string> keys;
        if (!consume('}')) {
            do {
                const std::string key = parse_string();
                if (!keys.emplace(key).second)
                    fail("duplicate asset field");
                expect(':');
                if (key == "id") {
                    asset.id = parse_string();
                    id_found = true;
                } else if (key == "type") {
                    asset.type = parse_string();
                    type_found = true;
                } else if (key == "path") {
                    asset.path = parse_string();
                    path_found = true;
                } else if (key == "required") {
                    asset.required = parse_boolean();
                    required_found = true;
                } else {
                    skip_value(0);
                }
            } while (consume(','));
            expect('}');
        }

        if (!id_found || asset.id.empty() || !type_found || asset.type.empty() || !path_found || asset.path.empty() ||
            !required_found) {
            fail("model asset requires id, type, path and required");
        }
        return asset;
    }

    std::vector<ModelAssetDescriptor> parse_assets() {
        std::vector<ModelAssetDescriptor> assets;
        expect('[');
        if (consume(']'))
            return assets;
        do {
            assets.push_back(parse_asset());
        } while (consume(','));
        expect(']');
        return assets;
    }

    const std::string& text_;
    std::size_t position_{};
};

std::string normalize_asset_path(const std::string& value) {
    if (value.empty() || value.find('\0') != std::string::npos) {
        throw std::runtime_error("model asset path must be a non-empty relative path");
    }
    std::string portable_path = value;
    for (char& ch : portable_path) {
        if (ch == '\\')
            ch = '/';
    }
    const std::filesystem::path path(portable_path);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory() ||
        (portable_path.size() >= 2U && std::isalpha(static_cast<unsigned char>(portable_path[0])) &&
         portable_path[1] == ':')) {
        throw std::runtime_error("model asset path must be relative to the model package");
    }
    for (const auto& component : path) {
        if (component == "..") {
            throw std::runtime_error("model asset path must not escape the model package");
        }
    }
    if (path.lexically_normal() == ".") {
        throw std::runtime_error("model asset path must identify an asset");
    }
    return path.lexically_normal().generic_string();
}

bool is_supported_runtime_asset_type(const std::string& type) {
    constexpr std::array<std::string_view, 4> supported_types{
        "kadoka.evaluator.linear.v1",
        "kadoka.evaluator.simple.v1",
        "kadoka.character_behavior.v1",
        "kadoka.script_evaluator.v1",
    };
    for (const std::string_view supported_type : supported_types) {
        if (type == supported_type)
            return true;
    }
    return false;
}

void validate_assets(ModelRootDescriptor& descriptor) {
    namespace fs = std::filesystem;
    const fs::path model_directory = fs::absolute(fs::path(descriptor.source_directory)).lexically_normal();
    std::error_code error;
    const fs::path canonical_model_directory = fs::weakly_canonical(model_directory, error);
    if (error) {
        throw std::runtime_error("failed to resolve model package directory: " + error.message());
    }
    std::unordered_set<std::string> ids;
    for (auto& asset : descriptor.assets) {
        if (!ids.emplace(asset.id).second) {
            throw std::runtime_error("duplicate model asset id: " + asset.id);
        }
        asset.path = normalize_asset_path(asset.path);
        if (asset.required && !is_supported_runtime_asset_type(asset.type)) {
            throw std::runtime_error("unsupported required model asset type: " + asset.type);
        }
        const fs::path asset_path = fs::weakly_canonical(canonical_model_directory / fs::path(asset.path), error);
        if (error) {
            throw std::runtime_error("failed to resolve model asset path: " + asset.path);
        }
        const fs::path relative_path = asset_path.lexically_relative(canonical_model_directory);
        if (relative_path.empty() || relative_path == "." || relative_path.is_absolute() ||
            *relative_path.begin() == "..") {
            throw std::runtime_error("model asset path must resolve inside the model package: " + asset.path);
        }
        if (!asset.required)
            continue;

        if (!fs::is_regular_file(asset_path, error) || error) {
            throw std::runtime_error("required model asset is missing or not a file: " + asset.id + " (" + asset.path +
                                     ")");
        }
    }
    descriptor.source_directory = model_directory.string();
}

} // namespace

const ModelAssetDescriptor* find_model_asset(const ModelRootDescriptor& descriptor,
                                             const std::string& asset_id) noexcept {
    for (const auto& asset : descriptor.assets) {
        if (asset.id == asset_id)
            return &asset;
    }
    return nullptr;
}

ModelRootDescriptor load_model_root_descriptor(const std::string& path) {
    const std::string text = read_all(path);
    ModelRootDescriptor descriptor = ModelJsonParser(text).parse();
    descriptor.source_directory = std::filesystem::absolute(std::filesystem::path(path)).parent_path().string();
    validate_assets(descriptor);
    return descriptor;
}

std::string find_model_asset_path(const ModelRootDescriptor& descriptor, const std::string& asset_id) {
    const ModelAssetDescriptor* asset = find_model_asset(descriptor, asset_id);
    if (asset == nullptr)
        return {};
    std::filesystem::path path(asset->path);
    if (path.is_relative())
        path = std::filesystem::path(descriptor.source_directory) / path;
    return path.string();
}

} // namespace kadoka::othello
