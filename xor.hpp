//
//#ifdef _MSC_VER
//#define _CRT_SECURE_NO_WARNINGS
//#endif
//
//
//#pragma once
//#include <string>
//#include <array>
//#include <cstdarg>
//
//#define BEGIN_NAMESPACE( x ) namespace x {
//#define END_NAMESPACE }
//
//BEGIN_NAMESPACE(XorCompileTime)
//
//constexpr auto time = __TIME__;
//constexpr auto seed = static_cast<int>(time[7]) + static_cast<int>(time[6]) * 10 + static_cast<int>(time[4]) * 60 + static_cast<int>(time[3]) * 600 + static_cast<int>(time[1]) * 3600 + static_cast<int>(time[0]) * 36000;
//
//
//template < int N >
//struct RandomGenerator
//{
//private:
//    static constexpr unsigned a = 16807; // 7^5
//    static constexpr unsigned m = 2147483647; // 2^31 - 1
//
//    static constexpr unsigned s = RandomGenerator< N - 1 >::value;
//    static constexpr unsigned lo = a * (s & 0xFFFF); // Multiply lower 16 bits by 16807
//    static constexpr unsigned hi = a * (s >> 16); // Multiply higher 16 bits by 16807
//    static constexpr unsigned lo2 = lo + ((hi & 0x7FFF) << 16); // Combine lower 15 bits of hi with lo's upper bits
//    static constexpr unsigned hi2 = hi >> 15; // Discard lower 15 bits of hi
//    static constexpr unsigned lo3 = lo2 + hi;
//
//public:
//    static constexpr unsigned max = m;
//    static constexpr unsigned value = lo3 > m ? lo3 - m : lo3;
//};
//
//template <>
//struct RandomGenerator< 0 >
//{
//    static constexpr unsigned value = seed;
//};
//
//template < int N, int M >
//struct RandomInt
//{
//    static constexpr auto value = RandomGenerator< N + 1 >::value % M;
//};
//
//template < int N >
//struct RandomChar
//{
//    static const char value = static_cast<char>(1 + RandomInt< N, 0x7F - 1 >::value);
//};
//
//template < int MIN, int MAX >
//struct RandomByte
//{
//    static const BYTE value = static_cast<BYTE>(1 + RandomInt< MIN, MAX - 1 >::value);
//};
//
//template < size_t N, int K >
//struct XorString
//{
//private:
//    const char _key;
//    std::array< char, N + 1 > _encrypted;
//
//    constexpr char enc(char c) const
//    {
//        return c ^ _key;
//    }
//
//    char dec(char c) const
//    {
//        return c ^ _key;
//    }
//
//public:
//    template < size_t... Is >
//    constexpr __forceinline XorString(const char* str, std::index_sequence< Is... >) : _key(RandomChar< K >::value), _encrypted{ enc(str[Is])... }
//    {
//    }
//
//    __forceinline decltype(auto) decrypt(void)
//    {
//        for (size_t i = 0; i < N; ++i) {
//            _encrypted[i] = dec(_encrypted[i]);
//        }
//        _encrypted[N] = '\0';
//        return _encrypted.data();
//    }
//};
//
////--------------------------------------------------------------------------------
////-- Note: XorStr will __NOT__ work directly with functions like printf.
////         To work with them you need a wrapper function that takes a const char*
////         as parameter and passes it to printf and alike.
////
////         The Microsoft Compiler/Linker is not working correctly with variadic 
////         templates!
////  
////         Use the functions below or use std::cout (and similar)!
////--------------------------------------------------------------------------------
//
//static auto w_printf = [](const char* fmt, ...) {
//    va_list args;
//    va_start(args, fmt);
//    vprintf_s(fmt, args);
//    va_end(args);
//};
//
//static auto w_printf_s = [](const char* fmt, ...) {
//    va_list args;
//    va_start(args, fmt);
//    vprintf_s(fmt, args);
//    va_end(args);
//};
//
//static auto w_sprintf = [](char* buf, const char* fmt, ...) {
//va_list args;
//va_start(args, fmt);
//vsprintf(buf, fmt, args);
//va_end(args);
//};
//
//static auto w_sprintf_s = [](char* buf, size_t buf_size, const char* fmt, ...) {
//    va_list args;
//    va_start(args, fmt);
//    vsprintf_s(buf, buf_size, fmt, args);
//    va_end(args);
//};
//
//#ifdef NDEBUG
//#define xorstr( s ) ( XorCompileTime::XorString< sizeof( s ) - 1, __COUNTER__ >( s, std::make_index_sequence< sizeof( s ) - 1>() ).decrypt() )
//#else
//#define xorstr( s ) ( s )
//#endif
//
//
//END_NAMESPACE

#pragma once
#ifndef JM_XORSTR_HPP
#define JM_XORSTR_HPP

#include <immintrin.h>
#include <cstdint>
#include <cstddef>
#include <utility>

#define xorstr_(str)                                              \
    ::jm::make_xorstr(                                           \
        []() { return str; },                                    \
        std::make_index_sequence<sizeof(str) / sizeof(*str)>{},  \
        std::make_index_sequence<::jm::detail::_buffer_size<sizeof(str)>()>{})
#define xorstr(str) xorstr_(str).crypt_get()

#ifdef _MSC_VER
#define XORSTR_FORCEINLINE __forceinline
#else
#define XORSTR_FORCEINLINE __attribute__((always_inline))
#endif

// you can define this macro to get possibly faster code on gcc/clang
// at the expense of constants being put into data section.
#if !defined(XORSTR_ALLOW_DATA)
// MSVC - no volatile
// GCC and clang - volatile everywhere
#if defined(__clang__) || defined(__GNUC__)
#define XORSTR_VOLATILE volatile
#endif

#endif
#ifndef XORSTR_VOLATILE
#define XORSTR_VOLATILE
#endif

namespace jm {

	namespace detail {

		template<std::size_t S>
		struct unsigned_;

		template<>
		struct unsigned_<1> {
			using type = std::uint8_t;
		};
		template<>
		struct unsigned_<2> {
			using type = std::uint16_t;
		};
		template<>
		struct unsigned_<4> {
			using type = std::uint32_t;
		};

		template<auto C, auto...>
		struct pack_value_type {
			using type = decltype(C);
		};

		template<std::size_t Size>
		constexpr std::size_t _buffer_size()
		{
			return ((Size / 16) + (Size % 16 != 0)) * 2;
		}

		template<auto... Cs>
		struct tstring_ {
			using value_type = typename pack_value_type<Cs...>::type;
			constexpr static std::size_t size = sizeof...(Cs);
			constexpr static value_type  str[size] = { Cs... };

			constexpr static std::size_t buffer_size = _buffer_size<sizeof(str)>();
			constexpr static std::size_t buffer_align =
#ifndef JM_XORSTR_DISABLE_AVX_INTRINSICS
			((sizeof(str) > 16) ? 32 : 16);
#else
				16;
#endif
		};

		template<std::size_t I, std::uint64_t K>
		struct _ki {
			constexpr static std::size_t   idx = I;
			constexpr static std::uint64_t key = K;
		};

		template<std::uint32_t Seed>
		constexpr std::uint32_t key4() noexcept
		{
			std::uint32_t value = Seed;
			for (char c : __TIME__)
				value = static_cast<std::uint32_t>((value ^ c) * 16777619ull);
			return value;
		}

		template<std::size_t S>
		constexpr std::uint64_t key8()
		{
			constexpr auto first_part = key4<2166136261 + S>();
			constexpr auto second_part = key4<first_part>();
			return (static_cast<std::uint64_t>(first_part) << 32) | second_part;
		}

		// clang and gcc try really hard to place the constants in data
		// sections. to counter that there was a need to create an intermediate
		// constexpr string and then copy it into a non constexpr container with
		// volatile storage so that the constants would be placed directly into
		// code.
		template<class T, std::uint64_t... Keys>
		struct string_storage {
			std::uint64_t storage[T::buffer_size];

			XORSTR_FORCEINLINE constexpr string_storage() noexcept : storage{ Keys... }
			{
				using cast_type =
					typename unsigned_<sizeof(typename T::value_type)>::type;
				constexpr auto value_size = sizeof(typename T::value_type);
				// puts the string into 64 bit integer blocks in a constexpr
				// fashion
				for (std::size_t i = 0; i < T::size; ++i)
					storage[i / (8 / value_size)] ^=
					(std::uint64_t{ static_cast<cast_type>(T::str[i]) }
				<< ((i % (8 / value_size)) * 8 * value_size));
			}
		};

	} // namespace detail

	template<class T, class... Keys>
	class xor_string {
		alignas(T::buffer_align) std::uint64_t _storage[T::buffer_size];

		// _single functions needed because MSVC crashes without them
		XORSTR_FORCEINLINE void _crypt_256_single(const std::uint64_t* keys,
			std::uint64_t* storage) noexcept

		{
			_mm256_store_si256(
				reinterpret_cast<__m256i*>(storage),
				_mm256_xor_si256(
					_mm256_load_si256(reinterpret_cast<const __m256i*>(storage)),
					_mm256_load_si256(reinterpret_cast<const __m256i*>(keys))));
		}

		template<std::size_t... Idxs>
		XORSTR_FORCEINLINE void _crypt_256(const std::uint64_t* keys,
			std::index_sequence<Idxs...>) noexcept
		{
			(_crypt_256_single(keys + Idxs * 4, _storage + Idxs * 4), ...);
		}

		XORSTR_FORCEINLINE void _crypt_128_single(const std::uint64_t* keys,
			std::uint64_t* storage) noexcept
		{
			_mm_store_si128(
				reinterpret_cast<__m128i*>(storage),
				_mm_xor_si128(_mm_load_si128(reinterpret_cast<const __m128i*>(storage)),
					_mm_load_si128(reinterpret_cast<const __m128i*>(keys))));
		}

		template<std::size_t... Idxs>
		XORSTR_FORCEINLINE void _crypt_128(const std::uint64_t* keys,
			std::index_sequence<Idxs...>) noexcept
		{
			(_crypt_128_single(keys + Idxs * 2, _storage + Idxs * 2), ...);
		}

		// loop generates vectorized code which places constants in data dir
		XORSTR_FORCEINLINE constexpr void _copy() noexcept
		{
			constexpr detail::string_storage<T, Keys::key...> storage;
			static_cast<void>(std::initializer_list<std::uint64_t>{
				(const_cast<XORSTR_VOLATILE std::uint64_t*>(_storage))[Keys::idx] =
					storage.storage[Keys::idx]... });
		}

	public:
		using value_type = typename T::value_type;
		using size_type = std::size_t;
		using pointer = value_type*;
		using const_pointer = const pointer;

		XORSTR_FORCEINLINE xor_string() noexcept { _copy(); }

		XORSTR_FORCEINLINE constexpr size_type size() const noexcept
		{
			return T::size - 1;
		}

		XORSTR_FORCEINLINE void crypt() noexcept
		{
			alignas(T::buffer_align) std::uint64_t keys[T::buffer_size];
			static_cast<void>(std::initializer_list<std::uint64_t>{
				(const_cast<XORSTR_VOLATILE std::uint64_t*>(keys))[Keys::idx] =
					Keys::key... });

			_copy();

#ifndef JM_XORSTR_DISABLE_AVX_INTRINSICS
			_crypt_256(keys, std::make_index_sequence<T::buffer_size / 4>{});
			if constexpr (T::buffer_size % 4 != 0)
				_crypt_128(keys, std::index_sequence<T::buffer_size / 2 - 1>{});
#else
			_crypt_128(keys, std::make_index_sequence<T::buffer_size / 2>{});
#endif
		}

		XORSTR_FORCEINLINE const_pointer get() const noexcept
		{
			return reinterpret_cast<const_pointer>(_storage);
		}

		XORSTR_FORCEINLINE const_pointer crypt_get() noexcept
		{
			crypt();
			return reinterpret_cast<const_pointer>(_storage);
		}
	};

	template<class Tstr, std::size_t... StringIndices, std::size_t... KeyIndices>
	XORSTR_FORCEINLINE constexpr auto
		make_xorstr(Tstr str_lambda,
			std::index_sequence<StringIndices...>,
			std::index_sequence<KeyIndices...>) noexcept
	{
		return xor_string<detail::tstring_<str_lambda()[StringIndices]...>,
			detail::_ki<KeyIndices, detail::key8<KeyIndices>()>...>{};
	}

} // namespace jm

#endif // include guard