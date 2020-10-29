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

#include "ReplaceBackgroundEffect.hpp"
#include <string>
#include <stdexcept>

ReplaceBackgroundEffect::ReplaceBackgroundEffect() :
m_lastTextureTick(0)
{
}

gs_texture_t* ReplaceBackgroundEffect::Apply(const Config& config, gs_texture_t* sourceTexture, gs_texture_t* filterTexture)
{
	if (m_texturePath != config.replacementTexturePath)
	{
		if (!m_imageFile)
			m_imageFile.reset(new gs_image_file_t);

		m_texturePath = config.replacementTexturePath;
		gs_image_file_init(m_imageFile.get(), m_texturePath.data());

		ObsGraphics gfx;
		gs_image_file_init_texture(m_imageFile.get());
	}

	if (!m_imageFile || !m_imageFile->texture)
		return sourceTexture;

	// Tick the texture if it's animated
	uint64_t now = obs_get_video_frame_time();
	if (m_lastTextureTick == 0)
		m_lastTextureTick = now;

	if (gs_image_file_tick(m_imageFile.get(), now - m_lastTextureTick))
	{
		ObsGraphics gfx;
		gs_image_file_update_texture(m_imageFile.get());
	}

	m_lastTextureTick = now;

	// Do the lerp
	return m_textureLerp.Lerp(m_imageFile->texture, sourceTexture, filterTexture);
}

obs_properties_t* ReplaceBackgroundEffect::BuildProperties()
{
	obs_properties_t* properties = obs_properties_create();

	std::string filter = obs_module_text("BrowsePath.Images");
	filter += " (*.bmp *.jpg *.jpeg *.tga *.gif *.png);;";
	filter += obs_module_text("BrowsePath.AllFiles");
	filter += " (*.*)";

	obs_properties_add_path(properties, "replacebackground_path", obs_module_text("ObsKinect.ReplaceBackground.Path"), OBS_PATH_FILE, filter.data(), nullptr);

	return properties;
}

void ReplaceBackgroundEffect::SetDefaultValues(obs_data_t* /*settings*/)
{
}

auto ReplaceBackgroundEffect::ToConfig(obs_data_t* settings) -> Config
{
	Config config;
	config.replacementTexturePath = obs_data_get_string(settings, "replacebackground_path");

	return config;
}
