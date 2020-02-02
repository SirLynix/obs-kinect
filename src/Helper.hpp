/******************************************************************************
	Copyright (C) 2020 by Jérôme Leclercq <lynix680@gmail.com>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#ifndef OBS_KINECT_PLUGIN_HELPER
#define OBS_KINECT_PLUGIN_HELPER

#include <memory>

template<typename Interface>
struct CloseDeleter
{
	void operator()(Interface* handle) const
	{
		handle->Close();
	}
};

template<typename T>
struct DummyDeleter
{
	template<typename U>
	void operator()(U*) const
	{
	}
};

template<typename Interface>
struct ReleaseDeleter
{
	void operator()(Interface* handle) const
	{
		handle->Release();
	}
};

template<typename T> using ClosePtr = std::unique_ptr<T, CloseDeleter<T>>;
template<typename T> using ObserverPtr = std::unique_ptr<T, DummyDeleter<T>>;
template<typename T> using ReleasePtr = std::unique_ptr<T, ReleaseDeleter<T>>;

#endif
