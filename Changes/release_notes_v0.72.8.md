# AlmondShell v0.72.8 Release Notes

- Synced version metadata across `aversion.hpp`, module partitions, and documentation so runtime banners, tools, and docs all advertise v0.72.8.
- Completed header-to-module conversions for atlas helpers and command-line plumbing, exposing `import aatlas.manager;`, `import aatlas.texture;`, and `import acommandline;` for mixed include/import builds.
- Clarified updater/configuration guidance so launcher manifests and release metadata pull their version strings from the runtime helpers and stay locked to v0.72.8.
