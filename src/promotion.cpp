#include "kadoka_othello/promotion.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
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

bool supported_board_size(std::size_t size) noexcept {
    return size == 6 || size == 8 || size == 10;
}

void validate_profile(const PromotionProfile& profile) {
    if (profile.profile_id.empty()) {
        throw std::invalid_argument(
            "promotion profile_id must not be empty");
    }
    if (profile.board_sizes.empty()) {
        throw std::invalid_argument(
            "promotion profile requires board_sizes");
    }
    if (profile.games_per_color == 0) {
        throw std::invalid_argument(
            "promotion games_per_color must be greater than zero");
    }
    if (!std::isfinite(profile.confidence_z) ||
        profile.confidence_z < 0.0) {
        throw std::invalid_argument(
            "promotion confidence_z must be finite and non-negative");
    }

    std::unordered_set<std::size_t> sizes;
    for (const std::size_t size : profile.board_sizes) {
        if (!supported_board_size(size)) {
            throw std::invalid_argument(
                "promotion board size must be 6, 8 or 10");
        }
        if (!sizes.insert(size).second) {
            throw std::invalid_argument(
                "promotion profile contains duplicate board size");
        }
    }

    if (profile.thresholds.min_games_per_board == 0) {
        throw std::invalid_argument(
            "promotion min_games_per_board must be greater than zero");
    }
    if (!std::isfinite(profile.thresholds.min_score_rate) ||
        profile.thresholds.min_score_rate < 0.0 ||
        profile.thresholds.min_score_rate > 1.0) {
        throw std::invalid_argument(
            "promotion min_score_rate must be within 0..1");
    }
    if (!std::isfinite(
            profile.thresholds.min_confidence_lower_bound) ||
        profile.thresholds.min_confidence_lower_bound < 0.0 ||
        profile.thresholds.min_confidence_lower_bound > 1.0) {
        throw std::invalid_argument(
            "promotion confidence lower bound must be within 0..1");
    }

    std::unordered_set<std::string> tuning;
    for (const auto& target : profile.tuning_targets) {
        if (target.empty() ||
            !tuning.insert(target).second) {
            throw std::invalid_argument(
                "promotion tuning targets must be unique non-empty strings");
        }
    }
}

void validate_generation(
    const ModelGeneration& generation,
    const PromotionProfile& profile,
    const LeagueRegistry& league_registry) {
    if (generation.generation_id.empty()) {
        throw std::invalid_argument(
            "model generation_id must not be empty");
    }
    if (generation.profile_id != profile.profile_id) {
        throw std::invalid_argument(
            "model generation profile_id mismatch");
    }
    if (generation.participant_ids.size() !=
        profile.board_sizes.size()) {
        throw std::invalid_argument(
            "model generation must have one participant per profile board size");
    }

    std::unordered_set<std::string> participant_ids;
    std::unordered_set<std::size_t> board_sizes;
    for (const auto& participant_id :
         generation.participant_ids) {
        if (!participant_ids.insert(participant_id).second) {
            throw std::invalid_argument(
                "model generation contains duplicate participant_id");
        }
        const LeagueRegistryEntry* entry =
            league_registry.find(participant_id);
        if (entry == nullptr) {
            throw std::invalid_argument(
                "model generation participant is missing from League Registry: " +
                participant_id);
        }
        board_sizes.insert(
            entry->participant.config.board_size);
    }

    for (const std::size_t size : profile.board_sizes) {
        if (board_sizes.find(size) == board_sizes.end()) {
            throw std::invalid_argument(
                "model generation is missing profile board size " +
                std::to_string(size));
        }
    }
}

std::size_t registry_index_for_participant(
    const LeagueRegistry& registry,
    const std::string& participant_id) {
    const auto& entries = registry.entries();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].participant.participant_id ==
            participant_id) {
            return i;
        }
    }
    throw std::invalid_argument(
        "participant is missing from League Registry: " +
        participant_id);
}

const LeagueRegistryEntry& generation_entry_for_board(
    const ModelGeneration& generation,
    const LeagueRegistry& registry,
    std::size_t board_size) {
    const LeagueRegistryEntry* found = nullptr;
    for (const auto& participant_id :
         generation.participant_ids) {
        const LeagueRegistryEntry* entry =
            registry.find(participant_id);
        if (entry == nullptr) {
            throw std::invalid_argument(
                "generation participant disappeared from League Registry");
        }
        if (entry->participant.config.board_size != board_size) {
            continue;
        }
        if (found != nullptr) {
            throw std::invalid_argument(
                "generation has multiple participants for one board size");
        }
        found = entry;
    }
    if (found == nullptr) {
        throw std::invalid_argument(
            "generation has no participant for board size " +
            std::to_string(board_size));
    }
    return *found;
}

double candidate_score(
    const LeagueGameRecord& game,
    const std::string& candidate_id) {
    const bool candidate_black =
        game.black_participant_id == candidate_id;
    const bool candidate_white =
        game.white_participant_id == candidate_id;
    if (!candidate_black && !candidate_white) {
        throw std::invalid_argument(
            "promotion game does not contain candidate");
    }

    if (game.outcome == LeagueGameOutcome::Draw) {
        return 0.5;
    }
    const bool candidate_won =
        (candidate_black &&
         game.outcome == LeagueGameOutcome::BlackWin) ||
        (candidate_white &&
         game.outcome == LeagueGameOutcome::WhiteWin);
    return candidate_won ? 1.0 : 0.0;
}

int candidate_disc_difference(
    const LeagueGameRecord& game,
    const std::string& candidate_id) {
    if (game.black_participant_id == candidate_id) {
        return static_cast<int>(game.black_discs) -
            static_cast<int>(game.white_discs);
    }
    if (game.white_participant_id == candidate_id) {
        return static_cast<int>(game.white_discs) -
            static_cast<int>(game.black_discs);
    }
    throw std::invalid_argument(
        "promotion game does not contain candidate");
}

const HeadlessAIMetrics& candidate_metrics(
    const LeagueGameRecord& game,
    const std::string& candidate_id) {
    if (game.black_participant_id == candidate_id) {
        return game.metrics.black_ai_metrics;
    }
    if (game.white_participant_id == candidate_id) {
        return game.metrics.white_ai_metrics;
    }
    throw std::invalid_argument(
        "promotion game does not contain candidate");
}

std::pair<double, double> score_confidence_interval(
    const std::vector<double>& scores,
    double z) {
    if (scores.empty()) return {0.0, 1.0};

    double mean = 0.0;
    for (const double score : scores) mean += score;
    mean /= static_cast<double>(scores.size());

    if (scores.size() == 1 || z == 0.0) {
        return {mean, mean};
    }

    double squared = 0.0;
    for (const double score : scores) {
        const double delta = score - mean;
        squared += delta * delta;
    }
    const double variance =
        squared /
        static_cast<double>(scores.size() - 1);
    const double standard_error =
        std::sqrt(
            variance /
            static_cast<double>(scores.size()));
    return {
        std::max(0.0, mean - z * standard_error),
        std::min(1.0, mean + z * standard_error),
    };
}

PromotionBoardMetrics aggregate_board_metrics(
    std::size_t board_size,
    const std::string& candidate_id,
    const LeagueRegistryEntry& candidate_after,
    const LeagueRegistryEntry& champion_after,
    const std::vector<LeagueGameRecord>& games,
    double confidence_z) {
    if (games.empty()) {
        throw std::invalid_argument(
            "promotion evaluation produced no games");
    }

    PromotionBoardMetrics metrics;
    metrics.board_size = board_size;
    metrics.games = games.size();
    metrics.min_disc_difference =
        std::numeric_limits<int>::max();
    metrics.max_disc_difference =
        std::numeric_limits<int>::min();

    std::vector<double> scores;
    double black_score = 0.0;
    double white_score = 0.0;
    double disc_total = 0.0;
    double think_total = 0.0;
    std::size_t think_calls = 0;
    std::uint64_t nodes = 0;
    std::uint64_t simulations = 0;
    std::size_t node_reports = 0;
    std::size_t simulation_reports = 0;

    for (const auto& game : games) {
        const double score =
            candidate_score(game, candidate_id);
        scores.push_back(score);

        if (game.black_participant_id == candidate_id) {
            ++metrics.candidate_black_games;
            black_score += score;
        } else {
            ++metrics.candidate_white_games;
            white_score += score;
        }

        const int disc_difference =
            candidate_disc_difference(
                game,
                candidate_id);
        disc_total +=
            static_cast<double>(disc_difference);
        metrics.min_disc_difference =
            std::min(
                metrics.min_disc_difference,
                disc_difference);
        metrics.max_disc_difference =
            std::max(
                metrics.max_disc_difference,
                disc_difference);

        const auto& ai =
            candidate_metrics(game, candidate_id);
        think_total += ai.total_think_us;
        think_calls += ai.calls;
        nodes += ai.total_nodes;
        simulations += ai.total_simulations;
        node_reports += ai.node_reports;
        simulation_reports +=
            ai.simulation_reports;
    }

    double score_total = 0.0;
    for (const double score : scores) {
        score_total += score;
    }
    metrics.score_rate =
        score_total /
        static_cast<double>(scores.size());

    const auto interval =
        score_confidence_interval(
            scores,
            confidence_z);
    metrics.confidence_lower_bound =
        interval.first;
    metrics.confidence_upper_bound =
        interval.second;

    metrics.candidate_black_score_rate =
        metrics.candidate_black_games == 0
            ? 0.0
            : black_score /
                static_cast<double>(
                    metrics.candidate_black_games);
    metrics.candidate_white_score_rate =
        metrics.candidate_white_games == 0
            ? 0.0
            : white_score /
                static_cast<double>(
                    metrics.candidate_white_games);

    metrics.average_disc_difference =
        disc_total /
        static_cast<double>(games.size());
    metrics.average_think_us =
        think_calls == 0
            ? 0.0
            : think_total /
                static_cast<double>(think_calls);

    if (node_reports > 0 && think_total > 0.0) {
        metrics.nodes_per_second =
            static_cast<double>(nodes) *
            1000000.0 / think_total;
    }
    if (simulation_reports > 0 &&
        think_total > 0.0) {
        metrics.simulations_per_second =
            static_cast<double>(simulations) *
            1000000.0 / think_total;
    }

    metrics.candidate_rating =
        candidate_after.rating.rating;
    metrics.champion_rating =
        champion_after.rating.rating;
    metrics.rating_delta =
        metrics.candidate_rating -
        metrics.champion_rating;
    return metrics;
}

void set_league_roles(
    LeagueRegistry& league_registry,
    const std::vector<std::string>& participant_ids,
    LeagueParticipantRole role) {
    for (const auto& participant_id : participant_ids) {
        LeagueRegistryEntry* entry =
            league_registry.find(participant_id);
        if (entry == nullptr) {
            throw std::invalid_argument(
                "participant missing while changing League role: " +
                participant_id);
        }
        entry->role = role;
    }
}

template <typename T>
void write_optional_number(
    std::ostringstream& out,
    const std::optional<T>& value) {
    if (value) out << *value;
    else out << "null";
}

void write_string_array(
    std::ostringstream& out,
    const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        out << '\"' << escape_json(values[i]) << '\"';
    }
    out << ']';
}

void write_string_pairs(
    std::ostringstream& out,
    const std::vector<std::pair<std::string, std::string>>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ',';
        out << "{\"key\":\""
            << escape_json(values[i].first)
            << "\",\"value\":\""
            << escape_json(values[i].second)
            << "\"}";
    }
    out << ']';
}

void append_failure(
    PromotionDecision& decision,
    std::string reason) {
    decision.reasons.push_back(std::move(reason));
}

}  // namespace

const char* model_generation_status_name(
    ModelGenerationStatus status) noexcept {
    switch (status) {
        case ModelGenerationStatus::Champion:
            return "champion";
        case ModelGenerationStatus::Candidate:
            return "candidate";
        case ModelGenerationStatus::Rejected:
            return "rejected";
        case ModelGenerationStatus::Retired:
            return "retired";
    }
    return "unknown";
}

ModelGenerationStatus parse_model_generation_status(
    std::string_view value) {
    if (value == "champion") {
        return ModelGenerationStatus::Champion;
    }
    if (value == "candidate") {
        return ModelGenerationStatus::Candidate;
    }
    if (value == "rejected") {
        return ModelGenerationStatus::Rejected;
    }
    if (value == "retired") {
        return ModelGenerationStatus::Retired;
    }
    throw std::invalid_argument(
        "unknown model generation status: " +
        std::string(value));
}

std::string generate_generation_id() {
    return "generation-" + generate_game_ulid();
}

std::string generate_promotion_decision_id() {
    return "promotion-" + generate_game_ulid();
}

void ChampionRegistry::register_profile(
    PromotionProfile profile) {
    validate_profile(profile);
    if (find_profile(profile.profile_id) != nullptr) {
        throw std::invalid_argument(
            "duplicate promotion profile_id: " +
            profile.profile_id);
    }
    profiles_.push_back(std::move(profile));
}

void ChampionRegistry::bootstrap_champion(
    ModelGeneration generation,
    LeagueRegistry& league_registry) {
    PromotionProfile* profile =
        find_profile(generation.profile_id);
    if (profile == nullptr) {
        throw std::invalid_argument(
            "cannot bootstrap Champion without promotion profile");
    }
    if (current_champion(generation.profile_id) != nullptr) {
        throw std::invalid_argument(
            "promotion profile already has a Champion");
    }
    if (find_generation(generation.generation_id) != nullptr) {
        throw std::invalid_argument(
            "duplicate generation_id");
    }
    validate_generation(
        generation,
        *profile,
        league_registry);
    generation.status =
        ModelGenerationStatus::Champion;
    set_league_roles(
        league_registry,
        generation.participant_ids,
        LeagueParticipantRole::Champion);
    generations_.push_back(std::move(generation));
}

void ChampionRegistry::register_candidate(
    ModelGeneration generation,
    LeagueRegistry& league_registry) {
    PromotionProfile* profile =
        find_profile(generation.profile_id);
    if (profile == nullptr) {
        throw std::invalid_argument(
            "cannot register Candidate without promotion profile");
    }
    if (current_champion(generation.profile_id) == nullptr) {
        throw std::invalid_argument(
            "cannot register Candidate before initial Champion");
    }
    if (find_generation(generation.generation_id) != nullptr) {
        throw std::invalid_argument(
            "duplicate generation_id");
    }
    validate_generation(
        generation,
        *profile,
        league_registry);
    generation.status =
        ModelGenerationStatus::Candidate;
    set_league_roles(
        league_registry,
        generation.participant_ids,
        LeagueParticipantRole::Candidate);
    generations_.push_back(std::move(generation));
}

const PromotionProfile* ChampionRegistry::find_profile(
    const std::string& profile_id) const noexcept {
    for (const auto& profile : profiles_) {
        if (profile.profile_id == profile_id) {
            return &profile;
        }
    }
    return nullptr;
}

PromotionProfile* ChampionRegistry::find_profile(
    const std::string& profile_id) noexcept {
    for (auto& profile : profiles_) {
        if (profile.profile_id == profile_id) {
            return &profile;
        }
    }
    return nullptr;
}

const ModelGeneration* ChampionRegistry::find_generation(
    const std::string& generation_id) const noexcept {
    for (const auto& generation : generations_) {
        if (generation.generation_id == generation_id) {
            return &generation;
        }
    }
    return nullptr;
}

ModelGeneration* ChampionRegistry::find_generation(
    const std::string& generation_id) noexcept {
    for (auto& generation : generations_) {
        if (generation.generation_id == generation_id) {
            return &generation;
        }
    }
    return nullptr;
}

const ModelGeneration* ChampionRegistry::current_champion(
    const std::string& profile_id) const noexcept {
    for (const auto& generation : generations_) {
        if (generation.profile_id == profile_id &&
            generation.status ==
                ModelGenerationStatus::Champion) {
            return &generation;
        }
    }
    return nullptr;
}

const std::vector<PromotionProfile>&
ChampionRegistry::profiles() const noexcept {
    return profiles_;
}

const std::vector<ModelGeneration>&
ChampionRegistry::generations() const noexcept {
    return generations_;
}

void ChampionRegistry::apply_decision(
    const PromotionDecision& decision,
    LeagueRegistry& league_registry) {
    ModelGeneration* candidate =
        find_generation(
            decision.evaluation.candidate_generation_id);
    ModelGeneration* champion =
        find_generation(
            decision.evaluation.champion_generation_id);
    if (candidate == nullptr || champion == nullptr) {
        throw std::invalid_argument(
            "promotion decision references unknown generation");
    }
    if (candidate->status !=
        ModelGenerationStatus::Candidate) {
        throw std::invalid_argument(
            "promotion decision target is not Candidate");
    }
    if (champion->status !=
        ModelGenerationStatus::Champion) {
        throw std::invalid_argument(
            "promotion decision reference is not current Champion");
    }
    if (candidate->profile_id !=
            decision.evaluation.profile_id ||
        champion->profile_id !=
            decision.evaluation.profile_id) {
        throw std::invalid_argument(
            "promotion decision profile mismatch");
    }

    if (decision.promoted) {
        champion->status =
            ModelGenerationStatus::Retired;
        candidate->status =
            ModelGenerationStatus::Champion;
        set_league_roles(
            league_registry,
            champion->participant_ids,
            LeagueParticipantRole::HallOfFame);
        set_league_roles(
            league_registry,
            candidate->participant_ids,
            LeagueParticipantRole::Champion);
    } else {
        candidate->status =
            ModelGenerationStatus::Rejected;
        set_league_roles(
            league_registry,
            candidate->participant_ids,
            LeagueParticipantRole::Standard);
    }
}

PromotionEvaluation evaluate_candidate_generation(
    LeagueRegistry& league_registry,
    const PromotionProfile& profile,
    const ModelGeneration& champion,
    const ModelGeneration& candidate,
    const PromotionSupplementalMetrics& supplemental,
    std::uint64_t evaluation_seed,
    const LeagueRunOutputs& outputs) {
    validate_profile(profile);
    validate_generation(
        champion,
        profile,
        league_registry);
    validate_generation(
        candidate,
        profile,
        league_registry);

    if (champion.status !=
        ModelGenerationStatus::Champion) {
        throw std::invalid_argument(
            "promotion evaluation requires Champion generation");
    }
    if (candidate.status !=
        ModelGenerationStatus::Candidate) {
        throw std::invalid_argument(
            "promotion evaluation requires Candidate generation");
    }

    PromotionEvaluation evaluation;
    evaluation.profile_id = profile.profile_id;
    evaluation.candidate_generation_id =
        candidate.generation_id;
    evaluation.champion_generation_id =
        champion.generation_id;
    evaluation.supplemental = supplemental;

    for (const std::size_t board_size :
         profile.board_sizes) {
        const auto& candidate_before =
            generation_entry_for_board(
                candidate,
                league_registry,
                board_size);
        const auto& champion_before =
            generation_entry_for_board(
                champion,
                league_registry,
                board_size);

        const std::string candidate_id =
            candidate_before.participant.participant_id;
        const std::string champion_id =
            champion_before.participant.participant_id;
        const std::size_t candidate_index =
            registry_index_for_participant(
                league_registry,
                candidate_id);
        const std::size_t champion_index =
            registry_index_for_participant(
                league_registry,
                champion_id);

        LeagueRunConfig config;
        config.games_per_color =
            profile.games_per_color;
        config.seed =
            evaluation_seed ^
            (static_cast<std::uint64_t>(board_size) << 32U);
        config.collect_metrics = true;

        const LeagueRunResult run =
            run_registry_schedule(
                league_registry,
                board_size,
                {LeagueRegistryPairing{
                    candidate_index,
                    champion_index,
                }},
                config,
                outputs);

        for (const auto& game : run.games) {
            evaluation.game_ids.push_back(
                game.game_id);
        }

        const LeagueRegistryEntry& candidate_after =
            *league_registry.find(candidate_id);
        const LeagueRegistryEntry& champion_after =
            *league_registry.find(champion_id);

        evaluation.board_metrics.push_back(
            aggregate_board_metrics(
                board_size,
                candidate_id,
                candidate_after,
                champion_after,
                run.games,
                profile.confidence_z));
    }

    return evaluation;
}

PromotionDecision decide_promotion(
    const PromotionProfile& profile,
    const ModelGeneration& candidate,
    const PromotionEvaluation& evaluation) {
    validate_profile(profile);
    if (candidate.generation_id !=
            evaluation.candidate_generation_id ||
        candidate.profile_id !=
            evaluation.profile_id) {
        throw std::invalid_argument(
            "promotion evaluation does not match Candidate");
    }

    PromotionDecision decision;
    decision.decision_id =
        generate_promotion_decision_id();
    decision.evaluation = evaluation;

    if (evaluation.board_metrics.size() !=
        profile.board_sizes.size()) {
        append_failure(
            decision,
            "not all profile board sizes were evaluated");
    }

    for (const std::size_t required_size :
         profile.board_sizes) {
        const auto found = std::find_if(
            evaluation.board_metrics.begin(),
            evaluation.board_metrics.end(),
            [required_size](
                const PromotionBoardMetrics& metrics) {
                return metrics.board_size ==
                    required_size;
            });
        if (found ==
            evaluation.board_metrics.end()) {
            append_failure(
                decision,
                "missing board-size evaluation: " +
                std::to_string(required_size));
            continue;
        }

        const auto& metrics = *found;
        const std::string prefix =
            "b" + std::to_string(required_size) + ": ";

        if (metrics.games <
            profile.thresholds.min_games_per_board) {
            append_failure(
                decision,
                prefix + "insufficient games");
        }
        if (metrics.score_rate <
            profile.thresholds.min_score_rate) {
            append_failure(
                decision,
                prefix + "score rate below threshold");
        }
        if (metrics.confidence_lower_bound <
            profile.thresholds
                .min_confidence_lower_bound) {
            append_failure(
                decision,
                prefix +
                "confidence lower bound below threshold");
        }
        if (profile.thresholds.min_rating_delta &&
            metrics.rating_delta <
                *profile.thresholds.min_rating_delta) {
            append_failure(
                decision,
                prefix + "rating delta below threshold");
        }
        if (profile.thresholds
                .min_average_disc_difference &&
            metrics.average_disc_difference <
                *profile.thresholds
                     .min_average_disc_difference) {
            append_failure(
                decision,
                prefix +
                "average disc difference below threshold");
        }
        if (profile.thresholds.max_average_think_us &&
            metrics.average_think_us >
                *profile.thresholds
                     .max_average_think_us) {
            append_failure(
                decision,
                prefix + "average inference time too high");
        }
        if (profile.thresholds.min_nodes_per_second) {
            if (!metrics.nodes_per_second) {
                append_failure(
                    decision,
                    prefix + "nodes/sec was not reported");
            } else if (
                *metrics.nodes_per_second <
                *profile.thresholds.min_nodes_per_second) {
                append_failure(
                    decision,
                    prefix + "nodes/sec below threshold");
            }
        }
        if (profile.thresholds.min_simulations_per_second) {
            if (!metrics.simulations_per_second) {
                append_failure(
                    decision,
                    prefix +
                    "simulations/sec was not reported");
            } else if (
                *metrics.simulations_per_second <
                *profile.thresholds
                     .min_simulations_per_second) {
                append_failure(
                    decision,
                    prefix +
                    "simulations/sec below threshold");
            }
        }
    }

    const auto& supplemental =
        evaluation.supplemental;
    if (profile.thresholds.min_opening_out_score) {
        if (!supplemental.opening_out_score ||
            *supplemental.opening_out_score <
                *profile.thresholds.min_opening_out_score) {
            append_failure(
                decision,
                "opening-out score below threshold or missing");
        }
    }
    if (profile.thresholds
            .min_difficult_midgame_score) {
        if (!supplemental.difficult_midgame_score ||
            *supplemental.difficult_midgame_score <
                *profile.thresholds
                     .min_difficult_midgame_score) {
            append_failure(
                decision,
                "difficult-midgame score below threshold or missing");
        }
    }
    if (profile.thresholds
            .min_exact_endgame_accuracy) {
        if (!supplemental.exact_endgame_accuracy ||
            *supplemental.exact_endgame_accuracy <
                *profile.thresholds
                     .min_exact_endgame_accuracy) {
            append_failure(
                decision,
                "exact-endgame accuracy below threshold or missing");
        }
    }
    if (profile.thresholds.max_major_blunder_rate) {
        if (!supplemental.major_blunder_rate ||
            *supplemental.major_blunder_rate >
                *profile.thresholds
                     .max_major_blunder_rate) {
            append_failure(
                decision,
                "major blunder rate above threshold or missing");
        }
    }

    if (profile.thresholds.max_model_size_bytes &&
        candidate.reproducibility.model_size_bytes >
            *profile.thresholds.max_model_size_bytes) {
        append_failure(
            decision,
            "model size exceeds profile threshold");
    }
    if (profile.thresholds.max_peak_memory_bytes &&
        candidate.reproducibility.peak_memory_bytes >
            *profile.thresholds.max_peak_memory_bytes) {
        append_failure(
            decision,
            "peak memory exceeds profile threshold");
    }
    if (profile.thresholds.require_reproducible &&
        !candidate.reproducibility.reproducible) {
        append_failure(
            decision,
            "candidate is not marked reproducible");
    }

    decision.promoted =
        decision.reasons.empty();
    if (decision.promoted) {
        decision.reasons.push_back(
            "all promotion conditions satisfied");
    }
    return decision;
}

std::string promotion_profile_to_json(
    const PromotionProfile& profile) {
    validate_profile(profile);
    std::ostringstream out;
    out << std::setprecision(17);
    out << "{\"format\":\""
        << kPromotionProfileFormat
        << "\",\"profile_id\":\""
        << escape_json(profile.profile_id)
        << "\",\"board_sizes\":[";
    for (std::size_t i = 0;
         i < profile.board_sizes.size();
         ++i) {
        if (i) out << ',';
        out << profile.board_sizes[i];
    }
    out << "],\"games_per_color\":"
        << profile.games_per_color
        << ",\"confidence_z\":"
        << profile.confidence_z
        << ",\"thresholds\":{"
        << "\"min_games_per_board\":"
        << profile.thresholds.min_games_per_board
        << ",\"min_score_rate\":"
        << profile.thresholds.min_score_rate
        << ",\"min_confidence_lower_bound\":"
        << profile.thresholds
               .min_confidence_lower_bound
        << ",\"min_rating_delta\":";
    write_optional_number(
        out,
        profile.thresholds.min_rating_delta);
    out << ",\"min_average_disc_difference\":";
    write_optional_number(
        out,
        profile.thresholds
            .min_average_disc_difference);
    out << ",\"max_average_think_us\":";
    write_optional_number(
        out,
        profile.thresholds.max_average_think_us);
    out << ",\"min_nodes_per_second\":";
    write_optional_number(
        out,
        profile.thresholds.min_nodes_per_second);
    out << ",\"min_simulations_per_second\":";
    write_optional_number(
        out,
        profile.thresholds
            .min_simulations_per_second);
    out << ",\"min_opening_out_score\":";
    write_optional_number(
        out,
        profile.thresholds.min_opening_out_score);
    out << ",\"min_difficult_midgame_score\":";
    write_optional_number(
        out,
        profile.thresholds
            .min_difficult_midgame_score);
    out << ",\"min_exact_endgame_accuracy\":";
    write_optional_number(
        out,
        profile.thresholds
            .min_exact_endgame_accuracy);
    out << ",\"max_major_blunder_rate\":";
    write_optional_number(
        out,
        profile.thresholds.max_major_blunder_rate);
    out << ",\"max_model_size_bytes\":";
    write_optional_number(
        out,
        profile.thresholds.max_model_size_bytes);
    out << ",\"max_peak_memory_bytes\":";
    write_optional_number(
        out,
        profile.thresholds.max_peak_memory_bytes);
    out << ",\"require_reproducible\":"
        << (profile.thresholds.require_reproducible
                ? "true"
                : "false")
        << "},\"tuning_targets\":";
    write_string_array(
        out,
        profile.tuning_targets);
    out << '}';
    return out.str();
}

std::string model_generation_to_json(
    const ModelGeneration& generation) {
    std::ostringstream out;
    out << "{\"format\":\""
        << kModelGenerationFormat
        << "\",\"generation_id\":\""
        << escape_json(generation.generation_id)
        << "\",\"profile_id\":\""
        << escape_json(generation.profile_id)
        << "\",\"status\":\""
        << model_generation_status_name(
               generation.status)
        << "\",\"participant_ids\":";
    write_string_array(
        out,
        generation.participant_ids);
    out << ",\"reproducibility\":{"
        << "\"parent_model_id\":\""
        << escape_json(
            generation.reproducibility
                .parent_model_id)
        << "\",\"parent_model_version\":\""
        << escape_json(
            generation.reproducibility
                .parent_model_version)
        << "\",\"training_recipe_id\":\""
        << escape_json(
            generation.reproducibility
                .training_recipe_id)
        << "\",\"dataset_versions\":";
    write_string_pairs(
        out,
        generation.reproducibility
            .dataset_versions);
    out << ",\"config_hash\":\""
        << escape_json(
            generation.reproducibility
                .config_hash)
        << "\",\"seed\":"
        << generation.reproducibility.seed
        << ",\"code_version\":\""
        << escape_json(
            generation.reproducibility
                .code_version)
        << "\",\"benchmark_result\":\""
        << escape_json(
            generation.reproducibility
                .benchmark_result)
        << "\",\"tuning_parameters\":";
    write_string_pairs(
        out,
        generation.reproducibility
            .tuning_parameters);
    out << ",\"model_size_bytes\":"
        << generation.reproducibility
               .model_size_bytes
        << ",\"peak_memory_bytes\":"
        << generation.reproducibility
               .peak_memory_bytes
        << ",\"reproducible\":"
        << (generation.reproducibility
                    .reproducible
                ? "true"
                : "false")
        << "}}";
    return out.str();
}

std::string promotion_decision_to_json(
    const PromotionDecision& decision) {
    std::ostringstream out;
    out << std::setprecision(17);
    out << "{\"format\":\""
        << kPromotionDecisionFormat
        << "\",\"decision_id\":\""
        << escape_json(decision.decision_id)
        << "\",\"promoted\":"
        << (decision.promoted ? "true" : "false")
        << ",\"profile_id\":\""
        << escape_json(
            decision.evaluation.profile_id)
        << "\",\"candidate_generation_id\":\""
        << escape_json(
            decision.evaluation
                .candidate_generation_id)
        << "\",\"champion_generation_id\":\""
        << escape_json(
            decision.evaluation
                .champion_generation_id)
        << "\",\"reasons\":";
    write_string_array(out, decision.reasons);

    out << ",\"board_metrics\":[";
    for (std::size_t i = 0;
         i < decision.evaluation
                 .board_metrics.size();
         ++i) {
        if (i) out << ',';
        const auto& m =
            decision.evaluation.board_metrics[i];
        out << "{\"board_size\":"
            << m.board_size
            << ",\"games\":" << m.games
            << ",\"score_rate\":"
            << m.score_rate
            << ",\"confidence_lower_bound\":"
            << m.confidence_lower_bound
            << ",\"confidence_upper_bound\":"
            << m.confidence_upper_bound
            << ",\"candidate_black_games\":"
            << m.candidate_black_games
            << ",\"candidate_black_score_rate\":"
            << m.candidate_black_score_rate
            << ",\"candidate_white_games\":"
            << m.candidate_white_games
            << ",\"candidate_white_score_rate\":"
            << m.candidate_white_score_rate
            << ",\"average_disc_difference\":"
            << m.average_disc_difference
            << ",\"min_disc_difference\":"
            << m.min_disc_difference
            << ",\"max_disc_difference\":"
            << m.max_disc_difference
            << ",\"average_think_us\":"
            << m.average_think_us
            << ",\"nodes_per_second\":";
        write_optional_number(
            out,
            m.nodes_per_second);
        out << ",\"simulations_per_second\":";
        write_optional_number(
            out,
            m.simulations_per_second);
        out << ",\"candidate_rating\":"
            << m.candidate_rating
            << ",\"champion_rating\":"
            << m.champion_rating
            << ",\"rating_delta\":"
            << m.rating_delta
            << '}';
    }
    out << ']';

    const auto& s =
        decision.evaluation.supplemental;
    out << ",\"supplemental\":{"
        << "\"opening_out_score\":";
    write_optional_number(out, s.opening_out_score);
    out << ",\"difficult_midgame_score\":";
    write_optional_number(
        out,
        s.difficult_midgame_score);
    out << ",\"exact_endgame_accuracy\":";
    write_optional_number(
        out,
        s.exact_endgame_accuracy);
    out << ",\"major_blunder_rate\":";
    write_optional_number(
        out,
        s.major_blunder_rate);
    out << "},\"game_ids\":";
    write_string_array(
        out,
        decision.evaluation.game_ids);
    out << '}';
    return out.str();
}

void write_champion_registry_jsonl(
    const ChampionRegistry& registry,
    std::ostream& output) {
    for (const auto& profile :
         registry.profiles()) {
        output <<
            promotion_profile_to_json(profile)
            << '\n';
    }
    for (const auto& generation :
         registry.generations()) {
        output <<
            model_generation_to_json(generation)
            << '\n';
    }
}

void write_promotion_decision_jsonl(
    const PromotionDecision& decision,
    std::ostream& output) {
    output << promotion_decision_to_json(decision)
           << '\n';
}

}  // namespace kadoka::othello
