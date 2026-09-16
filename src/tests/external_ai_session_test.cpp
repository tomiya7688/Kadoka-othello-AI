#include "kadoka_othello/external_ai_session.hpp"

#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "kadoka_othello/game.hpp"
#include "kadoka_othello/state.hpp"

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

}  // namespace

int main(int argc, char** argv) {
    assert(argc >= 2);
    const std::string helper = argv[1];

    {
        Game game(8);
        const auto legal_moves = game.legal_moves();
        const AdaptedAIInput input{&game.board(), &legal_moves};
        ExternalAISession session(make_config(helper));

        const AIInspection first = session.inspect(input);
        const AIInspection second = session.inspect(input);
        assert(is_legal(first.output.move, legal_moves));
        assert(is_legal(second.output.move, legal_moves));
        assert(diagnostic_value(first, "request_count") == "1");
        assert(diagnostic_value(second, "request_count") == "2");
    }

    {
        Game game(8);
        const auto legal_moves = game.legal_moves();
        const AdaptedAIInput input{&game.board(), &legal_moves};
        ExternalAISession session(make_config(helper, "illegal"));

        const std::string before = snapshot_to_json(make_snapshot(game));
        const AIInspection result = session.inspect(input);
        assert(!game.play(result.output.move));
        const std::string after = snapshot_to_json(make_snapshot(game));
        assert(before == after);
    }

    {
        Game game(8);
        const auto legal_moves = game.legal_moves();
        const AdaptedAIInput input{&game.board(), &legal_moves};
        ExternalAISession session(make_config(helper, "malformed"));
        bool failed = false;
        try {
            (void)session.inspect(input);
        } catch (const std::runtime_error&) {
            failed = true;
        }
        assert(failed);
    }

    {
        Game game(8);
        const auto legal_moves = game.legal_moves();
        const AdaptedAIInput input{&game.board(), &legal_moves};
        ExternalAISession session(make_config(helper, "exit"));
        bool failed = false;
        try {
            (void)session.inspect(input);
        } catch (const std::runtime_error&) {
            failed = true;
        }
        assert(failed);
    }

    {
        Game game(8);
        const auto legal_moves = game.legal_moves();
        const AdaptedAIInput input{&game.board(), &legal_moves};
        ExternalAISessionConfig config = make_config(helper, "timeout");
        config.response_timeout = std::chrono::milliseconds(50);
        ExternalAISession session(std::move(config));
        bool failed = false;
        try {
            (void)session.inspect(input);
        } catch (const std::runtime_error&) {
            failed = true;
        }
        assert(failed);
    }

    return 0;
}
