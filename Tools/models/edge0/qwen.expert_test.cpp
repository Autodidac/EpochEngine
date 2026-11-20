// Real-pack CPU/CUDA parity, changing selections, multi-token and reuse check.
#include "qwen.edge0_pack.hpp"
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <vector>

std::string format(const char * pattern, ...) {
    va_list args;
    va_start(args, pattern);
    char text[4096];
    vsnprintf(text, sizeof(text), pattern, args);
    va_end(args);
    return text;
}

// Standalone test logger for llama-mmap and the overlay, outside the engine.
void llama_log_internal(ggml_log_level, const char * format, ...) {
    char text[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    std::clog << text;
}

std::vector<float> run(llama_edge0_pack & pack, int layer, int pass) {
    std::unique_ptr<ggml_context, decltype(&ggml_free)> ctx(ggml_init({1 << 20, nullptr, true}), ggml_free);
    std::unique_ptr<ggml_backend, decltype(&ggml_backend_free)> cpu(ggml_backend_cpu_init(), ggml_backend_free);
    ggml_backend_cpu_set_n_threads(cpu.get(), 4);
    constexpr int tokens = 3, width = llama_edge0_pack::width, slots = llama_edge0_pack::active_experts;
    auto * x = ggml_new_tensor_2d(ctx.get(), GGML_TYPE_F32, width, tokens);
    auto * ids = ggml_new_tensor_2d(ctx.get(), GGML_TYPE_I32, slots, tokens);
    auto * out = pack.build(ctx.get(), x, ids, layer);
    auto * graph = ggml_new_graph(ctx.get());
    ggml_build_forward_expand(graph, out);
    std::unique_ptr<ggml_backend_buffer, decltype(&ggml_backend_buffer_free)> buffer(
        ggml_backend_alloc_ctx_tensors(ctx.get(), cpu.get()), ggml_backend_buffer_free);
    if (!buffer) throw std::runtime_error("CPU fixture allocation failed");
    std::array<float, width * tokens> input;
    std::array<int32_t, slots * tokens> selected;
    for (int token = 0; token < tokens; ++token) {
        for (int i = 0; i < width; ++i) input[token * width + i] = std::sin(float(i) * (0.013f + token * 0.003f));
        for (int slot = 0; slot < slots; ++slot) selected[token * slots + slot] =
            (pass * 19 + token * 67 + slot * 53) % llama_edge0_pack::experts;
    }
    selected[0] = 0;
    selected[1] = 511;
    selected[2] = 0; // Duplicate ID must preserve routing-slot ordering.
    // The third token routes to the same experts in a different order. This
    // exercises the GPU's route-to-resident-slot mapping without new uploads.
    std::rotate_copy(selected.data() + slots, selected.data() + slots + 3,
                     selected.data() + 2 * slots, selected.data() + 2 * slots);
    ggml_backend_tensor_set(x, input.data(), 0, sizeof(input));
    ggml_backend_tensor_set(ids, selected.data(), 0, sizeof(selected));
    if (ggml_backend_graph_compute(cpu.get(), graph) != GGML_STATUS_SUCCESS) throw std::runtime_error("Fixture graph failed");
    std::vector<float> values(width * slots * tokens);
    ggml_backend_tensor_get(out, values.data(), 0, values.size() * sizeof(float));
    return values;
}

int main(int argc, char ** argv) try {
    if (argc != 2) return 2;
    ggml_backend_load_all();
    _putenv_s("EPOCH_EDGE0_EXPERT_DEVICE", "cpu");
    llama_edge0_pack cpu(argv[1], 320001446);
    _putenv_s("EPOCH_EDGE0_EXPERT_DEVICE", "CUDA0");
    llama_edge0_pack cuda(argv[1], 320001446);
    for (int layer : {0, 23, 47}) {
        for (int pass : {0, 1, 1}) {
            const auto expected = run(cpu, layer, pass);
            const auto actual = run(cuda, layer, pass);
            double error = 0, norm = 0;
            for (size_t i = 0; i < expected.size(); ++i) {
                error += std::pow(double(expected[i]) - actual[i], 2);
                norm += double(expected[i]) * expected[i];
            }
            const auto relative = std::sqrt(error / norm);
            std::cout << "layer=" << layer << " pass=" << pass << " relative_l2=" << relative << '\n';
            if (!std::isfinite(relative) || norm < 1e-12 || relative > 0.07) return 3;
            for (size_t begin = 0; begin < expected.size(); begin += llama_edge0_pack::width) {
                double slot_error = 0, slot_norm = 0;
                for (int i = 0; i < llama_edge0_pack::width; ++i) {
                    slot_error += std::pow(double(expected[begin + i]) - actual[begin + i], 2);
                    slot_norm += double(expected[begin + i]) * expected[begin + i];
                }
                if (!std::isfinite(slot_error) || slot_norm < 1e-12 || std::sqrt(slot_error / slot_norm) > 0.07) return 4;
            }
        }
    }
    return 0;
} catch (const std::exception & error) {
    std::cerr << error.what() << '\n';
    return 1;
}
