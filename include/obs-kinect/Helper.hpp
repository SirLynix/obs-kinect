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

#ifdef _WIN32
	#define OBSKINECT_EXPORT __declspec(dllexport)
	#define OBSKINECT_IMPORT __declspec(dllimport)
#else
	#define OBSKINECT_EXPORT __attribute__((visibility ("default")))
	#define OBSKINECT_IMPORT __attribute__((visibility ("default")))
#endif

#ifdef OBS_KINECT_CORE_EXPORT
	#define OBSKINECT_API OBSKINECT_EXPORT
#else
	#define OBSKINECT_API OBSKINECT_IMPORT
#endif

#define OBSKINECT_VERSION_MAJOR 0
#define OBSKINECT_VERSION_MINOR 3
#define OBSKINECT_VERSION ((OBSKINECT_VERSION_MAJOR << 8) | OBSKINECT_VERSION_MINOR)

#include <obs-module.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <memory>

#define blog(log_level, format, ...)                    \
	blog(log_level, "[obs-kinect] " format, ##__VA_ARGS__)

#define debuglog(format, ...) blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define errorlog(format, ...) blog(LOG_ERROR, format, ##__VA_ARGS__)
#define infolog(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define warnlog(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)

struct DummyDeleter
{
	void operator()(void*) const
	{
	}
};

template<typename T> using ObserverPtr = std::unique_ptr<T, DummyDeleter>;

struct ObsBFreeDeleter
{
	void operator()(void* ptr) const
	{
		bfree(ptr);
	}
};

template<typename T> using ObsMemoryPtr = std::unique_ptr<T, ObsBFreeDeleter>;

struct ObsGraphics
{
	ObsGraphics() { obs_enter_graphics(); }
	~ObsGraphics() { obs_leave_graphics(); }
};

struct ObsLibDeleter
{
	void operator()(void* lib) const
	{
		os_dlclose(lib);
	}
};

using ObsLibPtr = std::unique_ptr<void, ObsLibDeleter>;

struct ObsImageFileDeleter
{
	void operator()(gs_image_file_t* imageFile) const
	{
		ObsGraphics gfx;
		gs_image_file_free(imageFile);

		delete imageFile;
	}
};

using ObsImageFilePtr = std::unique_ptr<gs_image_file_t, ObsImageFileDeleter>;

struct ObsTextureDeleter
{
	void operator()(gs_texture_t* texture) const
	{
		ObsGraphics gfx;
		gs_texture_destroy(texture);
	}
};

using ObsTexturePtr = std::unique_ptr<gs_texture_t, ObsTextureDeleter>;

OBSKINECT_API const char* Translate(const char* key); //< implemented in kinect-plugin.cpp

#endif
