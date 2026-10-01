# Epoch AI automatic intent routing and clarification - v0.90.20

## Problem
The previous UI exposed several AI modes as operator workflow state. Project authoring/tool responses also reached the visible transcript before their strict protocol parser ran, so malformed model envelopes looked like chat syntax errors and approval controls could be absent when parsing failed.

## Design
The chat composer is the single input surface. The host classifies high-confidence intent into conversation, guarded project authoring, guarded project tooling, or isolated engine source work. A mutation whose target is not clear enters a host-owned clarification state; no model request is sent until the operator chooses a target. Generic UI words such as “button” do not silently choose project authoring; explicit project/scene context or explicit engine/editor context is required, otherwise Epoch asks. `Force Engine Next` is a one-shot override.

Structured authoring/tool/source responses are private transport payloads. They are retained for validation but never appended verbatim to chat. A valid plan produces a human-readable status plus approval controls. The same approval/rejection can be expressed in plain language, so rendering a button is never the only continuation path.

Authoring and tool prompts may emit one strict blocking-question envelope when a critical detail is missing. Epoch extracts only the question text, keeps the original request, and resumes that same lane with the user's clarification. While that question is pending, the next chat reply belongs to the clarification first; a short “yes” or “no” cannot accidentally approve an unrelated staged action.

## Safety
Automatic routing does not weaken authority. Project authoring/tool calls still require explicit approval before execution. Engine-source work still runs only inside the disposable self-coding candidate and retains its existing validation/promotion gates. Ambiguous mutation requests do not dispatch until resolved.
