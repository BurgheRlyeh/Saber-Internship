/**
 * @file EnumHelpers.h
 * @brief Utility concepts, type-conversion helpers, and a type-safe bitfield
 *        wrapper (@ref EnumFlags) for scoped enumerations.
 */
#pragma once

#include <cassert>
#include <limits>
#include <type_traits>

// ------------------------------------------------------------------ Concepts

/**
 * @brief Concept satisfied by any scoped or unscoped enum type.
 * @tparam Enum Type to test.
 */
template<typename Enum>
concept EnumConcept = std::is_enum_v<Enum>;

// ----------------------------------------------------------------- Conversion

/**
 * @brief Casts an enum value to its underlying integer type.
 * @tparam Enum Enum type.
 * @param  enumValue Value to convert.
 * @return Underlying integer representation.
 */
template <EnumConcept Enum>
constexpr std::underlying_type_t<Enum> ToUnderlying(Enum enumValue) {
	return static_cast<std::underlying_type_t<Enum>>(enumValue);
}

/**
 * @brief Casts an underlying integer value back to the enum type.
 * @tparam Enum Enum type.
 * @param  value Integer to convert.
 * @return Enum value.
 */
template <EnumConcept Enum>
constexpr Enum FromUnderlying(std::underlying_type_t<Enum> value) {
	return static_cast<Enum>(value);
}

/**
 * @brief Returns the zero-based ordinal index of an enum value (alias for @ref ToUnderlying).
 * @tparam Enum Enum type.
 * @param  enumValue Enum value.
 * @return Index as the underlying type.
 */
template <EnumConcept Enum>
constexpr std::underlying_type_t<Enum> ToId(Enum enumValue) {
	return ToUnderlying<Enum>(enumValue);
}

/**
 * @brief Constructs an enum value from a zero-based index (alias for @ref FromUnderlying).
 * @tparam Enum Enum type.
 * @param  value Index.
 * @return Enum value.
 */
template <EnumConcept Enum>
constexpr Enum FromId(std::underlying_type_t<Enum> value) {
	return FromUnderlying<Enum>(value);
}

// ------------------------------------------------------------- EnumFlags

/**
 * @brief Trait used to opt an enum into @ref EnumFlags bitfield semantics.
 *
 * Specialize via the @ref ENABLE_ENUM_FLAGS macro.
 */
template<typename Enum>
struct IsFlagEnum : std::false_type {};

/**
 * @brief Enables @ref EnumFlags bitfield operators for @p Enum.
 * @param Enum Scoped enum type to enable.
 */
#define ENABLE_ENUM_FLAGS(Enum) template<> struct IsFlagEnum<Enum> : std::true_type {}

/**
 * @brief Concept satisfied by enums that have been enabled for flag operations.
 * @tparam Enum Type to test.
 */
template<typename Enum>
concept EnumFlagsConcept = std::is_enum_v<Enum> && IsFlagEnum<Enum>::value;

/**
 * @brief Type-safe bitfield wrapper for enums marked with @ref ENABLE_ENUM_FLAGS.
 *
 * Provides bitwise OR, AND, XOR, NOT, and comparison operators while
 * preventing accidental mixing of unrelated enum types.
 *
 * @tparam Enum Flag enum type (must satisfy @ref EnumFlagsConcept).
 */
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

/**
 * @brief Returns the underlying integer value of an @ref EnumFlags wrapper.
 * @tparam Enum Flag enum type.
 * @param  enumValue Flags value.
 * @return Raw underlying integer.
 */
template <EnumFlagsConcept Enum>
constexpr std::underlying_type_t<Enum> ToUnderlying(EnumFlags<Enum> enumValue) {
	return static_cast<std::underlying_type_t<Enum>>(enumValue);
}

/**
 * @brief Returns the ordinal id of an @ref EnumFlags value (alias for @ref ToUnderlying).
 * @tparam Enum Flag enum type.
 * @param  enumValue Flags value.
 * @return Raw underlying integer.
 */
template <EnumFlagsConcept Enum>
constexpr std::underlying_type_t<Enum> ToId(EnumFlags<Enum> enumValue) {
	return ToUnderlying<Enum>(enumValue);
}
