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

#include "TextureLerpShader.hpp"
#include "Helper.hpp"
#include <string>
#include <stdexcept>

TextureLerpShader::TextureLerpShader()
{
	ObsMemoryPtr<char> effectFilename(obs_module_file("texture_lerp.effect"));

	ObsGraphics gfx;

	char* errStr = nullptr;
	m_effect = gs_effect_create_from_file(effectFilename.get(), &errStr);
	ObsMemoryPtr<char> errStrOwner(errStr);

	if (m_effect)
	{
		m_params_FactorImage = gs_effect_get_param_by_name(m_effect, "FactorImage");
		m_params_FromImage = gs_effect_get_param_by_name(m_effect, "FromImage");
		m_params_ToImage = gs_effect_get_param_by_name(m_effect, "ToImage");
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

TextureLerpShader::~TextureLerpShader()
{
	ObsGraphics gfx;

	gs_effect_destroy(m_effect);
	gs_texrender_destroy(m_workTexture);
}

gs_texture_t* TextureLerpShader::Lerp(gs_texture_t* from, gs_texture_t* to, gs_texture_t* factor)
{
	std::uint32_t colorWidth = std::max(gs_texture_get_width(from), gs_texture_get_width(to));
	std::uint32_t colorHeight = std::max(gs_texture_get_height(from), gs_texture_get_height(to));

	gs_texrender_reset(m_workTexture);
	if (!gs_texrender_begin(m_workTexture, colorWidth, colorHeight))
		return nullptr;

	vec4 black = { 0.f, 0.f, 0.f, 0.f };
	gs_clear(GS_CLEAR_COLOR, &black, 0.f, 0);
	gs_ortho(0.0f, float(colorWidth), 0.0f, float(colorHeight), -100.0f, 100.0f);

	gs_effect_set_texture(m_params_FactorImage, factor);
	gs_effect_set_texture(m_params_FromImage, from);
	gs_effect_set_texture(m_params_ToImage, to);

	gs_technique_begin(m_tech_Draw);
	gs_technique_begin_pass(m_tech_Draw, 0);
	gs_draw_sprite(nullptr, 0, colorWidth, colorHeight);
	gs_technique_end_pass(m_tech_Draw);
	gs_technique_end(m_tech_Draw);

	gs_texrender_end(m_workTexture);

	return gs_texrender_get_texture(m_workTexture);
}
