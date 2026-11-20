// Overlay for the pinned Epoch Qwen4Exp decoder. The pack and GGML types stay native.
#pragma once

#include "llama-mmap.h"
#include "llama-impl.h"

#include <ggml-alloc.h>
#include <ggml-backend.h>
#include <ggml-cpu.h>
#include <ggml.h>
#include <gguf.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace llama_edge0_detail {

// Opt-in CPU wall-time diagnostics. Async enqueue time is not CUDA kernel time;
// no synchronization is added to measure it.
struct phase_times {
    using clock = std::chrono::steady_clock;
    bool enabled = [] { const auto * value = std::getenv("EPOCH_EDGE0_PHASE_TIMING");
        return value && std::strcmp(value, "1") == 0; }();
    uint64_t prepare_ns{}, read_ns{}, enqueue_ns{}, compute_ns{}, readback_ns{};
    clock::time_point begin() const { return enabled ? clock::now() : clock::time_point{}; }
    void end(clock::time_point started, uint64_t & total) const {
        if (enabled) total += static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - started).count());
    }
};

inline constexpr int routed_experts = 10;
using expert_slots = std::array<int32_t, routed_experts>;

struct expert_upload_plan {
    expert_slots slots_by_route;
    expert_slots experts_by_slot;
    std::array<bool, routed_experts> misses;
};

inline expert_upload_plan plan_expert_uploads(const expert_slots & resident, const int32_t * selected) {
    expert_upload_plan result{{}, resident, {}};
    result.slots_by_route.fill(-1);
    std::array<bool, routed_experts> claimed{};

    // Claim every resident match before reusing stale slots. Some CUDA MUL_MAT_ID
    // paths require distinct slot IDs per token, including for repeated experts.
    for (int route = 0; route < routed_experts; ++route) {
        for (int slot = 0; slot < routed_experts; ++slot) {
            if (!claimed[slot] && resident[slot] == selected[route]) {
                claimed[slot] = true;
                result.slots_by_route[route] = slot;
                break;
            }
        }
    }
    int free_slot = 0;
    for (int route = 0; route < routed_experts; ++route) {
        if (result.slots_by_route[route] >= 0) continue;
        while (claimed[free_slot]) ++free_slot;
        result.slots_by_route[route] = free_slot;
        result.experts_by_slot[free_slot] = selected[route];
        result.misses[free_slot] = true;
        ++free_slot;
    }
    return result;
}

} // namespace llama_edge0_detail

class llama_edge0_pack {
public:
    static constexpr int layers = 48;
    static constexpr int experts = 512;
    static constexpr int active_experts = llama_edge0_detail::routed_experts;
    static constexpr int groups = 8;
    static constexpr int width = 2560;
    static constexpr int latent = 1792;
    static constexpr int intermediate = 640;
    static constexpr int ple_width = 160;

    explicit llama_edge0_pack(const std::filesystem::path & directory, uint64_t minimum_rows)
        : a(directory / "experts.bin", layers * experts * record_size()),
          du(directory / "latent_dense_q4.bin", layers * groups * (d_size() + u_size())),
          labels(directory / "labels.bin", layers * experts * sizeof(int32_t)),
          ple(read_ple(directory / "ple-fp8.gguf", minimum_rows)) {
        const auto * indices = reinterpret_cast<const int32_t *>(labels.data());
        if (std::any_of(indices, indices + layers * experts, [](int32_t group) {
                return group < 0 || group >= groups;
            })) {
            throw std::runtime_error("Edge0 pack contains an invalid expert group ID");
        }
        for (int layer = 0; layer < layers; ++layer) {
            const auto path = directory / "scales" / ("l" + std::to_string(layer) + ".f32");
            llama_file scales(path.string().c_str(), "rb");
            auto & inverse = inverse_columns[layer];
            if (scales.size() != sizeof(inverse)) {
                throw std::runtime_error("Edge0 column scale file size mismatch: " + path.string());
            }
            scales.read_raw(inverse.data(), sizeof(inverse));
            for (float & scale : inverse) {
                if (!std::isfinite(scale) || scale <= 0 || !std::isfinite(1.0f / scale)) {
                    throw std::runtime_error("Edge0 pack contains an invalid expert column scale");
                }
                scale = 1.0f / scale;
            }
            dispatch[layer] = {this, layer};
        }
        for (auto type : {GGML_TYPE_Q4_0, GGML_TYPE_Q3_K, GGML_TYPE_IQ4_NL}) {
            const auto * traits = ggml_get_type_traits_cpu(type);
            const auto * activation = traits ? ggml_get_type_traits_cpu(traits->vec_dot_type) : nullptr;
            if (!traits || !traits->vec_dot || !activation || !activation->from_float) {
                throw std::runtime_error("This GGML CPU backend lacks an Edge0 quantization kernel");
            }
        }
        const char * selected = std::getenv("EPOCH_EDGE0_EXPERT_DEVICE");
        if (!selected || std::string(selected) != "cpu") {
            device = ggml_backend_dev_by_name(selected ? selected : "CUDA0");
            if (device) gpu.reset(ggml_backend_dev_init(device, nullptr));
            if (selected && !gpu) throw std::runtime_error("Requested Edge0 expert device is unavailable");
        }
        if (gpu) {
            const auto host_type = ggml_backend_dev_host_buffer_type(device);
            if (host_type) gpu_staging.reset(ggml_backend_buft_alloc_buffer(host_type, active_experts * record_size()));
            if (!gpu_staging) throw std::runtime_error("Cannot allocate bounded Edge0 GPU upload staging");
            expert_read_buffer.resize(record_size());
        }
        LLAMA_LOG_INFO("Edge0 routed experts: %s; bounded quantized residency, FP8 row streaming\n",
                       gpu ? ggml_backend_name(gpu.get()) : "CPU");
    }

    void read_embedding(int32_t row, float * output) const {
        if (row < 0 || static_cast<uint64_t>(row) >= ple.rows) {
            throw std::runtime_error("Edge0 embedding row is out of range");
        }
        const auto * source = ple.file.data() + ple.offset + size_t(row) * ple_width;
        for (int column = 0; column < ple_width; ++column) {
            const uint8_t value = source[column];
            const int exponent = (value >> 3) & 15;
            const int fraction = value & 7;
            if (exponent == 15 && fraction == 7) {
                throw std::runtime_error("Edge0 embedding table contains a nonfinite FP8 value");
            }
            const int significand = fraction + (exponent ? 8 : 0);
            const float magnitude = std::ldexp(float(significand), std::max(1, exponent) - 10) * ple.scale;
            output[column] = value & 0x80 ? -magnitude : magnitude;
        }
    }

    ggml_tensor * build(ggml_context * ctx, ggml_tensor * input, ggml_tensor * ids, int layer) {
        GGML_ASSERT(layer >= 0 && layer < layers && input->type == GGML_TYPE_F32 && input->ne[0] == width);
        GGML_ASSERT(ids->type == GGML_TYPE_I32 && ids->ne[0] == active_experts && ids->ne[1] == input->ne[1]);
        ggml_tensor * args[] = {input, ids};
        return ggml_custom_4d(ctx, GGML_TYPE_F32, width, ids->ne[0], input->ne[1], 1, args, 2,
                              evaluate, gpu ? 1 : GGML_N_TASKS_MAX, &dispatch[layer]);
    }

private:
    class mapped_file {
    public:
        mapped_file(const std::filesystem::path & path, uint64_t expected)
            : source(open(path, expected)), mapping(std::make_unique<llama_mmap>(source.get(), 0)) {}

        const uint8_t * data() const { return static_cast<const uint8_t *>(mapping->addr()); }

        // Buffered file reads keep cold expert pages in the reclaimable OS file
        // cache instead of faulting the entire pack into the process working set.
        // The caller serializes seek/read through the pack's GPU mutex.
        void read(size_t offset, void * destination, size_t bytes) const {
            GGML_ASSERT(offset <= source->size() && bytes <= source->size() - offset);
            source->seek(offset, SEEK_SET);
            source->read_raw(destination, bytes);
        }

    private:
        static std::unique_ptr<llama_file> open(const std::filesystem::path & path, uint64_t expected) {
            if (!llama_mmap::SUPPORTED) throw std::runtime_error("Edge0 requires memory mapping");
            auto file = std::make_unique<llama_file>(path.string().c_str(), "rb");
            if (file->size() != expected) {
                throw std::runtime_error("Edge0 pack file size mismatch: " + path.string());
            }
            return file;
        }
        std::unique_ptr<llama_file> source;
        std::unique_ptr<llama_mmap> mapping;
    };

    struct embedding_table {
        mapped_file file;
        size_t offset;
        uint64_t rows;
        float scale;
    };

    static embedding_table read_ple(const std::filesystem::path & path, uint64_t minimum_rows) {
        const std::string name = path.string();
        std::unique_ptr<gguf_context, decltype(&gguf_free)> metadata(
            gguf_init_from_file(name.c_str(), {true, nullptr}), gguf_free);
        if (!metadata) throw std::runtime_error("Cannot read the Edge0 FP8 embedding header: " + name);
        const auto * header = metadata.get();
        const auto tensor = gguf_find_tensor(header, "per_layer_token_embd.weight");
        const auto format = gguf_find_key(header, "strata.ple.format");
        const auto scale_key = gguf_find_key(header, "strata.ple.scale");
        if (tensor < 0 || format < 0 || scale_key < 0 || gguf_get_n_tensors(header) != 1 ||
            gguf_get_kv_type(header, format) != GGUF_TYPE_STRING ||
            gguf_get_kv_type(header, scale_key) != GGUF_TYPE_FLOAT32 ||
            std::string(gguf_get_val_str(header, format)) != "f8_e4m3" ||
            gguf_get_tensor_type(header, tensor) != GGML_TYPE_I8) {
            throw std::runtime_error("Unsupported Edge0 FP8 embedding header: " + name);
        }
        const auto * shape = gguf_get_tensor_ne(header, tensor);
        const float scale = gguf_get_val_f32(header, scale_key);
        if (shape[0] != ple_width || shape[1] <= 0 || uint64_t(shape[1]) < minimum_rows ||
            shape[2] != 1 || shape[3] != 1 || !std::isfinite(scale * 448.0f) || scale <= 0) {
            throw std::runtime_error("Invalid Edge0 FP8 embedding geometry or scale");
        }
        const auto offset = gguf_get_data_offset(header) + gguf_get_tensor_offset(header, tensor);
        const auto bytes = gguf_get_tensor_size(header, tensor);
        if (offset > SIZE_MAX - bytes) throw std::runtime_error("Edge0 FP8 embedding size overflow");
        return {mapped_file(path, offset + bytes), offset, uint64_t(shape[1]), scale};
    }

    static size_t d_size() { return ggml_row_size(GGML_TYPE_Q4_0, width) * latent; }
    static size_t u_size() { return ggml_row_size(GGML_TYPE_Q4_0, latent) * width; }
    static size_t gate_size() { return ggml_row_size(GGML_TYPE_Q3_K, latent) * intermediate; }
    static size_t record_size() {
        return 2 * gate_size() + ggml_row_size(GGML_TYPE_IQ4_NL, intermediate) * latent;
    }

    static void project(ggml_type type, const uint8_t * matrix, const float * input,
                        int columns, int rows, float * result) {
        const auto & dot = *ggml_get_type_traits_cpu(type);
        alignas(64) std::array<float, width> quantized;
        GGML_ASSERT(ggml_row_size(dot.vec_dot_type, columns) <= sizeof(quantized));
        ggml_get_type_traits_cpu(dot.vec_dot_type)->from_float(input, quantized.data(), columns);
        const size_t stride = ggml_row_size(type, columns);
        for (int row = 0; row < rows; ++row) {
            dot.vec_dot(columns, result + row, 0, matrix + row * stride, 0, quantized.data(), 0, 1);
        }
    }

    struct layer_dispatch {
        const llama_edge0_pack * pack;
        int layer;
    };

    // The outer scheduler owns input/output staging. A private backend stream
    // executes native quantized GGML operations, never a second model runtime.
    // Serializing this stream also supports multiple contexts sharing one model.
    struct gpu_layer {
        std::unique_ptr<ggml_context, decltype(&ggml_free)> context{nullptr, ggml_free};
        std::unique_ptr<ggml_backend_buffer, decltype(&ggml_backend_buffer_free)> buffer{nullptr, ggml_backend_buffer_free};
        ggml_tensor * input = nullptr;
        ggml_tensor * group_ids = nullptr;
        ggml_tensor * slots = nullptr;
        ggml_tensor * gate = nullptr;
        ggml_tensor * up = nullptr;
        ggml_tensor * down = nullptr;
        ggml_tensor * output = nullptr;
        ggml_cgraph * graph = nullptr;
        llama_edge0_detail::expert_slots resident;
        uint64_t last_use = 0;
        size_t bytes = 0;
    };

    gpu_layer * prepare_gpu_layer(int layer) const {
        if (gpu_layers[layer]) return gpu_layers[layer].get();
        auto state = std::make_unique<gpu_layer>();
        state->context.reset(ggml_init({1 << 20, nullptr, true}));
        if (!state->context) return nullptr;
        const auto ctx = state->context.get();
        auto * d = ggml_new_tensor_3d(ctx, GGML_TYPE_Q4_0, width, latent, groups);
        auto * u = ggml_new_tensor_3d(ctx, GGML_TYPE_Q4_0, latent, width, groups);
        state->gate = ggml_new_tensor_3d(ctx, GGML_TYPE_Q3_K, latent, intermediate, active_experts);
        state->up = ggml_new_tensor_3d(ctx, GGML_TYPE_Q3_K, latent, intermediate, active_experts);
        state->down = ggml_new_tensor_3d(ctx, GGML_TYPE_IQ4_NL, intermediate, latent, active_experts);
        state->input = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, width, 1, 1);
        state->group_ids = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, active_experts, 1);
        state->slots = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, active_experts, 1);
        auto * scales = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, latent, 1, 1);
        auto * projected = ggml_mul(ctx, ggml_mul_mat_id(ctx, d, state->input, state->group_ids), scales);
        auto * gate = ggml_silu(ctx, ggml_mul_mat_id(ctx, state->gate, projected, state->slots));
        auto * up = ggml_mul_mat_id(ctx, state->up, projected, state->slots);
        auto * down = ggml_mul_mat_id(ctx, state->down, ggml_mul(ctx, gate, up), state->slots);
        state->output = ggml_mul_mat_id(ctx, u, down, state->group_ids);
        state->graph = ggml_new_graph(ctx);
        ggml_build_forward_expand(state->graph, state->output);
        for (int i = 0; i < ggml_graph_n_nodes(state->graph); ++i) {
            if (!ggml_backend_supports_op(gpu.get(), ggml_graph_node(state->graph, i))) {
                GGML_ABORT("Edge0 expert device lacks a required native operation");
            }
        }
        size_t estimate = 0;
        for (auto * tensor = ggml_get_first_tensor(ctx); tensor; tensor = ggml_get_next_tensor(ctx, tensor)) {
            estimate += ggml_nbytes(tensor) + 256;
        }
        // Bound pack-owned allocations to 3 GiB and keep 1.5 GiB free for the
        // host graph and desktop. Eviction never removes in-flight graph data.
        constexpr size_t budget = size_t(3) << 30;
        constexpr size_t reserve = size_t(1536) << 20;
        for (;;) {
            size_t free = 0, total = 0;
            ggml_backend_dev_memory(device, &free, &total);
            if (gpu_bytes + estimate <= budget && free >= estimate + reserve) break;
            int oldest = -1;
            for (int i = 0; i < layers; ++i) {
                if (gpu_layers[i] && (oldest < 0 || gpu_layers[i]->last_use < gpu_layers[oldest]->last_use)) oldest = i;
            }
            if (oldest < 0) return nullptr;
            gpu_bytes -= gpu_layers[oldest]->bytes;
            gpu_layers[oldest].reset();
        }
        state->buffer.reset(ggml_backend_alloc_ctx_tensors(ctx, gpu.get()));
        if (!state->buffer) return nullptr;
        state->bytes = ggml_backend_buffer_get_size(state->buffer.get());
        const auto * dense = du.data() + layer * groups * (d_size() + u_size());
        ggml_backend_tensor_set(d, dense, 0, groups * d_size());
        ggml_backend_tensor_set(u, dense + groups * d_size(), 0, groups * u_size());
        ggml_backend_tensor_set(scales, inverse_columns[layer].data(), 0, latent * sizeof(float));
        state->resident.fill(-1);
        gpu_bytes += state->bytes;
        gpu_layers[layer] = std::move(state);
        return gpu_layers[layer].get();
    }

    int upload_missing_experts(gpu_layer & state, int layer, const llama_edge0_detail::expert_upload_plan & plan) const {
        const int misses = static_cast<int>(std::count(plan.misses.begin(), plan.misses.end(), true));
        if (misses == 0) return 0;
        auto * gate_staging = static_cast<uint8_t *>(ggml_backend_buffer_get_base(gpu_staging.get()));
        const auto gate_bytes = gate_size();
        const auto record_bytes = record_size();
        const auto down_bytes = record_bytes - 2 * gate_bytes;
        auto * up_staging = gate_staging + active_experts * gate_bytes;
        auto * down_staging = up_staging + active_experts * gate_bytes;

        const auto read_started = gpu_phases.begin();
        // Finish all potentially throwing file reads before changing GPU data.
        // Duplicate misses still need distinct slots, but can share a file read.
        for (int slot = 0; slot < active_experts; ++slot) {
            if (!plan.misses[slot]) continue;
            const int expert = plan.experts_by_slot[slot];
            int previous_slot = 0;
            while (previous_slot < slot && (!plan.misses[previous_slot] || plan.experts_by_slot[previous_slot] != expert)) {
                ++previous_slot;
            }
            if (previous_slot == slot) {
                a.read((layer * experts + expert) * record_bytes, expert_read_buffer.data(), record_bytes);
                gpu_file_read_bytes += record_bytes;
                const auto * record = expert_read_buffer.data();
                std::memcpy(gate_staging + slot * gate_bytes, record, gate_bytes);
                std::memcpy(up_staging + slot * gate_bytes, record + gate_bytes, gate_bytes);
                std::memcpy(down_staging + slot * down_bytes, record + 2 * gate_bytes, down_bytes);
            } else {
                std::memcpy(gate_staging + slot * gate_bytes, gate_staging + previous_slot * gate_bytes, gate_bytes);
                std::memcpy(up_staging + slot * gate_bytes, up_staging + previous_slot * gate_bytes, gate_bytes);
                std::memcpy(down_staging + slot * down_bytes, down_staging + previous_slot * down_bytes, down_bytes);
            }
        }

        gpu_phases.end(read_started, gpu_phases.read_ns);
        const auto enqueue_started = gpu_phases.begin();
        // Batch adjacent misses using pinned staging. The GPU graph completes
        // under gpu_mutex before another token can reuse any of these bytes.
        int begin = 0;
        while (begin < active_experts) {
            if (!plan.misses[begin]) {
                ++begin;
                continue;
            }
            int end = begin + 1;
            while (end < active_experts && plan.misses[end]) ++end;
            const auto gate_offset = begin * gate_bytes;
            const auto down_offset = begin * down_bytes;
            const auto gate_count = (end - begin) * gate_bytes;
            const auto down_count = (end - begin) * down_bytes;
            ggml_backend_tensor_set_async(gpu.get(), state.gate, gate_staging + gate_offset, gate_offset, gate_count);
            ggml_backend_tensor_set_async(gpu.get(), state.up, up_staging + gate_offset, gate_offset, gate_count);
            ggml_backend_tensor_set_async(gpu.get(), state.down, down_staging + down_offset, down_offset, down_count);
            gpu_upload_bytes += 2 * gate_count + down_count;
            begin = end;
        }
        gpu_phases.end(enqueue_started, gpu_phases.enqueue_ns);
        return misses;
    }

    void report_gpu_progress() const {
        LLAMA_LOG_INFO("Edge0 GPU experts: %llu layer-tokens, %llu expert upload bytes, %zu resident bytes, "
                       "%llu slot hits, %llu slot misses, %llu expert file-read bytes\n",
            static_cast<unsigned long long>(gpu_tokens), static_cast<unsigned long long>(gpu_upload_bytes), gpu_bytes,
            static_cast<unsigned long long>(gpu_slot_hits), static_cast<unsigned long long>(gpu_slot_misses),
            static_cast<unsigned long long>(gpu_file_read_bytes));
        if (gpu_phases.enabled) {
            LLAMA_LOG_INFO("Edge0 expert CPU wall phases (cumulative ms; enqueue is not GPU time): "
                "prepare=%.3f read_stage=%.3f enqueue=%.3f compute_wait=%.3f readback=%.3f\n",
                gpu_phases.prepare_ns / 1e6, gpu_phases.read_ns / 1e6,
                gpu_phases.enqueue_ns / 1e6, gpu_phases.compute_ns / 1e6, gpu_phases.readback_ns / 1e6);
        }
    }

    bool evaluate_gpu(ggml_tensor * output, int layer) const {
        std::lock_guard lock(gpu_mutex);
        const auto prepare_started = gpu_phases.begin();
        auto * state = prepare_gpu_layer(layer);
        gpu_phases.end(prepare_started, gpu_phases.prepare_ns);
        if (!state) return false;
        state->last_use = ++gpu_clock;
        const auto * input = output->src[0];
        const auto * ids = output->src[1];
        const auto * group_labels = reinterpret_cast<const int32_t *>(labels.data()) + layer * experts;
        for (int64_t token = 0; token < ids->ne[1]; ++token) {
            const auto * selected = reinterpret_cast<const int32_t *>(static_cast<const char *>(ids->data) + token * ids->nb[1]);
            std::array<int32_t, active_experts> selected_groups;
            for (int slot = 0; slot < active_experts; ++slot) {
                const int32_t expert = selected[slot];
                GGML_ASSERT(expert >= 0 && expert < experts);
                selected_groups[slot] = group_labels[expert];
            }
            const auto plan = llama_edge0_detail::plan_expert_uploads(state->resident, selected);
            const int misses = upload_missing_experts(*state, layer, plan);
            const auto enqueue_started = gpu_phases.begin();
            ggml_backend_tensor_set_async(gpu.get(), state->input, static_cast<const char *>(input->data) + token * input->nb[1], 0, width * sizeof(float));
            ggml_backend_tensor_set_async(gpu.get(), state->group_ids, selected_groups.data(), 0, sizeof(selected_groups));
            ggml_backend_tensor_set_async(gpu.get(), state->slots, plan.slots_by_route.data(), 0, sizeof(plan.slots_by_route));
            gpu_phases.end(enqueue_started, gpu_phases.enqueue_ns);
            const auto compute_started = gpu_phases.begin();
            if (ggml_backend_graph_compute(gpu.get(), state->graph) != GGML_STATUS_SUCCESS) {
                GGML_ABORT("Edge0 native GPU expert graph failed");
            }
            gpu_phases.end(compute_started, gpu_phases.compute_ns);
            const auto readback_started = gpu_phases.begin();
            ggml_backend_tensor_get(state->output, static_cast<char *>(output->data) + token * output->nb[2],
                                    0, width * active_experts * sizeof(float));
            gpu_phases.end(readback_started, gpu_phases.readback_ns);
            state->resident = plan.experts_by_slot;
            gpu_slot_hits += active_experts - misses;
            gpu_slot_misses += misses;
            ++gpu_tokens;
            if (gpu_tokens == layers || gpu_tokens % 4096 == 0) report_gpu_progress();
        }
        return true;
    }

    static void evaluate(ggml_tensor * output, int thread, int threads, void * user_data) {
        const auto & job = *static_cast<const layer_dispatch *>(user_data);
        const auto & pack = *job.pack;
        if (pack.gpu) {
            if (pack.evaluate_gpu(output, job.layer)) return;
            if (!pack.gpu_fallback_reported.exchange(true)) {
                LLAMA_LOG_WARN("Edge0 GPU expert allocation reserve reached; using CPU for this layer\n");
            }
        }
        const auto * layer_du = pack.du.data() + job.layer * groups * (d_size() + u_size());
        const auto * labels = reinterpret_cast<const int32_t *>(pack.labels.data()) + job.layer * experts;
        const auto * input = output->src[0];
        const auto * ids = output->src[1];
        std::array<float, latent> projected;
        std::array<float, latent> down;
        std::array<float, intermediate> gate;
        std::array<float, intermediate> up;
        for (int64_t item = thread; item < ids->ne[0] * ids->ne[1]; item += threads) {
            const int64_t token = item / ids->ne[0];
            const int64_t slot = item % ids->ne[0];
            const auto * selected = reinterpret_cast<const int32_t *>(static_cast<const char *>(ids->data) + token * ids->nb[1]);
            const int32_t expert = selected[slot];
            GGML_ASSERT(expert >= 0 && expert < experts);
            const int32_t group = labels[expert];
            const auto * record = pack.a.data() + (job.layer * experts + expert) * record_size();
            const auto * activation = reinterpret_cast<const float *>(static_cast<const char *>(input->data) + token * input->nb[1]);
            auto * destination = reinterpret_cast<float *>(static_cast<char *>(output->data) + token * output->nb[2] + slot * output->nb[1]);

            project(GGML_TYPE_Q4_0, layer_du + group * d_size(), activation, width, latent, projected.data());
            for (int i = 0; i < latent; ++i) projected[i] *= pack.inverse_columns[job.layer][i];
            project(GGML_TYPE_Q3_K, record, projected.data(), latent, intermediate, gate.data());
            project(GGML_TYPE_Q3_K, record + gate_size(), projected.data(), latent, intermediate, up.data());
            for (int i = 0; i < intermediate; ++i) gate[i] = gate[i] / (1.0f + std::exp(-gate[i])) * up[i];
            project(GGML_TYPE_IQ4_NL, record + 2 * gate_size(), gate.data(), intermediate, latent, down.data());
            project(GGML_TYPE_Q4_0, layer_du + groups * d_size() + group * u_size(), down.data(), latent, width, destination);
        }
    }

    mapped_file a;
    mapped_file du;
    mapped_file labels;
    embedding_table ple;
    std::array<std::array<float, latent>, layers> inverse_columns{};
    std::array<layer_dispatch, layers> dispatch{};
    ggml_backend_dev_t device = nullptr;
    std::unique_ptr<ggml_backend, decltype(&ggml_backend_free)> gpu{nullptr, ggml_backend_free};
    std::unique_ptr<ggml_backend_buffer, decltype(&ggml_backend_buffer_free)> gpu_staging{nullptr, ggml_backend_buffer_free};
    mutable std::vector<uint8_t> expert_read_buffer;
    mutable std::mutex gpu_mutex;
    mutable llama_edge0_detail::phase_times gpu_phases;
    mutable std::array<std::unique_ptr<gpu_layer>, layers> gpu_layers{};
    mutable size_t gpu_bytes = 0;
    mutable uint64_t gpu_clock = 0, gpu_tokens = 0, gpu_upload_bytes = 0;
    mutable uint64_t gpu_slot_hits = 0, gpu_slot_misses = 0, gpu_file_read_bytes = 0;
    mutable std::atomic<bool> gpu_fallback_reported{false};

public:
    ~llama_edge0_pack() {
        if (gpu) report_gpu_progress();
    }
};
