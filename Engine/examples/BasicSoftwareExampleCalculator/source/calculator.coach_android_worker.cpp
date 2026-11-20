// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include "calculator.coach.hpp"

#include <ggml-backend.h>
#include <jni.h>
#include <llama.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;

    constexpr std::int32_t context_tokens = 1024;
    constexpr std::int32_t batch_tokens = 128;
    constexpr std::int32_t maximum_output_tokens = 96;
    constexpr auto generation_timeout = 45s;

    constexpr char coach_grammar[] = R"grammar(
root ::= "CALC " unit? expression | "HELP " help | "CLARIFY " clarify
unit ::= "DEG " | "RAD "
expression ::= term (space [+-] space term)*
term ::= factor (space [*/] space factor)*
factor ::= [+-]? atom (space "^" space factor)?
atom ::= number | "pi" | "e" | "(" space expression space ")" | function "(" space expression space ")"
number ::= ([0-9]+ ("." [0-9]*)? | "." [0-9]+) ([eE] [+-]? [0-9]+)?
function ::= "sqrt" | "abs" | "sin" | "cos" | "tan" | "asin" | "acos" | "atan" | "ln" | "log" | "log10" | "exp"
space ::= " "?
help ::= "syntax" | "precedence" | "angles" | "functions" | "constants" | "errors" | "circle_distance" | "percentages"
clarify ::= "expression" | "angle_unit" | "unsupported" | "circle_distance" | "discount"
)grammar";

    constexpr std::string_view system_prompt =
        "<|im_start|>system\n"
        "Translate the user's request into exactly one calculator command. Do not do the math. "
        "Copy the numbers from the request and write an expression after CALC. "
        "Use + - * / ^ and supported one-argument functions. Express percentages by dividing by 100. "
        "Add DEG or RAD only when explicitly requested. Otherwise use the current setting if appropriate. "
        "Use HELP for a question about how to use the calculator. Use CLARIFY if the request is ambiguous "
        "or unsupported. No prose or code. Examples:\n"
        "What is 9 divided by 4? => CALC 9/4\n"
        "Sine of 30 degrees => CALC DEG sin(30)\n"
        "How do powers group? => HELP precedence\n"
        "Find the distance between circles. => CLARIFY circle_distance\n"
        "Current angle setting: ";
    constexpr std::string_view user_marker = ".<|im_end|>\n<|im_start|>user\n";
    constexpr std::string_view assistant_marker =
        "<|im_end|>\n<|im_start|>assistant\n<think>\n\n</think>\n\n";

    enum class GenerationState { idle, prepared, running, cancelled };
    std::atomic<GenerationState> generation_state{GenerationState::idle};

    class Generation final
    {
    public:
        Generation() noexcept
        {
            auto expected = GenerationState::prepared;
            active_ = generation_state.compare_exchange_strong(expected, GenerationState::running);
        }

        Generation(const Generation&) = delete;
        Generation& operator=(const Generation&) = delete;

        ~Generation()
        {
            if (active_) generation_state.store(GenerationState::idle);
        }

        [[nodiscard]] bool stop_requested() const noexcept
        {
            return !active_ || generation_state.load() != GenerationState::running
                || Clock::now() >= deadline_;
        }

    private:
        const Clock::time_point deadline_ = Clock::now() + generation_timeout;
        bool active_{};
    };

    [[nodiscard]] bool load_continue(float, void* generation) noexcept
    {
        return !static_cast<const Generation*>(generation)->stop_requested();
    }

    [[nodiscard]] bool decode_abort(void* generation) noexcept
    {
        return static_cast<const Generation*>(generation)->stop_requested();
    }

    void append_utf8(std::string& text, std::uint32_t code_point)
    {
        const auto continuation = [&](int shift) {
            text.push_back(static_cast<char>(0x80u | ((code_point >> shift) & 0x3fu)));
        };

        if (code_point < 0x80u)
            text.push_back(static_cast<char>(code_point));
        else if (code_point < 0x800u)
        {
            text.push_back(static_cast<char>(0xc0u | (code_point >> 6)));
            continuation(0);
        }
        else if (code_point < 0x10000u)
        {
            text.push_back(static_cast<char>(0xe0u | (code_point >> 12)));
            continuation(6);
            continuation(0);
        }
        else
        {
            text.push_back(static_cast<char>(0xf0u | (code_point >> 18)));
            continuation(12);
            continuation(6);
            continuation(0);
        }
    }

    [[nodiscard]] std::string read_question(JNIEnv& env, jstring value)
    {
        if (!value) return {};
        const auto units = env.GetStringLength(value);
        constexpr auto limit = calculator_example::maximum_coach_question_bytes;
        if (units <= 0 || units > static_cast<jsize>(limit)) return {};

        // JNI's modified UTF-8 does not encode surrogate pairs as standard UTF-8.
        std::array<jchar, limit> input{};
        env.GetStringRegion(value, 0, units, input.data());
        if (env.ExceptionCheck()) return {};

        std::string question;
        question.reserve(static_cast<std::size_t>(units));
        for (jsize index = 0; index < units; ++index)
        {
            std::uint32_t code_point = input[static_cast<std::size_t>(index)];
            if (code_point >= 0xd800u && code_point <= 0xdfffu)
            {
                if (code_point >= 0xdc00u || ++index == units) return {};
                const auto low = input[static_cast<std::size_t>(index)];
                if (low < 0xdc00u || low > 0xdfffu) return {};
                code_point = 0x10000u + ((code_point - 0xd800u) << 10) + (low - 0xdc00u);
            }
            append_utf8(question, code_point);
            if (question.size() > limit) return {};
        }
        if (!calculator_example::valid_coach_question(question)) return {};
        return question;
    }

    using ModelFile = std::unique_ptr<FILE, decltype(&std::fclose)>;

    [[nodiscard]] ModelFile borrow_model_file(int descriptor) noexcept
    {
        ModelFile file{nullptr, std::fclose};
        struct stat status{};
        const int flags = ::fcntl(descriptor, F_GETFL);
        if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY || ::fstat(descriptor, &status) != 0
            || !S_ISREG(status.st_mode))
            return file;

        const int copy = ::fcntl(descriptor, F_DUPFD_CLOEXEC, 0);
        if (copy < 0) return file;
        file.reset(::fdopen(copy, "rb"));
        if (!file)
        {
            ::close(copy);
            return file;
        }
        if (std::fseek(file.get(), 0, SEEK_SET) != 0) file.reset();
        return file;
    }

    [[nodiscard]] std::int32_t encode_prompt(const llama_vocab* vocabulary,
        std::string_view question, bool degrees, std::array<llama_token, context_tokens>& tokens)
    {
        std::string prefix{system_prompt};
        prefix += degrees ? "degrees" : "radians";
        prefix += user_marker;

        constexpr auto capacity = context_tokens - maximum_output_tokens;
        std::int32_t used{};
        const auto append = [&](std::string_view text, bool template_text) {
            const auto count = llama_tokenize(vocabulary, text.data(), static_cast<std::int32_t>(text.size()),
                tokens.data() + used, capacity - used, false, template_text);
            if (count <= 0) return false;
            used += count;
            return true;
        };

        // Tokenize the question as text even if it spells a template control token.
        if (!append(prefix, true) || !append(question, false) || !append(assistant_marker, true)) return 0;
        return used;
    }

    [[nodiscard]] bool decode(llama_context* context, llama_batch_ext* batch,
        const llama_token* tokens, std::int32_t count, llama_pos start, bool sample_after)
    {
        llama_batch_ext_clear(batch);
        for (std::int32_t index = 0; index < count; ++index)
        {
            const auto entry = llama_batch_ext_add_token(batch, 0, tokens[index]);
            const llama_pos position = start + index;
            if (entry < 0 || !llama_batch_ext_set_pos(batch, entry, &position)
                || !llama_batch_ext_set_output_logits(batch, entry, sample_after && index + 1 == count))
                return false;
        }
        return llama_process(context, LLAMA_PROCESS_TYPE_DECODE, batch) == 0;
    }

    [[nodiscard]] std::string generate_reply(int descriptor, std::string_view question,
        bool degrees, Generation& generation)
    {
        auto file = borrow_model_file(descriptor);
        if (!file || generation.stop_requested()) return {};

        // A statically linked CPU backend has already registered itself here.
        // Refuse to initialize if llama.cpp would need to discover backends.
        if (ggml_backend_dev_count() != 1 || !ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_CPU)) return {};
        static std::once_flag backend_initialized;
        std::call_once(backend_initialized, llama_backend_init);

        auto model_options = llama_model_default_params();
        ggml_backend_dev_t no_offload_devices[] = {nullptr};
        model_options.devices = no_offload_devices;
        model_options.n_gpu_layers = 0;
        model_options.load_mode = LLAMA_LOAD_MODE_MMAP;
        model_options.progress_callback = load_continue;
        model_options.progress_callback_user_data = &generation;
        std::unique_ptr<llama_model, decltype(&llama_model_free)> model{
            llama_model_load_from_file_ptr(file.get(), model_options), llama_model_free};
        if (!model || generation.stop_requested()) return {};

        const auto* vocabulary = llama_model_get_vocab(model.get());
        if (!vocabulary) return {};
        std::array<llama_token, context_tokens> prompt{};
        const auto prompt_size = encode_prompt(vocabulary, question, degrees, prompt);
        if (prompt_size == 0 || generation.stop_requested()) return {};

        auto context_options = llama_context_default_params();
        context_options.n_ctx = context_tokens;
        context_options.n_batch = batch_tokens;
        context_options.n_ubatch = batch_tokens;
        context_options.n_seq_max = 1;
        context_options.n_threads = 2;
        context_options.n_threads_batch = 2;
        context_options.offload_kqv = false;
        context_options.op_offload = false;
        context_options.abort_callback = decode_abort;
        context_options.abort_callback_data = &generation;
        std::unique_ptr<llama_context, decltype(&llama_free)> context{
            llama_init_from_model(model.get(), context_options), llama_free};
        if (!context || generation.stop_requested()) return {};
        std::unique_ptr<llama_batch_ext, decltype(&llama_batch_ext_free)> batch{
            llama_batch_ext_init(context.get()), llama_batch_ext_free};
        if (!batch) return {};

        using Sampler = std::unique_ptr<llama_sampler, decltype(&llama_sampler_free)>;
        Sampler chain{llama_sampler_chain_init(llama_sampler_chain_default_params()), llama_sampler_free};
        Sampler grammar{llama_sampler_init_grammar(vocabulary, coach_grammar, "root"), llama_sampler_free};
        Sampler greedy{llama_sampler_init_greedy(), llama_sampler_free};
        if (!chain || !grammar || !greedy) return {};
        llama_sampler_chain_add(chain.get(), grammar.release());
        llama_sampler_chain_add(chain.get(), greedy.release());

        for (llama_pos position = 0; position < prompt_size;)
        {
            const auto size = (std::min)(batch_tokens, prompt_size - position);
            if (generation.stop_requested() || !decode(context.get(), batch.get(), prompt.data() + position,
                    size, position, position + size == prompt_size))
                return {};
            position += size;
        }

        std::string reply;
        std::array<char, calculator_example::maximum_coach_reply_bytes> piece{};
        for (std::int32_t index = 0; index < maximum_output_tokens; ++index)
        {
            if (generation.stop_requested()) return {};
            const auto token = llama_sampler_sample(chain.get(), context.get(), -1);
            if (token == LLAMA_TOKEN_NULL) return {};
            if (llama_vocab_is_eog(vocabulary, token)) return reply;

            const auto size = llama_token_to_piece(vocabulary, token, piece.data(),
                static_cast<std::int32_t>(piece.size()), 0, false);
            if (size <= 0 || reply.size() + static_cast<std::size_t>(size) > piece.size()) return {};
            for (std::int32_t offset = 0; offset < size; ++offset)
            {
                const auto byte = static_cast<unsigned char>(piece[static_cast<std::size_t>(offset)]);
                if (byte < 32u || byte > 126u) return {};
            }
            reply.append(piece.data(), static_cast<std::size_t>(size));
            if (!decode(context.get(), batch.get(), &token, 1, prompt_size + index, true)) return {};
        }
        return {};
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_epochengine_runtime_EpochCalculatorCoachService_prepareGeneration(JNIEnv*, jclass)
{
    auto expected = GenerationState::idle;
    (void)generation_state.compare_exchange_strong(expected, GenerationState::prepared);
}

extern "C" JNIEXPORT void JNICALL
Java_org_epochengine_runtime_EpochCalculatorCoachService_cancelGeneration(JNIEnv*, jclass)
{
    auto state = generation_state.load();
    while (state == GenerationState::prepared || state == GenerationState::running)
    {
        const auto cancelled = state == GenerationState::prepared ? GenerationState::idle : GenerationState::cancelled;
        if (generation_state.compare_exchange_weak(state, cancelled)) return;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_epochengine_runtime_EpochCalculatorCoachService_generate(JNIEnv* env, jclass,
    jint descriptor, jstring question, jboolean degrees)
{
    Generation generation;
    if (generation.stop_requested()) return nullptr;
    try
    {
        const auto text = read_question(*env, question);
        if (text.empty()) return nullptr;
        const auto reply = generate_reply(descriptor, text, degrees == JNI_TRUE, generation);
        if (reply.empty() || generation.stop_requested()) return nullptr;
        return env->NewStringUTF(reply.c_str());
    }
    catch (...)
    {
        return nullptr;
    }
}
