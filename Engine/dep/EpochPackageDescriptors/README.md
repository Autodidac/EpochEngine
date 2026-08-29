# Epoch package descriptors

This subtree contains small, reviewable descriptors for optional upstream
artifacts. It never contains model weights and never authorizes an automatic
download, install, service, listener, or execution path.

The Epoch Site may publish each descriptor directory as an immutable package.
Package Manager can then discover the descriptor from the Site while the local
engine admission policy and operator approval remain authoritative.

The current LLM catalog is deliberately limited to Qwen3.8 27B and NVIDIA
Nemotron 3 Nano 4B BF16. Other image-generation model lanes are separate from
the LLM category.
