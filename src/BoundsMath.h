#pragma once

#include <cstddef> // IWYU pragma: keep
#include <optional>
#include <span>

#include <RE/N/NiBound.h>

[[nodiscard]] std::optional<RE::NiBound> SelectRotationBound(
    const RE::NiBound& a_root,
    std::span<const RE::NiBound> a_samples
);
