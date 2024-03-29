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

#include <obs-kinect/Shaders/AlphaMaskShader.hpp>
#include <obs-kinect-core/Helper.hpp>
#include <string>
#include <stdexcept>

AlphaMaskShader::AlphaMaskShader()
{
	ObsMemoryPtr<char> effectFilename(obs_module_file("alpha_mask.effect"));

	ObsGraphics gfx;

	char* errStr = nullptr;
	m_effect = gs_effect_create_from_file(effectFilename.get(), &errStr);
	ObsMemoryPtr<char> errStrOwner(errStr);

	if (m_effect)
	{
		m_params_ColorImage = gs_effect_get_param_by_name(m_effect, "ColorImage");
		m_params_MaskImage = gs_effect_get_param_by_name(m_effect, "MaskImage");
		m_tech_Draw = gs_effect_get_technique(m_effect, "Draw");

		m_workTexture = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	}
	else
	{
		std::string err("failed to create effect: ");
		err.append((errStr) ? errStr : "shader error");

		throw std::runtime_error(err);
	}
}

AlphaMaskShader::~AlphaMaskShader()
{
	ObsGraphics gfx;

	gs_effect_destroy(m_effect);
	gs_texrender_destroy(m_workTexture);
}

gs_texture_t* AlphaMaskShader::Filter(gs_texture_t* color, gs_texture_t* mask)
{
	std::uint32_t colorWidth = gs_texture_get_width(color);
	std::uint32_t colorHeight = gs_texture_get_height(color);

	gs_texrender_reset(m_workTexture);
	if (!gs_texrender_begin(m_workTexture, colorWidth, colorHeight))
		return nullptr;

	vec4 black = { 0.f, 0.f, 0.f, 0.f };
	gs_clear(GS_CLEAR_COLOR, &black, 0.f, 0);
	gs_ortho(0.0f, float(colorWidth), 0.0f, float(colorHeight), -100.0f, 100.0f);

	gs_effect_set_texture(m_params_ColorImage, color);
	gs_effect_set_texture(m_params_MaskImage, mask);

	gs_technique_begin(m_tech_Draw);
	gs_technique_begin_pass(m_tech_Draw, 0);
	gs_draw_sprite(nullptr, 0, colorWidth, colorHeight);
	gs_technique_end_pass(m_tech_Draw);
	gs_technique_end(m_tech_Draw);

	gs_texrender_end(m_workTexture);

	return gs_texrender_get_texture(m_workTexture);
}
