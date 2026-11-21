#pragma once
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Physical source identity. Public exports replace only this value with 1.
// An installed updater must never switch editions through a build flag.
#define EPOCH_SOURCE_EDITION_PUBLIC 1
#if EPOCH_SOURCE_EDITION_PUBLIC != 0 && EPOCH_SOURCE_EDITION_PUBLIC != 1
#error Invalid Epoch source edition marker
#endif
#if defined(EPOCH_EDITION_PUBLIC) && EPOCH_EDITION_PUBLIC != EPOCH_SOURCE_EDITION_PUBLIC
#error Epoch edition build flag conflicts with physical source edition
#endif
#ifndef EPOCH_EDITION_PUBLIC
#define EPOCH_EDITION_PUBLIC EPOCH_SOURCE_EDITION_PUBLIC
#endif

// Source projection may remove engine self-coding without changing update identity.
#ifdef EPOCH_SOURCE_SELF_CODING
#error Epoch self-coding capability comes from the physical source tree
#endif
#define EPOCH_SOURCE_SELF_CODING (1 - EPOCH_SOURCE_EDITION_PUBLIC)
#if EPOCH_SOURCE_EDITION_PUBLIC && EPOCH_SOURCE_SELF_CODING
#error Public Epoch source cannot include engine self-coding
#endif
#if !EPOCH_SOURCE_EDITION_PUBLIC && !EPOCH_SOURCE_SELF_CODING \
    && defined(EPOCH_BINARY_ONLY_DISTRIBUTION) && EPOCH_BINARY_ONLY_DISTRIBUTION
#error Owner runtime requires the authenticated source updater
#endif
