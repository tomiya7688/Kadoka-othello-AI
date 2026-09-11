#include "kadoka_othello/obake_kadoka.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "kadoka_othello/model_descriptor.hpp"

namespace kadoka::othello {
namespace {

std::string read_all(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("failed to open Obake Kadoka model asset: " + path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

double read_number(const std::string& text, const std::string& key, double fallback) {
    const std::string token = "\"" + key + "\"";
    const std::size_t key_pos = text.find(token);
    if (key_pos == std::string::npos) return fallback;
    const std::size_t colon = text.find(':', key_pos + token.size());
    if (colon == std::string::npos) return fallback;
    std::size_t start = colon + 1;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) ++start;
    std::size_t end = start;
    while (end < text.size() &&
           (std::isdigit(static_cast<unsigned char>(text[end])) ||
            text[end] == '-' || text[end] == '+' || text[end] == '.' ||
            text[end] == 'e' || text[end] == 'E')) {
        ++end;
    }
    return end == start ? fallback : std::stod(text.substr(start, end - start));
}

std::size_t read_size(const std::string& text, const std::string& key, std::size_t fallback) {
    const double value = read_number(text, key, static_cast<double>(fallback));
    return value < 0.0 ? fallback : static_cast<std::size_t>(value);
}

void apply_evaluator_asset(ObakeKadokaConfig& config, const std::string& path) {
    const std::string text = read_all(path);
    config.evaluator_weights.occupied_neighbors = read_number(text, "occupied_neighbors", config.evaluator_weights.occupied_neighbors);
    config.evaluator_weights.empty_neighbors = read_number(text, "empty_neighbors", config.evaluator_weights.empty_neighbors);
    config.evaluator_weights.mixed_color = read_number(text, "mixed_color", config.evaluator_weights.mixed_color);
    config.evaluator_weights.color_transitions = read_number(text, "color_transitions", config.evaluator_weights.color_transitions);
    config.evaluator_weights.line_interest = read_number(text, "line_interest", config.evaluator_weights.line_interest);
    config.evaluator_weights.local_density = read_number(text, "local_density", config.evaluator_weights.local_density);
    config.evaluator_weights.early_center = read_number(text, "early_center", config.evaluator_weights.early_center);
}

void apply_behavior_asset(ObakeKadokaConfig& config, const std::string& path) {
    const std::string text = read_all(path);
    config.recent_retry_penalty = read_number(text, "recent_retry_penalty", config.recent_retry_penalty);
    config.inferred_illegal_retry_penalty = read_number(text, "inferred_illegal_retry_penalty", config.inferred_illegal_retry_penalty);
    config.exploration_floor = read_number(text, "exploration_floor", config.exploration_floor);
    config.randomizer_temperature = read_number(text, "randomizer_temperature", config.randomizer_temperature);
    config.memory_depth = std::min<std::size_t>(read_size(text, "memory_depth", config.memory_depth), 2U);
}

}  // namespace

ObakeKadokaConfig load_obake_kadoka_model(const std::string& model_root_path) {
    const ModelRootDescriptor descriptor = load_model_root_descriptor(model_root_path);
    if (descriptor.model_id != "kadoka.obake_kadoka") {
        throw std::runtime_error("unexpected model id for Obake Kadoka: " + descriptor.model_id);
    }

    ObakeKadokaConfig config;

    const std::string evaluator_path = find_model_asset_path(descriptor, "evaluator");
    const std::string behavior_path = find_model_asset_path(descriptor, "behavior");

    if (evaluator_path.empty()) throw std::runtime_error("Obake Kadoka model requires evaluator asset");
    if (behavior_path.empty()) throw std::runtime_error("Obake Kadoka model requires behavior asset");

    apply_evaluator_asset(config, evaluator_path);
    apply_behavior_asset(config, behavior_path);
    return config;
}

}  // namespace kadoka::othello
