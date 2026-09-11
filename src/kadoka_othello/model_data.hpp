#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "kadoka_othello/ai.hpp"
#include "kadoka_othello/game.hpp"

namespace kadoka::othello {

struct CandidateEvaluation {
    Position move{};
    double value{};
    double policy{};
};

struct ModelRecord {
    std::string format_version{"kadoka.model_record.v1"};
    std::string model_id;
    std::string game_id;
    std::size_t ply{};
    std::size_t board_size{};
    Player player{Player::Black};
    std::vector<Cell> board;
    std::vector<Position> legal_moves;
    Position selected_move{};
    std::vector<CandidateEvaluation> candidates;
    std::vector<AIDiagnostic> diagnostics;
};

class IModelDataCodec {
public:
    virtual ~IModelDataCodec() = default;

    [[nodiscard]] virtual std::string id() const = 0;
    virtual void write(std::ostream& output, const ModelRecord& record) const = 0;
};

class KadokaJsonlCodec final : public IModelDataCodec {
public:
    [[nodiscard]] std::string id() const override;
    void write(std::ostream& output, const ModelRecord& record) const override;
};

[[nodiscard]] ModelRecord make_model_record(
    const Game& game,
    const std::string& model_id,
    const std::string& game_id,
    const AIInspection& inspection);

}  // namespace kadoka::othello
