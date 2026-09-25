#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "kadoka_othello/confidence.hpp"
#include "kadoka_othello/game_record.hpp"
#include "kadoka_othello/relabel.hpp"

namespace {

using namespace kadoka::othello;

std::string key_for(
    const std::string& game_id,
    std::size_t ply) {
    return game_id + "#" + std::to_string(ply);
}

std::vector<BoardStateRecord> load_board_states(
    const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error(
            "failed to open BoardState JSONL: " + path);
    }

    std::vector<BoardStateRecord> records;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        records.push_back(
            parse_board_state_record_json(line));
    }
    return records;
}

std::unordered_map<std::string, RelabelOriginalLabel>
load_original_labels(
    const std::string& path) {
    std::unordered_map<
        std::string,
        RelabelOriginalLabel> labels;
    if (path == "-") return labels;

    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error(
            "failed to open GameAux JSONL: " + path);
    }

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        const GameAuxRecord event =
            parse_game_aux_record_json(line);
        if (event.event_type != "accepted_move" ||
            event.ply == 0 ||
            !event.action ||
            event.action->type !=
                GameAuxActionType::Move ||
            !event.action->move) {
            continue;
        }

        RelabelOriginalLabel label;
        label.selected_move =
            event.action->move;
        label.source =
            "game_aux:accepted_move";
        labels[key_for(
            event.game_id,
            event.ply - 1)] =
            std::move(label);
    }
    return labels;
}

std::unordered_map<std::string, ConfidenceSample>
load_confidence(
    const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error(
            "failed to open confidence JSONL: " + path);
    }

    std::unordered_map<
        std::string,
        ConfidenceSample> samples;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        ConfidenceSample sample =
            parse_confidence_sample_json(line);
        const std::string key =
            key_for(sample.game_id, sample.ply);
        if (!samples.emplace(
                key,
                std::move(sample)).second) {
            throw std::runtime_error(
                "duplicate confidence sample: " + key);
        }
    }
    return samples;
}

std::vector<std::string> split(
    const std::string& value,
    char delimiter) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t pos =
            value.find(delimiter, start);
        if (pos == std::string::npos) {
            parts.push_back(value.substr(start));
            break;
        }
        parts.push_back(
            value.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

RelabelEngineSpec parse_engine_spec(
    const std::string& text) {
    const auto parts = split(text, ',');
    if (parts.empty() || parts[0].empty()) {
        throw std::invalid_argument(
            "engine spec requires manifest path");
    }
    if (parts.size() > 5) {
        throw std::invalid_argument(
            "engine spec accepts at most 5 comma-separated fields");
    }

    RelabelEngineSpec spec;
    spec.manifest_path = parts[0];
    if (parts.size() >= 2 && !parts[1].empty()) {
        spec.rating = std::strtod(
            parts[1].c_str(), nullptr);
    }
    if (parts.size() >= 3 && !parts[2].empty()) {
        spec.rating_deviation = std::strtod(
            parts[2].c_str(), nullptr);
    }
    if (parts.size() >= 4 && !parts[3].empty()) {
        spec.seed = std::strtoull(
            parts[3].c_str(), nullptr, 10);
    }
    if (parts.size() >= 5) {
        spec.search_config = parts[4];
    }
    return spec;
}

void print_usage() {
    std::cout
        << "Kadoka Multi-engine Relabel Tool\n"
        << "  kadoka_relabel_tool "
        << "<board-state.jsonl> <game-aux.jsonl|-> "
        << "<confidence.jsonl> <output.jsonl> "
        << "<source-dataset-id|-> "
        << "<max-positions> <max-engine-calls> "
        << "<max-exact-nodes> <max-elapsed-ms> "
        << "<all|top-k> <top-k> <sampler-seed> "
        << "[manifest[,rating,rd,seed,search-config] ...]\n";
}

}  // namespace

int main(int argc, char** argv) {
    using namespace kadoka::othello;

    if (argc < 13) {
        print_usage();
        return 2;
    }

    try {
        const auto board_states =
            load_board_states(argv[1]);
        const auto original_labels =
            load_original_labels(argv[2]);
        const auto confidence =
            load_confidence(argv[3]);

        std::vector<RelabelInput> inputs;
        inputs.reserve(board_states.size());
        const std::string source_dataset_id =
            std::string(argv[5]) == "-"
                ? std::string{}
                : std::string(argv[5]);

        for (const auto& state : board_states) {
            const std::string key =
                key_for(state.game_id, state.ply);
            const auto confidence_it =
                confidence.find(key);
            if (confidence_it ==
                confidence.end()) {
                continue;
            }

            RelabelOriginalLabel before;
            const auto before_it =
                original_labels.find(key);
            if (before_it !=
                original_labels.end()) {
                before = before_it->second;
            }

            RelabelInput input;
            input.position =
                make_relabel_position(
                    state,
                    std::move(before),
                    source_dataset_id);
            input.confidence =
                confidence_it->second;
            inputs.push_back(std::move(input));
        }

        if (inputs.empty()) {
            throw std::runtime_error(
                "no BoardState rows matched confidence samples");
        }

        RelabelBudget budget;
        budget.max_positions =
            static_cast<std::size_t>(
                std::strtoull(
                    argv[6], nullptr, 10));
        budget.max_engine_calls =
            static_cast<std::size_t>(
                std::strtoull(
                    argv[7], nullptr, 10));
        budget.max_exact_nodes =
            std::strtoull(
                argv[8], nullptr, 10);
        budget.max_elapsed_ms =
            std::strtoull(
                argv[9], nullptr, 10);
        const std::string candidate_mode =
            argv[10];
        if (candidate_mode == "all") {
            budget.candidate_mode =
                RelabelCandidateMode::AllLegal;
        } else if (candidate_mode == "top-k") {
            budget.candidate_mode =
                RelabelCandidateMode::TopK;
        } else {
            throw std::invalid_argument(
                "candidate mode must be all or top-k");
        }
        budget.top_k =
            static_cast<std::size_t>(
                std::strtoull(
                    argv[11], nullptr, 10));
        const std::uint64_t sampler_seed =
            std::strtoull(
                argv[12], nullptr, 10);

        std::vector<RelabelEngineSpec> engines;
        for (int i = 13; i < argc; ++i) {
            engines.push_back(
                parse_engine_spec(argv[i]));
        }

        BaselineConfidenceCalculator calculator;
        const RelabelBatchResult result =
            relabel_positions(
                inputs,
                engines,
                calculator,
                default_confidence_board_parameters(),
                default_relabel_board_parameters(),
                budget,
                sampler_seed);

        std::ofstream output(
            argv[4],
            std::ios::out | std::ios::trunc);
        if (!output) {
            throw std::runtime_error(
                "failed to create relabel output");
        }
        write_relabel_jsonl(
            result,
            output);

        std::cout
            << "matched_inputs=" << inputs.size()
            << " selected_positions="
            << result.selected_positions
            << " records=" << result.records.size()
            << " engine_calls="
            << result.engine_calls
            << " exact_nodes="
            << result.exact_nodes
            << " engine_budget_exhausted="
            << (result.engine_budget_exhausted
                    ? "yes" : "no")
            << " exact_budget_exhausted="
            << (result.exact_node_budget_exhausted
                    ? "yes" : "no")
            << " elapsed_budget_exhausted="
            << (result.elapsed_budget_exhausted
                    ? "yes" : "no")
            << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Relabel Tool error: "
                  << error.what() << '\n';
        return 1;
    }

    return 0;
}
