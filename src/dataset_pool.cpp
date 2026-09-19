#include "kadoka_othello/dataset_pool.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "kadoka_othello/game_record.hpp"

namespace kadoka::othello {
namespace {

std::string escape_json(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += ch; break;
        }
    }
    return escaped;
}

std::optional<std::size_t> find_value_start_optional(
    std::string_view json,
    std::string_view key) {
    const std::string token = "\"" + std::string(key) + "\"";
    std::size_t search_from = 0;
    while (true) {
        const std::size_t key_pos = json.find(token, search_from);
        if (key_pos == std::string_view::npos) return std::nullopt;

        std::size_t colon = key_pos + token.size();
        while (colon < json.size() &&
               std::isspace(static_cast<unsigned char>(json[colon]))) {
            ++colon;
        }
        if (colon < json.size() && json[colon] == ':') {
            std::size_t pos = colon + 1;
            while (pos < json.size() &&
                   std::isspace(static_cast<unsigned char>(json[pos]))) {
                ++pos;
            }
            return pos;
        }
        search_from = key_pos + token.size();
    }
}

std::size_t find_value_start(std::string_view json, std::string_view key) {
    const auto pos = find_value_start_optional(json, key);
    if (!pos) {
        throw std::invalid_argument(
            "dataset JSON missing field: " + std::string(key));
    }
    return *pos;
}

std::string parse_string_at(std::string_view json, std::size_t pos) {
    if (pos >= json.size() || json[pos] != '"') {
        throw std::invalid_argument("dataset JSON expected string");
    }

    std::string result;
    for (++pos; pos < json.size(); ++pos) {
        const char ch = json[pos];
        if (ch == '"') return result;
        if (ch != '\\') {
            result += ch;
            continue;
        }
        if (++pos >= json.size()) {
            throw std::invalid_argument("dataset JSON unterminated escape");
        }
        switch (json[pos]) {
            case '\\': result += '\\'; break;
            case '"': result += '"'; break;
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            default:
                throw std::invalid_argument(
                    "dataset JSON contains unsupported escape");
        }
    }
    throw std::invalid_argument("dataset JSON unterminated string");
}

std::string read_string(std::string_view json, std::string_view key) {
    return parse_string_at(json, find_value_start(json, key));
}

std::optional<std::string> read_optional_string(
    std::string_view json,
    std::string_view key) {
    const std::size_t pos = find_value_start(json, key);
    if (json.substr(pos, 4) == "null") return std::nullopt;
    return parse_string_at(json, pos);
}

std::uint64_t read_uint(std::string_view json, std::string_view key) {
    std::size_t pos = find_value_start(json, key);
    const std::size_t begin = pos;
    while (pos < json.size() &&
           std::isdigit(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    if (pos == begin) {
        throw std::invalid_argument(
            "dataset JSON field must be unsigned integer: " +
            std::string(key));
    }
    return std::stoull(std::string(json.substr(begin, pos - begin)));
}

double read_double(std::string_view json, std::string_view key) {
    std::size_t pos = find_value_start(json, key);
    const std::size_t begin = pos;
    while (pos < json.size()) {
        const char ch = json[pos];
        if (!(std::isdigit(static_cast<unsigned char>(ch)) ||
              ch == '-' || ch == '+' || ch == '.' ||
              ch == 'e' || ch == 'E')) {
            break;
        }
        ++pos;
    }
    if (pos == begin) {
        throw std::invalid_argument(
            "dataset JSON field must be number: " + std::string(key));
    }
    return std::stod(std::string(json.substr(begin, pos - begin)));
}

std::string_view read_container_at(
    std::string_view json,
    std::size_t start,
    char open,
    char close) {
    if (start >= json.size() || json[start] != open) {
        throw std::invalid_argument("dataset JSON container has wrong type");
    }

    std::size_t depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (std::size_t pos = start; pos < json.size(); ++pos) {
        const char ch = json[pos];
        if (in_string) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') in_string = false;
            continue;
        }
        if (ch == '"') {
            in_string = true;
            continue;
        }
        if (ch == open) {
            ++depth;
        } else if (ch == close) {
            if (depth == 0) {
                throw std::invalid_argument(
                    "dataset JSON container is malformed");
            }
            --depth;
            if (depth == 0) {
                return json.substr(start, pos - start + 1);
            }
        }
    }
    throw std::invalid_argument("dataset JSON container is not closed");
}

std::string_view read_object(std::string_view json, std::string_view key) {
    return read_container_at(json, find_value_start(json, key), '{', '}');
}

std::optional<std::string_view> read_optional_object(
    std::string_view json,
    std::string_view key) {
    const std::size_t pos = find_value_start(json, key);
    if (json.substr(pos, 4) == "null") return std::nullopt;
    return read_container_at(json, pos, '{', '}');
}

std::string_view read_array(std::string_view json, std::string_view key) {
    return read_container_at(json, find_value_start(json, key), '[', ']');
}

std::vector<std::string> parse_string_array(std::string_view array) {
    std::vector<std::string> values;
    if (array.size() < 2 || array.front() != '[' || array.back() != ']') {
        throw std::invalid_argument("dataset JSON expected string array");
    }

    std::size_t pos = 1;
    while (pos + 1 < array.size()) {
        while (pos + 1 < array.size() &&
               (std::isspace(static_cast<unsigned char>(array[pos])) ||
                array[pos] == ',')) {
            ++pos;
        }
        if (pos + 1 >= array.size() || array[pos] == ']') break;
        values.push_back(parse_string_at(array, pos));

        ++pos;
        bool escaped = false;
        while (pos < array.size()) {
            if (!escaped && array[pos] == '"') {
                ++pos;
                break;
            }
            if (!escaped && array[pos] == '\\') escaped = true;
            else escaped = false;
            ++pos;
        }
    }
    return values;
}

std::vector<std::string_view> parse_object_array(std::string_view array) {
    std::vector<std::string_view> objects;
    if (array.size() < 2 || array.front() != '[' || array.back() != ']') {
        throw std::invalid_argument("dataset JSON expected object array");
    }

    std::size_t pos = 1;
    while (pos + 1 < array.size()) {
        while (pos + 1 < array.size() &&
               (std::isspace(static_cast<unsigned char>(array[pos])) ||
                array[pos] == ',')) {
            ++pos;
        }
        if (pos + 1 >= array.size() || array[pos] == ']') break;
        const std::string_view object =
            read_container_at(array, pos, '{', '}');
        objects.push_back(object);
        pos += object.size();
    }
    return objects;
}

void write_string_array(
    std::ostringstream& out,
    const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        out << "\"" << escape_json(values[i]) << "\"";
    }
    out << ']';
}

void write_model_ref(
    std::ostringstream& out,
    const std::optional<DatasetModelRef>& model) {
    if (!model) {
        out << "null";
        return;
    }
    out << "{\"model_id\":\"" << escape_json(model->model_id)
        << "\",\"model_version\":\"" << escape_json(model->model_version)
        << "\"}";
}

std::optional<DatasetModelRef> parse_model_ref(
    std::string_view json,
    std::string_view key) {
    const auto object = read_optional_object(json, key);
    if (!object) return std::nullopt;
    return DatasetModelRef{
        read_string(*object, "model_id"),
        read_string(*object, "model_version"),
    };
}

void validate_board_size(std::size_t board_size) {
    if (board_size != 6 && board_size != 8 && board_size != 10) {
        throw std::invalid_argument(
            "Dataset Pool currently supports board sizes 6, 8 and 10");
    }
}

void validate_model_ref(
    const std::optional<DatasetModelRef>& model,
    std::string_view name) {
    if (!model) return;
    if (model->model_id.empty() || model->model_version.empty()) {
        throw std::invalid_argument(
            std::string(name) + " requires model_id and model_version");
    }
}

void validate_unique_strings(
    const std::vector<std::string>& values,
    std::string_view label,
    bool require_non_empty) {
    if (require_non_empty && values.empty()) {
        throw std::invalid_argument(
            std::string(label) + " must not be empty");
    }

    std::unordered_set<std::string> seen;
    for (const auto& value : values) {
        if (value.empty()) {
            throw std::invalid_argument(
                std::string(label) + " contains an empty value");
        }
        if (!seen.insert(value).second) {
            throw std::invalid_argument(
                std::string(label) + " contains duplicate value: " + value);
        }
    }
}

void validate_entry(const DatasetEntry& entry) {
    if (entry.dataset_id.empty()) {
        throw std::invalid_argument("dataset_id must not be empty");
    }
    if (entry.provenance.source_type.empty()) {
        throw std::invalid_argument("source_type must not be empty");
    }
    validate_board_size(entry.provenance.board_size);
    validate_model_ref(entry.provenance.generator_model, "generator_model");
    validate_model_ref(entry.provenance.opponent_model, "opponent_model");
    if (entry.provenance.parent_dataset &&
        entry.provenance.parent_dataset->empty()) {
        throw std::invalid_argument("parent_dataset must not be empty");
    }
    if (entry.provenance.created_at.empty()) {
        throw std::invalid_argument("created_at must not be empty");
    }
    if (entry.provenance.license.empty()) {
        throw std::invalid_argument("license must not be empty");
    }
    validate_unique_strings(
        entry.provenance.game_ids,
        "game_ids",
        true);
    validate_unique_strings(entry.tags, "tags", false);
    validate_unique_strings(entry.artifacts, "artifacts", false);
}

void validate_recipe_shape(const DatasetRecipe& recipe) {
    if (recipe.recipe_id.empty()) {
        throw std::invalid_argument("recipe_id must not be empty");
    }
    if (recipe.model_id.empty()) {
        throw std::invalid_argument("recipe model_id must not be empty");
    }
    if (recipe.datasets.empty()) {
        throw std::invalid_argument("recipe must contain at least one dataset");
    }

    std::unordered_set<std::string> seen;
    for (const auto& item : recipe.datasets) {
        if (item.dataset_id.empty()) {
            throw std::invalid_argument(
                "recipe dataset_id must not be empty");
        }
        if (!std::isfinite(item.ratio) || item.ratio <= 0.0) {
            throw std::invalid_argument(
                "recipe ratio must be finite and greater than zero");
        }
        if (!seen.insert(item.dataset_id).second) {
            throw std::invalid_argument(
                "recipe contains duplicate dataset_id: " +
                item.dataset_id);
        }
    }
}

bool contains_game_id(
    const std::vector<std::string>& game_ids,
    const std::string& game_id) {
    return std::find(game_ids.begin(), game_ids.end(), game_id) !=
        game_ids.end();
}

}  // namespace

const char* dataset_usage_name(DatasetUsage usage) noexcept {
    switch (usage) {
        case DatasetUsage::Training: return "training";
        case DatasetUsage::Validation: return "validation";
        case DatasetUsage::Benchmark: return "benchmark";
        case DatasetUsage::LeagueEvaluation: return "league_evaluation";
    }
    return "unknown";
}

DatasetUsage parse_dataset_usage(std::string_view value) {
    if (value == "training") return DatasetUsage::Training;
    if (value == "validation") return DatasetUsage::Validation;
    if (value == "benchmark") return DatasetUsage::Benchmark;
    if (value == "league_evaluation") {
        return DatasetUsage::LeagueEvaluation;
    }
    throw std::invalid_argument(
        "unknown dataset usage: " + std::string(value));
}

std::string generate_dataset_id() {
    return "dataset-" + generate_game_ulid();
}

void DatasetRegistry::register_dataset(DatasetEntry entry) {
    validate_entry(entry);
    if (find(entry.dataset_id) != nullptr) {
        throw std::invalid_argument(
            "duplicate dataset_id: " + entry.dataset_id);
    }
    entries_.push_back(std::move(entry));
}

const DatasetEntry* DatasetRegistry::find(
    const std::string& dataset_id) const noexcept {
    for (const auto& entry : entries_) {
        if (entry.dataset_id == dataset_id) return &entry;
    }
    return nullptr;
}

const std::vector<DatasetEntry>& DatasetRegistry::entries() const noexcept {
    return entries_;
}

ResolvedDatasetRecipe DatasetRegistry::resolve(
    const DatasetRecipe& recipe) const {
    validate_recipe_shape(recipe);

    ResolvedDatasetRecipe resolved;
    resolved.recipe_id = recipe.recipe_id;
    resolved.model_id = recipe.model_id;
    resolved.usage = recipe.usage;

    std::unordered_set<std::string> seen_games;
    double retained_ratio_total = 0.0;

    for (const auto& item : recipe.datasets) {
        const DatasetEntry* entry = find(item.dataset_id);
        if (entry == nullptr) {
            throw std::invalid_argument(
                "recipe references unknown dataset: " + item.dataset_id);
        }
        if (entry->usage != recipe.usage) {
            throw std::invalid_argument(
                "recipe usage " +
                std::string(dataset_usage_name(recipe.usage)) +
                " cannot consume dataset " + item.dataset_id +
                " with usage " + dataset_usage_name(entry->usage));
        }

        ResolvedDatasetSlice slice;
        slice.dataset_id = item.dataset_id;
        slice.ratio = item.ratio;
        for (const auto& game_id : entry->provenance.game_ids) {
            if (seen_games.insert(game_id).second) {
                slice.game_ids.push_back(game_id);
            }
        }
        if (slice.game_ids.empty()) continue;
        retained_ratio_total += slice.ratio;
        resolved.datasets.push_back(std::move(slice));
    }

    if (resolved.datasets.empty()) {
        throw std::invalid_argument(
            "recipe contains no unique games after deduplication");
    }

    for (auto& slice : resolved.datasets) {
        slice.ratio /= retained_ratio_total;
        resolved.unique_game_count += slice.game_ids.size();
    }
    return resolved;
}

DatasetEntry derive_dataset(
    const DatasetEntry& parent,
    std::string dataset_id,
    std::string derivation_tag,
    DatasetUsage usage,
    std::string created_at,
    std::vector<std::string> game_ids) {
    validate_entry(parent);
    if (derivation_tag.empty()) {
        throw std::invalid_argument("derivation_tag must not be empty");
    }
    validate_unique_strings(game_ids, "derived game_ids", true);
    for (const auto& game_id : game_ids) {
        if (!contains_game_id(parent.provenance.game_ids, game_id)) {
            throw std::invalid_argument(
                "derived dataset game_id is not present in parent: " +
                game_id);
        }
    }

    DatasetEntry derived;
    derived.dataset_id =
        dataset_id.empty() ? generate_dataset_id() : std::move(dataset_id);
    derived.usage = usage;
    derived.provenance = parent.provenance;
    derived.provenance.source_type = "derived";
    derived.provenance.parent_dataset = parent.dataset_id;
    derived.provenance.created_at = std::move(created_at);
    derived.provenance.game_ids = std::move(game_ids);
    derived.tags = parent.tags;
    if (std::find(
            derived.tags.begin(),
            derived.tags.end(),
            derivation_tag) == derived.tags.end()) {
        derived.tags.push_back(std::move(derivation_tag));
    }
    derived.artifacts = parent.artifacts;
    validate_entry(derived);
    return derived;
}

std::string dataset_entry_to_json(const DatasetEntry& entry) {
    validate_entry(entry);

    std::ostringstream out;
    out << "{\"format\":\"" << kDatasetEntryFormat << "\""
        << ",\"dataset_id\":\"" << escape_json(entry.dataset_id) << "\""
        << ",\"usage\":\"" << dataset_usage_name(entry.usage) << "\""
        << ",\"source_type\":\""
        << escape_json(entry.provenance.source_type) << "\""
        << ",\"board_size\":" << entry.provenance.board_size
        << ",\"generator_model\":";
    write_model_ref(out, entry.provenance.generator_model);
    out << ",\"opponent_model\":";
    write_model_ref(out, entry.provenance.opponent_model);
    out << ",\"parent_dataset\":";
    if (entry.provenance.parent_dataset) {
        out << "\"" << escape_json(*entry.provenance.parent_dataset) << "\"";
    } else {
        out << "null";
    }
    out << ",\"search_config\":\""
        << escape_json(entry.provenance.search_config) << "\""
        << ",\"relabel_history\":";
    write_string_array(out, entry.provenance.relabel_history);
    out << ",\"created_at\":\""
        << escape_json(entry.provenance.created_at) << "\""
        << ",\"license\":\""
        << escape_json(entry.provenance.license) << "\""
        << ",\"game_ids\":";
    write_string_array(out, entry.provenance.game_ids);
    out << ",\"tags\":";
    write_string_array(out, entry.tags);
    out << ",\"artifacts\":";
    write_string_array(out, entry.artifacts);
    out << '}';
    return out.str();
}

DatasetEntry parse_dataset_entry_json(std::string_view json) {
    if (read_string(json, "format") != kDatasetEntryFormat) {
        throw std::invalid_argument(
            "unsupported Dataset Entry format");
    }

    DatasetEntry entry;
    entry.dataset_id = read_string(json, "dataset_id");
    entry.usage = parse_dataset_usage(read_string(json, "usage"));
    entry.provenance.source_type = read_string(json, "source_type");
    entry.provenance.board_size =
        static_cast<std::size_t>(read_uint(json, "board_size"));
    entry.provenance.generator_model =
        parse_model_ref(json, "generator_model");
    entry.provenance.opponent_model =
        parse_model_ref(json, "opponent_model");
    entry.provenance.parent_dataset =
        read_optional_string(json, "parent_dataset");
    entry.provenance.search_config =
        read_string(json, "search_config");
    entry.provenance.relabel_history =
        parse_string_array(read_array(json, "relabel_history"));
    entry.provenance.created_at = read_string(json, "created_at");
    entry.provenance.license = read_string(json, "license");
    entry.provenance.game_ids =
        parse_string_array(read_array(json, "game_ids"));
    entry.tags = parse_string_array(read_array(json, "tags"));
    entry.artifacts =
        parse_string_array(read_array(json, "artifacts"));

    validate_entry(entry);
    return entry;
}

std::string dataset_recipe_to_json(const DatasetRecipe& recipe) {
    validate_recipe_shape(recipe);

    std::ostringstream out;
    out << "{\"format\":\"" << kDatasetRecipeFormat << "\""
        << ",\"recipe_id\":\"" << escape_json(recipe.recipe_id) << "\""
        << ",\"model_id\":\"" << escape_json(recipe.model_id) << "\""
        << ",\"usage\":\"" << dataset_usage_name(recipe.usage) << "\""
        << ",\"datasets\":[";
    for (std::size_t i = 0; i < recipe.datasets.size(); ++i) {
        if (i) out << ',';
        out << "{\"dataset_id\":\""
            << escape_json(recipe.datasets[i].dataset_id)
            << "\",\"ratio\":"
            << std::setprecision(17)
            << recipe.datasets[i].ratio << '}';
    }
    out << "]}";
    return out.str();
}

DatasetRecipe parse_dataset_recipe_json(std::string_view json) {
    if (read_string(json, "format") != kDatasetRecipeFormat) {
        throw std::invalid_argument(
            "unsupported Dataset Recipe format");
    }

    DatasetRecipe recipe;
    recipe.recipe_id = read_string(json, "recipe_id");
    recipe.model_id = read_string(json, "model_id");
    recipe.usage = parse_dataset_usage(read_string(json, "usage"));

    const auto objects =
        parse_object_array(read_array(json, "datasets"));
    for (const auto object : objects) {
        recipe.datasets.push_back(DatasetRecipeItem{
            read_string(object, "dataset_id"),
            read_double(object, "ratio"),
        });
    }

    validate_recipe_shape(recipe);
    return recipe;
}

void write_dataset_registry_jsonl(
    const DatasetRegistry& registry,
    std::ostream& output) {
    for (const auto& entry : registry.entries()) {
        output << dataset_entry_to_json(entry) << '\n';
    }
}

DatasetRegistry read_dataset_registry_jsonl(std::istream& input) {
    DatasetRegistry registry;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        registry.register_dataset(parse_dataset_entry_json(line));
    }
    return registry;
}

std::string metadata_with_training_recipe(
    std::string_view metadata_json,
    const DatasetRecipe& recipe) {
    const std::string recipe_json = dataset_recipe_to_json(recipe);

    std::size_t begin = 0;
    while (begin < metadata_json.size() &&
           std::isspace(static_cast<unsigned char>(metadata_json[begin]))) {
        ++begin;
    }
    std::size_t end = metadata_json.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(metadata_json[end - 1]))) {
        --end;
    }
    if (end <= begin ||
        metadata_json[begin] != '{' ||
        metadata_json[end - 1] != '}') {
        throw std::invalid_argument(
            "model metadata must be a JSON object");
    }
    if (find_value_start_optional(metadata_json, "training_recipe")) {
        throw std::invalid_argument(
            "model metadata already contains training_recipe");
    }

    std::string result(metadata_json.substr(0, end - 1));
    std::size_t content = begin + 1;
    while (content < end - 1 &&
           std::isspace(static_cast<unsigned char>(metadata_json[content]))) {
        ++content;
    }
    if (content < end - 1) result += ',';
    result += "\"training_recipe\":";
    result += recipe_json;
    result += '}';
    result.append(metadata_json.substr(end));
    return result;
}

}  // namespace kadoka::othello
