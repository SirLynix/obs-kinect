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

#include "NuiSensorLibHelper.hpp"
#include <cassert>

#if HAS_NUISENSOR_LIB

// From https://github.com/microsoft/MixedRealityCompanionKit/blob/e01d8e1bf60cd20a62e182610e8a9bfb757a7654/KinectIPD/NuiSensor/Helpers.cpp#L67
BOOL ColorChangeCameraSettingsSync(NUISENSOR_HANDLE nuiSensorHandle, void* scratchBuffer, DWORD scratchBufferSize, NUISENSOR_RGB_CHANGE_STREAM_SETTING* settings, DWORD settingsSizeInBytes, NUISENSOR_RGB_CHANGE_STREAM_SETTING_REPLY* replies, DWORD replySizeInBytes)
{
	OVERLAPPED overlapped = { 0 };

	overlapped.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	BOOL success = NuiSensor_ColorChangeCameraSettings(
		nuiSensorHandle,
		scratchBuffer,
		scratchBufferSize,
		settings,
		settingsSizeInBytes,
		replies,
		replySizeInBytes,
		&overlapped);

	if (!success && ::GetLastError() == ERROR_IO_PENDING)
	{
		DWORD bytesTransferred;
		success = GetOverlappedResult(
			nuiSensorHandle,
			&overlapped,
			&bytesTransferred,
			TRUE);
	}

	CloseHandle(overlapped.hEvent);

	return success;
}

NuiSensorColorCameraSettings::NuiSensorColorCameraSettings()
{
	NUISENSOR_RGB_CHANGE_STREAM_SETTING* settings = GetSettings();
	settings->NumCommands = 0;
}

void NuiSensorColorCameraSettings::AddCommand(NUISENSOR_RGB_COMMAND_TYPE command)
{
	AddCommand(command, 0);
}

void NuiSensorColorCameraSettings::AddCommand(NUISENSOR_RGB_COMMAND_TYPE command, std::uint32_t data)
{
	NUISENSOR_RGB_CHANGE_STREAM_SETTING* settings = GetSettings();

	assert(settings->NumCommands < NUISENSOR_RGB_CHANGE_SETTING_MAX_NUM_CMD);
	settings->Commands[settings->NumCommands].Arg = data;
	settings->Commands[settings->NumCommands].Cmd = command;
	settings->NumCommands++;
}

void NuiSensorColorCameraSettings::AddCommandFloat(NUISENSOR_RGB_COMMAND_TYPE command, float data)
{
	// Not standard but supported as an extension by MSVC and MinGW
	union
	{
		float f;
		std::uint32_t u32;
	} floatToUInt32;

	floatToUInt32.f = data;

	return AddCommand(command, floatToUInt32.u32);
}

bool NuiSensorColorCameraSettings::Execute(NUISENSOR_HANDLE sensor)
{
	std::array<std::uint8_t, NUISENSOR_SEND_SCRATCH_SPACE_REQUIRED> scratchBuffer;

	NUISENSOR_RGB_CHANGE_STREAM_SETTING* settings = GetSettings();
	if (settings->NumCommands == 0)
		return false;

	NUISENSOR_RGB_CHANGE_STREAM_SETTING_REPLY* replies = GetReplies();
	settings->SequenceId = s_sequenceId++; //< I'm not sure if this is really required as Microsoft both incremented it and left it to zero in MixedRealityCompanionKit repository, but in doubt...
	replies->NumStatus = settings->NumCommands;

	DWORD settingsSize = sizeof(*settings) + sizeof(settings->Commands) * (settings->NumCommands - 1); //< Minus 1 as sizeof(NUISENSOR_RGB_CHANGE_STREAM_SETTING) already includes one element
	DWORD replySize = sizeof(*replies) + sizeof(replies->Status) * (replies->NumStatus - 1); //< Same as above

	return ColorChangeCameraSettingsSync(sensor, scratchBuffer.data(), DWORD(scratchBuffer.size()), settings, settingsSize, replies, replySize);
}

bool NuiSensorColorCameraSettings::ExecuteAndReset(NUISENSOR_HANDLE sensor)
{
	bool success = Execute(sensor);
	Reset();

	return success;
}

std::size_t NuiSensorColorCameraSettings::GetCommandCount() const
{
	const NUISENSOR_RGB_CHANGE_STREAM_SETTING* settings = GetSettings();
	return settings->NumCommands;
}

std::optional<std::uint32_t> NuiSensorColorCameraSettings::GetReplyData(std::size_t commandIndex) const
{
	const auto& replyStatus = GetReplyStatusInternal(commandIndex);
	if (replyStatus.Status == 0)
		return replyStatus.Data;
	else
		return {};
}

std::optional<float> NuiSensorColorCameraSettings::GetReplyDataFloat(std::size_t commandIndex) const
{
	const auto& replyStatus = GetReplyStatusInternal(commandIndex);
	if (replyStatus.Status == 0)
	{
		// Not standard but supported as an extension by MSVC and MinGW
		union
		{
			float f;
			std::uint32_t u32;
		} uint32ToFloat;

		uint32ToFloat.u32 = replyStatus.Data;

		return uint32ToFloat.f;
	}
	else
		return {};
}

bool NuiSensorColorCameraSettings::GetReplyStatus(std::size_t commandIndex) const
{
	return GetReplyStatusInternal(commandIndex).Status == 0;
}

void NuiSensorColorCameraSettings::Reset()
{
	NUISENSOR_RGB_CHANGE_STREAM_SETTING* settings = GetSettings();
	settings->NumCommands = 0;
}

NUISENSOR_RGB_CHANGE_STREAM_SETTING* NuiSensorColorCameraSettings::GetSettings()
{
	return reinterpret_cast<NUISENSOR_RGB_CHANGE_STREAM_SETTING*>(m_settingBuffer.data());
}

const NUISENSOR_RGB_CHANGE_STREAM_SETTING* NuiSensorColorCameraSettings::GetSettings() const
{
	return reinterpret_cast<const NUISENSOR_RGB_CHANGE_STREAM_SETTING*>(m_settingBuffer.data());
}

NUISENSOR_RGB_CHANGE_STREAM_SETTING_REPLY* NuiSensorColorCameraSettings::GetReplies()
{
	return reinterpret_cast<NUISENSOR_RGB_CHANGE_STREAM_SETTING_REPLY*>(m_replyBuffer.data());
}

const NUISENSOR_RGB_CHANGE_STREAM_SETTING_REPLY* NuiSensorColorCameraSettings::GetReplies() const
{
	return reinterpret_cast<const NUISENSOR_RGB_CHANGE_STREAM_SETTING_REPLY*>(m_replyBuffer.data());
}

const NUISENSOR_RGB_CHANGE_STREAM_SETTING_REPLY_STATUS& NuiSensorColorCameraSettings::GetReplyStatusInternal(std::size_t commandIndex) const
{
	const NUISENSOR_RGB_CHANGE_STREAM_SETTING_REPLY* replies = GetReplies();

	assert(commandIndex < replies->NumStatus);
	return replies->Status[commandIndex];
}

std::atomic_uint32_t NuiSensorColorCameraSettings::s_sequenceId = 0;
#endif
