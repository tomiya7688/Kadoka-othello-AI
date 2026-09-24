#include "kadoka_othello/confidence.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace kadoka::othello {
namespace {

double clamp01(double value) noexcept {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

double scaled_confidence(
    const ConfidenceFactors& factors,
    std::string_view key,
    double scale,
    bool inverse = false) {
    const auto value = factors.get(key);
    if (!value) return std::numeric_limits<double>::quiet_NaN();
    if (scale <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    const double normalized = clamp01(*value / scale);
    return inverse ? 1.0 - normalized : normalized;
}

void add_if_present(
    double value,
    double& total,
    std::size_t& count) {
    if (!std::isfinite(value)) return;
    total += clamp01(value);
    ++count;
}

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

std::size_t find_value_start(
    std::string_view json,
    std::string_view key) {
    const std::string token = "\"" + std::string(key) + "\"";
    std::size_t search_from = 0;
    while (true) {
        const std::size_t key_pos = json.find(token, search_from);
        if (key_pos == std::string_view::npos) {
            throw std::invalid_argument(
                "confidence JSON missing field: " +
                std::string(key));
        }
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

std::string parse_string_at(
    std::string_view json,
    std::size_t pos) {
    if (pos >= json.size() || json[pos] != '"') {
        throw std::invalid_argument(
            "confidence JSON expected string");
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
            throw std::invalid_argument(
                "confidence JSON unterminated escape");
        }
        switch (json[pos]) {
            case '\\': result += '\\'; break;
            case '"': result += '"'; break;
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            default:
                throw std::invalid_argument(
                    "confidence JSON unsupported escape");
        }
    }
    throw std::invalid_argument(
        "confidence JSON unterminated string");
}

std::string read_string(
    std::string_view json,
    std::string_view key) {
    return parse_string_at(json, find_value_start(json, key));
}

std::uint64_t read_uint(
    std::string_view json,
    std::string_view key) {
    std::size_t pos = find_value_start(json, key);
    const std::size_t begin = pos;
    while (pos < json.size() &&
           std::isdigit(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    if (pos == begin) {
        throw std::invalid_argument(
            "confidence JSON expected unsigned integer: " +
            std::string(key));
    }
    return std::stoull(std::string(json.substr(begin, pos - begin)));
}

std::string_view read_object(
    std::string_view json,
    std::string_view key) {
    const std::size_t start = find_value_start(json, key);
    if (start >= json.size() || json[start] != '{') {
        throw std::invalid_argument(
            "confidence JSON expected object: " +
            std::string(key));
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
        if (ch == '{') ++depth;
        else if (ch == '}') {
            if (--depth == 0) {
                return json.substr(start, pos - start + 1);
            }
        }
    }
    throw std::invalid_argument(
        "confidence JSON object is not closed");
}

ConfidenceFactors parse_factor_object(std::string_view object) {
    ConfidenceFactors factors;
    std::size_t pos = 1;
    while (pos + 1 < object.size()) {
        while (pos + 1 < object.size() &&
               (std::isspace(static_cast<unsigned char>(object[pos])) ||
                object[pos] == ',')) {
            ++pos;
        }
        if (pos + 1 >= object.size() || object[pos] == '}') break;

        const std::string key = parse_string_at(object, pos);
        pos = object.find(':', pos);
        if (pos == std::string_view::npos) {
            throw std::invalid_argument(
                "confidence factors missing colon");
        }
        ++pos;
        while (pos < object.size() &&
               std::isspace(static_cast<unsigned char>(object[pos]))) {
            ++pos;
        }
        const std::size_t begin = pos;
        while (pos < object.size()) {
            const char ch = object[pos];
            if (!(std::isdigit(static_cast<unsigned char>(ch)) ||
                  ch == '-' || ch == '+' || ch == '.' ||
                  ch == 'e' || ch == 'E')) {
                break;
            }
            ++pos;
        }
        if (begin == pos) {
            throw std::invalid_argument(
                "confidence factor must be numeric: " + key);
        }
        factors.set(
            key,
            std::stod(std::string(object.substr(begin, pos - begin))));
    }
    return factors;
}

double factor_or(
    const ConfidenceFactors& factors,
    std::string_view key,
    double fallback) {
    const auto value = factors.get(key);
    return value ? *value : fallback;
}

}  // namespace

void ConfidenceFactors::set(
    std::string key,
    double value) {
    if (key.empty()) {
        throw std::invalid_argument(
            "confidence factor key must not be empty");
    }
    if (!std::isfinite(value)) {
        throw std::invalid_argument(
            "confidence factor must be finite: " + key);
    }

    for (auto& entry : entries_) {
        if (entry.first == key) {
            entry.second = value;
            return;
        }
    }
    entries_.push_back({std::move(key), value});
}

std::optional<double> ConfidenceFactors::get(
    std::string_view key) const noexcept {
    for (const auto& entry : entries_) {
        if (entry.first == key) return entry.second;
    }
    return std::nullopt;
}

const std::vector<std::pair<std::string, double>>&
ConfidenceFactors::entries() const noexcept {
    return entries_;
}

std::string BaselineConfidenceCalculator::id() const {
    return "kadoka.confidence.baseline.v1";
}

ConfidenceEstimate BaselineConfidenceCalculator::evaluate(
    const ConfidenceSample& sample,
    const ConfidenceBoardParameters& parameters) const {
    if (sample.board_size != parameters.board_size) {
        throw std::invalid_argument(
            "confidence sample/parameter board size mismatch");
    }

    double evidence_total = 0.0;
    std::size_t evidence_count = 0;

    add_if_present(
        scaled_confidence(
            sample.factors,
            confidence_factor::kSourceRatingDeviation,
            350.0,
            true),
        evidence_total,
        evidence_count);
    add_if_present(
        scaled_confidence(
            sample.factors,
            confidence_factor::kSearchDepth,
            parameters.depth_scale),
        evidence_total,
        evidence_count);
    add_if_present(
        scaled_confidence(
            sample.factors,
            confidence_factor::kNodes,
            parameters.nodes_scale),
        evidence_total,
        evidence_count);
    add_if_present(
        scaled_confidence(
            sample.factors,
            confidence_factor::kSimulations,
            parameters.simulations_scale),
        evidence_total,
        evidence_count);
    add_if_present(
        scaled_confidence(
            sample.factors,
            confidence_factor::kTopCandidateGap,
            parameters.candidate_gap_scale),
        evidence_total,
        evidence_count);
    add_if_present(
        scaled_confidence(
            sample.factors,
            confidence_factor::kPolicyEntropy,
            1.0,
            true),
        evidence_total,
        evidence_count);
    add_if_present(
        scaled_confidence(
            sample.factors,
            confidence_factor::kMultiAiMoveDisagreement,
            1.0,
            true),
        evidence_total,
        evidence_count);
    add_if_present(
        scaled_confidence(
            sample.factors,
            confidence_factor::kMultiAiValueDisagreement,
            parameters.value_disagreement_scale,
            true),
        evidence_total,
        evidence_count);
    add_if_present(
        scaled_confidence(
            sample.factors,
            confidence_factor::kEvalModelDisagreement,
            parameters.eval_disagreement_scale,
            true),
        evidence_total,
        evidence_count);
    add_if_present(
        scaled_confidence(
            sample.factors,
            confidence_factor::kMonteCarloVariance,
            parameters.monte_carlo_variance_scale,
            true),
        evidence_total,
        evidence_count);

    if (const auto exact = sample.factors.get(
            confidence_factor::kExactEndgameAgreement)) {
        add_if_present(
            *exact,
            evidence_total,
            evidence_count);
    }
    if (const auto result = sample.factors.get(
            confidence_factor::kResultConsistency)) {
        add_if_present(
            *result,
            evidence_total,
            evidence_count);
    }

    ConfidenceEstimate estimate;
    estimate.confidence = evidence_count == 0
        ? 0.0
        : clamp01(
            evidence_total /
            static_cast<double>(evidence_count));
    estimate.uncertainty = 1.0 - estimate.confidence;
    estimate.reanalysis_priority = estimate.uncertainty;

    if (const auto disagreement = sample.factors.get(
            confidence_factor::kMultiAiMoveDisagreement)) {
        estimate.reanalysis_priority += 0.75 * clamp01(*disagreement);
    }
    if (const auto disagreement = sample.factors.get(
            confidence_factor::kMultiAiValueDisagreement)) {
        estimate.reanalysis_priority +=
            0.5 * clamp01(
                *disagreement /
                parameters.value_disagreement_scale);
    }
    if (const auto variance = sample.factors.get(
            confidence_factor::kMonteCarloVariance)) {
        estimate.reanalysis_priority +=
            0.5 * clamp01(
                *variance /
                parameters.monte_carlo_variance_scale);
    }
    if (const auto entropy = sample.factors.get(
            confidence_factor::kPolicyEntropy)) {
        estimate.reanalysis_priority +=
            0.5 * clamp01(*entropy);
    }
    if (const auto gap = sample.factors.get(
            confidence_factor::kTopCandidateGap)) {
        estimate.reanalysis_priority +=
            0.5 * (
                1.0 -
                clamp01(
                    *gap /
                    parameters.candidate_gap_scale));
    }

    if (sample.ply >= parameters.midgame_ply_begin &&
        sample.ply <= parameters.midgame_ply_end) {
        estimate.reanalysis_priority += 0.25;
    }

    const double empty_count = factor_or(
        sample.factors,
        confidence_factor::kEmptyCount,
        std::numeric_limits<double>::infinity());
    const bool exact_by_size =
        std::isfinite(empty_count) &&
        empty_count <= static_cast<double>(
            parameters.exact_endgame_empty_threshold);
    const bool exact_reported =
        factor_or(
            sample.factors,
            confidence_factor::kExactEndgameAvailable,
            0.0) >= 0.5;
    estimate.exact_endgame_candidate =
        exact_by_size || exact_reported;

    if (estimate.exact_endgame_candidate) {
        estimate.reanalysis_priority += 0.75;
        if (!sample.factors.get(
                confidence_factor::kExactEndgameAgreement)) {
            estimate.reanalysis_priority += 0.5;
        }
    }

    if (const auto exact = sample.factors.get(
            confidence_factor::kExactEndgameAgreement)) {
        estimate.reanalysis_priority +=
            1.0 - clamp01(*exact);
    }

    return estimate;
}

const ConfidenceBoardParameters&
find_confidence_board_parameters(
    const std::vector<ConfidenceBoardParameters>& parameters,
    std::size_t board_size) {
    for (const auto& item : parameters) {
        if (item.board_size == board_size) return item;
    }
    throw std::invalid_argument(
        "missing confidence parameters for board size " +
        std::to_string(board_size));
}

std::vector<ConfidenceBoardParameters>
default_confidence_board_parameters() {
    ConfidenceBoardParameters six;
    six.board_size = 6;
    six.exact_endgame_empty_threshold = 12;
    six.midgame_ply_begin = 8;
    six.midgame_ply_end = 24;
    six.depth_scale = 10.0;
    six.nodes_scale = 50000.0;
    six.simulations_scale = 5000.0;

    ConfidenceBoardParameters eight;
    eight.board_size = 8;
    eight.exact_endgame_empty_threshold = 14;
    eight.midgame_ply_begin = 14;
    eight.midgame_ply_end = 46;
    eight.depth_scale = 14.0;
    eight.nodes_scale = 150000.0;
    eight.simulations_scale = 15000.0;

    ConfidenceBoardParameters ten;
    ten.board_size = 10;
    ten.exact_endgame_empty_threshold = 10;
    ten.midgame_ply_begin = 20;
    ten.midgame_ply_end = 74;
    ten.depth_scale = 16.0;
    ten.nodes_scale = 250000.0;
    ten.simulations_scale = 25000.0;

    return {six, eight, ten};
}

std::vector<ReanalysisSelection>
select_reanalysis_samples(
    const std::vector<ConfidenceSample>& samples,
    const IConfidenceCalculator& calculator,
    const std::vector<ConfidenceBoardParameters>& parameters,
    std::size_t max_samples,
    std::uint64_t seed) {
    if (max_samples == 0 || samples.empty()) return {};

    struct Candidate {
        std::size_t index{};
        ConfidenceEstimate estimate;
        const ConfidenceBoardParameters* parameters{};
        bool audited{false};
    };

    std::vector<Candidate> candidates;
    candidates.reserve(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto& board_parameters =
            find_confidence_board_parameters(
                parameters,
                samples[i].board_size);
        candidates.push_back(Candidate{
            i,
            calculator.evaluate(samples[i], board_parameters),
            &board_parameters,
            false,
        });
    }

    std::mt19937_64 rng(seed == 0 ? 1 : seed);
    std::vector<ReanalysisSelection> selected;
    selected.reserve(std::min(max_samples, samples.size()));
    std::vector<bool> used(samples.size(), false);

    // Audit high-confidence samples independently before priority filling.
    for (Candidate& candidate : candidates) {
        if (selected.size() >= max_samples) break;
        if (candidate.estimate.confidence <
            candidate.parameters->high_confidence_threshold) {
            continue;
        }
        const double rate = clamp01(
            candidate.parameters->high_confidence_audit_rate);
        std::bernoulli_distribution audit(rate);
        if (!audit(rng)) continue;

        candidate.audited = true;
        used[candidate.index] = true;
        selected.push_back(ReanalysisSelection{
            candidate.index,
            candidate.estimate,
            true,
        });
    }

    std::stable_sort(
        candidates.begin(),
        candidates.end(),
        [](const Candidate& lhs, const Candidate& rhs) {
            if (lhs.estimate.reanalysis_priority !=
                rhs.estimate.reanalysis_priority) {
                return lhs.estimate.reanalysis_priority >
                    rhs.estimate.reanalysis_priority;
            }
            return lhs.index < rhs.index;
        });

    for (const Candidate& candidate : candidates) {
        if (selected.size() >= max_samples) break;
        if (used[candidate.index]) continue;
        used[candidate.index] = true;
        selected.push_back(ReanalysisSelection{
            candidate.index,
            candidate.estimate,
            false,
        });
    }

    return selected;
}

double normalized_policy_entropy(
    const std::vector<double>& policy) {
    if (policy.size() <= 1) return 0.0;

    double sum = 0.0;
    for (const double value : policy) {
        if (!std::isfinite(value) || value < 0.0) {
            throw std::invalid_argument(
                "policy values must be finite and non-negative");
        }
        sum += value;
    }
    if (sum <= 0.0) {
        throw std::invalid_argument(
            "policy sum must be greater than zero");
    }

    double entropy = 0.0;
    for (const double value : policy) {
        if (value <= 0.0) continue;
        const double probability = value / sum;
        entropy -= probability * std::log(probability);
    }
    return clamp01(
        entropy /
        std::log(static_cast<double>(policy.size())));
}

double move_disagreement(
    const std::vector<Position>& moves) {
    if (moves.size() <= 1) return 0.0;

    std::vector<std::size_t> counts;
    std::vector<Position> unique;
    for (const Position move : moves) {
        bool found = false;
        for (std::size_t i = 0; i < unique.size(); ++i) {
            if (unique[i] == move) {
                ++counts[i];
                found = true;
                break;
            }
        }
        if (!found) {
            unique.push_back(move);
            counts.push_back(1);
        }
    }

    const std::size_t maximum =
        *std::max_element(counts.begin(), counts.end());
    return 1.0 -
        static_cast<double>(maximum) /
        static_cast<double>(moves.size());
}

double value_disagreement_stddev(
    const std::vector<double>& values) {
    if (values.size() <= 1) return 0.0;
    double mean = 0.0;
    for (const double value : values) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument(
                "engine values must be finite");
        }
        mean += value;
    }
    mean /= static_cast<double>(values.size());

    double squared = 0.0;
    for (const double value : values) {
        const double delta = value - mean;
        squared += delta * delta;
    }
    return std::sqrt(
        squared / static_cast<double>(values.size()));
}

double sample_variance(
    const std::vector<double>& values) {
    if (values.size() <= 1) return 0.0;
    double mean = 0.0;
    for (const double value : values) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument(
                "sample values must be finite");
        }
        mean += value;
    }
    mean /= static_cast<double>(values.size());

    double squared = 0.0;
    for (const double value : values) {
        const double delta = value - mean;
        squared += delta * delta;
    }
    return squared /
        static_cast<double>(values.size() - 1);
}

std::string confidence_sample_to_json(
    const ConfidenceSample& sample) {
    if (sample.game_id.empty()) {
        throw std::invalid_argument(
            "confidence sample requires game_id");
    }
    if (sample.board_size != 6 &&
        sample.board_size != 8 &&
        sample.board_size != 10) {
        throw std::invalid_argument(
            "confidence sample board_size must be 6, 8 or 10");
    }

    std::ostringstream out;
    out.precision(17);
    out << "{\"format\":\"" << kConfidenceSampleFormat << "\""
        << ",\"game_id\":\"" << escape_json(sample.game_id) << "\""
        << ",\"ply\":" << sample.ply
        << ",\"board_size\":" << sample.board_size
        << ",\"factors\":{";

    const auto& entries = sample.factors.entries();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (i) out << ',';
        out << "\"" << escape_json(entries[i].first)
            << "\":" << entries[i].second;
    }
    out << "}}";
    return out.str();
}

ConfidenceSample parse_confidence_sample_json(
    std::string_view json) {
    if (read_string(json, "format") !=
        kConfidenceSampleFormat) {
        throw std::invalid_argument(
            "unsupported confidence sample format");
    }

    ConfidenceSample sample;
    sample.game_id = read_string(json, "game_id");
    sample.ply = static_cast<std::size_t>(
        read_uint(json, "ply"));
    sample.board_size = static_cast<std::size_t>(
        read_uint(json, "board_size"));
    sample.factors =
        parse_factor_object(read_object(json, "factors"));

    if (sample.game_id.empty()) {
        throw std::invalid_argument(
            "confidence sample requires game_id");
    }
    if (sample.board_size != 6 &&
        sample.board_size != 8 &&
        sample.board_size != 10) {
        throw std::invalid_argument(
            "confidence sample board_size must be 6, 8 or 10");
    }
    return sample;
}

}  // namespace kadoka::othello
