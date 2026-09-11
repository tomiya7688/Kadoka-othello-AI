#pragma once

#include <cstddef>

#include "kadoka_othello/board.hpp"

namespace kadoka::othello {

struct ObakeKadokaFeatureVector {
    double occupied_neighbors{};
    double empty_neighbors{};
    double mixed_color{};
    double color_transitions{};
    double line_interest{};
    double local_density{};
    double early_center{};
};

struct ObakeKadokaEvaluatorWeights {
    double occupied_neighbors{0.45};
    double empty_neighbors{-0.18};
    double mixed_color{1.1};
    double color_transitions{0.55};
    double line_interest{2.6};
    double local_density{0.3};
    double early_center{0.12};
};

struct ObakeKadokaEvaluation {
    ObakeKadokaFeatureVector features;
    double score{};
};

class ObakeKadokaEvaluator {
public:
    explicit ObakeKadokaEvaluator(ObakeKadokaEvaluatorWeights weights = {});

    [[nodiscard]] const ObakeKadokaEvaluatorWeights& weights() const noexcept;
    void set_weights(ObakeKadokaEvaluatorWeights weights) noexcept;

    [[nodiscard]] ObakeKadokaEvaluation evaluate(
        const Board& board,
        Position move,
        std::size_t empty_cells) const;

private:
    ObakeKadokaEvaluatorWeights weights_;
};

}  // namespace kadoka::othello
