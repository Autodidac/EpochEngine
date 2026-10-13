# EpochEngine 0.90.40 — bounded AI source discovery

- Prevent unbounded model-only source discovery by limiting initial verified read rounds to three; grant two additional rounds per actual compiler-repair attempt, bounded at three repair allowances.
- At the penultimate read, explicitly prioritize a grounded source-edit proposal over another generic source investigation.
- Reject further model-initiated context reads once the limit is reached, including novel but nonproductive paths and repeated insufficient-evidence responses.
- Preserve existing host-confirmed exact edit-anchor recovery, current source evidence, candidate sandbox, and compiled-source safety gates.
- Record an explicit source_discovery_budget exhaustion event and stop rather than queueing another generation or claiming a build.
- Add a built-in contract case demonstrating that three distinct, productive source reads cannot reset the overall budget.

The budget limits futile inference, but cannot guarantee that an incapable or misconfigured model produces a valid source edit. Exact source edits still require validation and isolated build/test acceptance.

Native MSVC build and live model test are not available in this packaging environment.
