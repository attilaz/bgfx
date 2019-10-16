// Created from filament shaders https://github.com/google/filament
/*

macro parameters:

// vertex parameters
HAS_ATTRIBUTE_TANGENTS		- process vertex tangents / required for lighting
HAS_ATTRIBUTE_BONE_INDICES  - process bone indices - required for skinning
HAS_ATTRIBUTE_BONE_WEIGHTS  - process bone weights - required for skinning
HAS_SKINNING_OR_MORPHING    - enabled for mesh skinning or morphing

VERTEX_DOMAIN_OBJECT  - attribute 'position' is in object space
VERTEX_DOMAIN_WORLD   - attribute 'position' is in world space
VERTEX_DOMAIN_VIEW    - attribute 'position' is in view space
VERTEX_DOMAIN_DEVICE  - attribute 'position' is in device/clip space

	//enable one of these
SHADING_MODEL_SPECULAR_GLOSSINESS - standard specular glossiness shading
SHADING_MODEL_CLOTH              - cloth shading
SHADING_MODEL_SUBSURFACE         - subsurface scattering
SHADING_MODEL_UNLIT              - unlit

	// material
MATERIAL_HAS_SUBSURFACE_COLOR
MATERIAL_HAS_NORMAL	//vertex/fragment option
MATERIAL_HAS_CLEAR_COAT
MATERIAL_HAS_CLEAR_COAT_NORMAL		//vertex/fragment option
MATERIAL_HAS_ANISOTROPY			//vertex/fragment option
MATERIAL_HAS_DOUBLE_SIDED_CAPABILITY
MATERIAL_HAS_AMBIENT_OCCLUSION
MATERIAL_HAS_CLEAR_COAT_ROUGHNESS
MATERIAL_HAS_EMISSIVE

	//lighting
HAS_SHADOWING	//vertex/fragment
HAS_DIRECTIONAL_LIGHTING		//vertex/fragment
HAS_DYNAMIC_LIGHTING
HAS_SHADOW_MULTIPLIER	// used when SHADING_MODEL_UNLIT

GEOMETRIC_SPECULAR_AA
SPECULAR_AMBIENT_OCCLUSION	// 0 / 1
MULTI_BOUNCE_AMBIENT_OCCLUSION		// 0 / 1
CLEAR_COAT_IOR_CHANGE

BLEND_MODE_MASKED
BLEND_MODE_TRANSPARENT
BLEND_MODE_FADE

*/

uniform float4 u_frameUniforms[21];


#define u_frameUniforms_lightFromWorldMatrix mat4(u_frameUniforms[0],u_frameUniforms[1],u_frameUniforms[2],u_frameUniforms[3])	//directional light shadow
#define u_frameUniforms_cameraPosition  u_frameUniforms[4].xyz
#define u_frameUniforms_lightColorIntensity u_frameUniforms[5]	// xyz - directional light color, .w - light intensity premultiplied with exposure
#define u_frameUniforms_sun u_frameUniforms[6]	// area light: cos(radius), sin(radius), 1.0f / (cos(radius * haloSize) - cos(radius)), haloFalloff
#define u_frameUniforms_lightDirection u_frameUniforms[7].xyz // directional light direction
#define u_frameUniforms_fParamsX uint(u_frameUniforms[7].w) // froxelCoordScale X
#define u_frameUniforms_shadowBias u_frameUniforms[8].xyz // 0, normalBias * texelSizeWorldSpace, 0
#define u_frameUniforms_oneOverFroxelDimensionY uint(u_frameUniforms[8].w)  // 1.0 / FroxelDimY
#define u_frameUniforms_zParams u_frameUniforms[9]   // needed by froxel Z coord computation
#define u_frameUniforms_oneOverFroxelDimension u_frameUniforms[10].x   // 1.0 / FroxelDimX
#define u_frameUniforms_iblLuminance u_frameUniforms[10].y
#define u_frameUniforms_exposure u_frameUniforms[10].z
#define u_frameUniforms_ev100 u_frameUniforms[10].w
#define u_frameUniforms_iblSH(_index) u_frameUniforms[11+_index].xyz
#define u_frameUniforms_iblMaxMipLevel u_frameUniforms[20].xy
#define u_frameUniforms_fParams uint2(u_frameUniforms[20].xy)   // froxelCoordScale YZ


uniform float4 u_objectUniforms[6];
#define u_objectUniforms_worldFromModelNormalMatrix mat3(u_objectUniforms[0].xyz,u_objectUniforms[1].xyz,u_objectUniforms[2].xyz)
#define u_objectUniforms_morphWeights u_objectUniforms[3]
#define u_objectUniforms_skinningEnabled int(u_objectUniforms[4].x)
#define u_objectUniforms_morphingEnabled int(u_objectUniforms[4].y)
#define u_objectUniforms_padding0 u_objectUniforms[4].zw
#define u_objectUniforms_specularAntiAliasingVariance u_objectUniforms[5].x
#define u_objectUniforms_specularAntiAliasingThreshold u_objectUniforms[5].y
#define u_objectUniforms_maskThreshold u_objectUniforms[5].z
#define u_objectUniforms_doubleSided (0.0 != u_objectUniforms[5].w)

#if BGFX_SHADER_TYPE_VERTEX

#define CONFIG_MAX_BONE_COUNT (256)
mediump float4 u_bones[CONFIG_MAX_BONE_COUNT*4];

#endif

#if BGFX_SHADER_TYPE_FRAGMENT

#define CONFIG_MAX_LIGHT_COUNT (256)
uniform highp float4 u_lights[CONFIG_MAX_LIGHT_COUNT*4];

SAMPLER2DSHADOW(s_texShadowMap, 0);
ISAMPLER2D(s_texLightRecords, 1);
ISAMPLER2D(s_texLightFroxels, 2);
SAMPLER2D(s_texIblDFG, 3);
SAMPLERCUBE(s_texIblSpecular, 4);
SAMPLER2D(s_texSsao, 5);

static highp vec4 s_FragCoord;
#if defined(HAS_SHADOWING) && defined(HAS_DIRECTIONAL_LIGHTING)
static highp vec4 vertex_lightSpacePosition;
#endif

#endif

//------------------------------------------------------------------------------
// Common math
//------------------------------------------------------------------------------

/** @public-api */
#define PI                 3.14159265359
/** @public-api */
#define HALF_PI            1.570796327

#define MEDIUMP_FLT_MAX    65504.0
#define MEDIUMP_FLT_MIN    0.00006103515625

#ifdef TARGET_MOBILE
#define FLT_EPS            MEDIUMP_FLT_MIN
#define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)
#else
#define FLT_EPS            1e-5
#define saturateMediump(x) x
#endif

#define saturate(x)        clamp(x, 0.0, 1.0)

//------------------------------------------------------------------------------
// Scalar operations
//------------------------------------------------------------------------------

/**
 * Computes x^5 using only multiply operations.
 *
 * @public-api
 */
float pow5(float x) {
    float x2 = x * x;
    return x2 * x2 * x;
}

/**
 * Computes x^2 as a single multiplication.
 *
 * @public-api
 */
float sq(float x) {
    return x * x;
}

/**
 * Returns the maximum component of the specified vector.
 *
 * @public-api
 */
float max3(const vec3 v) {
    return max(v.x, max(v.y, v.z));
}

/**
 * Extracts the normal vector of the tangent frame encoded in the specified quaternion.
 */
void toTangentFrame(const highp vec4 q, out highp vec3 n) {
    n = vec3( 0.0,  0.0,  1.0) +
        vec3( 2.0, -2.0, -2.0) * q.x * q.zwx +
        vec3( 2.0,  2.0, -2.0) * q.y * q.wzy;
}

/**
 * Extracts the normal and tangent vectors of the tangent frame encoded in the
 * specified quaternion.
 */
void toTangentFrame(const highp vec4 q, out highp vec3 n, out highp vec3 t) {
    toTangentFrame(q, n);
    t = vec3( 1.0,  0.0,  0.0) +
        vec3(-2.0,  2.0, -2.0) * q.y * q.yxw +
        vec3(-2.0,  2.0,  2.0) * q.z * q.zwx;
}

#if BGFX_SHADER_TYPE_VERTEX
 static vec4 mesh_position;

#if defined(HAS_ATTRIBUTE_TANGENTS)
 static vec4 mesh_tangents;
#endif

#if defined(HAS_ATTRIBUTE_BONE_INDICES)
 static uvec4 mesh_bone_indices;
#endif

#if defined(HAS_ATTRIBUTE_BONE_WEIGHTS)
 static vec4 mesh_bone_weights;
#endif


#if defined(HAS_SKINNING_OR_MORPHING)
	static vec4 mesh_morph_position0;
	static vec4 mesh_morph_position1;
	static vec4 mesh_morph_position2;
	static vec4 mesh_morph_position3;
	static vec4 mesh_morph_tangents0;
	static vec4 mesh_morph_tangents1;
	static vec4 mesh_morph_tangents2;
	static vec4 mesh_morph_tangents3;
#endif
//------------------------------------------------------------------------------
// Uniforms access
//------------------------------------------------------------------------------

/** @public-api */
mat4 getViewFromWorldMatrix() {
    return u_view;
}

/** @public-api */
mat4 getWorldFromViewMatrix() {
    return u_invView;
}

/** @public-api */
mat4 getClipFromViewMatrix() {
    return u_proj;
}

/** @public-api */
mat4 getViewFromClipMatrix() {
    return u_invProj;
}

/** @public-api */
mat4 getClipFromWorldMatrix() {
    return u_viewProj;
}

/** @public-api */
mat4 getWorldFromClipMatrix() {
    return u_invViewProj;
}

/** @public-api */
vec3 getWorldCameraPosition() {
    return u_frameUniforms_cameraPosition;
}

/** @public-api */
float getExposure() {
    return u_frameUniforms_exposure;
}

/** @public-api */
float getEV100() {
    return u_frameUniforms_ev100;
}

//------------------------------------------------------------------------------
// Uniforms access
//------------------------------------------------------------------------------

mat4 getLightFromWorldMatrix() {
    return u_frameUniforms_lightFromWorldMatrix;
}

/** @public-api */
mat4 getWorldFromModelMatrix() {
    return u_model[0];
}

/** @public-api */
mat3 getWorldFromModelNormalMatrix() {
    return u_objectUniforms_worldFromModelNormalMatrix;
}

//------------------------------------------------------------------------------
// Attributes access
//------------------------------------------------------------------------------

#if defined(HAS_SKINNING_OR_MORPHING)
vec3 mulBoneNormal(vec3 n, uint i) {
    vec4 q  = u_bones[i + 0u];
    vec3 is = u_bones[i + 3u].xyz;

    // apply the inverse of the non-uniform scales
    n *= is;
    // apply the rigid transform (valid only for unit quaternions)
    n += 2.0 * cross(q.xyz, cross(q.xyz, n) + q.w * n);

    return n;
}

vec3 mulBoneVertex(vec3 v, uint i) {
    vec4 q = u_bones[i + 0u];
    vec3 t = u_bones[i + 1u].xyz;
    vec3 s = u_bones[i + 2u].xyz;

    // apply the non-uniform scales
    v *= s;
    // apply the rigid transform (valid only for unit quaternions)
    v += 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
    // apply the translation
    v += t;

    return v;
}

void skinNormal(inout vec3 n, const uvec4 ids, const vec4 weights) {
    n =   mulBoneNormal(n, ids.x * 4u) * weights.x
        + mulBoneNormal(n, ids.y * 4u) * weights.y
        + mulBoneNormal(n, ids.z * 4u) * weights.z
        + mulBoneNormal(n, ids.w * 4u) * weights.w;
}

void skinPosition(inout vec3 p, const uvec4 ids, const vec4 weights) {
    p =   mulBoneVertex(p, ids.x * 4u) * weights.x
        + mulBoneVertex(p, ids.y * 4u) * weights.y
        + mulBoneVertex(p, ids.z * 4u) * weights.z
        + mulBoneVertex(p, ids.w * 4u) * weights.w;
}
#endif

/** @public-api */
vec4 getPosition() {
    vec4 pos = mesh_position;

#if defined(HAS_SKINNING_OR_MORPHING)

    if (u_objectUniforms_morphingEnabled == 1) {
        pos += u_objectUniforms_morphWeights.x * mesh_morph_position0;
        pos += u_objectUniforms_morphWeights.y * mesh_morph_position1;
        pos += u_objectUniforms_morphWeights.z * mesh_morph_position2;
        pos += u_objectUniforms_morphWeights.w * mesh_morph_position3;
    }

    if (u_objectUniforms_skinningEnabled == 1) {
        skinPosition(pos.xyz, mesh_bone_indices, mesh_bone_weights);
    }

#endif

    return pos;
}

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Computes and returns the position in world space of the current vertex.
 * The world position computation depends on the current vertex domain. This
 * function optionally applies vertex skinning if needed.
 *
 * NOTE: the "transform" and "position" temporaries are necessary to work around
 * an issue with Adreno drivers (b/110851741).
 */
vec4 computeWorldPosition() {
#if defined(VERTEX_DOMAIN_OBJECT)
    mat4 transform = getWorldFromModelMatrix();
    vec3 position = getPosition().xyz;
    return mul(transform, vec4(position,1.0));
#elif defined(VERTEX_DOMAIN_WORLD)
    return vec4(getPosition().xyz, 1.0);
#elif defined(VERTEX_DOMAIN_VIEW)
    mat4 transform = getWorldFromViewMatrix();
    vec3 position = getPosition().xyz;
    return mul(transform, vec4(position,1.0));
#else
    mat4 transform = getWorldFromViewMatrix() * getViewFromClipMatrix();
    vec3 position = getPosition().xyz;
    return mul(transform, vec4(position,1.0));
#endif
}

//------------------------------------------------------------------------------
// Shadowing
//------------------------------------------------------------------------------

#if defined(HAS_SHADOWING) && defined(HAS_DIRECTIONAL_LIGHTING)
/**
 * Computes the light space position of the specified world space point.
 * The returned point may contain a bias to attempt to eliminate common
 * shadowing artifacts such as "acne". To achieve this, the world space
 * normal at the point must also be passed to this function.
 */
vec4 getLightSpacePosition(const vec3 p, const vec3 n) {
    vec3 l = u_frameUniforms_lightDirection;
    float NoL = saturate(dot(n, l));
    float sinTheta = sqrt(1.0 - NoL * NoL);
    vec3 offsetPosition = p + n * (sinTheta * u_frameUniforms_shadowBias.y);
    vec4 lightSpacePosition = mul(getLightFromWorldMatrix(),  vec4(offsetPosition, 1.0));
    return lightSpacePosition;
}
#endif

#endif //BGFX_SHADER_TYPE_VERTEX
#if BGFX_SHADER_TYPE_FRAGMENT
// Decide if we can skip lighting when dot(n, l) <= 0.0
#if defined(SHADING_MODEL_CLOTH)
#if !defined(MATERIAL_HAS_SUBSURFACE_COLOR)
    #define MATERIAL_CAN_SKIP_LIGHTING
#endif
#elif defined(SHADING_MODEL_SUBSURFACE)
    // Cannot skip lighting
#else
    #define MATERIAL_CAN_SKIP_LIGHTING
#endif

struct MaterialInputs {
    vec4  baseColor;
#if !defined(SHADING_MODEL_UNLIT)
#if !defined(SHADING_MODEL_SPECULAR_GLOSSINESS)
    float roughness;
#endif
#if !defined(SHADING_MODEL_CLOTH) && !defined(SHADING_MODEL_SPECULAR_GLOSSINESS)
    float metallic;
    float reflectance;
#endif
    float ambientOcclusion;
#endif
    vec4  emissive;

    float clearCoat;
    float clearCoatRoughness;

    float anisotropy;
    vec3  anisotropyDirection;

#if defined(SHADING_MODEL_SUBSURFACE)
    float thickness;
    float subsurfacePower;
    vec3  subsurfaceColor;
#endif

#if defined(SHADING_MODEL_CLOTH)
    vec3  sheenColor;
#if defined(MATERIAL_HAS_SUBSURFACE_COLOR)
    vec3  subsurfaceColor;
#endif
#endif

#if defined(SHADING_MODEL_SPECULAR_GLOSSINESS)
    vec3  specularColor;
    float glossiness;
#endif

#if defined(MATERIAL_HAS_NORMAL)
    vec3  normal;
#endif
#if defined(MATERIAL_HAS_CLEAR_COAT) && defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
    vec3  clearCoatNormal;
#endif

};

void initMaterial(out MaterialInputs material) {
   material = (MaterialInputs)0;
    material.baseColor = vec4_splat(1.0);
#if !defined(SHADING_MODEL_UNLIT)
#if !defined(SHADING_MODEL_SPECULAR_GLOSSINESS)
    material.roughness = 1.0;
#endif
#if !defined(SHADING_MODEL_CLOTH) && !defined(SHADING_MODEL_SPECULAR_GLOSSINESS)
    material.metallic = 0.0;
    material.reflectance = 0.5;
#endif
    material.ambientOcclusion = 1.0;
#endif
    material.emissive = vec4_splat(0.0);

#if defined(MATERIAL_HAS_CLEAR_COAT)
    material.clearCoat = 1.0;
    material.clearCoatRoughness = 0.0;
#endif

#if defined(MATERIAL_HAS_ANISOTROPY)
    material.anisotropy = 0.0;
    material.anisotropyDirection = vec3(1.0, 0.0, 0.0);
#endif

#if defined(SHADING_MODEL_SUBSURFACE)
    material.thickness = 0.5;
    material.subsurfacePower = 12.234;
    material.subsurfaceColor = vec3_splat(1.0);
#endif

#if defined(SHADING_MODEL_CLOTH)
    material.sheenColor = sqrt(material.baseColor.rgb);
#if defined(MATERIAL_HAS_SUBSURFACE_COLOR)
    material.subsurfaceColor = vec3_splat(0.0);
#endif
#endif

#if defined(SHADING_MODEL_SPECULAR_GLOSSINESS)
    material.glossiness = 0.0;
    material.specularColor = vec3_splat(0.0);
#endif

#if defined(MATERIAL_HAS_NORMAL)
    material.normal = vec3(0.0, 0.0, 1.0);
#endif
#if defined(MATERIAL_HAS_CLEAR_COAT) && defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
    material.clearCoatNormal = vec3(0.0, 0.0, 1.0);
#endif

}

// These variables should be in a struct but some GPU drivers ignore the
// precision qualifier on individual struct members
static highp mat3  shading_tangentToWorld;   // TBN matrix
static highp vec3  shading_position;         // position of the fragment in world space
      static vec3  shading_view;             // normalized vector from the fragment to the eye
      static vec3  shading_normal;           // normalized normal, in world space
      static vec3  shading_reflected;        // reflection of view about normal
      static float shading_NoV;              // dot(normal, view), always strictly >= MIN_N_DOT_V

#if defined(MATERIAL_HAS_CLEAR_COAT)
      static vec3  shading_clearCoatNormal;  // normalized clear coat layer normal, in world space
#endif

#if defined(BLEND_MODE_MASKED)
/** @public-api */
float getMaskThreshold() {
    return u_objectUniforms_maskThreshold;
}
#endif

/** @public-api */
highp mat3 getWorldTangentFrame() {
    return shading_tangentToWorld;
}

/** @public-api */
highp vec3 getWorldPosition() {
    return shading_position;
}

/** @public-api */
vec3 getWorldViewVector() {
    return shading_view;
}

/** @public-api */
vec3 getWorldNormalVector() {
    return shading_normal;
}

/** @public-api */
vec3 getWorldGeometricNormalVector() {
    return shading_tangentToWorld[2];
}

/** @public-api */
vec3 getWorldReflectedVector() {
    return shading_reflected;
}

/** @public-api */
float getNdotV() {
    return shading_NoV;
}

#if defined(HAS_SHADOWING) && defined(HAS_DIRECTIONAL_LIGHTING)
highp vec3 getLightSpacePosition() {
    return vertex_lightSpacePosition.xyz * (1.0 / vertex_lightSpacePosition.w);
}
#endif

#if defined(MATERIAL_HAS_DOUBLE_SIDED_CAPABILITY)
bool isDoubleSided() {
    return u_objectUniforms_doubleSided;
}
#endif

#if defined(TARGET_MOBILE)
    // min roughness such that (MIN_PERCEPTUAL_ROUGHNESS^4) > 0 in fp16 (i.e. 2^(-14/4), rounded up)
    #define MIN_PERCEPTUAL_ROUGHNESS 0.089
    #define MIN_ROUGHNESS            0.007921
#else
    #define MIN_PERCEPTUAL_ROUGHNESS 0.045
    #define MIN_ROUGHNESS            0.002025
#endif

#define MIN_N_DOT_V 1e-4

float clampNoV(float NoV) {
    // Neubelt and Pettineo 2013, "Crafting a Next-gen Material Pipeline for The Order: 1886"
    return max(dot(shading_normal, shading_view), MIN_N_DOT_V);
}

vec3 computeDiffuseColor(const vec4 baseColor, float metallic) {
    return baseColor.rgb * (1.0 - metallic);
}

vec3 computeF0(const vec4 baseColor, float metallic, float reflectance) {
    return baseColor.rgb * metallic + (reflectance * (1.0 - metallic));
}

float computeDielectricF0(float reflectance) {
    return 0.16 * reflectance * reflectance;
}

float computeMetallicFromSpecularColor(const vec3 specularColor) {
    return max3(specularColor);
}

float computeRoughnessFromGlossiness(float glossiness) {
    return 1.0 - glossiness;
}

float perceptualRoughnessToRoughness(float perceptualRoughness) {
    return perceptualRoughness * perceptualRoughness;
}

float roughnessToPerceptualRoughness(float roughness) {
    return sqrt(roughness);
}

float iorToF0(float transmittedIor, float incidentIor) {
    return sq((transmittedIor - incidentIor) / (transmittedIor + incidentIor));
}

float f0ToIor(float f0) {
    float r = sqrt(f0);
    return (1.0 + r) / (1.0 - r);
}

vec3 f0ClearCoatToSurface(const vec3 f0) {
    // Approximation of iorTof0(f0ToIor(f0), 1.5)
    // This assumes that the clear coat layer has an IOR of 1.5
#if defined(TARGET_MOBILE)
    return saturate(f0 * (f0 * 0.526868 + 0.529324) - 0.0482256);
#else
    return saturate(f0 * (f0 * (0.941892 - 0.263008 * f0) + 0.346479) - 0.0285998);
#endif
}



/**
 * Computes global shading parameters that the material might need to access
 * before lighting: N dot V, the reflected vector and the shading normal (before
 * applying the normal map). These parameters can be useful to material authors
 * to compute other material properties.
 *
 * This function must be invoked by the user's material code (guaranteed by
 * the material compiler) after setting a value for MaterialInputs.normal.
 */
void prepareMaterial(const MaterialInputs material) {
#if defined(HAS_ATTRIBUTE_TANGENTS)
#if defined(MATERIAL_HAS_NORMAL)
    shading_normal = normalize(mul(shading_tangentToWorld, material.normal));
#else
    shading_normal = getWorldGeometricNormalVector();
#endif
    shading_NoV = clampNoV(dot(shading_normal, shading_view));
    shading_reflected = reflect(-shading_view, shading_normal);

#if defined(MATERIAL_HAS_CLEAR_COAT)
#if defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
    shading_clearCoatNormal = normalize(shading_tangentToWorld * material.clearCoatNormal);
#else
    shading_clearCoatNormal = getWorldGeometricNormalVector();
#endif
#endif
#endif
}

struct Light {
    vec4 colorIntensity;  // rgb, pre-exposed intensity
    vec3 l;
    float attenuation;
    float NoL;
};

struct PixelParams {
    vec3  diffuseColor;
    float perceptualRoughness;
    vec3  f0;
    float roughness;
    vec3  dfg;
    vec3  energyCompensation;

#if defined(MATERIAL_HAS_CLEAR_COAT)
    float clearCoat;
    float clearCoatPerceptualRoughness;
    float clearCoatRoughness;
#endif

#if defined(MATERIAL_HAS_ANISOTROPY)
    vec3  anisotropicT;
    vec3  anisotropicB;
    float anisotropy;
#endif

#if defined(SHADING_MODEL_SUBSURFACE)
    float thickness;
    vec3  subsurfaceColor;
    float subsurfacePower;
#endif

#if defined(SHADING_MODEL_CLOTH) && defined(MATERIAL_HAS_SUBSURFACE_COLOR)
    vec3  subsurfaceColor;
#endif
};

float computeMicroShadowing(float NoL, float visibility) {
    // Chan 2018, "Material Advances in Call of Duty: WWII"
    float aperture = inversesqrt(1.0 - visibility);
    float microShadow = saturate(NoL * aperture);
    return microShadow * microShadow;
}

//------------------------------------------------------------------------------
// Common color operations
//------------------------------------------------------------------------------

/**
 * Computes the luminance of the specified linear RGB color using the
 * luminance coefficients from Rec. 709.
 *
 * @public-api
 */
float luminance(const vec3 linearColor) {
    return dot(linearColor, vec3(0.2126, 0.7152, 0.0722));
}

/**
 * Computes the pre-exposed intensity using the specified intensity and exposure.
 * This function exists to force highp precision on the two parameters
 */
float computePreExposedIntensity(const highp float intensity, const highp float exposure) {
    return intensity * exposure;
}

void unpremultiply(inout vec4 color) {
    color.rgb /= max(color.a, FLT_EPS);
}

//------------------------------------------------------------------------------
// Tone mapping operations
//------------------------------------------------------------------------------

/*
 * The input must be in the [0, 1] range.
 */
vec3 Inverse_Tonemap_Unreal(const vec3 x) {
    return (x * -0.155) / (x - 1.019);
}

/**
 * Applies the inverse of the tone mapping operator to the specified HDR or LDR
 * sRGB (non-linear) color and returns a linear sRGB color. The inverse tone mapping
 * operator may be an approximation of the real inverse operation.
 *
 * @public-api
 */
vec3 inverseTonemapSRGB(vec3 color) {
    // sRGB input
    color = clamp(color, 0.0, 1.0);
    return Inverse_Tonemap_Unreal(color);
}

/**
 * Applies the inverse of the tone mapping operator to the specified HDR or LDR
 * linear RGB color and returns a linear RGB color. The inverse tone mapping operator
 * may be an approximation of the real inverse operation.
 *
 * @public-api
 */
vec3 inverseTonemap(vec3 linearColor) {
    // Linear input
    linearColor = clamp(linearColor, 0.0, 1.0);
    return Inverse_Tonemap_Unreal(pow(linearColor, vec3_splat(1.0 / 2.2)));
}

//------------------------------------------------------------------------------
// Common texture operations
//------------------------------------------------------------------------------

/**
 * Decodes the specified RGBM value to linear HDR RGB.
 */
vec3 decodeRGBM(vec4 c) {
    c.rgb *= (c.a * 16.0);
    return c.rgb * c.rgb;
}

//------------------------------------------------------------------------------
// Common debug
//------------------------------------------------------------------------------

vec3 heatmap(float v) {
    vec3 r = v * 2.1 - vec3(1.8, 1.14, 0.3);
    return 1.0 - r * r;
}

//------------------------------------------------------------------------------
// Ambient occlusion helpers
//------------------------------------------------------------------------------

float evaluateSSAO() {
    // TODO: Don't use gl_FragCoord.xy, use the view bounds
    vec2 uv = s_FragCoord.xy * u_viewTexel.xy;
    return texture2DBias(s_texSsao, uv, 0.0).r;
}

/**
 * Computes a specular occlusion term from the ambient occlusion term.
 */
float computeSpecularAO(float NoV, float visibility, float roughness) {
#if SPECULAR_AMBIENT_OCCLUSION == 1
    return saturate(pow(NoV + visibility, exp2(-16.0 * roughness - 1.0)) - 1.0 + visibility);
#else
    return 1.0;
#endif
}

#if MULTI_BOUNCE_AMBIENT_OCCLUSION == 1
/**
 * Returns a color ambient occlusion based on a pre-computed visibility term.
 * The albedo term is meant to be the diffuse color or f0 for the diffuse and
 * specular terms respectively.
 */
vec3 gtaoMultiBounce(float visibility, const vec3 albedo) {
    // Jimenez et al. 2016, "Practical Realtime Strategies for Accurate Indirect Occlusion"
    vec3 a =  2.0404 * albedo - 0.3324;
    vec3 b = -4.7951 * albedo + 0.6417;
    vec3 c =  2.7552 * albedo + 0.6903;

    return max(vec3_splat(visibility), ((visibility * a + b) * visibility + c) * visibility);
}
#endif

void multiBounceAO(float visibility, const vec3 albedo, inout vec3 color) {
#if MULTI_BOUNCE_AMBIENT_OCCLUSION == 1
    color *= gtaoMultiBounce(visibility, albedo);
#endif
}

void multiBounceSpecularAO(float visibility, const vec3 albedo, inout vec3 color) {
#if MULTI_BOUNCE_AMBIENT_OCCLUSION == 1 && SPECULAR_AMBIENT_OCCLUSION == 1
    color *= gtaoMultiBounce(visibility, albedo);
#endif
}

float singleBounceAO(float visibility) {
#if MULTI_BOUNCE_AMBIENT_OCCLUSION == 1
    return 1.0;
#else
    return visibility;
#endif
}

//------------------------------------------------------------------------------
// BRDF configuration
//------------------------------------------------------------------------------

// Diffuse BRDFs
#define DIFFUSE_LAMBERT             0
#define DIFFUSE_BURLEY              1

// Specular BRDF
// Normal distribution functions
#define SPECULAR_D_GGX              0

// Anisotropic NDFs
#define SPECULAR_D_GGX_ANISOTROPIC  0

// Cloth NDFs
#define SPECULAR_D_CHARLIE          0

// Visibility functions
#define SPECULAR_V_SMITH_GGX        0
#define SPECULAR_V_SMITH_GGX_FAST   1
#define SPECULAR_V_GGX_ANISOTROPIC  2
#define SPECULAR_V_KELEMEN          3
#define SPECULAR_V_NEUBELT          4

// Fresnel functions
#define SPECULAR_F_SCHLICK          0

#define BRDF_DIFFUSE                DIFFUSE_LAMBERT

#if defined(TARGET_MOBILE)
#define BRDF_SPECULAR_D             SPECULAR_D_GGX
#define BRDF_SPECULAR_V             SPECULAR_V_SMITH_GGX_FAST
#define BRDF_SPECULAR_F             SPECULAR_F_SCHLICK
#else
#define BRDF_SPECULAR_D             SPECULAR_D_GGX
#define BRDF_SPECULAR_V             SPECULAR_V_SMITH_GGX
#define BRDF_SPECULAR_F             SPECULAR_F_SCHLICK
#endif

#define BRDF_CLEAR_COAT_D           SPECULAR_D_GGX
#define BRDF_CLEAR_COAT_V           SPECULAR_V_KELEMEN

#define BRDF_ANISOTROPIC_D          SPECULAR_D_GGX_ANISOTROPIC
#define BRDF_ANISOTROPIC_V          SPECULAR_V_GGX_ANISOTROPIC

#define BRDF_CLOTH_D                SPECULAR_D_CHARLIE
#define BRDF_CLOTH_V                SPECULAR_V_NEUBELT

//------------------------------------------------------------------------------
// Specular BRDF implementations
//------------------------------------------------------------------------------

float D_GGX(float roughness, float NoH, const vec3 h) {
    // Walter et al. 2007, "Microfacet Models for Refraction through Rough Surfaces"

    // In mediump, there are two problems computing 1.0 - NoH^2
    // 1) 1.0 - NoH^2 suffers floating point cancellation when NoH^2 is close to 1 (highlights)
    // 2) NoH doesn't have enough precision around 1.0
    // Both problem can be fixed by computing 1-NoH^2 in highp and providing NoH in highp as well

    // However, we can do better using Lagrange's identity:
    //      ||a x b||^2 = ||a||^2 ||b||^2 - (a . b)^2
    // since N and H are unit vectors: ||N x H||^2 = 1.0 - NoH^2
    // This computes 1.0 - NoH^2 directly (which is close to zero in the highlights and has
    // enough precision).
    // Overall this yields better performance, keeping all computations in mediump
#if defined(TARGET_MOBILE)
    vec3 NxH = cross(shading_normal, h);
    float oneMinusNoHSquared = dot(NxH, NxH);
#else
    float oneMinusNoHSquared = 1.0 - NoH * NoH;
#endif

    float a = NoH * roughness;
    float k = roughness / (oneMinusNoHSquared + a * a);
    float d = k * k * (1.0 / PI);
    return saturateMediump(d);
}

float D_GGX_Anisotropic(float at, float ab, float ToH, float BoH, float NoH) {
    // Burley 2012, "Physically-Based Shading at Disney"

    // The values at and ab are perceptualRoughness^2, a2 is therefore perceptualRoughness^4
    // The dot product below computes perceptualRoughness^8. We cannot fit in fp16 without clamping
    // the roughness to too high values so we perform the dot product and the division in fp32
    float a2 = at * ab;
    highp vec3 d = vec3(ab * ToH, at * BoH, a2 * NoH);
    highp float d2 = dot(d, d);
    float b2 = a2 / d2;
    return a2 * b2 * b2 * (1.0 / PI);
}

float D_Charlie(float roughness, float NoH) {
    // Estevez and Kulla 2017, "Production Friendly Microfacet Sheen BRDF"
    float invAlpha  = 1.0 / roughness;
    float cos2h = NoH * NoH;
    float sin2h = max(1.0 - cos2h, 0.0078125); // 2^(-14/2), so sin2h^2 > 0 in fp16
    return (2.0 + invAlpha) * pow(sin2h, invAlpha * 0.5) / (2.0 * PI);
}

float V_SmithGGXCorrelated(float roughness, float NoV, float NoL) {
    // Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
    float a2 = roughness * roughness;
    // TODO: lambdaV can be pre-computed for all the lights, it should be moved out of this function
    float lambdaV = NoL * sqrt((NoV - a2 * NoV) * NoV + a2);
    float lambdaL = NoV * sqrt((NoL - a2 * NoL) * NoL + a2);
    float v = 0.5 / (lambdaV + lambdaL);
    // a2=0 => v = 1 / 4*NoL*NoV   => min=1/4, max=+inf
    // a2=1 => v = 1 / 2*(NoL+NoV) => min=1/4, max=+inf
    // clamp to the maximum value representable in mediump
    return saturateMediump(v);
}

float V_SmithGGXCorrelated_Fast(float roughness, float NoV, float NoL) {
    // Hammon 2017, "PBR Diffuse Lighting for GGX+Smith Microsurfaces"
    float v = 0.5 / mix(2.0 * NoL * NoV, NoL + NoV, roughness);
    return saturateMediump(v);
}

float V_SmithGGXCorrelated_Anisotropic(float at, float ab, float ToV, float BoV,
        float ToL, float BoL, float NoV, float NoL) {
    // Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
    // TODO: lambdaV can be pre-computed for all the lights, it should be moved out of this function
    float lambdaV = NoL * length(vec3(at * ToV, ab * BoV, NoV));
    float lambdaL = NoV * length(vec3(at * ToL, ab * BoL, NoL));
    float v = 0.5 / (lambdaV + lambdaL);
    return saturateMediump(v);
}

float V_Kelemen(float LoH) {
    // Kelemen 2001, "A Microfacet Based Coupled Specular-Matte BRDF Model with Importance Sampling"
    return saturateMediump(0.25 / (LoH * LoH));
}

float V_Neubelt(float NoV, float NoL) {
    // Neubelt and Pettineo 2013, "Crafting a Next-gen Material Pipeline for The Order: 1886"
    return saturateMediump(1.0 / (4.0 * (NoL + NoV - NoL * NoV)));
}

vec3 F_Schlick(const vec3 f0, float f90, float VoH) {
    // Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"
    return f0 + (f90 - f0) * pow5(1.0 - VoH);
}

vec3 F_Schlick(const vec3 f0, float VoH) {
    float f = pow(1.0 - VoH, 5.0);
    return f + f0 * (1.0 - f);
}

float F_Schlick(float f0, float f90, float VoH) {
    return f0 + (f90 - f0) * pow5(1.0 - VoH);
}

//------------------------------------------------------------------------------
// Specular BRDF dispatch
//------------------------------------------------------------------------------

float distribution(float roughness, float NoH, const vec3 h) {
#if BRDF_SPECULAR_D == SPECULAR_D_GGX
    return D_GGX(roughness, NoH, h);
#endif
}

float visibility(float roughness, float NoV, float NoL) {
#if BRDF_SPECULAR_V == SPECULAR_V_SMITH_GGX
    return V_SmithGGXCorrelated(roughness, NoV, NoL);
#elif BRDF_SPECULAR_V == SPECULAR_V_SMITH_GGX_FAST
    return V_SmithGGXCorrelated_Fast(roughness, NoV, NoL);
#endif
}

vec3 fresnel(const vec3 f0, float LoH) {
#if BRDF_SPECULAR_F == SPECULAR_F_SCHLICK
#if defined(TARGET_MOBILE)
    return F_Schlick(f0, LoH); // f90 = 1.0
#else
    float f90 = saturate(dot(f0, vec3_splat(50.0 * 0.33)));
    return F_Schlick(f0, f90, LoH);
#endif
#endif
}

float distributionAnisotropic(float at, float ab, float ToH, float BoH, float NoH) {
#if BRDF_ANISOTROPIC_D == SPECULAR_D_GGX_ANISOTROPIC
    return D_GGX_Anisotropic(at, ab, ToH, BoH, NoH);
#endif
}

float visibilityAnisotropic(float roughness, float at, float ab,
        float ToV, float BoV, float ToL, float BoL, float NoV, float NoL) {
#if BRDF_ANISOTROPIC_V == SPECULAR_V_SMITH_GGX
    return V_SmithGGXCorrelated(roughness, NoV, NoL);
#elif BRDF_ANISOTROPIC_V == SPECULAR_V_GGX_ANISOTROPIC
    return V_SmithGGXCorrelated_Anisotropic(at, ab, ToV, BoV, ToL, BoL, NoV, NoL);
#endif
}

float distributionClearCoat(float roughness, float NoH, const vec3 h) {
#if BRDF_CLEAR_COAT_D == SPECULAR_D_GGX
    return D_GGX(roughness, NoH, h);
#endif
}

float visibilityClearCoat(float LoH) {
#if BRDF_CLEAR_COAT_V == SPECULAR_V_KELEMEN
    return V_Kelemen(LoH);
#endif
}

float distributionCloth(float roughness, float NoH) {
#if BRDF_CLOTH_D == SPECULAR_D_CHARLIE
    return D_Charlie(roughness, NoH);
#endif
}

float visibilityCloth(float NoV, float NoL) {
#if BRDF_CLOTH_V == SPECULAR_V_NEUBELT
    return V_Neubelt(NoV, NoL);
#endif
}

//------------------------------------------------------------------------------
// Diffuse BRDF implementations
//------------------------------------------------------------------------------

float Fd_Lambert() {
    return 1.0 / PI;
}

float Fd_Burley(float roughness, float NoV, float NoL, float LoH) {
    // Burley 2012, "Physically-Based Shading at Disney"
    float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    float lightScatter = F_Schlick(1.0, f90, NoL);
    float viewScatter  = F_Schlick(1.0, f90, NoV);
    return lightScatter * viewScatter * (1.0 / PI);
}

// Energy conserving wrap diffuse term, does *not* include the divide by pi
float Fd_Wrap(float NoL, float w) {
    return saturate((NoL + w) / sq(1.0 + w));
}

//------------------------------------------------------------------------------
// Diffuse BRDF dispatch
//------------------------------------------------------------------------------

float diffuse(float roughness, float NoV, float NoL, float LoH) {
#if BRDF_DIFFUSE == DIFFUSE_LAMBERT
    return Fd_Lambert();
#elif BRDF_DIFFUSE == DIFFUSE_BURLEY
    return Fd_Burley(roughness, NoV, NoL, LoH);
#endif
}

#ifdef SHADING_MODEL_CLOTH
/**
 * Evaluates lit materials with the cloth shading model. Similar to the standard
 * model, the cloth shading model is based on a Cook-Torrance microfacet model.
 * Its distribution and visibility terms are however very different to take into
 * account the softer apperance of many types of cloth. Some highly reflecting
 * fabrics like satin or leather should use the standard model instead.
 *
 * This shading model optionally models subsurface scattering events. The
 * computation of these events is not physically based but can add necessary
 * details to a material.
 */
vec3 surfaceShading(const PixelParams pixel, const Light light, float occlusion) {
    vec3 h = normalize(shading_view + light.l);
    float NoL = light.NoL;
    float NoH = saturate(dot(shading_normal, h));
    float LoH = saturate(dot(light.l, h));

    // specular BRDF
    float D = distributionCloth(pixel.roughness, NoH);
    float V = visibilityCloth(shading_NoV, NoL);
    vec3  F = pixel.f0;
    // Ignore pixel.energyCompensation since we use a different BRDF here
    vec3 Fr = (D * V) * F;

    // diffuse BRDF
    float diffuseColor = diffuse(pixel.roughness, shading_NoV, NoL, LoH);
#if defined(MATERIAL_HAS_SUBSURFACE_COLOR)
    // Energy conservative wrap diffuse to simulate subsurface scattering
    diffuseColor *= Fd_Wrap(dot(shading_normal, light.l), 0.5);
#endif

    // We do not multiply the diffuse term by the Fresnel term as discussed in
    // Neubelt and Pettineo 2013, "Crafting a Next-gen Material Pipeline for The Order: 1886"
    // The effect is fairly subtle and not deemed worth the cost for mobile
    vec3 Fd = diffuseColor * pixel.diffuseColor;

#if defined(MATERIAL_HAS_SUBSURFACE_COLOR)
    // Cheap subsurface scatter
    Fd *= saturate(pixel.subsurfaceColor + NoL);
    // We need to apply NoL separately to the specular lobe since we already took
    // it into account in the diffuse lobe
    vec3 color = Fd + Fr * NoL;
    color *= light.colorIntensity.rgb * (light.colorIntensity.w * light.attenuation * occlusion);
#else
    vec3 color = Fd + Fr;
    color *= light.colorIntensity.rgb * (light.colorIntensity.w * light.attenuation * NoL * occlusion);
#endif

    return color;
}

#elif defined(SHADING_MODEL_SUBSURFACE)
/**
 * Evalutes lit materials with the subsurface shading model. This model is a
 * combination of a BRDF (the same used in shading_model_standard.fs, refer to that
 * file for more information) and of an approximated BTDF to simulate subsurface
 * scattering. The BTDF itself is not physically based and does not represent a
 * correct interpretation of transmission events.
 */
vec3 surfaceShading(const PixelParams pixel, const Light light, float occlusion) {
    vec3 h = normalize(shading_view + light.l);

    float NoL = light.NoL;
    float NoH = saturate(dot(shading_normal, h));
    float LoH = saturate(dot(light.l, h));

    vec3 Fr = vec3_splat(0.0);
    if (NoL > 0.0) {
        // specular BRDF
        float D = distribution(pixel.roughness, NoH, h);
        float V = visibility(pixel.roughness, shading_NoV, NoL);
        vec3  F = fresnel(pixel.f0, LoH);
        Fr = (D * V) * F * pixel.energyCompensation;
    }

    // diffuse BRDF
    vec3 Fd = pixel.diffuseColor * diffuse(pixel.roughness, shading_NoV, NoL, LoH);

    // NoL does not apply to transmitted light
    vec3 color = (Fd + Fr) * (NoL * occlusion);

    // subsurface scattering
    // Use a spherical gaussian approximation of pow() for forwardScattering
    // We could include distortion by adding shading_normal * distortion to light.l
    float scatterVoH = saturate(dot(shading_view, -light.l));
    float forwardScatter = exp2(scatterVoH * pixel.subsurfacePower - pixel.subsurfacePower);
    float backScatter = saturate(NoL * pixel.thickness + (1.0 - pixel.thickness)) * 0.5;
    float subsurface = mix(backScatter, 1.0, forwardScatter) * (1.0 - pixel.thickness);
    color += pixel.subsurfaceColor * (subsurface * Fd_Lambert());

    // TODO: apply occlusion to the transmitted light
    return (color * light.colorIntensity.rgb) * (light.colorIntensity.w * light.attenuation);
}

#elif !defined(SHADING_MODEL_UNLIT)
#if defined(MATERIAL_HAS_CLEAR_COAT)
float clearCoatLobe(const PixelParams pixel, const vec3 h, float NoH, float LoH, out float Fcc) {

#if defined(MATERIAL_HAS_NORMAL) || defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
    // If the material has a normal map, we want to use the geometric normal
    // instead to avoid applying the normal map details to the clear coat layer
    float clearCoatNoH = saturate(dot(shading_clearCoatNormal, h));
#else
    float clearCoatNoH = NoH;
#endif

    // clear coat specular lobe
    float D = distributionClearCoat(pixel.clearCoatRoughness, clearCoatNoH, h);
    float V = visibilityClearCoat(LoH);
    float F = F_Schlick(0.04, 1.0, LoH) * pixel.clearCoat; // fix IOR to 1.5

    Fcc = F;
    return D * V * F;
}
#endif

#if defined(MATERIAL_HAS_ANISOTROPY)
vec3 anisotropicLobe(const PixelParams pixel, const Light light, const vec3 h,
        float NoV, float NoL, float NoH, float LoH) {

    vec3 l = light.l;
    vec3 t = pixel.anisotropicT;
    vec3 b = pixel.anisotropicB;
    vec3 v = shading_view;

    float ToV = dot(t, v);
    float BoV = dot(b, v);
    float ToL = dot(t, l);
    float BoL = dot(b, l);
    float ToH = dot(t, h);
    float BoH = dot(b, h);

    // Anisotropic parameters: at and ab are the roughness along the tangent and bitangent
    // to simplify materials, we derive them from a single roughness parameter
    // Kulla 2017, "Revisiting Physically Based Shading at Imageworks"
    float at = max(pixel.roughness * (1.0 + pixel.anisotropy), MIN_ROUGHNESS);
    float ab = max(pixel.roughness * (1.0 - pixel.anisotropy), MIN_ROUGHNESS);

    // specular anisotropic BRDF
    float D = distributionAnisotropic(at, ab, ToH, BoH, NoH);
    float V = visibilityAnisotropic(pixel.roughness, at, ab, ToV, BoV, ToL, BoL, NoV, NoL);
    vec3  F = fresnel(pixel.f0, LoH);

    return (D * V) * F;
}
#endif

vec3 isotropicLobe(const PixelParams pixel, const Light light, const vec3 h,
        float NoV, float NoL, float NoH, float LoH) {

    float D = distribution(pixel.roughness, NoH, h);
    float V = visibility(pixel.roughness, NoV, NoL);
    vec3  F = fresnel(pixel.f0, LoH);

    return (D * V) * F;
}

vec3 specularLobe(const PixelParams pixel, const Light light, const vec3 h,
        float NoV, float NoL, float NoH, float LoH) {
#if defined(MATERIAL_HAS_ANISOTROPY)
    return anisotropicLobe(pixel, light, h, NoV, NoL, NoH, LoH);
#else
    return isotropicLobe(pixel, light, h, NoV, NoL, NoH, LoH);
#endif
}

vec3 diffuseLobe(const PixelParams pixel, float NoV, float NoL, float LoH) {
    return pixel.diffuseColor * diffuse(pixel.roughness, NoV, NoL, LoH);
}

/**
 * Evaluates lit materials with the standard shading model. This model comprises
 * of 2 BRDFs: an optional clear coat BRDF, and a regular surface BRDF.
 *
 * Surface BRDF
 * The surface BRDF uses a diffuse lobe and a specular lobe to render both
 * dielectrics and conductors. The specular lobe is based on the Cook-Torrance
 * micro-facet model (see brdf.fs for more details). In addition, the specular
 * can be either isotropic or anisotropic.
 *
 * Clear coat BRDF
 * The clear coat BRDF simulates a transparent, absorbing dielectric layer on
 * top of the surface. Its IOR is set to 1.5 (polyutherane) to simplify
 * our computations. This BRDF only contains a specular lobe and while based
 * on the Cook-Torrance microfacet model, it uses cheaper terms than the surface
 * BRDF's specular lobe (see brdf.fs).
 */
vec3 surfaceShading(const PixelParams pixel, const Light light, float occlusion) {
    vec3 h = normalize(shading_view + light.l);

    float NoV = shading_NoV;
    float NoL = saturate(light.NoL);
    float NoH = saturate(dot(shading_normal, h));
    float LoH = saturate(dot(light.l, h));

    vec3 Fr = specularLobe(pixel, light, h, NoV, NoL, NoH, LoH);
    vec3 Fd = diffuseLobe(pixel, NoV, NoL, LoH);

    // TODO: attenuate the diffuse lobe to avoid energy gain

#if defined(MATERIAL_HAS_CLEAR_COAT)
    float Fcc;
    float clearCoat = clearCoatLobe(pixel, h, NoH, LoH, Fcc);
    // Energy compensation and absorption; the clear coat Fresnel term is
    // squared to take into account both entering through and exiting through
    // the clear coat layer
    float attenuation = 1.0 - Fcc;

#if defined(MATERIAL_HAS_NORMAL) || defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
    vec3 color = (Fd + Fr * pixel.energyCompensation) * attenuation * NoL;

    // If the material has a normal map, we want to use the geometric normal
    // instead to avoid applying the normal map details to the clear coat layer
    float clearCoatNoL = saturate(dot(shading_clearCoatNormal, light.l));
    color += clearCoat * clearCoatNoL;

    // Early exit to avoid the extra multiplication by NoL
    return (color * light.colorIntensity.rgb) *
            (light.colorIntensity.w * light.attenuation * occlusion);
#else
    vec3 color = (Fd + Fr * pixel.energyCompensation) * attenuation + clearCoat;
#endif
#else
    // The energy compensation term is used to counteract the darkening effect
    // at high roughness
    vec3 color = Fd + Fr * pixel.energyCompensation;
#endif

    return (color * light.colorIntensity.rgb) *
            (light.colorIntensity.w * light.attenuation * NoL * occlusion);
}

#endif
#ifdef SHADING_MODEL_UNLIT
/**
 * Evaluates unlit materials. In this lighting model, only the base color and
 * emissive properties are taken into account:
 *
 * finalColor = baseColor + emissive
 *
 * The emissive factor is only applied if the fragment passes the optional
 * alpha test.
 *
 * When the shadowMultiplier property is enabled on the material, the final
 * color is multiplied by the inverse light visibility to apply a shadow.
 * This is mostly useful in AR to cast shadows on unlit transparent shadow
 * receiving planes.
 */
vec4 evaluateMaterial(const MaterialInputs material) {
    vec4 color = material.baseColor;

#if defined(BLEND_MODE_MASKED)
    if (color.a < getMaskThreshold()) {
        discard;
    }
#endif

#if defined(MATERIAL_HAS_EMISSIVE)
    color.rgb += material.emissive.rgb;
#endif

#if defined(HAS_DIRECTIONAL_LIGHTING)
#if defined(HAS_SHADOWING)
    color *= 1.0 - shadow2D(s_texShadowMap, getLightSpacePosition());
#else
    color = vec4_splat(0.0);
#endif
#elif defined(HAS_SHADOW_MULTIPLIER)
    color = vec4_splat(0.0);
#endif

    return color;
}

#else
//------------------------------------------------------------------------------
// Image based lighting configuration
//------------------------------------------------------------------------------

// Number of spherical harmonics bands (1, 2 or 3)
#if defined(TARGET_MOBILE)
#define SPHERICAL_HARMONICS_BANDS           2
#else
#define SPHERICAL_HARMONICS_BANDS           3
#endif

// IBL integration algorithm
#define IBL_INTEGRATION_PREFILTERED_CUBEMAP         0
#define IBL_INTEGRATION_IMPORTANCE_SAMPLING         1

#define IBL_INTEGRATION                             IBL_INTEGRATION_PREFILTERED_CUBEMAP

#define IBL_INTEGRATION_IMPORTANCE_SAMPLING_COUNT   64

//------------------------------------------------------------------------------
// IBL utilities
//------------------------------------------------------------------------------

vec3 decodeDataForIBL(const vec4 data) {
    return data.rgb;
}

//------------------------------------------------------------------------------
// IBL prefiltered DFG term implementations
//------------------------------------------------------------------------------

vec3 PrefilteredDFG_LUT(float lod, float NoV) {
    // coord = sqrt(linear_roughness), which is the mapping used by cmgen.
    return texture2DLod(s_texIblDFG, vec2(NoV, lod), 0.0).rgb;
}

//------------------------------------------------------------------------------
// IBL environment BRDF dispatch
//------------------------------------------------------------------------------

vec3 prefilteredDFG(float perceptualRoughness, float NoV) {
    // PrefilteredDFG_LUT() takes a LOD, which is sqrt(roughness) = perceptualRoughness
    return PrefilteredDFG_LUT(perceptualRoughness, NoV);
}

//------------------------------------------------------------------------------
// IBL irradiance implementations
//------------------------------------------------------------------------------

vec3 Irradiance_SphericalHarmonics(const vec3 n) {
    return max(
          u_frameUniforms_iblSH(0)
#if SPHERICAL_HARMONICS_BANDS >= 2
        + u_frameUniforms_iblSH(1) * (n.y)
        + u_frameUniforms_iblSH(2) * (n.z)
        + u_frameUniforms_iblSH(3) * (n.x)
#endif
#if SPHERICAL_HARMONICS_BANDS >= 3
        + u_frameUniforms_iblSH(4) * (n.y * n.x)
        + u_frameUniforms_iblSH(5) * (n.y * n.z)
        + u_frameUniforms_iblSH(6) * (3.0 * n.z * n.z - 1.0)
        + u_frameUniforms_iblSH(7) * (n.z * n.x)
        + u_frameUniforms_iblSH(8) * (n.x * n.x - n.y * n.y)
#endif
        , 0.0);
}

//------------------------------------------------------------------------------
// IBL irradiance dispatch
//------------------------------------------------------------------------------

vec3 diffuseIrradiance(const vec3 n) {
    return Irradiance_SphericalHarmonics(n);
}

//------------------------------------------------------------------------------
// IBL specular
//------------------------------------------------------------------------------

vec3 prefilteredRadiance(const vec3 r, float perceptualRoughness) {
    // lod = lod_count * sqrt(roughness), which is the mapping used by cmgen
    // where roughness = perceptualRoughness^2
    // using all the mip levels requires seamless cubemap sampling
    float lod = u_frameUniforms_iblMaxMipLevel.x * perceptualRoughness;
    return decodeDataForIBL(textureCubeLod(s_texIblSpecular, r, lod));
}

vec3 prefilteredRadiance(const vec3 r, float roughness, float offset) {
    float lod = u_frameUniforms_iblMaxMipLevel.x * roughness;
    return decodeDataForIBL(textureCubeLod(s_texIblSpecular, r, lod + offset));
}

vec3 getSpecularDominantDirection(const vec3 n, const vec3 r, float roughness) {
    return mix(r, n, roughness * roughness);
}

vec3 specularDFG(const PixelParams pixel) {
#if defined(SHADING_MODEL_CLOTH)
    return pixel.f0 * pixel.dfg.z;
#else
    return mix(pixel.dfg.xxx, pixel.dfg.yyy, pixel.f0);
#endif
}

/**
 * Returns the reflected vector at the current shading point. The reflected vector
 * return by this function might be different from shading_reflected:
 * - For anisotropic material, we bend the reflection vector to simulate
 *   anisotropic indirect lighting
 * - The reflected vector may be modified to point towards the dominant specular
 *   direction to match reference renderings when the roughness increases
 */

vec3 getReflectedVector(const PixelParams pixel, const vec3 v, const vec3 n) {
#if defined(MATERIAL_HAS_ANISOTROPY)
    vec3  anisotropyDirection = pixel.anisotropy >= 0.0 ? pixel.anisotropicB : pixel.anisotropicT;
    vec3  anisotropicTangent  = cross(anisotropyDirection, v);
    vec3  anisotropicNormal   = cross(anisotropicTangent, anisotropyDirection);
    float bendFactor          = abs(pixel.anisotropy) * saturate(5.0 * pixel.perceptualRoughness);
    vec3  bentNormal          = normalize(mix(n, anisotropicNormal, bendFactor));

    vec3 r = reflect(-v, bentNormal);
#else
    vec3 r = reflect(-v, n);
#endif
    return r;
}

vec3 getReflectedVector(const PixelParams pixel, const vec3 n) {
#if defined(MATERIAL_HAS_ANISOTROPY)
    vec3 r = getReflectedVector(pixel, shading_view, n);
#else
    vec3 r = shading_reflected;
#endif
    return getSpecularDominantDirection(n, r, pixel.roughness);
}

//------------------------------------------------------------------------------
// Prefiltered importance sampling
//------------------------------------------------------------------------------

#if IBL_INTEGRATION == IBL_INTEGRATION_IMPORTANCE_SAMPLING
vec2 hammersley(uint index) {
    // Compute Hammersley sequence
    // TODO: these should come from uniforms
    // TODO: we should do this with logical bit operations
    const uint numSamples = uint(IBL_INTEGRATION_IMPORTANCE_SAMPLING_COUNT);
    const uint numSampleBits = uint(log2(float(numSamples)));
    const float invNumSamples = 1.0 / float(numSamples);
    uint i = uint(index);
    uint t = i;
    uint bits = 0u;
    for (uint j = 0u; j < numSampleBits; j++) {
        bits = bits * 2u + (t - (2u * (t / 2u)));
        t /= 2u;
    }
    return vec2(float(i), float(bits)) * invNumSamples;
}

vec3 importanceSamplingNdfDggx(vec2 u, float roughness) {
    // Importance sampling D_GGX
    float a2 = roughness * roughness;
    float phi = 2.0 * PI * u.x;
    float cosTheta2 = (1.0 - u.y) / (1.0 + (a2 - 1.0) * u.y);
    float cosTheta = sqrt(cosTheta2);
    float sinTheta = sqrt(1.0 - cosTheta2);
    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

vec3 importanceSamplingVNdfDggx(vec2 u, float roughness, vec3 v) {
    // See: "A Simpler and Exact Sampling Routine for the GGX Distribution of Visible Normals", Eric Heitz
    float alpha = roughness;

    // stretch view
    v = normalize(vec3(alpha * v.x, alpha * v.y, v.z));

    // orthonormal basis
    vec3 up = abs(v.z) < 0.9999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 t = normalize(cross(up, v));
    vec3 b = cross(t, v);

    // sample point with polar coordinates (r, phi)
    float a = 1.0 / (1.0 + v.z);
    float r = sqrt(u.x);
    float phi = (u.y < a) ? u.y / a * PI : PI + (u.y - a) / (1.0 - a) * PI;
    float p1 = r * cos(phi);
    float p2 = r * sin(phi) * ((u.y < a) ? 1.0 : v.z);

    // compute normal
    vec3 h = p1 * t + p2 * b + sqrt(max(0.0, 1.0 - p1*p1 - p2*p2)) * v;

    // unstretch
    h = normalize(vec3(alpha * h.x, alpha * h.y, max(0.0, h.z)));
    return h;
}

float prefilteredImportanceSampling(float ipdf, vec2 iblMaxMipLevel) {
    // See: "Real-time Shading with Filtered Importance Sampling", Jaroslav Krivanek
    // Prefiltering doesn't work with anisotropy
    const float numSamples = float(IBL_INTEGRATION_IMPORTANCE_SAMPLING_COUNT);
    const float invNumSamples = 1.0 / float(numSamples);
    const float dim = iblMaxMipLevel.y;
    const float omegaP = (4.0 * PI) / (6.0 * dim * dim);
    const float invOmegaP = 1.0 / omegaP;
    const float K = 4.0;
    float omegaS = invNumSamples * ipdf;
    float mipLevel = clamp(log2(K * omegaS * invOmegaP) * 0.5, 0.0, iblMaxMipLevel.x);
    return mipLevel;
}

vec3 isEvaluateIBL(const PixelParams pixel, vec3 n, vec3 v, float NoV) {
    // TODO: for a true anisotropic BRDF, we need a real tangent space
    vec3 up = abs(n.z) < 0.9999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);

    mat3 tangentToWorld;
    tangentToWorld[0] = normalize(cross(up, n));
    tangentToWorld[1] = cross(n, tangentToWorld[0]);
    tangentToWorld[2] = n;

    float roughness = pixel.roughness;
    float a2 = roughness * roughness;

    vec2 iblMaxMipLevel = u_frameUniforms_iblMaxMipLevel;
    const uint numSamples = uint(IBL_INTEGRATION_IMPORTANCE_SAMPLING_COUNT);
    const float invNumSamples = 1.0 / float(numSamples);

    vec3 indirectSpecular = vec3_splat(0.0);
    for (uint i = 0u; i < numSamples; i++) {
        vec2 u = hammersley(i);
        vec3 h = tangentToWorld * importanceSamplingNdfDggx(u, roughness);

        // Since anisotropy doesn't work with prefiltering, we use the same "faux" anisotropy
        // we do when we use the prefiltered cubemap
        vec3 l = getReflectedVector(pixel, v, h);

        // Compute this sample's contribution to the brdf
        float NoL = dot(n, l);
        if (NoL > 0.0) {
            float NoH = dot(n, h);
            float LoH = max(dot(l, h), 0.0);

            // PDF inverse (we must use D_GGX() here, which is used to generate samples)
            float ipdf = (4.0 * LoH) / (D_GGX(roughness, NoH, h) * NoH);

            float mipLevel = prefilteredImportanceSampling(ipdf, iblMaxMipLevel);

            // we use texture() instead of textureLod() to take advantage of mipmapping
            vec3 L = decodeDataForIBL(textureCubeBias(s_texIblSpecular, l, mipLevel));

            float D = distribution(roughness, NoH, h);
            float V = visibility(roughness, NoV, NoL);
            vec3  F = fresnel(pixel.f0, LoH);
            vec3 Fr = F * (D * V * NoL * ipdf * invNumSamples);

            indirectSpecular += (Fr * L);
        }
    }

    return indirectSpecular;
}

void isEvaluateClearCoatIBL(const PixelParams pixel, float specularAO, inout vec3 Fd, inout vec3 Fr) {
#if defined(MATERIAL_HAS_CLEAR_COAT)
#if defined(MATERIAL_HAS_NORMAL) || defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
    // We want to use the geometric normal for the clear coat layer
    float clearCoatNoV = clampNoV(dot(shading_clearCoatNormal, shading_view));
    vec3 clearCoatNormal = shading_clearCoatNormal;
#else
    float clearCoatNoV = shading_NoV;
    vec3 clearCoatNormal = shading_normal;
#endif
    // The clear coat layer assumes an IOR of 1.5 (4% reflectance)
    float Fc = F_Schlick(0.04, 1.0, clearCoatNoV) * pixel.clearCoat;
    float attenuation = 1.0 - Fc;
    Fd *= attenuation;
    Fr *= attenuation;

    PixelParams p;
    p.perceptualRoughness = pixel.clearCoatPerceptualRoughness;
    p.f0 = vec3_splat(0.04);
    p.roughness = perceptualRoughnessToRoughness(p.perceptualRoughness);
    p.anisotropy = 0.0;

    vec3 clearCoatLobe = isEvaluateIBL(p, clearCoatNormal, shading_view, clearCoatNoV);
    Fr += clearCoatLobe * (specularAO * pixel.clearCoat);
#endif
}
#endif

//------------------------------------------------------------------------------
// IBL evaluation
//------------------------------------------------------------------------------

void evaluateClothIndirectDiffuseBRDF(const PixelParams pixel, inout float diffuse) {
#if defined(SHADING_MODEL_CLOTH)
#if defined(MATERIAL_HAS_SUBSURFACE_COLOR)
    // Simulate subsurface scattering with a wrap diffuse term
    diffuse *= Fd_Wrap(shading_NoV, 0.5);
#endif
#endif
}

void evaluateClearCoatIBL(const PixelParams pixel, float specularAO, inout vec3 Fd, inout vec3 Fr) {
#if IBL_INTEGRATION == IBL_INTEGRATION_IMPORTANCE_SAMPLING
    isEvaluateClearCoatIBL(pixel, specularAO, Fd, Fr);
    return;
#endif

#if defined(MATERIAL_HAS_CLEAR_COAT)
#if defined(MATERIAL_HAS_NORMAL) || defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
    // We want to use the geometric normal for the clear coat layer
    float clearCoatNoV = clampNoV(dot(shading_clearCoatNormal, shading_view));
    vec3 clearCoatR = reflect(-shading_view, shading_clearCoatNormal);
#else
    float clearCoatNoV = shading_NoV;
    vec3 clearCoatR = shading_reflected;
#endif
    // The clear coat layer assumes an IOR of 1.5 (4% reflectance)
    float Fc = F_Schlick(0.04, 1.0, clearCoatNoV) * pixel.clearCoat;
    float attenuation = 1.0 - Fc;
    Fd *= attenuation;
    Fr *= attenuation;
    Fr += prefilteredRadiance(clearCoatR, pixel.clearCoatPerceptualRoughness) * (specularAO * Fc);
#endif
}

void evaluateSubsurfaceIBL(const PixelParams pixel, const vec3 diffuseIrradianceColor,
        inout vec3 Fd, inout vec3 Fr) {
#if defined(SHADING_MODEL_SUBSURFACE)
    vec3 viewIndependent = diffuseIrradianceColor;
    vec3 viewDependent = prefilteredRadiance(-shading_view, pixel.roughness, 1.0 + pixel.thickness);
    float attenuation = (1.0 - pixel.thickness) / (2.0 * PI);
    Fd += pixel.subsurfaceColor * (viewIndependent + viewDependent) * attenuation;
#elif defined(SHADING_MODEL_CLOTH) && defined(MATERIAL_HAS_SUBSURFACE_COLOR)
    Fd *= saturate(pixel.subsurfaceColor + shading_NoV);
#endif
}

void evaluateIBL(const MaterialInputs material, const PixelParams pixel, inout vec3 color) {
    // Apply transform here if we wanted to rotate the IBL
    vec3 n = shading_normal;

    float ssao = evaluateSSAO();
    float diffuseAO = min(material.ambientOcclusion, ssao);
    float specularAO = computeSpecularAO(shading_NoV, diffuseAO, pixel.roughness);

    // specular layer
    vec3 Fr;
#if IBL_INTEGRATION == IBL_INTEGRATION_PREFILTERED_CUBEMAP
    vec3 E = specularDFG(pixel);
    vec3 r = getReflectedVector(pixel, n);
    Fr = E * prefilteredRadiance(r, pixel.perceptualRoughness);
#elif IBL_INTEGRATION == IBL_INTEGRATION_IMPORTANCE_SAMPLING
    vec3 E = vec3_splat(0.0); // TODO: fix for importance sampling
    Fr = isEvaluateIBL(pixel, shading_normal, shading_view, shading_NoV);
#endif
    Fr *= singleBounceAO(specularAO) * pixel.energyCompensation;

    // diffuse layer
    float diffuseBRDF = singleBounceAO(diffuseAO); // Fd_Lambert() is baked in the SH below
    evaluateClothIndirectDiffuseBRDF(pixel, diffuseBRDF);

    vec3 diffuseIrradianceColor = diffuseIrradiance(n);
    vec3 Fd = pixel.diffuseColor * diffuseIrradianceColor * (1.0 - E) * diffuseBRDF;

    // clear coat layer
    evaluateClearCoatIBL(pixel, specularAO, Fd, Fr);

    // subsurface layer
    evaluateSubsurfaceIBL(pixel, diffuseIrradianceColor, Fd, Fr);

    // extra ambient occlusion term
    multiBounceAO(diffuseAO, pixel.diffuseColor, Fd);
    multiBounceSpecularAO(specularAO, pixel.f0, Fr);

    // Note: iblLuminance is already premultiplied by the exposure
    color.rgb += (Fd + Fr) * u_frameUniforms_iblLuminance;
}

//------------------------------------------------------------------------------
// Punctual lights evaluation
//------------------------------------------------------------------------------

// Make sure this matches the same constants in Froxel.cpp
#define FROXEL_BUFFER_WIDTH_SHIFT   6u
#define FROXEL_BUFFER_WIDTH         (1u << FROXEL_BUFFER_WIDTH_SHIFT)
#define FROXEL_BUFFER_WIDTH_MASK    (FROXEL_BUFFER_WIDTH - 1u)

#define RECORD_BUFFER_WIDTH_SHIFT   5u
#define RECORD_BUFFER_WIDTH         (1u << RECORD_BUFFER_WIDTH_SHIFT)
#define RECORD_BUFFER_WIDTH_MASK    (RECORD_BUFFER_WIDTH - 1u)

struct FroxelParams {
    uint recordOffset; // offset at which the list of lights for this froxel starts
    uint pointCount;   // number of point lights in this froxel
    uint spotCount;    // number of spot lights in this froxel
};

/**
 * Returns the coordinates of the froxel at the specified fragment coordinates.
 * The coordinates are a 3D position in the froxel grid.
 */
uvec3 getFroxelCoords(const vec3 fragCoords) {
    uvec3 froxelCoord;

    froxelCoord.xy = uvec2((fragCoords.xy - u_viewRect.xy) *
            vec2(u_frameUniforms_oneOverFroxelDimension, u_frameUniforms_oneOverFroxelDimensionY));

    froxelCoord.z = uint(max(0.0,
            log2(u_frameUniforms_zParams.x * fragCoords.z + u_frameUniforms_zParams.y) *
                    u_frameUniforms_zParams.z + u_frameUniforms_zParams.w));

    return froxelCoord;
}

/**
 * Computes the froxel index of the fragment at the specified coordinates.
 * The froxel index is computed from the 3D coordinates of the froxel in the
 * froxel grid and later used to fetch from the froxel data texture
 * (light_froxels).
 */
uint getFroxelIndex(const vec3 fragCoords) {
    uvec3 froxelCoord = getFroxelCoords(fragCoords);
    return froxelCoord.x * u_frameUniforms_fParamsX +
           froxelCoord.y * u_frameUniforms_fParams.x +
           froxelCoord.z * u_frameUniforms_fParams.y;
}

/**
 * Computes the texture coordinates of the froxel data given a froxel index.
 */
ivec2 getFroxelTexCoord(uint froxelIndex) {
    return ivec2(froxelIndex & FROXEL_BUFFER_WIDTH_MASK, froxelIndex >> FROXEL_BUFFER_WIDTH_SHIFT);
}

/**
 * Returns the froxel data for the given froxel index. The data is fetched
 * from the light_froxels texture.
 */
FroxelParams getFroxelParams(uint froxelIndex) {
    ivec2 texCoord = getFroxelTexCoord(froxelIndex);
    uvec2 entry = texelFetch(s_texLightFroxels, texCoord, 0).rg;

    FroxelParams froxel;
    froxel.recordOffset = entry.r;
    froxel.pointCount = entry.g & 0xFFu;
    froxel.spotCount = entry.g >> 8u;
    return froxel;
}

/**
 * Returns the coordinates of the light record in the light_records texture
 * given the specified index. A light record is a single uint index into the
 * lights data buffer (lightsUniforms UBO).
 */
ivec2 getRecordTexCoord(uint index) {
    return ivec2(index & RECORD_BUFFER_WIDTH_MASK, index >> RECORD_BUFFER_WIDTH_SHIFT);
}

float getSquareFalloffAttenuation(float distanceSquare, float falloff) {
    float factor = distanceSquare * falloff;
    float smoothFactor = saturate(1.0 - factor * factor);
    // We would normally divide by the square distance here
    // but we do it at the call site
    return smoothFactor * smoothFactor;
}

float getDistanceAttenuation(const highp vec3 posToLight, float falloff) {
    float distanceSquare = dot(posToLight, posToLight);
    float attenuation = getSquareFalloffAttenuation(distanceSquare, falloff);
    // Assume a punctual light occupies a volume of 1cm to avoid a division by 0
    return attenuation * 1.0 / max(distanceSquare, 1e-4);
}

float getAngleAttenuation(const vec3 lightDir, const vec3 l, const vec2 scaleOffset) {
    float cd = dot(lightDir, l);
    float attenuation  = saturate(cd * scaleOffset.x + scaleOffset.y);
    return attenuation * attenuation;
}

/**
 * Light setup common to point and spot light. This function sets the light vector
 * "l" and the attenuation factor in the Light structure. The attenuation factor
 * can be partial: it only takes distance attenuation into account. Spot lights
 * must compute an additional angle attenuation.
 */
void setupPunctualLight(inout Light light, const highp vec4 positionFalloff) {
    highp vec3 worldPosition = shading_position;
    highp vec3 posToLight = positionFalloff.xyz - worldPosition;
    light.l = normalize(posToLight);
    light.attenuation = getDistanceAttenuation(posToLight, positionFalloff.w);
    light.NoL = saturate(dot(shading_normal, light.l));
}

/**
 * Returns a Light structure (see common_lighting.fs) describing a spot light.
 * The colorIntensity field will store the *pre-exposed* intensity of the light
 * in the w component.
 *
 * The light parameters used to compute the Light structure are fetched from the
 * lightsUniforms uniform buffer.
 */
Light getSpotLight(uint index) {
    Light light;
    ivec2 texCoord = getRecordTexCoord(index);
    uint lightIndex = texelFetch(s_texLightRecords, texCoord, 0).r;

    highp vec4 positionFalloff = u_lights[4 * (lightIndex) + (0)];
    highp vec4 colorIntensity  = u_lights[4 * (lightIndex) + (1)];
          vec4 directionIES    = u_lights[4 * (lightIndex) + (2)];
          vec2 scaleOffset     = u_lights[4 * (lightIndex) + (3)].xy;

    light.colorIntensity.rgb = colorIntensity.rgb;
    light.colorIntensity.w = computePreExposedIntensity(colorIntensity.w, u_frameUniforms_exposure);

    setupPunctualLight(light, positionFalloff);

    light.attenuation *= getAngleAttenuation(-directionIES.xyz, light.l, scaleOffset);

    return light;
}

/**
 * Returns a Light structure (see common_lighting.fs) describing a point light.
 * The colorIntensity field will store the *pre-exposed* intensity of the light
 * in the w component.
 *
 * The light parameters used to compute the Light structure are fetched from the
 * lightsUniforms uniform buffer.
 */
Light getPointLight(uint index) {
    Light light;
    ivec2 texCoord = getRecordTexCoord(index);
    uint lightIndex = texelFetch(s_texLightRecords, texCoord, 0).r;

    highp vec4 positionFalloff = u_lights[4 * (lightIndex) + (0)];
    highp vec4 colorIntensity  = u_lights[4 * (lightIndex) + (1)];

    light.colorIntensity.rgb = colorIntensity.rgb;
    light.colorIntensity.w = computePreExposedIntensity(colorIntensity.w, u_frameUniforms_exposure);

    setupPunctualLight(light, positionFalloff);

    return light;
}

/**
 * Evaluates all punctual lights that my affect the current fragment.
 * The result of the lighting computations is accumulated in the color
 * parameter, as linear HDR RGB.
 */
void evaluatePunctualLights(const PixelParams pixel, inout vec3 color) {
    // Fetch the light information stored in the froxel that contains the
    // current fragment
    FroxelParams froxel = getFroxelParams(getFroxelIndex(s_FragCoord.xyz));

    // Each froxel contains how many point and spot lights can influence
    // the current fragment. A froxel also contains a record offset that
    // tells us where the indices of those lights are in the records
    // texture. The records texture contains the indices of the actual
    // light data in the lightsUniforms uniform buffer

    uint index = froxel.recordOffset;
    uint end = index + froxel.pointCount;

    // Iterate point lights
    for ( ; index < end; index++) {
        Light light = getPointLight(index);
#if defined(MATERIAL_CAN_SKIP_LIGHTING)
        if (light.NoL > 0.0) {
            color.rgb += surfaceShading(pixel, light, 1.0);
        }
#else
        color.rgb += surfaceShading(pixel, light, 1.0);
#endif
    }

    end += froxel.spotCount;

    // Iterate spotlights
    for ( ; index < end; index++) {
        Light light = getSpotLight(index);
#if defined(MATERIAL_CAN_SKIP_LIGHTING)
        if (light.NoL > 0.0) {
            color.rgb += surfaceShading(pixel, light, 1.0);
        }
#else
        color.rgb += surfaceShading(pixel, light, 1.0);
#endif
    }
}

//------------------------------------------------------------------------------
// Directional light evaluation
//------------------------------------------------------------------------------

#if !defined(TARGET_MOBILE)
#define SUN_AS_AREA_LIGHT
#endif

vec3 sampleSunAreaLight(const vec3 lightDirection) {
#if defined(SUN_AS_AREA_LIGHT)
    if (u_frameUniforms_sun.w >= 0.0) {
        // simulate sun as disc area light
        float LoR = dot(lightDirection, shading_reflected);
        float d = u_frameUniforms_sun.x;
        highp vec3 s = shading_reflected - LoR * lightDirection;
        return LoR < d ?
                normalize(lightDirection * d + normalize(s) * u_frameUniforms_sun.y) : shading_reflected;
    }
#endif
    return lightDirection;
}

Light getDirectionalLight() {
    Light light;
    // note: lightColorIntensity.w is always premultiplied by the exposure
    light.colorIntensity = u_frameUniforms_lightColorIntensity;
    light.l = sampleSunAreaLight(u_frameUniforms_lightDirection);
    light.attenuation = 1.0;
    light.NoL = saturate(dot(shading_normal, light.l));
    return light;
}

void evaluateDirectionalLight(const MaterialInputs material,
        const PixelParams pixel, inout vec3 color) {

    Light light = getDirectionalLight();

    float visibility = 1.0;
#if defined(HAS_SHADOWING)
    if (light.NoL > 0.0) {
        visibility = shadow2D(s_texShadowMap, getLightSpacePosition());
        #if defined(MATERIAL_HAS_AMBIENT_OCCLUSION)
        visibility *= computeMicroShadowing(light.NoL, material.ambientOcclusion);
        #endif
    } else {
#if defined(MATERIAL_CAN_SKIP_LIGHTING)
        return;
#endif
    }
#elif defined(MATERIAL_CAN_SKIP_LIGHTING)
    if (light.NoL <= 0.0) return;
#endif

    color.rgb += surfaceShading(pixel, light, visibility);
}

//------------------------------------------------------------------------------
// Lighting
//------------------------------------------------------------------------------

float computeDiffuseAlpha(float a) {
#if defined(BLEND_MODE_TRANSPARENT) || defined(BLEND_MODE_FADE) || defined(BLEND_MODE_MASKED)
    return a;
#else
    return 1.0;
#endif
}

#if defined(BLEND_MODE_MASKED)
float computeMaskedAlpha(float a) {
    // Use derivatives to smooth alpha tested edges
    return (a - getMaskThreshold()) / max(fwidth(a), 1e-3) + 0.5;
}
#endif

void applyAlphaMask(inout vec4 baseColor) {
#if defined(BLEND_MODE_MASKED)
    baseColor.a = computeMaskedAlpha(baseColor.a);
    if (baseColor.a <= 0.0) {
        discard;
    }
#endif
}

#if defined(GEOMETRIC_SPECULAR_AA)
float normalFiltering(float perceptualRoughness, const vec3 worldNormal) {
    // Kaplanyan 2016, "Stable specular highlights"
    // Tokuyoshi 2017, "Error Reduction and Simplification for Shading Anti-Aliasing"
    // Tokuyoshi and Kaplanyan 2019, "Improved Geometric Specular Antialiasing"

    // This implementation is meant for deferred rendering in the original paper but
    // we use it in forward rendering as well (as discussed in Tokuyoshi and Kaplanyan
    // 2019). The main reason is that the forward version requires an expensive transform
    // of the half vector by the tangent frame for every light. This is therefore an
    // approximation but it works well enough for our needs and provides an improvement
    // over our original implementation based on Vlachos 2015, "Advanced VR Rendering".

    vec3 du = dFdx(worldNormal);
    vec3 dv = dFdy(worldNormal);

    float variance = u_objectUniforms_specularAntiAliasingVariance * (dot(du, du) + dot(dv, dv));

    float roughness = perceptualRoughnessToRoughness(perceptualRoughness);
    float kernelRoughness = min(2.0 * variance, u_objectUniforms_specularAntiAliasingThreshold);
    float squareRoughness = saturate(roughness * roughness + kernelRoughness);

    return roughnessToPerceptualRoughness(sqrt(squareRoughness));
}
#endif

void getCommonPixelParams(const MaterialInputs material, inout PixelParams pixel) {
    vec4 baseColor = material.baseColor;
    applyAlphaMask(baseColor);

#if defined(BLEND_MODE_FADE) && !defined(SHADING_MODEL_UNLIT)
    // Since we work in premultiplied alpha mode, we need to un-premultiply
    // in fade mode so we can apply alpha to both the specular and diffuse
    // components at the end
    unpremultiply(baseColor);
#endif

#if defined(SHADING_MODEL_SPECULAR_GLOSSINESS)
    // This is from KHR_materials_pbrSpecularGlossiness.
    vec3 specularColor = material.specularColor;
    float metallic = computeMetallicFromSpecularColor(specularColor);

    pixel.diffuseColor = computeDiffuseColor(baseColor, metallic);
    pixel.f0 = specularColor;
#elif !defined(SHADING_MODEL_CLOTH)
    pixel.diffuseColor = computeDiffuseColor(baseColor, material.metallic);

    // Assumes an interface from air to an IOR of 1.5 for dielectrics
    float reflectance = computeDielectricF0(material.reflectance);
    pixel.f0 = computeF0(baseColor, material.metallic, reflectance);
#else
    pixel.diffuseColor = baseColor.rgb;
    pixel.f0 = material.sheenColor;
#if defined(MATERIAL_HAS_SUBSURFACE_COLOR)
    pixel.subsurfaceColor = material.subsurfaceColor;
#endif
#endif
}

void getClearCoatPixelParams(const MaterialInputs material, inout PixelParams pixel) {
#if defined(MATERIAL_HAS_CLEAR_COAT)
    pixel.clearCoat = material.clearCoat;

    // Clamp the clear coat roughness to avoid divisions by 0
    float clearCoatPerceptualRoughness = material.clearCoatRoughness;
    clearCoatPerceptualRoughness =
            clamp(clearCoatPerceptualRoughness, MIN_PERCEPTUAL_ROUGHNESS, 1.0);

#if defined(GEOMETRIC_SPECULAR_AA)
    clearCoatPerceptualRoughness =
            normalFiltering(clearCoatPerceptualRoughness, getWorldGeometricNormalVector());
#endif

    pixel.clearCoatPerceptualRoughness = clearCoatPerceptualRoughness;
    pixel.clearCoatRoughness = perceptualRoughnessToRoughness(clearCoatPerceptualRoughness);

#if defined(CLEAR_COAT_IOR_CHANGE)
    // The base layer's f0 is computed assuming an interface from air to an IOR
    // of 1.5, but the clear coat layer forms an interface from IOR 1.5 to IOR
    // 1.5. We recompute f0 by first computing its IOR, then reconverting to f0
    // by using the correct interface
    pixel.f0 = mix(pixel.f0, f0ClearCoatToSurface(pixel.f0), pixel.clearCoat);
#endif
#endif
}

void getRoughnessPixelParams(const MaterialInputs material, inout PixelParams pixel) {
#if defined(SHADING_MODEL_SPECULAR_GLOSSINESS)
    float perceptualRoughness = computeRoughnessFromGlossiness(material.glossiness);
#else
    float perceptualRoughness = material.roughness;
#endif

    // Clamp the roughness to a minimum value to avoid divisions by 0 during lighting
    perceptualRoughness = clamp(perceptualRoughness, MIN_PERCEPTUAL_ROUGHNESS, 1.0);

#if defined(GEOMETRIC_SPECULAR_AA)
    perceptualRoughness = normalFiltering(perceptualRoughness, getWorldGeometricNormalVector());
#endif

#if defined(MATERIAL_HAS_CLEAR_COAT) && defined(MATERIAL_HAS_CLEAR_COAT_ROUGHNESS)
    // This is a hack but it will do: the base layer must be at least as rough
    // as the clear coat layer to take into account possible diffusion by the
    // top layer
    float basePerceptualRoughness = max(perceptualRoughness, pixel.clearCoatPerceptualRoughness);
    perceptualRoughness = mix(perceptualRoughness, basePerceptualRoughness, pixel.clearCoat);
#endif

    // Remaps the roughness to a perceptually linear roughness (roughness^2)
    pixel.perceptualRoughness = perceptualRoughness;
    pixel.roughness = perceptualRoughnessToRoughness(perceptualRoughness);
}

void getSubsurfacePixelParams(const MaterialInputs material, inout PixelParams pixel) {
#if defined(SHADING_MODEL_SUBSURFACE)
    pixel.subsurfacePower = material.subsurfacePower;
    pixel.subsurfaceColor = material.subsurfaceColor;
    pixel.thickness = saturate(material.thickness);
#endif
}

void getAnisotropyPixelParams(const MaterialInputs material, inout PixelParams pixel) {
#if defined(MATERIAL_HAS_ANISOTROPY)
    vec3 direction = material.anisotropyDirection;
    pixel.anisotropy = material.anisotropy;
    pixel.anisotropicT = normalize(mul(shading_tangentToWorld, direction));
    pixel.anisotropicB = normalize(cross(getWorldGeometricNormalVector(), pixel.anisotropicT));
#endif
}

void getEnergyCompensationPixelParams(inout PixelParams pixel) {
    // Pre-filtered DFG term used for image-based lighting
    pixel.dfg = prefilteredDFG(pixel.perceptualRoughness, shading_NoV);

#if !defined(SHADING_MODEL_CLOTH)
    // Energy compensation for multiple scattering in a microfacet model
    // See "Multiple-Scattering Microfacet BSDFs with the Smith Model"
    pixel.energyCompensation = 1.0 + pixel.f0 * (1.0 / pixel.dfg.y - 1.0);
#else
    pixel.energyCompensation = vec3_splat(1.0);
#endif
}

/**
 * Computes all the parameters required to shade the current pixel/fragment.
 * These parameters are derived from the MaterialInputs structure computed
 * by the user's material code.
 *
 * This function is also responsible for discarding the fragment when alpha
 * testing fails.
 */
void getPixelParams(const MaterialInputs material, out PixelParams pixel) {
   pixel = (PixelParams)0;
    getCommonPixelParams(material, pixel);
    getClearCoatPixelParams(material, pixel);
    getRoughnessPixelParams(material, pixel);
    getSubsurfacePixelParams(material, pixel);
    getAnisotropyPixelParams(material, pixel);
    getEnergyCompensationPixelParams(pixel);
}

/**
 * This function evaluates all lights one by one:
 * - Image based lights (IBL)
 * - Directional lights
 * - Punctual lights
 *
 * Area lights are currently not supported.
 *
 * Returns a pre-exposed HDR RGBA color in linear space.
 */
vec4 evaluateLights(const MaterialInputs material) {
    PixelParams pixel;
    getPixelParams(material, pixel);

    // Ideally we would keep the diffuse and specular components separate
    // until the very end but it costs more ALUs on mobile. The gains are
    // currently not worth the extra operations
    vec3 color = vec3_splat(0.0);

    // We always evaluate the IBL as not having one is going to be uncommon,
    // it also saves 1 shader variant
    evaluateIBL(material, pixel, color);

#if defined(HAS_DIRECTIONAL_LIGHTING)
    evaluateDirectionalLight(material, pixel, color);
#endif

#if defined(HAS_DYNAMIC_LIGHTING)
    evaluatePunctualLights(pixel, color);
#endif

#if defined(BLEND_MODE_FADE) && !defined(SHADING_MODEL_UNLIT)
    // In fade mode we un-premultiply baseColor early on, so we need to
    // premultiply again at the end (affects diffuse and specular lighting)
    color *= material.baseColor.a;
#endif

    return vec4(color, computeDiffuseAlpha(material.baseColor.a));
}

void addEmissive(const MaterialInputs material, inout vec4 color) {
#if defined(MATERIAL_HAS_EMISSIVE)
    // The emissive property applies independently of the shading model
    // It is defined as a color + exposure compensation
    highp vec4 emissive = material.emissive;
    highp float attenuation = computePreExposedIntensity(
            pow(2.0, u_frameUniforms_ev100 + emissive.w - 3.0), u_frameUniforms_exposure);
    color.rgb += emissive.rgb * attenuation;
#endif
}

/**
 * Evaluate lit materials. The actual shading model used to do so is defined
 * by the function surfaceShading() found in shading_model_*.fs.
 *
 * Returns a pre-exposed HDR RGBA color in linear space.
 */
vec4 evaluateMaterial(const MaterialInputs material) {
    vec4 color = evaluateLights(material);
    addEmissive(material, color);
    return color;
}

#endif
#endif //BGFX_SHADER_TYPE_FRAGMENT
#if BGFX_SHADER_TYPE_VERTEX

struct VertexAttributes
{
	highp vec4 position;
	highp vec4 tangents;
	uvec4 bone_indices;
	uvec4 bone_weights;
    vec4 morph_position0;
    vec4 morph_position1;
    vec4 morph_position2;
    vec4 morph_position3;
    vec4 morph_tangents0;
    vec4 morph_tangents1;
    vec4 morph_tangents2;
    vec4 morph_tangents3;
};

void initAttributes(out VertexAttributes _attributes)
{
	_attributes = (VertexAttributes)0;
}


struct VertexOutput
{
	highp vec3 worldPosition;
	mediump vec3 worldNormal;
	mediump vec3 worldTangent;
	mediump vec3 worldBitangent;
	highp vec4 lightSpacePosition;
	highp vec4 clipPosition;
};

void evaluate(out VertexOutput _output, in VertexAttributes _attributes)
{
	_output = (VertexOutput)0;

	mesh_position = _attributes.position;
#if defined(HAS_ATTRIBUTE_TANGENTS)
	mesh_tangents = _attributes.tangents;
#endif

#if defined(HAS_ATTRIBUTE_BONE_INDICES)
	mesh_bone_indices = _attributes.bone_indices;
#endif

#if defined(HAS_ATTRIBUTE_BONE_WEIGHTS)
	mesh_bone_weights = _attributes.bone_weights;
#endif

#if defined(HAS_SKINNING_OR_MORPHING)
	mesh_morph_position0 = _attributes.morph_position0;
	mesh_morph_position1 = _attributes.morph_position1;
	mesh_morph_position2 = _attributes.morph_position2;
	mesh_morph_position3 = _attributes.morph_position3;

	mesh_morph_tangents0 = _attributes.morph_tangents0;
	mesh_morph_tangents1 = _attributes.morph_tangents1;
	mesh_morph_tangents2 = _attributes.morph_tangents2;
	mesh_morph_tangents3 = _attributes.morph_tangents3;
#endif

#if defined(HAS_ATTRIBUTE_TANGENTS)
    // If the material defines a value for the "normal" property, we need to _output
    // the full orthonormal basis to apply normal mapping
    #if defined(MATERIAL_HAS_ANISOTROPY) || defined(MATERIAL_HAS_NORMAL) || defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
        // Extract the normal and tangent in world space from the input quaternion
        // We encode the orthonormal basis as a quaternion to save space in the attributes
        toTangentFrame(mesh_tangents, _output.worldNormal, _output.worldTangent);

        #if defined(HAS_SKINNING_OR_MORPHING)

            if (u_objectUniforms_morphingEnabled == 1) {
                vec3 normal0, normal1, normal2, normal3;
                toTangentFrame(mesh_morph_tangents0, normal0);
                toTangentFrame(mesh_morph_tangents1, normal1);
                toTangentFrame(mesh_morph_tangents2, normal2);
                toTangentFrame(mesh_morph_tangents3, normal3);
                _output.worldNormal += u_objectUniforms_morphWeights.x * normal0;
                _output.worldNormal += u_objectUniforms_morphWeights.y * normal1;
                _output.worldNormal += u_objectUniforms_morphWeights.z * normal2;
                _output.worldNormal += u_objectUniforms_morphWeights.w * normal3;
                _output.worldNormal = normalize(_output.worldNormal);
            }

            if (u_objectUniforms_skinningEnabled == 1) {
                skinNormal(_output.worldNormal, mesh_bone_indices, mesh_bone_weights);
                skinNormal(_output.worldTangent, mesh_bone_indices, mesh_bone_weights);
            }

        #endif

        // We don't need to normalize here, even if there's a scale in the matrix
        // because we ensure the worldFromModelNormalMatrix pre-scales the normal such that
        // all its components are < 1.0. This precents the bitangent to exceed the range of fp16
        // in the fragment shader, where we renormalize after interpolation
        _output.worldTangent = mul(u_objectUniforms_worldFromModelNormalMatrix, _output.worldTangent);
        _output.worldNormal = mul(u_objectUniforms_worldFromModelNormalMatrix, _output.worldNormal);

        // Reconstruct the bitangent from the normal and tangent. We don't bother with
        // normalization here since we'll do it after interpolation in the fragment stage
        _output.worldBitangent =
                cross(_output.worldNormal, _output.worldTangent) * sign(mesh_tangents.w);
    #else // MATERIAL_HAS_ANISOTROPY || MATERIAL_HAS_NORMAL
        // Without anisotropy or normal mapping we only need the normal vector
        toTangentFrame(mesh_tangents, _output.worldNormal);
        _output.worldNormal = mul(u_objectUniforms_worldFromModelNormalMatrix, _output.worldNormal);
        #if defined(HAS_SKINNING_OR_MORPHING)
            if (u_objectUniforms_skinningEnabled == 1) {
                skinNormal(_output.worldNormal, mesh_bone_indices, mesh_bone_weights);
            }
        #endif
    #endif // MATERIAL_HAS_ANISOTROPY || MATERIAL_HAS_NORMAL
#endif // HAS_ATTRIBUTE_TANGENTS

    // The world position can be changed by the user in materialVertex()
    _output.worldPosition = computeWorldPosition().xyz;

#if defined(HAS_SHADOWING) && defined(HAS_DIRECTIONAL_LIGHTING)
    _output.lightSpacePosition = getLightSpacePosition(_output.worldPosition, _output.worldNormal);
#endif

#if defined(VERTEX_DOMAIN_DEVICE)
    // The other vertex domains are handled in initMaterialVertex()->computeWorldPosition()
    _output.clipPosition = getPosition();
#else
    _output.clipPosition = mul(getClipFromWorldMatrix(), vec4(_output.worldPosition,1.0));
#endif

}

#endif

#if BGFX_SHADER_TYPE_FRAGMENT

struct FragmentStageInputs
{
	highp vec3 worldPosition;
	highp vec3 worldNormal;
	highp vec3 worldTangent;
	highp vec3 worldBitangent;
	highp vec4 lightSpacePosition;
	bool frontFacing;
	highp vec4 fragCoord;
};

void initFragmentStageInputs(out FragmentStageInputs _input)
{
	_input = (FragmentStageInputs)0;
}


/*
 * Returns a pre-exposed HDR RGBA color in linear space.
 */
vec4 evaluate(const MaterialInputs _inputs, FragmentStageInputs _stageInputs)
{
	shading_position = _stageInputs.worldPosition;
	shading_view = normalize(u_frameUniforms_cameraPosition - shading_position);

#if defined(HAS_SHADOWING) && defined(HAS_DIRECTIONAL_LIGHTING)
	vertex_lightSpacePosition = _stageInputs.lightSpacePosition;
#endif

#if defined(HAS_ATTRIBUTE_TANGENTS)
	vec3 n = _stageInputs.worldNormal;
#if defined(MATERIAL_HAS_DOUBLE_SIDED_CAPABILITY)
	if (isDoubleSided()) {
		n = _stageInputs.frontFacing ? n : -n;
	}
#endif

#if defined(MATERIAL_HAS_ANISOTROPY) || defined(MATERIAL_HAS_NORMAL) || defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
	// Re-normalize post-interpolation values
	shading_tangentToWorld = mat3(normalize(_stageInputs.worldTangent), normalize(_stageInputs.worldBitangent), normalize(n));
#endif
	// Leave the tangent and bitangent uninitialized, we won't use them
	shading_tangentToWorld[2] = normalize(n);
#endif

	s_FragCoord = _stageInputs.fragCoord;

	prepareMaterial(_inputs);

	vec4 fragColor = evaluateMaterial(_inputs);

	return fragColor;
}

#endif



