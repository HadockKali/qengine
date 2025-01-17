/*
/********************************************************************
*                                                                  *
*  Polyc 3-dimensional Polymorphic Encryption Algorithm Prototype  *
*                                                                  *
*********************************************************************
*/

#ifndef POLYC_H
#define POLYC_H

#include <chrono>
#include <random>
#include <string>

#include "../qbase/qdef.hpp"

#pragma optimize("", on)
#pragma inline_depth(255)
#pragma inline_recursion(on) 

namespace qengine {

	namespace polyc {
		/*
		/****************************************************************************************
		*                                                                                      *
		*  seeding arrays to be initialized at runtime if intended use is polymorphism		   *
		*  static initialization if this is to be maintained outside of runtime / reproducable *
		*                                                                                      *
		*****************************************************************************************
		*/

#pragma region Algorithm Constants

#ifdef _WIN64

#define BIT_ACTIVE 0x0101010101010101ui64

#else

#define BIT_ACTIVE 0x01010101ui32

#endif

#pragma endregion

#pragma region Other Globals

		extern bool initialized;

#pragma endregion

#pragma region Seeding Values

#pragma region Raw arrays

		extern uintptr_t ciph_x[16];

		extern uintptr_t ciph_y[16];

		extern uintptr_t ciph_z[16];

#pragma endregion

#pragma region Used Indice(s)

		extern unsigned char indice_map_x[4];

		extern unsigned char indice_map_y[8];

		extern unsigned char indice_map_z[12];

#pragma endregion

#pragma endregion

#pragma region Pseudo-Ctor

		static __forceinline void __cdecl polyc_init() {
			/* fill our seeding values */

			for (auto i = 0; i < 16; ++i) {
				auto time_n = std::chrono::high_resolution_clock::now();
				ciph_x[i] = ((static_cast<uintptr_t>(time_n.time_since_epoch().count() % RAND(16, 1)) ^ 16ui64) * 1024) | BIT_ACTIVE;

				time_n = std::chrono::high_resolution_clock::now();
				ciph_y[i] = ((static_cast<uintptr_t>(time_n.time_since_epoch().count() % RAND(32, 1)) ^ 32ui64) * 2048) | BIT_ACTIVE;

				time_n = std::chrono::high_resolution_clock::now();
				ciph_z[i] = ((static_cast<uintptr_t>(time_n.time_since_epoch().count() % RAND(64, 1)) ^ 64ui64) * 4096) | BIT_ACTIVE;
			}

			/* seed for our indice maps */
			auto time_n_s = std::chrono::high_resolution_clock::now();

			/* determine indices in x vector to use */

			for (auto x = 0; x < sizeof(indice_map_x); ++x) {
				indice_map_x[x] = static_cast<char>(time_n_s.time_since_epoch().count() % static_cast<char>(RAND(16, 1)));
			}

			/* determine indices in y vector to use + refresh seed */
			time_n_s = std::chrono::high_resolution_clock::now();

			for (auto y = 0; y < sizeof(indice_map_y); ++y)
				indice_map_y[y] = static_cast<char>(time_n_s.time_since_epoch().count() % static_cast<char>(RAND(16, 1)));

			/* determine indices in z vector to use + refresh seed*/
			time_n_s = std::chrono::high_resolution_clock::now();

			for (auto z = 0; z < sizeof(indice_map_z); ++z)
				indice_map_z[z] = static_cast<char>(time_n_s.time_since_epoch().count() % static_cast<char>(RAND(16, 1)));

			initialized = true;
		}

#pragma endregion

#pragma region Algorithm

		static __forceinline void __fastcall algo(void* data, size_t length) {
			if (!initialized)
				polyc_init();

			/* obvious */
			if (!length)
				return;

			auto data_c = reinterpret_cast<byte*>(data);

			/* iterate each individual byte of data in the source */

			for (auto i = 0; i < length; ++i) {
				/* run our first pass on the data */

				for (auto x = 0; x < sizeof(indice_map_x); ++x) {
					//data_c[i] ^= ciph_x[indice_map_x[x]];
					XORWORD(data_c[i], ciph_x[indice_map_x[x]]);
				}

				/* run our second pass on the data */

				for (auto y = 0; y < sizeof(indice_map_y); ++y) {
					//data_c[i] ^= ciph_y[indice_map_y[y]];
					XORWORD(data_c[i], ciph_y[indice_map_y[y]]);
				}

				/* run our third pass on the data */

				for (auto z = 0; z < sizeof(indice_map_z); ++z) {
					XORWORD(data_c[i], ciph_x[indice_map_z[z]]);
				}
			}
		}

		static __forceinline void* __fastcall algo_inst(void* data, size_t length) {
			if (!data || !length)
				return nullptr;

			auto copy_d = reinterpret_cast<byte*>(malloc(length));

			if (!copy_d)
				return nullptr;

			memcpy(copy_d, data, length);

			algo(copy_d, length);

			return copy_d;
		}

		template<typename T>
		static __forceinline void __fastcall algo_t(T& data) {
			auto ptr_t = reinterpret_cast<byte*>(&data);

			algo(ptr_t, sizeof(decltype(data)));
		}

		template<typename T>
		static __forceinline T __fastcall algo_t_inst(T data) {
			T copy_t = data;

			algo_t(copy_t);

			return copy_t;
		}

		static __forceinline void __cdecl algo_str(const std::string& data) {
			algo(const_cast<char*>(data.c_str()), data.size());
		}

		static __forceinline std::string __cdecl algo_str_inst(const std::string& data) {
			std::string copy_str = data;

			algo_str(copy_str);

			return copy_str;
		}

		static __forceinline void __cdecl algo_wstr(const std::wstring& data) {
			algo(const_cast<wchar_t*>(data.c_str()), data.size() * sizeof(wchar_t));
		}

		static __forceinline std::wstring __cdecl algo_wstr_inst(const std::wstring& data) {
			std::wstring copy_wstr = data;

			algo_wstr(copy_wstr);

			return copy_wstr;
		}

#pragma endregion
	}

}

#endif