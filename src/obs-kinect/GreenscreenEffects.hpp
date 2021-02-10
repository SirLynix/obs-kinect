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

#ifndef OBS_KINECT_PLUGIN_GREENSCREENEFFECTS
#define OBS_KINECT_PLUGIN_GREENSCREENEFFECTS

#include <obs-kinect/GreenscreenEffects/BlurBackgroundEffect.hpp>
#include <obs-kinect/GreenscreenEffects/RemoveBackgroundEffect.hpp>
#include <obs-kinect/GreenscreenEffects/ReplaceBackgroundEffect.hpp>
#include <variant>

using GreenscreenEffects = std::variant<BlurBackgroundEffect, RemoveBackgroundEffect, ReplaceBackgroundEffect>;

// Build a std::variant<T1::Config, T2::Config> from std::variant<T1, T2>
template<typename>
struct GreenscreenEffectConfigVariantGen;

template<typename... Args>
struct GreenscreenEffectConfigVariantGen<std::variant<Args...>>
{
	template<typename T>
	struct ToConfig
	{
		using R = typename T::Config;
	};

	template<typename T>
	using ToConfig_t = typename ToConfig<T>::R;

	using Type = std::variant<ToConfig_t<Args>...>;
};

using GreenscreenEffectConfigs = typename GreenscreenEffectConfigVariantGen<GreenscreenEffects>::Type;

#endif
