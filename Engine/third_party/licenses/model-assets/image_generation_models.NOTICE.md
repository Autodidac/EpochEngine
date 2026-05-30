# Image Generation Model Notice Plan

Epoch does not commit image-generation model weights to tracked source. These
records exist so Package Manager download plans, generated-project opt-ins, and
future release notices have a stable source of truth before any model archive is
downloaded into executable-local `cache/models/`.

## Apache-2.0 Handling

- Ship `Engine/third_party/licenses/apache-2.0/LICENSE.txt` with any package or
  generated project that redistributes Apache-2.0 model weights.
- Preserve any upstream `NOTICE`, license, model-card attribution, and source
  URL files when a model archive is downloaded, quantized, converted, or
  redistributed.
- Add a modification notice when Epoch converts, quantizes, repackages, or
  fine-tunes any model files.
- Do not imply endorsement by Prism ML, Black Forest Labs, Hugging Face, or any
  upstream model author.
- Keep weight downloads operator-approved and executable-local by default.

## Current Local Image Lanes

- Preferred default: `prism-ml/bonsai-image-ternary-4B-mlx-2bit`
  (`https://huggingface.co/prism-ml/bonsai-image-ternary-4B-mlx-2bit`) for the
  best local quality/footprint balance.
- Low-memory option: `prism-ml/bonsai-image-binary-4B-mlx-1bit`
  (`https://huggingface.co/prism-ml/bonsai-image-binary-4B-mlx-1bit`) for the
  smallest local footprint.
- Higher-memory fallback: `black-forest-labs/FLUX.2-klein-4B`
  (`https://huggingface.co/black-forest-labs/FLUX.2-klein-4B`) when the project
  deliberately opts into the larger image lane.
