/******************************************************************************
	Copyright (C) 2021 by Jérôme Leclercq <lynix680@gmail.com>

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

#include <obs-kinect/GreenscreenEffects/BlurBackgroundEffect.hpp>
#include <obs-kinect-core/Helper.hpp>
#include <string>
#include <stdexcept>

BlurBackgroundEffect::BlurBackgroundEffect() :
m_backgroundBlur(GS_RGBA)
{
}

gs_texture_t* BlurBackgroundEffect::Apply(const Config& config, gs_texture_t* sourceTexture, gs_texture_t* filterTexture)
{
	if (config.backgroundBlurPassCount == 0)
		return sourceTexture;
	
	gs_texture_t* blurredBackground = m_backgroundBlur.Blur(sourceTexture, config.backgroundBlurPassCount);
	gs_texture_t* from = blurredBackground;
	gs_texture_t* to = sourceTexture;
	if (config.reversed)
		std::swap(from, to);

	return m_textureLerp.Lerp(from, to, filterTexture);
}

obs_properties_t* BlurBackgroundEffect::BuildProperties()
{
	obs_properties_t* properties = obs_properties_create();

	obs_properties_add_int_slider(properties, "blurbackground_blurstrength", obs_module_text("ObsKinect.BlurBackground.Strength"), 0, 50, 1);
	obs_properties_add_bool(properties, "blurbackground_reversed", obs_module_text("ObsKinect.BlurBackground.Reversed"));

	return properties;
}

void BlurBackgroundEffect::SetDefaultValues(obs_data_t* settings)
{
	Config defaultValues;
	obs_data_set_default_int(settings, "blurbackground_blurstrength", defaultValues.backgroundBlurPassCount);
	obs_data_set_default_bool(settings, "blurbackground_reversed", defaultValues.reversed);
}

auto BlurBackgroundEffect::ToConfig(obs_data_t* settings) -> Config
{
	Config config;
	config.backgroundBlurPassCount = obs_data_get_int(settings, "blurbackground_blurstrength");
	config.reversed = obs_data_get_bool(settings, "blurbackground_reversed");

	return config;
}
