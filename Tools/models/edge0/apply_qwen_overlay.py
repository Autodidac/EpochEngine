"""Apply the Edge0 graph changes to a disposable copy of the pinned Epoch decoder."""

import argparse
import hashlib
from pathlib import Path


SOURCE_DIGESTS = {
    'llama-model.cpp': '26d9ca15395256e49b80fb5782a0af958faa255102fc9f51f8c73be60d4a02f6',
    'common/fit.cpp': 'b598e410e9171da1453adaafe00483efaad1fcf3c5e8a4ccaa784398f37a43e8',
    'llama-model-loader.h': '740e957c1d1a22dfe6add5a47f294d8efeb31b293c8fbabafe393ff2fe9ac750',
    'llama-model-loader.cpp': 'a98930ae0fd1776bf7af2990cfacac31d6d3afe853974d49d6eede295ec1d5d0',
    'models/models.h': 'bb88681facd9b9b53f6d1bdc2a894b4cb0969757952bf964072573da14fb5d80',
    'models/qwen4exp.cpp': '01507c324fede8768218455d600b9999d7a113314c28f51deaed72dc73e719c3',
}


def apply(source, pack_header=None, pack_header_sha256=None):
    # An explicitly reviewed algorithm can be built without overwriting the
    # working experimental overlay. Admission happens before candidate writes.
    selected_header = Path(pack_header) if pack_header else Path(__file__).with_name('qwen.edge0_pack.hpp')
    if bool(pack_header) != bool(pack_header_sha256):
        raise ValueError('An alternate pack header requires its exact SHA-256')
    header_bytes = selected_header.read_bytes()
    if pack_header and hashlib.sha256(header_bytes).hexdigest() != pack_header_sha256.lower():
        raise ValueError('Alternate pack header does not match the reviewed SHA-256')
    files = {}

    def replace(name, old, new):
        path = source / name if name.startswith('common/') else source / 'src' / name
        current = files.get(path)
        if current is None:
            current = path.read_text(encoding='utf-8')
            if hashlib.sha256(current.encode('utf-8')).hexdigest() != SOURCE_DIGESTS[name]:
                raise ValueError(f'{name} does not match the pinned Epoch Qwen decoder')
        if current.count(old) != 1:
            raise ValueError(f'Expected exactly one overlay anchor in {name}: {old!r}')
        files[path] = current.replace(old, new)

    replace('llama-model.cpp', '#include "../ggml/src/ggml-backend-moe.h"',
            '#include "../ggml/src/ggml-backend-moe.h"\n#include "../ggml/src/ggml-backend-impl.h"\n#include <set>')
    replace('llama-model.cpp', '        for (auto buft : owners) {', '''        // Mapped model shards have distinct buffer-type wrappers but share
        // the bounded backend owner. Configure that owner only once.
        std::set<std::pair<ggml_backend_reg_t, void *>> configured_owners;
        for (auto buft : owners) {
            const auto identity = std::make_pair(
                ggml_backend_dev_backend_reg(ggml_backend_buft_get_device(buft)), buft->context);
            if (!configured_owners.insert(identity).second) {
                continue;
            }''')
    replace('common/fit.cpp', '''        ggml_log_level min_level; // prints below this log level go to debug log
    };''', '''        ggml_log_level min_level; // prints below this log level go to debug log
        ~user_data_t() {
            // The logger captures this stack object. Restore it on exceptions
            // as well as normal exits before the caller logs a fitting error.
            llama_log_set(original_logger.callback, original_logger.user_data);
        }
    };''')
    replace('llama-model-loader.h', '    llama_files files;',
            '    const std::string model_path;\n    llama_files files;')
    replace('llama-model-loader.cpp', ': metadata(meta), set_tensor_data(set_tensor_data), set_tensor_data_ud(set_tensor_data_ud) {',
            ': metadata(meta), set_tensor_data(set_tensor_data), set_tensor_data_ud(set_tensor_data_ud), model_path(fname) {')
    replace('models/models.h', 'struct llama_model_qwen4exp : public llama_model_base {',
            'class llama_edge0_pack;\n\nstruct llama_model_qwen4exp : public llama_model_base {\n'
            '    std::shared_ptr<llama_edge0_pack> edge0;')
    replace('models/qwen4exp.cpp', '#include "models.h"',
            '#include "models.h"\n#include "qwen.edge0_pack.hpp"')
    replace('models/qwen4exp.cpp', '    // flat [ple_head_dim, n_rows] gather target', '''    const std::filesystem::path source(ml.model_path);
    const bool edge0_skeleton = source.filename() == "model-skeleton.gguf" &&
            ml.get_weight("blk.0.ffn_down_exps.weight") == nullptr &&
            std::filesystem::is_regular_file(source.parent_path() / "native_experts.txt");
    if (edge0_skeleton) {
        if (n_layer != llama_edge0_pack::layers || n_embd != llama_edge0_pack::width ||
            n_expert != llama_edge0_pack::experts || n_expert_used != llama_edge0_pack::active_experts ||
            hparams.ple_head_dim != llama_edge0_pack::ple_width || hparams.ple_n_heads == 0 ||
            hparams.n_layer_all != hparams.n_layer()) {
            throw std::runtime_error("Unsupported Edge0 Qwen4Exp geometry");
        }
    }

    // flat [ple_head_dim, n_rows] gather target''')
    replace('models/qwen4exp.cpp', '''        per_layer_tok_embd = create_tensor(tn(LLM_TENSOR_PER_LAYER_TOKEN_EMBD, "weight"),
                                           { hparams.ple_head_dim, ple_rows }, TENSOR_READ_LAZY);''', '''        if (edge0_skeleton) {
            edge0 = std::make_shared<llama_edge0_pack>(source.parent_path(), uint64_t(ple_rows));
        } else {
            per_layer_tok_embd = create_tensor(tn(LLM_TENSOR_PER_LAYER_TOKEN_EMBD, "weight"),
                                               { hparams.ple_head_dim, ple_rows }, TENSOR_READ_LAZY);
        }''')
    replace('models/qwen4exp.cpp', '''        layer.ffn_down_exps = create_tensor(tn(LLM_TENSOR_FFN_DOWN_EXPS, "weight", il), { n_ff_exp, n_embd, n_expert }, flags);
        create_tensor_gate_up_exps(layer, il, n_embd, n_ff_exp, n_expert, flags);''', '''        if (!edge0) {
            layer.ffn_down_exps = create_tensor(tn(LLM_TENSOR_FFN_DOWN_EXPS, "weight", il), { n_ff_exp, n_embd, n_expert }, flags);
            create_tensor_gate_up_exps(layer, il, n_embd, n_ff_exp, n_expert, flags);
        }''')
    replace('models/qwen4exp.cpp', '''    ggml_tensor * moe_out =
        build_moe_ffn(cur,''', '''    ggml_tensor * moe_out = nullptr;
    const auto & edge0 = static_cast<const llama_model_qwen4exp &>(model).edge0;
    if (edge0) {
        auto * logits = build_lora_mm(model.layers[il].ffn_gate_inp, cur);
        auto * selected = ggml_argsort_top_k(ctx0, logits, n_expert_used);
        cb(selected, "ffn_moe_topk", il);
        auto * selected_logits = ggml_get_rows(ctx0, ggml_reshape_3d(ctx0, logits, 1, n_expert, cur->ne[1]), selected);
        auto * weights = ggml_soft_max(ctx0, ggml_reshape_2d(ctx0, selected_logits, n_expert_used, cur->ne[1]));
        weights = ggml_reshape_3d(ctx0, weights, 1, n_expert_used, cur->ne[1]);
        if (hparams.expert_weights_scale != 0.0f && hparams.expert_weights_scale != 1.0f) {
            weights = ggml_scale(ctx0, weights, hparams.expert_weights_scale);
        }
        auto * routed = edge0->build(ctx0, ggml_cast(ctx0, cur, GGML_TYPE_F32), selected, il);
        auto * weighted = ggml_mul(ctx0, routed, weights);
        for (int slot = 0; slot < n_expert_used; ++slot) {
            auto * slice = ggml_view_2d(ctx0, weighted, n_embd, cur->ne[1], weighted->nb[2], slot * weighted->nb[1]);
            moe_out = moe_out ? ggml_add(ctx0, moe_out, slice) : slice;
        }
    } else {
        moe_out = build_moe_ffn(cur,''')
    replace('models/qwen4exp.cpp', '''            model.layers[il].ffn_down_exps_s);
    cb(moe_out, "ffn_moe_out", il);''', '''            model.layers[il].ffn_down_exps_s);
    }
    cb(moe_out, "ffn_moe_out", il);''')
    replace('models/qwen4exp.cpp', '''        return rows->ne[0] == (int64_t) pmodel.hparams.ple_n_heads * params.ubatch.n_tokens;
    }

    ggml_tensor * rows = nullptr;''', '''        if (embeddings) return embeddings->ne[1] == params.ubatch.n_tokens;
        return rows->ne[0] == (int64_t) pmodel.hparams.ple_n_heads * params.ubatch.n_tokens;
    }

    ggml_tensor * embeddings = nullptr;
    ggml_tensor * rows = nullptr;''')
    replace('models/qwen4exp.cpp', '    ggml_backend_tensor_set(rows, idx.data(), 0, idx.size()*ggml_element_size(rows));', '''    if (embeddings) {
        std::vector<float> values(idx.size() * hp.ple_head_dim);
        for (size_t i = 0; i < idx.size(); ++i) {
            pmodel.edge0->read_embedding(idx[i], values.data() + i * hp.ple_head_dim);
        }
        ggml_backend_tensor_set(embeddings, values.data(), 0, values.size() * sizeof(float));
    } else {
        ggml_backend_tensor_set(rows, idx.data(), 0, idx.size()*ggml_element_size(rows));
    }''')
    replace('models/qwen4exp.cpp', '    ple_inp->rows = ggml_new_tensor_1d(ctx0, GGML_TYPE_I32, n_heads * n_tokens);', '''    if (static_cast<const llama_model_qwen4exp &>(model).edge0) {
        auto * values = ggml_new_tensor_2d(ctx0, GGML_TYPE_F32, hparams.ple_head_dim * n_heads, n_tokens);
        ggml_set_input(values);
        ple_inp->embeddings = values;
        res->add_input(std::move(ple_inp));
        cb(values, "ple_embd", -1);
        return values;
    }

    ple_inp->rows = ggml_new_tensor_1d(ctx0, GGML_TYPE_I32, n_heads * n_tokens);''')

    header = source / 'src/models/qwen.edge0_pack.hpp'
    if header.exists():
        raise ValueError(f'Overlay already present at {header}')
    header.write_bytes(header_bytes)
    for path, contents in files.items():
        path.write_text(contents, encoding='utf-8', newline='\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--pack-header', type=Path)
    parser.add_argument('--pack-header-sha256')
    options = parser.parse_args()
    apply(options.candidate, options.pack_header, options.pack_header_sha256)
