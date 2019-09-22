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


#if 0
struct VertexOutput
{

};

void filementEvaluate()

#endif

void main()
{
   // Initialize the inputs to sensible default values, see material_inputs.vs

#if defined(HAS_ATTRIBUTE_TANGENTS)
    // If the material defines a value for the "normal" property, we need to output
    // the full orthonormal basis to apply normal mapping
    #if defined(MATERIAL_HAS_ANISOTROPY) || defined(MATERIAL_HAS_NORMAL) || defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
        // Extract the normal and tangent in world space from the input quaternion
        // We encode the orthonormal basis as a quaternion to save space in the attributes
        toTangentFrame(mesh_tangents, vertex_worldNormal, vertex_worldTangent);

        #if defined(HAS_SKINNING_OR_MORPHING)

            if (objectUniforms.morphingEnabled == 1) {
                vec3 normal0, normal1, normal2, normal3;
                toTangentFrame(mesh_custom4, normal0);
                toTangentFrame(mesh_custom5, normal1);
                toTangentFrame(mesh_custom6, normal2);
                toTangentFrame(mesh_custom7, normal3);
                vertex_worldNormal += objectUniforms.morphWeights.x * normal0;
                vertex_worldNormal += objectUniforms.morphWeights.y * normal1;
                vertex_worldNormal += objectUniforms.morphWeights.z * normal2;
                vertex_worldNormal += objectUniforms.morphWeights.w * normal3;
                vertex_worldNormal = normalize(vertex_worldNormal);
            }

            if (objectUniforms.skinningEnabled == 1) {
                skinNormal(vertex_worldNormal, mesh_bone_indices, mesh_bone_weights);
                skinNormal(vertex_worldTangent, mesh_bone_indices, mesh_bone_weights);
            }

        #endif

        // We don't need to normalize here, even if there's a scale in the matrix
        // because we ensure the worldFromModelNormalMatrix pre-scales the normal such that
        // all its components are < 1.0. This precents the bitangent to exceed the range of fp16
        // in the fragment shader, where we renormalize after interpolation
        vertex_worldTangent = objectUniforms.worldFromModelNormalMatrix * vertex_worldTangent;
        vertex_worldNormal = objectUniforms.worldFromModelNormalMatrix * vertex_worldNormal;

        // Reconstruct the bitangent from the normal and tangent. We don't bother with
        // normalization here since we'll do it after interpolation in the fragment stage
        vertex_worldBitangent =
                cross(vertex_worldNormal, vertex_worldTangent) * sign(mesh_tangents.w);
    #else // MATERIAL_HAS_ANISOTROPY || MATERIAL_HAS_NORMAL
        // Without anisotropy or normal mapping we only need the normal vector
        toTangentFrame(mesh_tangents, material.worldNormal);
        vertex_worldNormal = objectUniforms.worldFromModelNormalMatrix * material.worldNormal;
        #if defined(HAS_SKINNING_OR_MORPHING)
            if (objectUniforms.skinningEnabled == 1) {
                skinNormal(vertex_worldNormal, mesh_bone_indices, mesh_bone_weights);
            }
        #endif
    #endif // MATERIAL_HAS_ANISOTROPY || MATERIAL_HAS_NORMAL
#endif // HAS_ATTRIBUTE_TANGENTS

    // The world position can be changed by the user in materialVertex()
    vertex_worldPosition = computeWorldPosition();

	// user code here??

#if defined(HAS_SHADOWING) && defined(HAS_DIRECTIONAL_LIGHTING)
    vertex_lightSpacePosition = getLightSpacePosition(vertex_worldPosition, vertex_worldNormal);
#endif

#if defined(VERTEX_DOMAIN_DEVICE)
    // The other vertex domains are handled in initMaterialVertex()->computeWorldPosition()
    gl_Position = getPosition();
#else
    gl_Position = mul(getClipFromWorldMatrix(), vertex_worldPosition);
#endif

#if defined(TARGET_VULKAN_ENVIRONMENT)
    // In Vulkan, clip-space Z is [0,w] rather than [-w,+w] and Y is flipped.
    gl_Position.y = -gl_Position.y;
    gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;
#endif
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
