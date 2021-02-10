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

#include <obs-kinect/Shaders/GaussianBlurShader.hpp>
#include <obs-kinect/Helper.hpp>
#include <string>
#include <stdexcept>

GaussianBlurShader::GaussianBlurShader(gs_color_format colorFormat)
{
	ObsMemoryPtr<char> effectFilename(obs_module_file("gaussian_blur.effect"));

	ObsGraphics gfx;

	char* errStr = nullptr;
	m_effect = gs_effect_create_from_file(effectFilename.get(), &errStr);
	ObsMemoryPtr<char> errStrOwner(errStr);

	if (m_effect)
	{
		m_blurEffect_Filter = gs_effect_get_param_by_name(m_effect, "Filter");
		m_blurEffect_Image = gs_effect_get_param_by_name(m_effect, "Image");
		m_blurEffect_InvImageSize = gs_effect_get_param_by_name(m_effect, "InvImageSize");
		m_blurEffect_DrawTech = gs_effect_get_technique(m_effect, "Draw");

		m_workTextureA = gs_texrender_create(colorFormat, GS_ZS_NONE);
		m_workTextureB = gs_texrender_create(colorFormat, GS_ZS_NONE);
	}
	else
	{
		std::string err("failed to create effect: ");
		err.append((errStr) ? errStr : "shader error");

		throw std::runtime_error(err);
	}
}

GaussianBlurShader::~GaussianBlurShader()
{
	ObsGraphics gfx;

	gs_effect_destroy(m_effect);
	gs_texrender_destroy(m_workTextureA);
	gs_texrender_destroy(m_workTextureB);
}

gs_texture_t* GaussianBlurShader::Blur(gs_texture_t* source, std::size_t count)
{
	std::uint32_t width = gs_texture_get_width(source);
	std::uint32_t height = gs_texture_get_height(source);

	vec2 filter;
	vec2 invTextureSize = { 1.f / width, 1.f / height };

	for (std::size_t blurIndex = 0; blurIndex < count; ++blurIndex)
	{
		gs_texrender_reset(m_workTextureA);
		if (!gs_texrender_begin(m_workTextureA, width, height))
			return nullptr;

		gs_ortho(0.0f, float(width), 0.0f, float(height), -100.0f, 100.0f);

		filter.x = 1.f;
		filter.y = 0.f;

		gs_effect_set_vec2(m_blurEffect_Filter, &filter);
		gs_effect_set_vec2(m_blurEffect_InvImageSize, &invTextureSize);
		gs_effect_set_texture(m_blurEffect_Image, (blurIndex == 0) ? source : gs_texrender_get_texture(m_workTextureB));

		gs_technique_begin(m_blurEffect_DrawTech);
		gs_technique_begin_pass(m_blurEffect_DrawTech, 0);
		gs_draw_sprite(nullptr, 0, width, height);
		gs_technique_end_pass(m_blurEffect_DrawTech);
		gs_technique_end(m_blurEffect_DrawTech);

		gs_texrender_end(m_workTextureA);

		gs_texrender_reset(m_workTextureB);
		if (!gs_texrender_begin(m_workTextureB, width, height))
			return nullptr;
			
		gs_ortho(0.0f, float(width), 0.0f, float(height), -100.0f, 100.0f);

		filter.x = 0.f;
		filter.y = 1.f;

		gs_effect_set_vec2(m_blurEffect_Filter, &filter);
		gs_effect_set_vec2(m_blurEffect_InvImageSize, &invTextureSize);
		gs_effect_set_texture(m_blurEffect_Image, gs_texrender_get_texture(m_workTextureA));

		gs_technique_begin(m_blurEffect_DrawTech);
		gs_technique_begin_pass(m_blurEffect_DrawTech, 0);
		gs_draw_sprite(nullptr, 0, width, height);
		gs_technique_end_pass(m_blurEffect_DrawTech);
		gs_technique_end(m_blurEffect_DrawTech);

		gs_texrender_end(m_workTextureB);
	}

	return gs_texrender_get_texture(m_workTextureB);
}
