/*
	This file is part of D2DX.

	Copyright (C) 2021  Bolrog

	D2DX is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	D2DX is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with D2DX.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once

#include "Buffer.h"

namespace d2dx
{
	namespace detail
	{
		__declspec(noinline) void Log(_In_z_ const char* s);
		__declspec(noinline) void ThrowFromHRESULT(HRESULT hr, _In_z_ const char* func, int32_t line);
		__declspec(noinline) char* GetMessageForHRESULT(HRESULT hr, _In_z_ const char* func, int32_t line) noexcept;
		[[noreturn]] __declspec(noinline) void FatalException() noexcept;
		[[noreturn]] __declspec(noinline) void FatalError(_In_z_ const char* msg) noexcept;
	}

	int64_t TimeStart();
	float TimeEndMs(int64_t start);


#ifdef NDEBUG
#define D2DX_DEBUG_LOG(fmt, ...)
#else
#define D2DX_DEBUG_LOG(fmt, ...) \
	{ \
		static char ss[256]; \
		sprintf_s(ss, fmt "\n", __VA_ARGS__); \
		d2dx::detail::Log(ss); \
	}
#endif

#define D2DX_LOG(fmt, ...) \
	{ \
		static char ssss[256]; \
		sprintf_s(ssss, fmt "\n", __VA_ARGS__); \
		d2dx::detail::Log(ssss); \
	}

	struct ComException final : public std::runtime_error
	{
		ComException(HRESULT hr_, const char* func_, int32_t line_) :
			std::runtime_error(detail::GetMessageForHRESULT(hr, func_, line_)),
			hr{ hr_ },
			func{ func_ },
			line{ line_ } 
		{
		}

		HRESULT hr;
		const char* func;
		int32_t line;
	};

	#define D2DX_THROW_HR(hr) detail::ThrowFromHRESULT(hr, __FUNCTION__, __LINE__)
	#define D2DX_CHECK_HR(expr) { HRESULT hr = (expr); if (FAILED(hr)) { detail::ThrowFromHRESULT((hr), __FUNCTION__, __LINE__); } }
	#define D2DX_FATAL_EXCEPTION detail::FatalException()
	#define D2DX_FATAL_ERROR(msg) detail::FatalError(msg)

	struct WindowsVersion 
	{
		uint32_t major;
		uint32_t minor;
		uint32_t build;
	};

	WindowsVersion GetWindowsVersion();

	WindowsVersion GetActualWindowsVersion();

	Buffer<char> ReadTextFile(
		_In_z_ const char* filename);

	void DumpTexture(uint32_t hash, int32_t w, int32_t h, const uint8_t* pixels, uint32_t pixelsSize, uint32_t textureCategory, const uint32_t* palette);
}
