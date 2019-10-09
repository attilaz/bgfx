$input v_worldPosition, v_worldNormal, v_worldTangent, v_worldBitangent, v_lightSpacePosition

#include "../common/common.sh"

uniform vec4 u_materialInput[8];

#define u_materialInput_baseColor u_materialInput[0]
#define u_materialInput_roughness u_materialInput[1].x
#define u_materialInput_metallic u_materialInput[1].y
#define u_materialInput_reflectance u_materialInput[1].z
#define u_materialInput_emissive u_materialInput[2]
#define u_materialInput_clearCoat u_materialInput[3].x
#define u_materialInput_clearCoatRoughness u_materialInput[3].y
#define u_materialInput_anisotropy u_materialInput[3].z
#define u_materialInput_anisotropyDirection u_materialInput[4].xyz
#define u_materialInput_thickness u_materialInput[4].w
#define u_materialInput_subsurfaceColor u_materialInput[5].xyz
#define u_materialInput_subsurfacePower u_materialInput[5].w
#define u_materialInput_sheenColor u_materialInput[6].xyz
#define u_materialInput_specularColor u_materialInput[7].xyz
#define u_materialInput_glossiness u_materialInput[7].w

//#define SHADING_MODEL_SPECULAR_GLOSSINESS
//#define SHADING_MODEL_CLOTH
//#define SHADING_MODEL_SUBSURFACE         
//#define SHADING_MODEL_UNLIT

//#define MATERIAL_HAS_SUBSURFACE_COLOR
//#define MATERIAL_HAS_NORMAL
//#define MATERIAL_HAS_CLEAR_COAT
//#define MATERIAL_HAS_CLEAR_COAT_NORMAL
//#define MATERIAL_HAS_ANISOTROPY
//#define MATERIAL_HAS_DOUBLE_SIDED_CAPABILITY
//#defne MATERIAL_HAS_AMBIENT_OCCLUSION
//#define MATERIAL_HAS_CLEAR_COAT_ROUGHNESS
//#define MATERIAL_HAS_EMISSIVE

//lighting
//#define HAS_SHADOWING
//#define HAS_DIRECTIONAL_LIGHTING
//#define HAS_DYNAMIC_LIGHTING
//#define HAS_SHADOW_MULTIPLIER

//#define GEOMETRIC_SPECULAR_AA
//#define SPECULAR_AMBIENT_OCCLUSION	(1)
//#define MULTI_BOUNCE_AMBIENT_OCCLUSION (1)
//#define CLEAR_COAT_IOR_CHANGE

//#define BLEND_MODE_MASKED
//#define BLEND_MODE_TRANSPARENT
//#define BLEND_MODE_FADE



#include "filament.sh"

void main() {
	FragmentStageInputs stageIn;
	initFragmentStageInputs(stageIn);
	stageIn.worldPosition = v_worldPosition;
	stageIn.worldNormal = v_worldNormal;
	stageIn.worldTangent = v_worldTangent;
	stageIn.worldBitangent = v_worldBitangent;
	stageIn.fragCoord = gl_FragCoord;
	stageIn.frontFacing = gl_FrontFacing;

	MaterialInputs material;
	initMaterial(material);

	material.baseColor = u_materialInput_baseColor;
#if !defined(SHADING_MODEL_UNLIT)
#if !defined(SHADING_MODEL_SPECULAR_GLOSSINESS)
	material.roughness = u_materialInput_roughness;
#endif
#if !defined(SHADING_MODEL_CLOTH) && !defined(SHADING_MODEL_SPECULAR_GLOSSINESS)
	material.metallic = u_materialInput_metallic;
	material.reflectance = u_materialInput_reflectance;
#endif
	material.ambientOcclusion = 0.0;
#endif
	material.emissive = u_materialInput_emissive;
	material.clearCoat = u_materialInput_clearCoat;
	material.clearCoatRoughness = u_materialInput_clearCoatRoughness;
	material.anisotropy = u_materialInput_anisotropy;
	material.anisotropyDirection = u_materialInput_anisotropyDirection;
#if defined(SHADING_MODEL_SUBSURFACE)
	material.thickness = u_materialInput_thickness;
	material.subsurfaceColor = u_materialInput_subsurfaceColor;
	material.subsurfacePower = u_materialInput_subsurfacePower;
#endif
#if defined(SHADING_MODEL_CLOTH)
	material.sheenColor = u_materialInput_sheenColor;
#if defined(MATERIAL_HAS_SUBSURFACE_COLOR)
	material.subsurfaceColor = u_materialInput_subsurfaceColor;
#endif
#endif

#if defined(SHADING_MODEL_SPECULAR_GLOSSINESS)
	material.specularColor = u_materialInput_specularColor;
	material.glossiness = u_materialInput_glossiness;
#endif

#if defined(MATERIAL_HAS_NORMAL)
	material.normal = vec3(0.0,0.0,1.0);
#endif
#if defined(MATERIAL_HAS_CLEAR_COAT) && defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
	material.clearCoatNormal = vec3(0.0,0.0,1.0);
#endif

	gl_FragColor = evaluate(material, stageIn);
}

#if 0
#include <simd/simd.h>

using namespace metal;

constant float _1696 = {};
constant float3 _1697 = {};
constant float4 _1698 = {};

struct xlatMtlMain_out
{
    float4 bgfx_FragData0 [[color(0)]];
};

fragment xlatMtlMain_out xlatMtlMain(texture2d<float> light_iblDFG [[texture(3)]], texturecube<float> light_iblSpecular [[texture(4)]], texture2d<float> light_ssao [[texture(5)]], sampler light_iblDFGSampler [[sampler(3)]], sampler light_iblSpecularSampler [[sampler(4)]], sampler light_ssaoSampler [[sampler(5)]], float4 gl_FragCoord [[position]])
{
    xlatMtlMain_out out = {};
    float4 _1030 = light_iblDFG.sample(light_iblDFGSampler, float2(_1696, 1.0), level(0.0));
    float2 _1676;
    out.bgfx_FragData0 = float4((((float3(0.800000011920928955078125) * fast::max((((((((_1697 + (_1697 * _1697.y)) + (_1697 * _1697.z)) + (_1697 * _1697.x)) + (_1697 * (_1697.y * _1697.x))) + (_1697 * (_1697.y * _1697.z))) + (_1697 * (((3.0 * _1697.z) * _1697.z) - 1.0))) + (_1697 * (_1697.z * _1697.x))) + (_1697 * ((_1697.x * _1697.x) - (_1697.y * _1697.y))), float3(0.0))) * fast::min(1.0, light_ssao.sample(light_ssaoSampler, (gl_FragCoord.xy * _1698.zw)).x)) + (((float3(0.039999999105930328369140625) * _1030.x) + float3(_1030.y)) * light_iblSpecular.sample(light_iblSpecularSampler, _1697, level(_1676.x)).xyz)) * _1696, 1.0);
    return out;
}
#endif
