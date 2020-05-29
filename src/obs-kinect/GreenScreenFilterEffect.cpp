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

#include "GreenScreenFilterEffect.hpp"
#include "Helper.hpp"
#include <string>
#include <stdexcept>

GreenScreenFilterEffect::GreenScreenFilterEffect()
{
	ObsMemoryPtr<char> effectFilename(obs_module_file("greenscreen_filter.effect"));

	ObsGraphics gfx;

	char* errStr = nullptr;
	m_effect = gs_effect_create_from_file(effectFilename.get(), &errStr);
	ObsMemoryPtr<char> errStrOwner(errStr);

	if (m_effect)
	{
		m_params_BodyIndexImage = gs_effect_get_param_by_name(m_effect, "BodyIndexImage");
		m_params_DepthImage = gs_effect_get_param_by_name(m_effect, "DepthImage");
		m_params_DepthMappingImage = gs_effect_get_param_by_name(m_effect, "DepthMappingImage");
		m_params_InvDepthImageSize = gs_effect_get_param_by_name(m_effect, "InvDepthImageSize");
		m_params_InvDepthProgressive = gs_effect_get_param_by_name(m_effect, "InvDepthProgressive");
		m_params_MaxDepth = gs_effect_get_param_by_name(m_effect, "MaxDepth");
		m_params_MinDepth = gs_effect_get_param_by_name(m_effect, "MinDepth");

		m_tech_BodyOnlyWithDepthCorrection = gs_effect_get_technique(m_effect, "BodyOnlyWithDepthCorrection");
		m_tech_BodyOnlyWithoutDepthCorrection = gs_effect_get_technique(m_effect, "BodyOnlyWithoutDepthCorrection");

		m_tech_BodyOrDepthWithDepthCorrection = gs_effect_get_technique(m_effect, "BodyOrDepthWithDepthCorrection");
		m_tech_BodyOrDepthWithoutDepthCorrection = gs_effect_get_technique(m_effect, "BodyOrDepthWithoutDepthCorrection");

		m_tech_BodyWithinDepthWithDepthCorrection = gs_effect_get_technique(m_effect, "BodyWithinDepthWithDepthCorrection");
		m_tech_BodyWithinDepthWithoutDepthCorrection = gs_effect_get_technique(m_effect, "BodyWithinDepthWithoutDepthCorrection");

		m_tech_DepthOnlyWithDepthCorrection = gs_effect_get_technique(m_effect, "DepthOnlyWithDepthCorrection");
		m_tech_DepthOnlyWithoutDepthCorrection = gs_effect_get_technique(m_effect, "DepthOnlyWithoutDepthCorrection");

		m_workTexture = gs_texrender_create(GS_R8, GS_ZS_NONE);
	}
	else
	{
		std::string err("failed to create effect: ");
		err.append((errStr) ? errStr : "shader error");

		throw std::runtime_error(err);
	}
}

GreenScreenFilterEffect::~GreenScreenFilterEffect()
{
	ObsGraphics gfx;

	gs_effect_destroy(m_effect);
	gs_texrender_destroy(m_workTexture);
}

gs_texture_t* GreenScreenFilterEffect::Filter(std::uint32_t width, std::uint32_t height, const BodyFilterParams& params)
{
	if (!Begin(width, height))
		return nullptr;

	SetBodyParams(params);

	gs_technique_t* technique = (params.colorToDepthTexture) ? m_tech_BodyOnlyWithDepthCorrection : m_tech_BodyOnlyWithoutDepthCorrection;

	return Process(width, height, technique);
}

gs_texture_t* GreenScreenFilterEffect::Filter(std::uint32_t width, std::uint32_t height, const BodyOrDepthFilterParams& params)
{
	if (!Begin(width, height))
		return nullptr;

	SetBodyParams(params);
	SetDepthParams(params);

	gs_technique_t* technique = (params.colorToDepthTexture) ? m_tech_BodyOrDepthWithDepthCorrection : m_tech_BodyOrDepthWithoutDepthCorrection;

	return Process(width, height, technique);
}

gs_texture_t* GreenScreenFilterEffect::Filter(std::uint32_t width, std::uint32_t height, const BodyWithinDepthFilterParams& params)
{
	if (!Begin(width, height))
		return nullptr;

	SetBodyParams(params);
	SetDepthParams(params);

	gs_technique_t* technique = (params.colorToDepthTexture) ? m_tech_BodyWithinDepthWithDepthCorrection : m_tech_BodyWithinDepthWithoutDepthCorrection;

	return Process(width, height, technique);
}

gs_texture_t* GreenScreenFilterEffect::Filter(std::uint32_t width, std::uint32_t height, const DepthFilterParams& params)
{
	if (!Begin(width, height))
		return nullptr;

	SetDepthParams(params);

	gs_technique_t* technique = (params.colorToDepthTexture) ? m_tech_DepthOnlyWithDepthCorrection : m_tech_DepthOnlyWithoutDepthCorrection;

	return Process(width, height, technique);
}

bool GreenScreenFilterEffect::Begin(std::uint32_t width, std::uint32_t height)
{
	gs_texrender_reset(m_workTexture);
	if (!gs_texrender_begin(m_workTexture, width, height))
		return false;

	vec4 black = { 0.f, 0.f, 0.f, 1.f };
	gs_clear(GS_CLEAR_COLOR, &black, 0.f, 0);
	gs_ortho(0.0f, float(width), 0.0f, float(height), -100.0f, 100.0f);

	return true;
}

gs_texture* GreenScreenFilterEffect::Process(std::uint32_t width, std::uint32_t height, gs_technique_t* technique)
{
	gs_technique_begin(technique);
	gs_technique_begin_pass(technique, 0);
	gs_draw_sprite(nullptr, 0, width, height);
	gs_technique_end_pass(technique);
	gs_technique_end(technique);

	gs_texrender_end(m_workTexture);

	return gs_texrender_get_texture(m_workTexture);
}
