#include <iostream>
#include <string>
#include <vector>

#include "kadoka_othello/script_evaluator.hpp"

using namespace kadoka::othello;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "usage: kadoka_script_evaluator_probe <evaluator.json> [key=value ...]\n";
        return 0;
    }

    try {
        ScriptEvaluator evaluator(load_script_evaluator_config(argv[1]));
        ScriptEvaluatorCase item;
        item.id = 0;

        for (int i = 2; i < argc; ++i) {
            const std::string token = argv[i];
            const std::size_t equals = token.find('=');
            if (equals == std::string::npos) {
                throw std::invalid_argument("feature must be key=value: " + token);
            }
            item.features.push_back({
                token.substr(0, equals),
                std::stod(token.substr(equals + 1)),
            });
        }

        const auto results = evaluator.evaluate_batch({item});
        for (const auto& value : results.front().values) {
            std::cout << value.key << '=' << value.value << '\n';
        }
        for (const auto& diag : results.front().diagnostics) {
            std::cout << "diag." << diag.first << '=' << diag.second << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "script evaluator error: " << error.what() << '\n';
        return 1;
    }
}
