#include "kadoka_othello/external_ai_session.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "kadoka_othello/game.hpp"
#include "kadoka_othello/state.hpp"

#include "test_support.hpp"

using namespace kadoka::othello;

namespace {

ExternalAISessionConfig make_config(
    const std::string& executable,
    const std::string& mode = {}) {
    ExternalAISessionConfig config;
    config.executable = executable;
    config.arguments.push_back("--kadoka-session");
    if (!mode.empty()) config.arguments.push_back("--mode=" + mode);
    config.response_timeout = std::chrono::milliseconds(500);
    return config;
}

std::string diagnostic_value(const AIInspection& inspection, const std::string& key) {
    for (const auto& diagnostic : inspection.diagnostics) {
        if (diagnostic.key == key) return diagnostic.value;
    }
    return {};
}

bool is_legal(const Position move, const std::vector<Position>& legal_moves) {
    for (const Position legal : legal_moves) {
        if (legal == move) return true;
    }
    return false;
}

AIInput input_for(const Game& game) {
    return AIInput{&game.board(), game.current_player(), {}};
}

}  // namespace

int main(int argc, char** argv) {
    KADOKA_REQUIRE(argc >= 2);
    const std::string helper = argv[1];

    {
        Game game(8);
        const auto legal_moves = game.legal_moves();
        ExternalAISession session(make_config(helper));

        const AIInspection first = session.inspect(input_for(game));
        const AIInspection second = session.inspect(input_for(game));
        KADOKA_REQUIRE(is_legal(first.output.move, legal_moves));
        KADOKA_REQUIRE(is_legal(second.output.move, legal_moves));
        KADOKA_REQUIRE(diagnostic_value(first, "request_count") == "1");
        KADOKA_REQUIRE(diagnostic_value(second, "request_count") == "2");
    }

    {
        Game game(8);
        std::size_t invalid_events = 0;
        static_cast<void>(game.add_event_listener([&invalid_events](const GameEvent& event) {
            if (event.type == GameEventType::InvalidMove) ++invalid_events;
        }));
        ExternalAISession session(make_config(helper, "illegal"));

        const std::string before = snapshot_to_json(make_snapshot(game));
        const std::size_t ply_before = game.ply();
        const AIInspection result = session.inspect(input_for(game));
        KADOKA_REQUIRE(!game.play(result.output.move));
        const std::string after = snapshot_to_json(make_snapshot(game));
        KADOKA_REQUIRE(before == after);
        KADOKA_REQUIRE(game.ply() == ply_before);
        KADOKA_REQUIRE(invalid_events == 1);
    }

    for (const std::string mode : {"malformed", "exit", "timeout"}) {
        Game game(8);
        ExternalAISessionConfig config = make_config(helper, mode);
        if (mode == "timeout") {
            config.response_timeout = std::chrono::milliseconds(50);
        }
        ExternalAISession session(std::move(config));
        bool failed = false;
        try {
            (void)session.inspect(input_for(game));
        } catch (const std::runtime_error&) {
            failed = true;
        }
        KADOKA_REQUIRE(failed);
    }

    return 0;
}
