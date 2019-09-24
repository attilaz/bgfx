$input v_worldPosition, v_worldNormal, v_worldTangent, v_worldBitangent, v_lightSpacePosition

#include "../common/common.sh"

#if 0
    cg.generateDefine(fs, "GEOMETRIC_SPECULAR_AA", material.specularAntiAliasing && lit);

    cg.generateDefine(fs, "CLEAR_COAT_IOR_CHANGE", material.clearCoatIorChange);

    bool specularAO = material.specularAOSet ?
            material.specularAO : !isMobileTarget(shaderModel);
    cg.generateDefine(fs, "SPECULAR_AMBIENT_OCCLUSION", specularAO ? 1u : 0u);

    bool multiBounceAO = material.multiBounceAOSet ?
            material.multiBounceAO : !isMobileTarget(shaderModel);
    cg.generateDefine(fs, "MULTI_BOUNCE_AMBIENT_OCCLUSION", multiBounceAO ? 1u : 0u);

    // lighting variants
    bool litVariants = lit || material.hasShadowMultiplier;
    cg.generateDefine(fs, "HAS_DIRECTIONAL_LIGHTING", litVariants && variant.hasDirectionalLighting());
    cg.generateDefine(fs, "HAS_DYNAMIC_LIGHTING", litVariants && variant.hasDynamicLighting());
    cg.generateDefine(fs, "HAS_SHADOWING", litVariants && variant.hasShadowReceiver());
    cg.generateDefine(fs, "HAS_SHADOW_MULTIPLIER", material.hasShadowMultiplier);

    // material defines
    cg.generateDefine(fs, "MATERIAL_HAS_DOUBLE_SIDED_CAPABILITY", material.hasDoubleSidedCapability);
    switch (material.blendingMode) {
        case BlendingMode::OPAQUE:
            cg.generateDefine(fs, "BLEND_MODE_OPAQUE", true);
            break;
        case BlendingMode::TRANSPARENT:
            cg.generateDefine(fs, "BLEND_MODE_TRANSPARENT", true);
            break;
        case BlendingMode::ADD:
            cg.generateDefine(fs, "BLEND_MODE_ADD", true);
            break;
        case BlendingMode::MASKED:
            cg.generateDefine(fs, "BLEND_MODE_MASKED", true);
            break;
        case BlendingMode::FADE:
            // Fade is a special case of transparent
            cg.generateDefine(fs, "BLEND_MODE_TRANSPARENT", true);
            cg.generateDefine(fs, "BLEND_MODE_FADE", true);
            break;
        case BlendingMode::MULTIPLY:
            cg.generateDefine(fs, "BLEND_MODE_MULTIPLY", true);
            break;
        case BlendingMode::SCREEN:
            cg.generateDefine(fs, "BLEND_MODE_SCREEN", true);
            break;
    }
    switch (material.postLightingBlendingMode) {
        case BlendingMode::OPAQUE:
            cg.generateDefine(fs, "POST_LIGHTING_BLEND_MODE_OPAQUE", true);
            break;
        case BlendingMode::TRANSPARENT:
            cg.generateDefine(fs, "POST_LIGHTING_BLEND_MODE_TRANSPARENT", true);
            break;
        case BlendingMode::ADD:
            cg.generateDefine(fs, "POST_LIGHTING_BLEND_MODE_ADD", true);
            break;
        case BlendingMode::MULTIPLY:
            cg.generateDefine(fs, "POST_LIGHTING_BLEND_MODE_MULTIPLY", true);
            break;
        case BlendingMode::SCREEN:
            cg.generateDefine(fs, "POST_LIGHTING_BLEND_MODE_SCREEN", true);
            break;
        default:
            break;
    }
    cg.generateDefine(fs, getShadingDefine(material.shading), true);
        case filament::Shading::LIT:                 return "SHADING_MODEL_LIT";
        case filament::Shading::UNLIT:               return "SHADING_MODEL_UNLIT";
        case filament::Shading::SUBSURFACE:          return "SHADING_MODEL_SUBSURFACE";
        case filament::Shading::CLOTH:               return "SHADING_MODEL_CLOTH";
        case filament::Shading::SPECULAR_GLOSSINESS: return "SHADING_MODEL_SPECULAR_GLOSSINESS";

    generateMaterialDefines(fs, cg, mProperties);

    cg.generateShaderInputs(fs, ShaderType::FRAGMENT, material.requiredAttributes, interpolation);

    // custom material variables
    size_t variableIndex = 0;
    for (const auto& variable : mVariables) {
        cg.generateVariable(fs, ShaderType::FRAGMENT, variable, variableIndex++);
    }

    // uniforms and samplers
    cg.generateUniforms(fs, ShaderType::FRAGMENT,
            BindingPoints::PER_VIEW, UibGenerator::getPerViewUib());
    cg.generateUniforms(fs, ShaderType::FRAGMENT,
            BindingPoints::LIGHTS, UibGenerator::getLightsUib());
    cg.generateUniforms(fs, ShaderType::FRAGMENT,
            BindingPoints::PER_MATERIAL_INSTANCE, material.uib);
    cg.generateSeparator(fs);
    cg.generateSamplers(fs,
            material.samplerBindings.getBlockOffset(BindingPoints::PER_VIEW),
            SibGenerator::getPerViewSib());
    cg.generateSamplers(fs,
            material.samplerBindings.getBlockOffset(BindingPoints::PER_MATERIAL_INSTANCE),
            material.sib);

    // shading model
        appendShader(fs, mMaterialCode, mMaterialLineOffset);
        if (material.isLit) {
            cg.generateShaderLit(fs, ShaderType::FRAGMENT, variant, material.shading);
        } else {
            cg.generateShaderUnlit(fs, ShaderType::FRAGMENT, variant, material.hasShadowMultiplier);
        }
        // entry point
        cg.generateShaderMain(fs, ShaderType::FRAGMENT);
#endif

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

	// Initialize the inputs to sensible default values, see material_inputs.fs
	MaterialInputs materialIn;
	initMaterial(materialIn);

	// todo: modify material inputs here

	gl_FragColor = evaluate(materialIn, stageIn);
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
