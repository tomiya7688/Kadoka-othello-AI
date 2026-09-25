#include "kadoka_othello/league.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "kadoka_othello/game_record.hpp"
#include "kadoka_othello/model_descriptor.hpp"
#include "kadoka_othello/package_loader.hpp"

namespace kadoka::othello {
namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
constexpr double kGlickoQ = 0.005756462732485114;
constexpr double kMinDeviation = 30.0;

void hash_bytes(std::uint64_t& hash, const char* data, std::size_t size) noexcept {
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= static_cast<unsigned char>(data[i]);
        hash *= kFnvPrime;
    }
}

void hash_text(std::uint64_t& hash, std::string_view text) noexcept {
    hash_bytes(hash, text.data(), text.size());
    const char separator = '\0';
    hash_bytes(hash, &separator, 1);
}

void hash_file(std::uint64_t& hash, const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to hash league participant asset: " + path.string());
    }

    char buffer[4096];
    while (input) {
        input.read(buffer, sizeof(buffer));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            hash_bytes(hash, buffer, static_cast<std::size_t>(count));
        }
    }
}

std::string to_hex(std::uint64_t value) {
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << value;
    return out.str();
}

std::uint64_t participant_content_hash(
    const LeagueParticipantConfig& config,
    const AIPackageManifest& manifest) {
    namespace fs = std::filesystem;

    std::uint64_t hash = kFnvOffset;
    hash_text(hash, manifest.id);
    hash_text(hash, manifest.version);
    hash_text(hash, package_interface_name(manifest.interface_type));
    hash_text(hash, manifest.entry);
    hash_text(hash, manifest.model);
    hash_text(hash, config.config_hash);
    hash_text(hash, std::to_string(config.board_size));
    hash_text(hash, std::to_string(config.seed));

    hash_file(hash, fs::absolute(config.manifest_path));

    if (!manifest.model.empty()) {
        fs::path model_path(manifest.model);
        if (model_path.is_relative()) {
            model_path = fs::path(manifest.source_directory) / model_path;
        }
        hash_file(hash, model_path);

        const ModelRootDescriptor model = load_model_root_descriptor(model_path.string());
        for (const auto& asset : model.assets) {
            hash_text(hash, asset.id);
            hash_text(hash, asset.type);
            hash_text(hash, asset.required ? "required" : "optional");
            const std::string asset_path = find_model_asset_path(model, asset.id);
            if (!asset_path.empty()) {
                hash_file(hash, asset_path);
            }
        }
    }

    return hash;
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

std::uint64_t mix64(std::uint64_t value) noexcept {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

std::uint64_t game_seed(
    std::uint64_t league_seed,
    std::uint64_t participant_seed,
    std::size_t game_index,
    std::uint64_t salt) noexcept {
    std::uint64_t seed = mix64(
        league_seed ^
        participant_seed ^
        (static_cast<std::uint64_t>(game_index) * 0x9e3779b97f4a7c15ULL) ^
        salt);
    return seed == 0 ? 1 : seed;
}

std::string make_reproducibility_key(
    const LeagueParticipant& black,
    const LeagueParticipant& white,
    std::size_t game_index,
    std::uint64_t league_seed) {
    std::uint64_t hash = kFnvOffset;
    hash_text(hash, black.participant_id);
    hash_text(hash, white.participant_id);
    hash_text(hash, std::to_string(game_index));
    hash_text(hash, std::to_string(league_seed));
    return "run-" + to_hex(hash);
}

LeagueGameOutcome outcome_from_summary(const HeadlessSummary& summary) {
    if (summary.games != 1) {
        throw std::logic_error("league game must contain exactly one headless game");
    }
    if (summary.black_wins == 1) return LeagueGameOutcome::BlackWin;
    if (summary.white_wins == 1) return LeagueGameOutcome::WhiteWin;
    if (summary.draws == 1) return LeagueGameOutcome::Draw;
    throw std::logic_error("league game summary has no result");
}

double glicko_g(double deviation) noexcept {
    const double q2 = kGlickoQ * kGlickoQ;
    return 1.0 / std::sqrt(1.0 + (3.0 * q2 * deviation * deviation) /
                                      (3.14159265358979323846 * 3.14159265358979323846));
}

double glicko_expected(
    double rating,
    double opponent_rating,
    double opponent_deviation) noexcept {
    const double g = glicko_g(opponent_deviation);
    return 1.0 /
        (1.0 + std::pow(10.0, -g * (rating - opponent_rating) / 400.0));
}

LeagueRating updated_glicko(
    const LeagueRating& current,
    const LeagueRating& opponent,
    double score) {
    const double g = glicko_g(opponent.deviation);
    const double expected = glicko_expected(
        current.rating,
        opponent.rating,
        opponent.deviation);
    const double variance_denominator =
        kGlickoQ * kGlickoQ * g * g * expected * (1.0 - expected);
    const double d2 = variance_denominator > 0.0
        ? 1.0 / variance_denominator
        : std::numeric_limits<double>::infinity();
    const double precision =
        1.0 / (current.deviation * current.deviation) + 1.0 / d2;

    LeagueRating next = current;
    next.rating = current.rating +
        (kGlickoQ / precision) * g * (score - expected);
    next.deviation = std::max(kMinDeviation, std::sqrt(1.0 / precision));
    ++next.games;
    if (score > 0.5) ++next.wins;
    else if (score < 0.5) ++next.losses;
    else ++next.draws;
    return next;
}

void update_ratings(
    LeagueRating& black,
    LeagueRating& white,
    LeagueGameOutcome outcome) {
    const LeagueRating black_before = black;
    const LeagueRating white_before = white;
    const double black_score = outcome == LeagueGameOutcome::BlackWin
        ? 1.0
        : outcome == LeagueGameOutcome::WhiteWin ? 0.0 : 0.5;
    const double white_score = 1.0 - black_score;

    black = updated_glicko(black_before, white_before, black_score);
    white = updated_glicko(white_before, black_before, white_score);
}

void write_game_log(std::ostream& output, const LeagueGameRecord& record) {
    output << "{\"format\":\"kadoka.league_game.v1\""
           << ",\"game_id\":\"" << escape_json(record.game_id) << "\""
           << ",\"reproducibility_key\":\""
           << escape_json(record.reproducibility_key) << "\""
           << ",\"black\":\"" << escape_json(record.black_participant_id) << "\""
           << ",\"white\":\"" << escape_json(record.white_participant_id) << "\""
           << ",\"board_size\":" << record.board_size
           << ",\"black_seed\":" << record.black_seed
           << ",\"white_seed\":" << record.white_seed
           << ",\"outcome\":\"" << league_outcome_name(record.outcome) << "\""
           << ",\"black_discs\":" << record.black_discs
           << ",\"white_discs\":" << record.white_discs
           << ",\"elapsed_us\":" << record.elapsed_us
           << ",\"turns\":" << record.metrics.turns
           << ",\"ai_calls\":" << record.metrics.ai_calls
           << ",\"invalid_attempts\":" << record.metrics.invalid_move_attempts
           << ",\"total_ai_think_us\":" << record.metrics.total_ai_think_us
           << ",\"max_ai_think_us\":" << record.metrics.max_ai_think_us
           << ",\"total_nodes\":" << record.metrics.total_nodes
           << ",\"node_reports\":" << record.metrics.node_reports
           << ",\"total_simulations\":" << record.metrics.total_simulations
           << ",\"simulation_reports\":" << record.metrics.simulation_reports
           << ",\"max_depth\":" << record.metrics.max_depth
           << ",\"depth_reports\":" << record.metrics.depth_reports
           << ",\"total_search_effort\":" << record.metrics.total_search_effort
           << ",\"search_effort_reports\":" << record.metrics.search_effort_reports
           << ",\"black_rating_before\":" << record.black_rating_before
           << ",\"white_rating_before\":" << record.white_rating_before
           << ",\"black_deviation_before\":" << record.black_deviation_before
           << ",\"white_deviation_before\":" << record.white_deviation_before
           << ",\"black_rating_after\":" << record.black_rating_after
           << ",\"white_rating_after\":" << record.white_rating_after
           << ",\"black_deviation_after\":" << record.black_deviation_after
           << ",\"white_deviation_after\":" << record.white_deviation_after
           << "}\n";
}

void write_position_dataset(
    std::ostream& output,
    const LeagueGameRecord& record,
    const std::string& raw_snapshots) {
    std::istringstream lines(raw_snapshots);
    std::string snapshot;
    while (std::getline(lines, snapshot)) {
        if (snapshot.empty()) continue;
        output << "{\"format\":\"kadoka.league_position.v1\""
               << ",\"game_id\":\"" << escape_json(record.game_id) << "\""
               << ",\"black\":\"" << escape_json(record.black_participant_id) << "\""
               << ",\"white\":\"" << escape_json(record.white_participant_id) << "\""
               << ",\"board_size\":" << record.board_size
               << ",\"snapshot\":" << snapshot
               << "}\n";
    }
}

LeagueGameRecord run_league_game(
    const LeagueParticipant& black,
    const LeagueParticipant& white,
    std::size_t game_index,
    const LeagueRunConfig& config,
    LeagueRating& black_rating,
    LeagueRating& white_rating,
    const LeagueRunOutputs& outputs) {
    if (black.config.board_size != white.config.board_size) {
        throw std::invalid_argument("league participants must use the same board size");
    }

    const std::uint64_t black_seed = game_seed(
        config.seed,
        black.config.seed,
        game_index,
        0x424c41434bULL);
    const std::uint64_t white_seed = game_seed(
        config.seed,
        white.config.seed,
        game_index,
        0x5748495445ULL);

    LoadedAIPackage black_package = load_ai_package(black.manifest, black_seed);
    LoadedAIPackage white_package = load_ai_package(white.manifest, white_seed);

    HeadlessConfig headless;
    headless.board_size = black.config.board_size;
    headless.games = 1;
    headless.seed = config.seed;
    headless.max_invalid_attempts_per_turn = config.max_invalid_attempts_per_turn;
    headless.write_json_lines = outputs.position_dataset != nullptr;
    headless.collect_metrics = config.collect_metrics;
    headless.record_game_id = generate_game_ulid();

    std::ostringstream raw_dataset;
    std::ostream* dataset_stream =
        outputs.position_dataset != nullptr ? &raw_dataset : nullptr;

    const auto start = std::chrono::steady_clock::now();
    const HeadlessSummary summary = run_games(
        headless,
        black_package.view(),
        white_package.view(),
        dataset_stream,
        outputs.board_state_output,
        outputs.game_aux_output);
    const auto end = std::chrono::steady_clock::now();

    LeagueGameRecord record;
    record.game_id = headless.record_game_id;
    record.reproducibility_key =
        make_reproducibility_key(black, white, game_index, config.seed);
    record.black_participant_id = black.participant_id;
    record.white_participant_id = white.participant_id;
    record.board_size = black.config.board_size;
    record.black_seed = black_seed;
    record.white_seed = white_seed;
    record.outcome = outcome_from_summary(summary);
    record.black_discs = summary.total_black_discs;
    record.white_discs = summary.total_white_discs;
    record.elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
    record.metrics = summary;
    record.black_rating_before = black_rating.rating;
    record.white_rating_before = white_rating.rating;
    record.black_deviation_before = black_rating.deviation;
    record.white_deviation_before = white_rating.deviation;

    update_ratings(black_rating, white_rating, record.outcome);

    record.black_rating_after = black_rating.rating;
    record.white_rating_after = white_rating.rating;
    record.black_deviation_after = black_rating.deviation;
    record.white_deviation_after = white_rating.deviation;

    if (outputs.game_log != nullptr) {
        write_game_log(*outputs.game_log, record);
    }
    if (outputs.position_dataset != nullptr) {
        write_position_dataset(
            *outputs.position_dataset,
            record,
            raw_dataset.str());
    }
    return record;
}

}  // namespace

const char* league_outcome_name(LeagueGameOutcome outcome) noexcept {
    switch (outcome) {
        case LeagueGameOutcome::BlackWin: return "black_win";
        case LeagueGameOutcome::WhiteWin: return "white_win";
        case LeagueGameOutcome::Draw: return "draw";
    }
    return "unknown";
}

LeagueParticipant load_league_participant(LeagueParticipantConfig config) {
    if (config.manifest_path.empty()) {
        throw std::invalid_argument("league participant requires manifest_path");
    }
    if (config.board_size < 4 || config.board_size % 2 != 0) {
        throw std::invalid_argument("league participant board size must be even and >= 4");
    }

    LeagueParticipant participant;
    participant.config = std::move(config);
    participant.manifest = load_ai_manifest(participant.config.manifest_path);

    if (!participant.manifest.board_sizes.empty() &&
        std::find(
            participant.manifest.board_sizes.begin(),
            participant.manifest.board_sizes.end(),
            participant.config.board_size) == participant.manifest.board_sizes.end()) {
        throw std::invalid_argument(
            "AI package does not support requested league board size: " +
            participant.manifest.id);
    }

    const std::uint64_t content_hash = participant_content_hash(
        participant.config,
        participant.manifest);
    participant.participant_id =
        participant.manifest.id + "@" + participant.manifest.version +
        ":b" + std::to_string(participant.config.board_size) +
        ":s" + std::to_string(participant.config.seed) +
        ":h" + to_hex(content_hash);
    return participant;
}

LeagueRunResult run_league_schedule(
    const std::vector<LeagueParticipant>& participants,
    const std::vector<LeagueRating>& initial_ratings,
    const std::vector<LeaguePairing>& pairings,
    const LeagueRunConfig& config,
    const LeagueRunOutputs& outputs) {
    if (participants.size() < 2) {
        throw std::invalid_argument(
            "league requires at least two participants");
    }
    if (initial_ratings.size() != participants.size()) {
        throw std::invalid_argument(
            "initial rating count must match participant count");
    }
    if (pairings.empty()) {
        throw std::invalid_argument(
            "league schedule must contain at least one pairing");
    }
    if (config.games_per_color == 0) {
        throw std::invalid_argument(
            "games_per_color must be greater than zero");
    }
    if ((outputs.board_state_output == nullptr) !=
        (outputs.game_aux_output == nullptr)) {
        throw std::invalid_argument(
            "League Game Record requires both BoardState and GameAux outputs");
    }

    const std::size_t board_size =
        participants.front().config.board_size;
    for (const auto& participant : participants) {
        if (participant.config.board_size != board_size) {
            throw std::invalid_argument(
                "one league run requires one board size");
        }
    }

    LeagueRunResult result;
    std::vector<LeagueRating> ratings = initial_ratings;
    std::size_t game_index = 0;

    for (const LeaguePairing pairing : pairings) {
        if (pairing.first >= participants.size() ||
            pairing.second >= participants.size() ||
            pairing.first == pairing.second) {
            throw std::invalid_argument(
                "league schedule contains invalid participant indexes");
        }

        for (std::size_t round = 0;
             round < config.games_per_color;
             ++round) {
            result.games.push_back(run_league_game(
                participants[pairing.first],
                participants[pairing.second],
                game_index++,
                config,
                ratings[pairing.first],
                ratings[pairing.second],
                outputs));

            result.games.push_back(run_league_game(
                participants[pairing.second],
                participants[pairing.first],
                game_index++,
                config,
                ratings[pairing.second],
                ratings[pairing.first],
                outputs));
        }
    }

    result.table.reserve(participants.size());
    for (std::size_t i = 0; i < participants.size(); ++i) {
        result.table.push_back(
            LeagueTableEntry{participants[i], ratings[i]});
    }
    std::sort(
        result.table.begin(),
        result.table.end(),
        [](const LeagueTableEntry& lhs,
           const LeagueTableEntry& rhs) {
            if (lhs.rating.rating != rhs.rating.rating) {
                return lhs.rating.rating > rhs.rating.rating;
            }
            return lhs.participant.participant_id <
                rhs.participant.participant_id;
        });
    return result;
}

LeagueRunResult run_round_robin_league(
    const std::vector<LeagueParticipant>& participants,
    const LeagueRunConfig& config,
    std::ostream* game_log,
    std::ostream* position_dataset) {
    if (participants.size() < 2) {
        throw std::invalid_argument(
            "league requires at least two participants");
    }

    std::vector<LeaguePairing> pairings;
    for (std::size_t first = 0;
         first < participants.size();
         ++first) {
        for (std::size_t second = first + 1;
             second < participants.size();
             ++second) {
            pairings.push_back({first, second});
        }
    }

    return run_league_schedule(
        participants,
        std::vector<LeagueRating>(participants.size()),
        pairings,
        config,
        LeagueRunOutputs{
            game_log,
            position_dataset,
            nullptr,
            nullptr,
        });
}

}  // namespace kadoka::othello
