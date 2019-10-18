$input v_worldPosition, v_worldNormal, v_worldTangent, v_worldBitangent, v_lightSpacePosition

#include "../common/common.sh"

uniform vec4 u_materialUniforms[8];

#define u_materialInput_baseColor u_materialUniforms[0]
#define u_materialInput_roughness u_materialUniforms[1].x
#define u_materialInput_metallic u_materialUniforms[1].y
#define u_materialInput_reflectance u_materialUniforms[1].z
#define u_materialInput_emissive u_materialUniforms[2]
#define u_materialInput_clearCoat u_materialUniforms[3].x
#define u_materialInput_clearCoatRoughness u_materialUniforms[3].y
#define u_materialInput_anisotropy u_materialUniforms[3].z
#define u_materialInput_anisotropyDirection u_materialUniforms[4].xyz
#define u_materialInput_thickness u_materialUniforms[4].w
#define u_materialInput_subsurfaceColor u_materialUniforms[5].xyz
#define u_materialInput_subsurfacePower u_materialUniforms[5].w
#define u_materialInput_sheenColor u_materialUniforms[6].xyz
#define u_materialInput_specularColor u_materialUniforms[7].xyz
#define u_materialInput_glossiness u_materialUniforms[7].w

#define HAS_ATTRIBUTE_TANGENTS

//#define SHADING_MODEL_SPECULAR_GLOSSINESS
//#define SHADING_MODEL_CLOTH
//#define SHADING_MODEL_SUBSURFACE         
//#define SHADING_MODEL_UNLIT

#define MATERIAL_HAS_SUBSURFACE_COLOR
#define MATERIAL_HAS_NORMAL
#define MATERIAL_HAS_CLEAR_COAT
#define MATERIAL_HAS_CLEAR_COAT_NORMAL
#define MATERIAL_HAS_ANISOTROPY
#define MATERIAL_HAS_DOUBLE_SIDED_CAPABILITY
#define MATERIAL_HAS_AMBIENT_OCCLUSION
#define MATERIAL_HAS_CLEAR_COAT_ROUGHNESS
#define MATERIAL_HAS_EMISSIVE

//lighting
//#define HAS_SHADOWING
#define HAS_DIRECTIONAL_LIGHTING
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
	stageIn.worldNormal = normalize(v_worldNormal);
	stageIn.worldTangent = normalize(v_worldTangent);
	stageIn.worldBitangent = normalize(v_worldBitangent);
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
	material.normal = v_worldNormal;
#endif
#if defined(MATERIAL_HAS_CLEAR_COAT) && defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
	material.clearCoatNormal = vec3(0.0,0.0,1.0);
#endif

	gl_FragColor = evaluate(material, stageIn);
}
