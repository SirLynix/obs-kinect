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

#ifndef OBS_KINECT_PLUGIN_NUISENSORLIBHELPER
#define OBS_KINECT_PLUGIN_NUISENSORLIBHELPER

#if __has_include(<NuiSensorLib.h>)
#define HAS_NUISENSOR_LIB 1
#include <windows.h>
#include <NuiSensorLib.h>
#else
#define HAS_NUISENSOR_LIB 0
#endif

#if HAS_NUISENSOR_LIB
#include <cstdint>
#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <type_traits>

struct NuiSensorHandleDeleter
{
	void operator()(NUISENSOR_HANDLE handle) const
	{
		NuiSensor_Shutdown(handle);
	}
};

using NuiSensorHandle = std::unique_ptr<std::remove_pointer_t<NUISENSOR_HANDLE>, NuiSensorHandleDeleter>;

BOOL ColorChangeCameraSettingsSync(NUISENSOR_HANDLE nuiSensorHandle, void* scratchBuffer, DWORD scratchBufferSize, NUISENSOR_RGB_CHANGE_STREAM_SETTING* settings, DWORD settingsSizeInBytes, NUISENSOR_RGB_CHANGE_STREAM_SETTING_REPLY* replies, DWORD replySizeInBytes);

class NuiSensorColorCameraSettings
{
	public:
		NuiSensorColorCameraSettings();
		~NuiSensorColorCameraSettings() = default;

		void AddCommand(NUISENSOR_RGB_COMMAND_TYPE command);
		void AddCommand(NUISENSOR_RGB_COMMAND_TYPE command, std::uint32_t data);
		void AddCommandFloat(NUISENSOR_RGB_COMMAND_TYPE command, float data);

		bool Execute(NUISENSOR_HANDLE sensor);
		bool ExecuteAndReset(NUISENSOR_HANDLE sensor);

		std::size_t GetCommandCount() const;
		std::optional<std::uint32_t> GetReplyData(std::size_t commandIndex) const;
		std::optional<float> GetReplyDataFloat(std::size_t commandIndex) const;
		bool GetReplyStatus(std::size_t commandIndex) const;

		void Reset();

	private:
		NUISENSOR_RGB_CHANGE_STREAM_SETTING* GetSettings();
		const NUISENSOR_RGB_CHANGE_STREAM_SETTING* GetSettings() const;
		NUISENSOR_RGB_CHANGE_STREAM_SETTING_REPLY* GetReplies();
		const NUISENSOR_RGB_CHANGE_STREAM_SETTING_REPLY* GetReplies() const;
		const NUISENSOR_RGB_CHANGE_STREAM_SETTING_REPLY_STATUS& GetReplyStatusInternal(std::size_t commandIndex) const;

		std::array<std::uint8_t, NUISENSOR_MAX_USB_COMMAND_SIZE> m_settingBuffer;
		std::array<std::uint8_t, NUISENSOR_MAX_USB_COMMAND_SIZE> m_replyBuffer;

		static std::atomic_uint32_t s_sequenceId;
};

#endif

#endif
