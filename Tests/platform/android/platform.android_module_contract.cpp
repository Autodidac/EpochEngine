// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#if !defined(__ANDROID__) || (!defined(__aarch64__) && !defined(__x86_64__))
#error This consumer must be compiled for Android arm64-v8a or x86_64.
#endif

import render.math;

int main()
{
    using namespace epochengine::render_math;
    const Float3 unit = normalize({3.0f, 4.0f, 0.0f});
    const float length_squared = dot(unit, unit);
    return length_squared >= 0.999f && length_squared <= 1.001f ? 0 : 1;
}
