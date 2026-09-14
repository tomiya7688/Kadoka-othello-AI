#include "kadoka_othello/native_in_process_evaluator.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "kadoka_othello/script_evaluator_abi.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace kadoka::othello {
namespace {

struct SinkContext {
    std::vector<ScriptEvaluatorResult> results;
    ScriptEvaluatorResult current;
    bool active{};
    bool failed{};
    std::string failure;
};

int sink_fail(SinkContext& context, const char* message) {
    context.failed = true;
    context.failure = message;
    return 1;
}

int sink_begin_result(void* raw, std::size_t id) {
    auto& context = *static_cast<SinkContext*>(raw);
    if (context.failed) return 1;
    if (context.active) return sink_fail(context, "begin_result called before previous result ended");
    context.current = ScriptEvaluatorResult{};
    context.current.id = id;
    context.active = true;
    return 0;
}

int sink_value(void* raw, const char* key, double value) {
    auto& context = *static_cast<SinkContext*>(raw);
    if (context.failed) return 1;
    if (!context.active) return sink_fail(context, "value emitted outside result");
    if (key == nullptr || *key == '\0') return sink_fail(context, "value key is empty");
    context.current.values.push_back({key, value});
    return 0;
}

int sink_diagnostic(void* raw, const char* key, const char* value) {
    auto& context = *static_cast<SinkContext*>(raw);
    if (context.failed) return 1;
    if (!context.active) return sink_fail(context, "diagnostic emitted outside result");
    if (key == nullptr || *key == '\0') return sink_fail(context, "diagnostic key is empty");
    context.current.diagnostics.push_back({key, value == nullptr ? "" : value});
    return 0;
}

int sink_end_result(void* raw) {
    auto& context = *static_cast<SinkContext*>(raw);
    if (context.failed) return 1;
    if (!context.active) return sink_fail(context, "end_result called without begin_result");
    context.results.push_back(std::move(context.current));
    context.current = ScriptEvaluatorResult{};
    context.active = false;
    return 0;
}

void validate_result_ids(
    const std::vector<ScriptEvaluatorCase>& cases,
    const std::vector<ScriptEvaluatorResult>& results) {
    if (results.size() != cases.size()) {
        throw std::runtime_error("native in-process evaluator returned unexpected result count");
    }

    std::vector<std::size_t> expected;
    std::vector<std::size_t> actual;
    expected.reserve(cases.size());
    actual.reserve(results.size());
    for (const auto& item : cases) expected.push_back(item.id);
    for (const auto& item : results) actual.push_back(item.id);
    std::sort(expected.begin(), expected.end());
    std::sort(actual.begin(), actual.end());
    if (expected != actual) {
        throw std::runtime_error("native in-process evaluator returned mismatched result ids");
    }
}

}  // namespace

struct NativeInProcessEvaluator::Impl {
    std::string library_path;
    std::string symbol_name;
    KadokaScriptEvaluatorFunctionV1 function{};

#ifdef _WIN32
    HMODULE handle{};
#else
    void* handle{};
#endif

    Impl(std::string path, std::string symbol)
        : library_path(std::move(path)), symbol_name(std::move(symbol)) {
        namespace fs = std::filesystem;
        library_path = fs::absolute(fs::path(library_path)).string();

#ifdef _WIN32
        handle = LoadLibraryA(library_path.c_str());
        if (handle == nullptr) {
            throw std::runtime_error(
                "failed to load native in-process evaluator library: " + library_path +
                " error=" + std::to_string(GetLastError()));
        }
        const FARPROC raw = GetProcAddress(handle, symbol_name.c_str());
        if (raw == nullptr) {
            FreeLibrary(handle);
            handle = nullptr;
            throw std::runtime_error(
                "native in-process evaluator symbol not found: " + symbol_name);
        }
        function = reinterpret_cast<KadokaScriptEvaluatorFunctionV1>(raw);
#else
        handle = dlopen(library_path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) {
            const char* error = dlerror();
            throw std::runtime_error(
                "failed to load native in-process evaluator library: " + library_path +
                " error=" + (error == nullptr ? std::string("unknown") : std::string(error)));
        }
        dlerror();
        void* raw = dlsym(handle, symbol_name.c_str());
        const char* error = dlerror();
        if (error != nullptr || raw == nullptr) {
            dlclose(handle);
            handle = nullptr;
            throw std::runtime_error(
                "native in-process evaluator symbol not found: " + symbol_name);
        }
        function = reinterpret_cast<KadokaScriptEvaluatorFunctionV1>(raw);
#endif
    }

    ~Impl() {
#ifdef _WIN32
        if (handle != nullptr) FreeLibrary(handle);
#else
        if (handle != nullptr) dlclose(handle);
#endif
    }
};

NativeInProcessEvaluator::NativeInProcessEvaluator(
    std::string library_path,
    std::string symbol_name)
    : impl_(std::make_shared<Impl>(std::move(library_path), std::move(symbol_name))) {}

std::vector<ScriptEvaluatorResult> NativeInProcessEvaluator::evaluate_batch(
    const std::vector<ScriptEvaluatorCase>& cases) const {
    if (cases.empty()) return {};

    std::vector<std::vector<KadokaScriptEvaluatorFeatureV1>> feature_storage(cases.size());
    std::vector<KadokaScriptEvaluatorCaseV1> abi_cases(cases.size());

    for (std::size_t i = 0; i < cases.size(); ++i) {
        auto& features = feature_storage[i];
        features.reserve(cases[i].features.size());
        for (const auto& feature : cases[i].features) {
            features.push_back({feature.key.c_str(), feature.value});
        }
        abi_cases[i] = {
            cases[i].id,
            features.empty() ? nullptr : features.data(),
            features.size(),
        };
    }

    SinkContext context;
    context.results.reserve(cases.size());
    const KadokaScriptEvaluatorSinkV1 sink{
        &context,
        &sink_begin_result,
        &sink_value,
        &sink_diagnostic,
        &sink_end_result,
    };

    const int code = impl_->function(abi_cases.data(), abi_cases.size(), &sink);
    if (code != 0) {
        throw std::runtime_error(
            "native in-process evaluator failed with code " + std::to_string(code));
    }
    if (context.failed) {
        throw std::runtime_error("native in-process evaluator output error: " + context.failure);
    }
    if (context.active) {
        throw std::runtime_error("native in-process evaluator ended with an open result");
    }

    validate_result_ids(cases, context.results);
    return context.results;
}

const std::string& NativeInProcessEvaluator::library_path() const noexcept {
    return impl_->library_path;
}

const std::string& NativeInProcessEvaluator::symbol_name() const noexcept {
    return impl_->symbol_name;
}

}  // namespace kadoka::othello
