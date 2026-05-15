#pragma once

#include <cassert>
#include <limits>
#include <type_traits>

// ---------------- Enum helper function ----------------
template<typename Enum>
concept EnumConcept = std::is_enum_v<Enum>;

template <EnumConcept Enum>
constexpr std::underlying_type_t<Enum> ToUnderlying(Enum enumValue) {
	return static_cast<std::underlying_type_t<Enum>>(enumValue);
}

template <EnumConcept Enum>
constexpr Enum FromUnderlying(std::underlying_type_t<Enum> value) {
	return static_cast<Enum>(value);
}

template <EnumConcept Enum>
constexpr std::underlying_type_t<Enum> ToId(Enum enumValue) {
	return ToUnderlying<Enum>(enumValue);
}

template <EnumConcept Enum>
constexpr Enum FromId(std::underlying_type_t<Enum> value) {
	return FromUnderlying<Enum>(value);
}

// ---------------- EnumFlags wrapper ----------------
template<typename Enum>
struct IsFlagEnum : std::false_type {};

#define ENABLE_ENUM_FLAGS(Enum) template<> struct IsFlagEnum<Enum> : std::true_type {}

template<typename Enum>
concept EnumFlagsConcept = std::is_enum_v<Enum> && IsFlagEnum<Enum>::value;

template<EnumFlagsConcept Enum>
class EnumFlags {
	std::underlying_type_t<Enum> m_value{};

	constexpr explicit EnumFlags(std::underlying_type_t<Enum> v) : m_value(v) {}

public:
	constexpr EnumFlags() = default;
	constexpr EnumFlags(Enum e) : m_value(static_cast<std::underlying_type_t<Enum>>(e)) {}

	constexpr explicit operator Enum() const { return static_cast<Enum>(m_value); }
	constexpr explicit operator std::underlying_type_t<Enum>() const { return m_value; }
	constexpr operator bool() const { return m_value != 0; }

	// Bitwise OR
	constexpr EnumFlags& operator|=(Enum e) { m_value |= static_cast<std::underlying_type_t<Enum>>(e); return *this; }
	constexpr EnumFlags& operator|=(EnumFlags other) { m_value |= other.m_value; return *this; }

	// Bitwise AND
	constexpr EnumFlags& operator&=(Enum e) { m_value &= static_cast<std::underlying_type_t<Enum>>(e); return *this; }
	constexpr EnumFlags& operator&=(EnumFlags other) { m_value &= other.m_value; return *this; }

	// Bitwise XOR
	constexpr EnumFlags& operator^=(Enum e) { m_value ^= static_cast<std::underlying_type_t<Enum>>(e); return *this; }
	constexpr EnumFlags& operator^=(EnumFlags other) { m_value ^= other.m_value; return *this; }

	// Friend bitwise operators
	friend constexpr EnumFlags operator|<Enum>(EnumFlags lhs, EnumFlags rhs);
	friend constexpr EnumFlags operator&<Enum>(EnumFlags lhs, EnumFlags rhs);
	friend constexpr EnumFlags operator^<Enum>(EnumFlags lhs, EnumFlags rhs);
	friend constexpr EnumFlags operator~<Enum>(EnumFlags e);
};

// Bitwise OR
template<EnumFlagsConcept Enum>
constexpr EnumFlags<Enum> operator|(EnumFlags<Enum> lhs, EnumFlags<Enum> rhs) { return EnumFlags<Enum>(lhs.m_value | rhs.m_value); }
template<EnumFlagsConcept Enum>
constexpr EnumFlags<Enum> operator|(Enum lhs, Enum rhs) { return EnumFlags<Enum>(lhs) | EnumFlags<Enum>(rhs); }
template<EnumFlagsConcept Enum>
constexpr EnumFlags<Enum> operator|(Enum lhs, EnumFlags<Enum> rhs) { return EnumFlags<Enum>(lhs) | rhs; }
template<EnumFlagsConcept Enum>
constexpr EnumFlags<Enum> operator|(EnumFlags<Enum> lhs, Enum rhs) { return lhs | EnumFlags<Enum>(rhs); }

// Bitwise AND
template<EnumFlagsConcept Enum>
constexpr EnumFlags<Enum> operator&(EnumFlags<Enum> lhs, EnumFlags<Enum> rhs) { return EnumFlags<Enum>(lhs.m_value & rhs.m_value); }
template<EnumFlagsConcept Enum>
constexpr EnumFlags<Enum> operator&(Enum lhs, Enum rhs) { return EnumFlags<Enum>(lhs) & EnumFlags<Enum>(rhs); }
template<EnumFlagsConcept Enum>
constexpr EnumFlags<Enum> operator&(Enum lhs, EnumFlags<Enum> rhs) { return EnumFlags<Enum>(lhs) & rhs; }
template<EnumFlagsConcept Enum>
constexpr EnumFlags<Enum> operator&(EnumFlags<Enum> lhs, Enum rhs) { return lhs & EnumFlags<Enum>(rhs); }

// Bitwise XOR
template<EnumFlagsConcept Enum>
constexpr EnumFlags<Enum> operator^(EnumFlags<Enum> lhs, EnumFlags<Enum> rhs) { return EnumFlags<Enum>(lhs.m_value ^ rhs.m_value); }
template<EnumFlagsConcept Enum>
constexpr EnumFlags<Enum> operator^(Enum lhs, Enum rhs) { return EnumFlags<Enum>(lhs) ^ EnumFlags<Enum>(rhs); }
template<EnumFlagsConcept Enum>
constexpr EnumFlags<Enum> operator^(Enum lhs, EnumFlags<Enum> rhs) { return EnumFlags<Enum>(lhs) ^ rhs; }
template<EnumFlagsConcept Enum>
constexpr EnumFlags<Enum> operator^(EnumFlags<Enum> lhs, Enum rhs) { return lhs ^ EnumFlags<Enum>(rhs); }

// Bitwise NOT
template<EnumFlagsConcept Enum>
constexpr EnumFlags<Enum> operator~(EnumFlags<Enum> e) { return EnumFlags<Enum>(~e.m_value); }
template<EnumFlagsConcept Enum>
constexpr EnumFlags<Enum> operator~(Enum e) { return ~EnumFlags<Enum>(e); }

// Equality
template<EnumFlagsConcept Enum>
constexpr bool operator==(EnumFlags<Enum> lhs, EnumFlags<Enum> rhs) { return static_cast<Enum>(lhs) == static_cast<Enum>(rhs); }
template<EnumFlagsConcept Enum>
constexpr bool operator==(EnumFlags<Enum> lhs, Enum rhs) { return static_cast<Enum>(lhs) == rhs; }
template<EnumFlagsConcept Enum>
constexpr bool operator==(Enum lhs, EnumFlags<Enum> rhs) { return lhs == static_cast<Enum>(rhs); }

// Inequality
template<EnumFlagsConcept Enum>
constexpr bool operator!=(EnumFlags<Enum> lhs, EnumFlags<Enum> rhs) { return !(lhs == rhs); }
template<EnumFlagsConcept Enum>
constexpr bool operator!=(EnumFlags<Enum> lhs, Enum rhs) { return !(lhs == rhs); }
template<EnumFlagsConcept Enum>
constexpr bool operator!=(Enum lhs, EnumFlags<Enum> rhs) { return !(lhs == rhs); }

template <EnumFlagsConcept Enum>
constexpr std::underlying_type_t<Enum> ToUnderlying(EnumFlags<Enum> enumValue) {
	return static_cast<std::underlying_type_t<Enum>>(enumValue);
}

template <EnumFlagsConcept Enum>
constexpr std::underlying_type_t<Enum> ToId(EnumFlags<Enum> enumValue) {
	return ToUnderlying<Enum>(enumValue);
}
