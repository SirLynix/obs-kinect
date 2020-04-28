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

static const char* greenscreenFilterEffect = R"(
uniform float4x4 ViewProj;
uniform texture2d BodyIndexImage;
uniform texture2d DepthImage;
uniform texture2d DepthMappingImage;
uniform float2 InvDepthImageSize;
uniform float InvDepthProgressive;
uniform float MaxDepth;
uniform float MinDepth;

sampler_state textureSampler {
	Filter   = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

sampler_state depthSampler {
	Filter   = Point;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct VertData {
	float4 pos : POSITION;
	float2 uv : TEXCOORD0;
};

VertData VSDefault(VertData vert_in)
{
	VertData vert_out;
	vert_out.pos = mul(float4(vert_in.pos.xyz, 1.0), ViewProj);
	vert_out.uv = vert_in.uv;
	return vert_out;
}

float4 PSBodyOnlyDepthCorrection(VertData vert_in) : TARGET
{
	float2 texCoords = DepthMappingImage.Sample(textureSampler, vert_in.uv).xy * InvDepthImageSize;
	float bodyIndex = BodyIndexImage.Sample(depthSampler, texCoords).r;

	bool check = (bodyIndex < 0.1);
	float value = (check) ? 1.0 : 0.0;

	return float4(value, value, value, value);
}

float4 PSBodyOnlyNoDepthCorrection(VertData vert_in) : TARGET
{
	float bodyIndex = BodyIndexImage.Sample(depthSampler, vert_in.uv).r;

	bool check = (bodyIndex < 0.1);
	float value = (check) ? 1.0 : 0.0;

	return float4(value, value, value, value);
}

technique BodyOnlyWithDepthCorrection
{
	pass
	{
		vertex_shader = VSDefault(vert_in);
		pixel_shader = PSBodyOnlyDepthCorrection(vert_in);
	}
}

technique BodyOnlyWithoutDepthCorrection
{
	pass
	{
		vertex_shader = VSDefault(vert_in);
		pixel_shader = PSBodyOnlyNoDepthCorrection(vert_in);
	}
}
float4 PSDepthOnlyDepthCorrection(VertData vert_in) : TARGET
{
	float2 texCoords = DepthMappingImage.Sample(textureSampler, vert_in.uv).xy * InvDepthImageSize;
	float depth = DepthImage.Sample(depthSampler, texCoords).r;

	bool check = (texCoords.x > 0.0 && texCoords.y > 0.0 && texCoords.x < 1.0 && texCoords.y < 1.0) &&
	             (depth > MinDepth && depth < MaxDepth);

	float value = (check) ? saturate((MaxDepth - depth) * InvDepthProgressive) : 0.0;

	return float4(value, value, value, value);
}

float4 PSDepthOnlyWithoutDepthCorrection(VertData vert_in) : TARGET
{
	float depth = DepthImage.Sample(depthSampler, vert_in.uv).r;

	bool check = (depth > MinDepth && depth < MaxDepth);

	float value = (check) ? saturate((MaxDepth - depth) * InvDepthProgressive) : 0.0;

	return float4(value, value, value, value);
}

technique DepthOnlyWithDepthCorrection
{
	pass
	{
		vertex_shader = VSDefault(vert_in);
		pixel_shader = PSDepthOnlyDepthCorrection(vert_in);
	}
}

technique DepthOnlyWithoutDepthCorrection
{
	pass
	{
		vertex_shader = VSDefault(vert_in);
		pixel_shader = PSDepthOnlyWithoutDepthCorrection(vert_in);
	}
}
)";

GreenScreenFilterEffect::GreenScreenFilterEffect()
{
	ObsGraphics gfx;

	char* errStr;

	m_effect = gs_effect_create(greenscreenFilterEffect, "greenscreen_filter.effect", &errStr);
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

		m_tech_DepthOnlyWithDepthCorrection = gs_effect_get_technique(m_effect, "DepthOnlyWithDepthCorrection");
		m_tech_DepthOnlyWithoutDepthCorrection = gs_effect_get_technique(m_effect, "DepthOnlyWithoutDepthCorrection");

		m_workTexture = gs_texrender_create(GS_R8, GS_ZS_NONE);
	}
	else
	{
		std::string err("failed to create effect: ");
		err.append((errStr) ? errStr : "shader error");
		bfree(errStr);

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
	std::uint32_t bodyIndexWidth = gs_texture_get_width(params.bodyIndexTexture);
	std::uint32_t bodyIndexHeight = gs_texture_get_height(params.bodyIndexTexture);

	gs_texrender_reset(m_workTexture);
	if (!gs_texrender_begin(m_workTexture, width, height))
		return nullptr;

	vec4 black = { 0.f, 0.f, 0.f, 1.f };
	gs_clear(GS_CLEAR_COLOR, &black, 0.f, 0);
	gs_ortho(0.0f, float(width), 0.0f, float(height), -100.0f, 100.0f);

	vec2 invDepthSize = { 1.f / bodyIndexWidth, 1.f / bodyIndexHeight };

	constexpr float maxDepthValue = 0xFFFF;
	constexpr float invMaxDepthValue = 1.f / maxDepthValue;

	gs_effect_set_vec2(m_params_InvDepthImageSize, &invDepthSize);
	gs_effect_set_texture(m_params_BodyIndexImage, params.bodyIndexTexture);
	gs_effect_set_texture(m_params_DepthMappingImage, params.colorToDepthTexture);

	gs_technique_t* technique = (params.colorToDepthTexture) ? m_tech_BodyOnlyWithDepthCorrection : m_tech_BodyOnlyWithoutDepthCorrection;

	gs_technique_begin(technique);
	gs_technique_begin_pass(technique, 0);
	gs_draw_sprite(nullptr, 0, width, height);
	gs_technique_end_pass(technique);
	gs_technique_end(technique);

	gs_texrender_end(m_workTexture);

	return gs_texrender_get_texture(m_workTexture);
}

gs_texture_t* GreenScreenFilterEffect::Filter(std::uint32_t width, std::uint32_t height, const DepthFilterParams& params)
{
	std::uint32_t depthWidth = gs_texture_get_width(params.depthTexture);
	std::uint32_t depthHeight = gs_texture_get_height(params.depthTexture);

	gs_texrender_reset(m_workTexture);
	if (!gs_texrender_begin(m_workTexture, width, height))
		return nullptr;

	vec4 black = { 0.f, 0.f, 0.f, 1.f };
	gs_clear(GS_CLEAR_COLOR, &black, 0.f, 0);
	gs_ortho(0.0f, float(width), 0.0f, float(height), -100.0f, 100.0f);

	vec2 invDepthSize = { 1.f / depthWidth, 1.f / depthHeight };

	constexpr float maxDepthValue = 0xFFFF;
	constexpr float invMaxDepthValue = 1.f / maxDepthValue;

	gs_effect_set_vec2(m_params_InvDepthImageSize, &invDepthSize);
	gs_effect_set_texture(m_params_DepthImage, params.depthTexture);
	gs_effect_set_texture(m_params_DepthMappingImage, params.colorToDepthTexture);
	gs_effect_set_float(m_params_InvDepthProgressive, maxDepthValue / params.progressiveDepth);
	gs_effect_set_float(m_params_MaxDepth, params.maxDepth * invMaxDepthValue);
	gs_effect_set_float(m_params_MinDepth, params.minDepth * invMaxDepthValue);

	gs_technique_t* technique = (params.colorToDepthTexture) ? m_tech_DepthOnlyWithDepthCorrection : m_tech_DepthOnlyWithoutDepthCorrection;

	gs_technique_begin(technique);
	gs_technique_begin_pass(technique, 0);
	gs_draw_sprite(nullptr, 0, width, height);
	gs_technique_end_pass(technique);
	gs_technique_end(technique);

	gs_texrender_end(m_workTexture);

	return gs_texrender_get_texture(m_workTexture);
}
