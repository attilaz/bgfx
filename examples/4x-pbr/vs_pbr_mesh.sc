$input a_position, a_normal, a_texcoord0
$output v_worldPosition, v_worldNormal, v_worldTangent, v_worldBitangent, v_lightSpacePosition

#include "../common/common.sh"

#if 0
    cg.generateDefine(vs, "FLIP_UV_ATTRIBUTE", material.flipUV);

    cg.generateDefine(vs, "HAS_DIRECTIONAL_LIGHTING", litVariants && variant.hasDirectionalLighting());
    cg.generateDefine(vs, "HAS_SHADOWING", litVariants && variant.hasShadowReceiver());
    cg.generateDefine(vs, "HAS_SHADOW_MULTIPLIER", material.hasShadowMultiplier);
    cg.generateDefine(vs, "HAS_SKINNING_OR_MORPHING", variant.hasSkinningOrMorphing());
    cg.generateDefine(vs, getShadingDefine(material.shading), true);

        case filament::Shading::LIT:                 return "SHADING_MODEL_LIT";
        case filament::Shading::UNLIT:               return "SHADING_MODEL_UNLIT";
        case filament::Shading::SUBSURFACE:          return "SHADING_MODEL_SUBSURFACE";
        case filament::Shading::CLOTH:               return "SHADING_MODEL_CLOTH";
        case filament::Shading::SPECULAR_GLOSSINESS: return "SHADING_MODEL_SPECULAR_GLOSSINESS";

    generateMaterialDefines(vs, cg, mProperties);

    switch (domain) {
            cg.generateDefine(vs, "VERTEX_DOMAIN_OBJECT", true);
            cg.generateDefine(vs, "VERTEX_DOMAIN_WORLD", true);
            cg.generateDefine(vs, "VERTEX_DOMAIN_VIEW", true);
            cg.generateDefine(vs, "VERTEX_DOMAIN_DEVICE", true);
    }


    AttributeBitset attributes = material.requiredAttributes;
    if (variant.hasSkinningOrMorphing()) {
        attributes.set(VertexAttribute::BONE_INDICES);
        attributes.set(VertexAttribute::BONE_WEIGHTS);
        attributes.set(VertexAttribute::MORPH_POSITION_0);
        attributes.set(VertexAttribute::MORPH_POSITION_1);
        attributes.set(VertexAttribute::MORPH_POSITION_2);
        attributes.set(VertexAttribute::MORPH_POSITION_3);
        attributes.set(VertexAttribute::MORPH_TANGENTS_0);
        attributes.set(VertexAttribute::MORPH_TANGENTS_1);
        attributes.set(VertexAttribute::MORPH_TANGENTS_2);
        attributes.set(VertexAttribute::MORPH_TANGENTS_3);
    }
    cg.generateShaderInputs(vs, ShaderType::VERTEX, attributes, interpolation);

    // custom material variables
    size_t variableIndex = 0;
    for (const auto& variable : mVariables) {
        cg.generateVariable(vs, ShaderType::VERTEX, variable, variableIndex++);
    }

    // uniforms
    cg.generateUniforms(vs, ShaderType::VERTEX,
            BindingPoints::PER_VIEW, UibGenerator::getPerViewUib());
    cg.generateUniforms(vs, ShaderType::VERTEX,
            BindingPoints::PER_RENDERABLE, UibGenerator::getPerRenderableUib());
    if (variant.hasSkinningOrMorphing()) {
        cg.generateUniforms(vs, ShaderType::VERTEX,
                BindingPoints::PER_RENDERABLE_BONES,
                UibGenerator::getPerRenderableBonesUib());
    }
    cg.generateUniforms(vs, ShaderType::VERTEX,
            BindingPoints::PER_MATERIAL_INSTANCE, material.uib);
    cg.generateSeparator(vs);
    // TODO: should we generate per-view SIB in the vertex shader?
    cg.generateSamplers(vs,
            material.samplerBindings.getBlockOffset(BindingPoints::PER_MATERIAL_INSTANCE),
            material.sib);

    // main entry point
    appendShader(vs, mMaterialVertexCode, mMaterialVertexLineOffset);
    cg.generateShaderMain(vs, ShaderType::VERTEX);
#endif

#define VERTEX_DOMAIN_OBJECT
//#define VERTEX_DOMAIN_DEVICE

#include "filament.sh"


void main()
{
	filamentSetAttributePosition(a_position);
#if defined(HAS_ATTRIBUTE_TANGENTS)
	filamentSetAttributeTangents(vec4(0.0,0.0,0.0,1.0));	//todo
#endif

   // Initialize the inputs to sensible default values, see material_inputs.vs
   VertexOutput output;
   filamentEvaluate(output);

	v_worldPosition = output.worldPosition;
	v_worldNormal = output.worldNormal;
	v_worldTangent = output.worldTangent;
	v_worldBitangent = output.worldBitangent;
	v_lightSpacePosition = output.lightSpacePosition;
	gl_Position = output.clipPosition;
}


#if 0

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _Global
{
    float4x4 u_viewProj;
    float4x4 u_model[32];
};

struct xlatMtlMain_out
{
    float3 _entryPointOutput_v_normal [[user(locn0)]];
    float3 _entryPointOutput_v_vertex_worldPosition [[user(locn1)]];
    float3 _entryPointOutput_v_view [[user(locn2)]];
    float4 gl_Position [[position]];
};

struct xlatMtlMain_in
{
    float4 a_position [[attribute(1)]];
};

vertex xlatMtlMain_out xlatMtlMain(xlatMtlMain_in in [[stage_in]], constant _Global& _mtl_u [[buffer(0)]])
{
    xlatMtlMain_out out = {};
    float4 _253 = (float4(_mtl_u.u_model[0][0][0], _mtl_u.u_model[0][1][0], _mtl_u.u_model[0][2][0], _mtl_u.u_model[0][3][0]) * in.a_position.x) + ((float4(_mtl_u.u_model[0][0][1], _mtl_u.u_model[0][1][1], _mtl_u.u_model[0][2][1], _mtl_u.u_model[0][3][1]) * in.a_position.y) + ((float4(_mtl_u.u_model[0][0][2], _mtl_u.u_model[0][1][2], _mtl_u.u_model[0][2][2], _mtl_u.u_model[0][3][2]) * in.a_position.z) + float4(_mtl_u.u_model[0][0][3], _mtl_u.u_model[0][1][3], _mtl_u.u_model[0][2][3], _mtl_u.u_model[0][3][3])));
    out.gl_Position = _mtl_u.u_viewProj * _253;
    out._entryPointOutput_v_normal = float3(0.0, 0.0, 1.0);
    out._entryPointOutput_v_vertex_worldPosition = _253.xyz;
    out._entryPointOutput_v_view = float3(0.0);
    return out;
}

   € 
 #endif
