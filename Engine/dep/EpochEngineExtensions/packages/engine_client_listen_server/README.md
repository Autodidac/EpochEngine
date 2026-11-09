# Client Listen Server

<!-- SPDX-License-Identifier: LicenseRef-MIT-NoSell -->
<!-- Copyright (c) 2026 Adam Rushford -->

Optional client-hosted session planning package.

This package stages nondedicated/listen-server configuration and verifier logic
only. It never binds a socket or starts a listener from package install. A
future runtime adapter must receive an explicit human-approved run action before
network capability becomes live.
