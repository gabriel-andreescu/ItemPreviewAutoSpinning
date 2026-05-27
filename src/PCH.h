#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#define DLLEXPORT __declspec(dllexport)

namespace logger = SKSE::log;

using namespace std::literals;

namespace stl {
using namespace SKSE::stl;

template <typename T, std::size_t Size = 5>
void write_thunk_call(REL::Relocation<> a_target) noexcept {
    T::func = a_target.write_call<Size>(T::thunk);
}

template <typename T>
void write_vfunc(const REL::VariantID a_variant_id) noexcept {
    REL::Relocation target {a_variant_id};
    T::func = target.write_vfunc(T::idx, T::thunk);
}

template <typename TDest, typename TSource>
void write_vfunc(const std::size_t a_vtableIdx = 0) noexcept {
    write_vfunc<TSource>(TDest::VTABLE[a_vtableIdx]);
}
}
