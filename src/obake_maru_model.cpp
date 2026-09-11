#include "kadoka_othello/obake_maru.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "kadoka_othello/model_descriptor.hpp"

namespace kadoka::othello {
namespace {

std::string read_all(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("failed to open Obake Maru model asset: " + path);
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

void apply_evaluator_asset(ObakeMaruConfig& config, const std::string& path) {
    const std::string text = read_all(path);
    config.occupied_neighbor_weight = read_number(text, "occupied_neighbor_weight", config.occupied_neighbor_weight);
    config.local_cluster_weight = read_number(text, "local_cluster_weight", config.local_cluster_weight);
    config.mixed_color_weight = read_number(text, "mixed_color_weight", config.mixed_color_weight);
    config.center_bias_weight = read_number(text, "center_bias_weight", config.center_bias_weight);
}

void apply_behavior_asset(ObakeMaruConfig& config, const std::string& path) {
    const std::string text = read_all(path);
    config.exploration_floor = read_number(text, "exploration_floor", config.exploration_floor);
    config.randomizer_temperature = read_number(text, "randomizer_temperature", config.randomizer_temperature);
    config.rejected_retry_penalty = read_number(text, "rejected_retry_penalty", config.rejected_retry_penalty);
}

}  // namespace

ObakeMaruConfig load_obake_maru_model(const std::string& model_root_path) {
    const ModelRootDescriptor descriptor = load_model_root_descriptor(model_root_path);
    if (descriptor.model_id != "kadoka.obake_maru") {
        throw std::runtime_error("unexpected model id for Obake Maru: " + descriptor.model_id);
    }

    ObakeMaruConfig config;
    const std::string evaluator_path = find_model_asset_path(descriptor, "evaluator");
    const std::string behavior_path = find_model_asset_path(descriptor, "behavior");

    if (evaluator_path.empty()) throw std::runtime_error("Obake Maru model requires evaluator asset");
    if (behavior_path.empty()) throw std::runtime_error("Obake Maru model requires behavior asset");

    apply_evaluator_asset(config, evaluator_path);
    apply_behavior_asset(config, behavior_path);
    return config;
}

}  // namespace kadoka::othello
