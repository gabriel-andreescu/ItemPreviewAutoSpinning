#pragma once

#include <RE/Skyrim.h> // IWYU pragma: keep
#include <SKSE/SKSE.h>

#include <REL/Relocation.h>
#include <xbyak/xbyak.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace HookUtil {
inline constexpr std::size_t kAbsoluteJumpSize {0xE};

template <std::size_t SIZE>
[[nodiscard]] bool HasExpectedPrologue(
    const std::byte* a_address,
    const std::array<std::byte, SIZE>& a_expected
) noexcept {
    return std::memcmp(a_address, a_expected.data(), a_expected.size()) == 0;
}

template <std::size_t BYTES, class F>
[[nodiscard]] bool WriteAbsoluteJump(const std::uintptr_t a_src, F a_dst, const std::byte* a_originalBytes) {
    static_assert(BYTES >= kAbsoluteJumpSize);

#pragma pack(push, 1)
    struct Assembly {
        std::uint8_t jmp;
        std::uint8_t modrm;
        std::int32_t disp;
        std::uint64_t addr;
    };
    static_assert(sizeof(Assembly) == kAbsoluteJumpSize);
#pragma pack(pop)

    const Assembly assembly {
        .jmp = 0xFF,
        .modrm = 0x25,
        .disp = 0,
        .addr = SKSE::stl::unrestricted_cast<std::uintptr_t>(a_dst),
    };

    std::array<std::byte, BYTES> patch {};
    patch.fill(std::byte {0x90});
    std::memcpy(patch.data(), &assembly, sizeof(assembly));

    return REL::safe_write(a_src, patch.data(), patch.size(), a_originalBytes, BYTES);
}

template <class T, std::size_t BYTES>
[[nodiscard]] bool HookFunctionPrologue(const std::uintptr_t a_src, const std::byte* a_originalBytes) {
    struct Patch : Xbyak::CodeGenerator {
        Patch(const ::std::uintptr_t a_originalFuncAddr, ::std::span<const ::std::byte> a_originalBytes) {
            for (const auto byte : a_originalBytes) {
                db(::std::to_integer<::std::uint8_t>(byte));
            }

            jmp(ptr[rip]);
            dq(a_originalFuncAddr + a_originalBytes.size());
        }
    };

    Patch patch(a_src, {a_originalBytes, BYTES});
    patch.ready();

    auto& trampoline = SKSE::GetTrampoline();
    if constexpr (BYTES == 5 || BYTES == 6) {
        trampoline.write_branch<BYTES>(a_src, T::Thunk);
    } else {
        if (!WriteAbsoluteJump<BYTES>(a_src, T::Thunk, a_originalBytes)) {
            return false;
        }
    }

    const auto alloc = trampoline.allocate(patch.getSize());
    std::memcpy(alloc, patch.getCode(), patch.getSize());

    T::func = reinterpret_cast<std::uintptr_t>(alloc);
    return true;
}
}
