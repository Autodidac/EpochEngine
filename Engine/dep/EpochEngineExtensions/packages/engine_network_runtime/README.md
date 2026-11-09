# Engine Network Runtime

<!-- SPDX-License-Identifier: LicenseRef-MIT-NoSell -->
<!-- Copyright (c) 2026 Adam Rushford -->

Inert shared network/session contracts for Epoch extensions.

This package does not bind ports, open sockets, listen, or create a server. It
only stages value types, frame headers, replication channel definitions, and
policy checks that listen-server and dedicated-server packages can build on
after a human enables them.
