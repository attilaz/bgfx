/*
 * Copyright 2019-2019 Attila Kocsis. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include <vector>
#include <string>

#include "common.h"
#include "bgfx_utils.h"
#include "imgui/imgui.h"
#include "nanovg/nanovg.h"

#include <bx/readerwriter.h>
#include <bx/string.h>

namespace
{

struct Uniforms
{
	enum { FrameNumVec4 = 21 };
	enum { ObjectNumVec4 = 6 };
	enum { MaterialNumVec4 = 8 };

	void init()
	{
		u_frameUniforms = bgfx::createUniform("u_frameUniforms", bgfx::UniformType::Vec4, FrameNumVec4);
		u_objectUniforms = bgfx::createUniform("u_objectUniforms", bgfx::UniformType::Vec4, ObjectNumVec4);
		u_materialUniforms = bgfx::createUniform("u_materialUniforms", bgfx::UniformType::Vec4, MaterialNumVec4);
	}

	void submit()
	{
		bgfx::setUniform(u_frameUniforms, m_frameParams, FrameNumVec4);
		bgfx::setUniform(u_objectUniforms, m_objectParams, ObjectNumVec4);
		bgfx::setUniform(u_materialUniforms, m_materialParams, MaterialNumVec4);
	}

	void destroy()
	{
		bgfx::destroy(u_frameUniforms);
		bgfx::destroy(u_objectUniforms);
		bgfx::destroy(u_materialUniforms);
	}

	union
	{
		struct
		{
			/* 0-3 */ float m_lightFromWorldMatrix[16];
			/* 4   */ struct { float m_cameraPosition[3], m_unused0; };
			/* 5   */ struct { float m_lightColorIntensity[4]; };
			/* 6   */ struct { float m_sun[4]; };
			/* 7   */ struct { float m_lightDirection[3], m_fParamsX; };
			/* 8   */ struct { float m_shadowBias[3], m_oneOverFroxelDimensionY; };
			/* 9   */ struct { float m_zParams[4]; };
			/*10   */ struct { float m_oneOverFroxelDimension, m_iblLuminance, m_exposure, m_ev100; };
			/*11-19*/ struct { float m_iblSH[9][4]; };
			/*20   */ struct { float m_iblMaxMipLevel[2], m_fParams[2]; };
		};

		float m_frameParams[FrameNumVec4*4];
	};
	
	union
	{
		struct
		{
			/* 0-2 */ struct { float m_worldFromModelNormalMatrix[3*4]; };
			/* 3   */ struct { float m_morphWeights[4]; };
			/* 4   */ struct { float m_skinningEnabled, m_morphingEnabled, m_unused2[2]; };
			/* 5   */ struct { float m_specularAntiAliasingVariance, m_specularAntiAliasingThreshold, m_maskThreshold, m_doubleSided; };
		};
		
		float m_objectParams[ObjectNumVec4*4];
	};

	union
	{
		struct
		{
			/* 0 */ struct { float m_baseColor[4]; };
			/* 1 */ struct { float m_roughness, m_metallic, m_reflectance, m_unused4; };
			/* 2 */ struct { float m_emissive[4]; };
			/* 3 */ struct { float m_clearCoat, m_clearCoatRoughness, m_anisotropy, m_unused3; };
			/* 4 */ struct { float m_anisotropyDirection[3], m_thickness; };
			/* 5 */ struct { float m_subsurfaceColor[3], m_subsurfacePower; };
			/* 6 */ struct { float m_sheenColor[3], m_unused5; };
			/* 7 */ struct { float m_specularColor[3], m_glossiness; };
		};
		
		float m_materialParams[MaterialNumVec4 * 4];
	};

	bgfx::UniformHandle u_frameUniforms;
	bgfx::UniformHandle u_objectUniforms;
	bgfx::UniformHandle u_materialUniforms;
};
	
	

struct PosColorTexCoord0Vertex
{
	float m_x;
	float m_y;
	float m_z;
	uint32_t m_rgba;
	float m_u;
	float m_v;

	static void init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.end();
	}

	static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosColorTexCoord0Vertex::ms_layout;


struct LightProbe
{
	enum Enum
	{
		TeufelsbergLookout,
		CapeHill,
		SmallHangar,

		Count
	};

	void load(const char* _name, const float* _sh)
	{
		char filePath[512];
		bx::snprintf(filePath, BX_COUNTOF(filePath), "textures/%s.ktx", _name);

		bgfx::TextureInfo info;
		m_tex = loadTexture(filePath, BGFX_SAMPLER_U_CLAMP|BGFX_SAMPLER_V_CLAMP|BGFX_SAMPLER_W_CLAMP, 0, &info);
		m_texNumMips = info.numMips;

		bx::memCopy(m_sh, _sh, sizeof(m_sh));
	}

	void destroy()
	{
		bgfx::destroy(m_tex);
	}

	bgfx::TextureHandle m_tex;
	int					m_texNumMips;
	float               m_sh[9][4];
};

float s_teufelsberg_lookout_sh[9][4] =
{
	{ 1.033135294914246,  1.240853190422058,  1.641853094100952 }, // L00, irradiance, pre-scaled base
	{ 0.174028113484383,  0.271607637405396,  0.534212946891785 }, // L1-1, irradiance, pre-scaled base
	{ 0.517928361892700,  0.721139192581177,  1.262967824935913 }, // L10, irradiance, pre-scaled base
	{ 0.664545714855194,  0.628159761428833,  0.453870892524719 }, // L11, irradiance, pre-scaled base
	{ 0.073169127106667,  0.060536645352840,  0.036284871399403 }, // L2-2, irradiance, pre-scaled base
	{ 0.147108390927315,  0.310699999332428,  0.680426359176636 }, // L2-1, irradiance, pre-scaled base
	{ -0.013953230343759,  0.038428101688623,  0.148265913128853 }, // L20, irradiance, pre-scaled base
	{ 0.101323686540127,  0.099259167909622,  0.119761399924755 }, // L21, irradiance, pre-scaled base
	{ 0.463056743144989,  0.472212910652161,  0.450877964496613 }, // L22, irradiance, pre-scaled base
};
	
float s_cape_hill_sh[9][4] =
{
	{ 0.470912307500839,  0.375074952840805,  0.199702173471451 }, // L00, irradiance, pre-scaled base
	{ 0.110779501497746,  0.107183508574963,  0.101078890264034 }, // L1-1, irradiance, pre-scaled base
	{ 0.594431459903717,  0.433289110660553,  0.144471630454063 }, // L10, irradiance, pre-scaled base
	{-0.445300042629242, -0.324924767017365, -0.110353000462055 }, // L11, irradiance, pre-scaled base
	{-0.070068545639515, -0.056038860231638, -0.027694350108504 }, // L2-2, irradiance, pre-scaled base
	{ 0.101110078394413,  0.081764772534370,  0.041827443987131 }, // L2-1, irradiance, pre-scaled base
	{ 0.089141719043255,  0.064426310360432,  0.020202396437526 }, // L20, irradiance, pre-scaled base
	{-0.562590062618256, -0.405030250549316, -0.126809135079384 }, // L21, irradiance, pre-scaled base
	{ 0.114094585180283,  0.084839887917042,  0.030127054080367 }, // L22, irradiance, pre-scaled base
};

float s_small_hangar_01_sh[9][4] =
{
	{ 0.727795064449310,  0.659785389900208,  0.605563223361969 }, // L00, irradiance, pre-scaled base
	{ 0.141508892178535,  0.147570356726646,  0.154632449150085 }, // L1-1, irradiance, pre-scaled base
	{ 0.552958190441132,  0.513145864009857,  0.473500430583954 }, // L10, irradiance, pre-scaled base
	{-0.343778431415558, -0.372748464345932, -0.426187932491302 }, // L11, irradiance, pre-scaled base
	{-0.079005286097527, -0.081963807344437, -0.091765619814396 }, // L2-2, irradiance, pre-scaled base
	{ 0.124498769640923,  0.138117402791977,  0.148704007267952 }, // L2-1, irradiance, pre-scaled base
	{ 0.045846488326788,  0.040140554308891,  0.030297346413136 }, // L20, irradiance, pre-scaled base
	{-0.094629704952240, -0.119590513408184, -0.151496425271034 }, // L21, irradiance, pre-scaled base
	{ 0.183159321546555,  0.185391694307327,  0.191680058836937 }, // L22, irradiance, pre-scaled base
};
	

struct Camera
{
	Camera()
	{
		reset();
	}

	void reset()
	{
		m_target.curr = { 0.0f, 0.0f, 0.0f };
		m_target.dest = { 0.0f, 0.0f, 0.0f };

		m_pos.curr = { 0.0f, 0.0f, -3.0f };
		m_pos.dest = { 0.0f, 0.0f, -3.0f };

		m_orbit[0] = 0.0f;
		m_orbit[1] = 0.0f;
	}

	void mtxLookAt(float* _outViewMtx)
	{
		bx::mtxLookAt(_outViewMtx, m_pos.curr, m_target.curr);
	}

	void orbit(float _dx, float _dy)
	{
		m_orbit[0] += _dx;
		m_orbit[1] += _dy;
	}

	void dolly(float _dz)
	{
		const float cnear = 1.0f;
		const float cfar  = 100.0f;

		const bx::Vec3 toTarget     = bx::sub(m_target.dest, m_pos.dest);
		const float toTargetLen     = bx::length(toTarget);
		const float invToTargetLen  = 1.0f / (toTargetLen + bx::kFloatMin);
		const bx::Vec3 toTargetNorm = bx::mul(toTarget, invToTargetLen);

		float delta  = toTargetLen * _dz;
		float newLen = toTargetLen + delta;
		if ( (cnear  < newLen || _dz < 0.0f)
		&&   (newLen < cfar   || _dz > 0.0f) )
		{
			m_pos.dest = bx::mad(toTargetNorm, delta, m_pos.dest);
		}
	}

	void consumeOrbit(float _amount)
	{
		float consume[2];
		consume[0] = m_orbit[0] * _amount;
		consume[1] = m_orbit[1] * _amount;
		m_orbit[0] -= consume[0];
		m_orbit[1] -= consume[1];

		const bx::Vec3 toPos     = bx::sub(m_pos.curr, m_target.curr);
		const float toPosLen     = bx::length(toPos);
		const float invToPosLen  = 1.0f / (toPosLen + bx::kFloatMin);
		const bx::Vec3 toPosNorm = bx::mul(toPos, invToPosLen);

		float ll[2];
		bx::toLatLong(&ll[0], &ll[1], toPosNorm);
		ll[0] += consume[0];
		ll[1] -= consume[1];
		ll[1]  = bx::clamp(ll[1], 0.02f, 0.98f);

		const bx::Vec3 tmp  = bx::fromLatLong(ll[0], ll[1]);
		const bx::Vec3 diff = bx::mul(bx::sub(tmp, toPosNorm), toPosLen);

		m_pos.curr = bx::add(m_pos.curr, diff);
		m_pos.dest = bx::add(m_pos.dest, diff);
	}

	void update(float _dt)
	{
		const float amount = bx::min(_dt / 0.12f, 1.0f);

		consumeOrbit(amount);

		m_target.curr = bx::lerp(m_target.curr, m_target.dest, amount);
		m_pos.curr    = bx::lerp(m_pos.curr,    m_pos.dest,    amount);
	}

	void envViewMtx(float* _mtx)
	{
		const bx::Vec3 toTarget     = bx::sub(m_target.curr, m_pos.curr);
		const float toTargetLen     = bx::length(toTarget);
		const float invToTargetLen  = 1.0f / (toTargetLen + bx::kFloatMin);
		const bx::Vec3 toTargetNorm = bx::mul(toTarget, invToTargetLen);

		const bx::Vec3 right = bx::normalize(bx::cross({ 0.0f, 1.0f, 0.0f }, toTargetNorm) );
		const bx::Vec3 up    = bx::normalize(bx::cross(toTargetNorm, right) );

		_mtx[ 0] = right.x;
		_mtx[ 1] = right.y;
		_mtx[ 2] = right.z;
		_mtx[ 3] = 0.0f;
		_mtx[ 4] = up.x;
		_mtx[ 5] = up.y;
		_mtx[ 6] = up.z;
		_mtx[ 7] = 0.0f;
		_mtx[ 8] = toTargetNorm.x;
		_mtx[ 9] = toTargetNorm.y;
		_mtx[10] = toTargetNorm.z;
		_mtx[11] = 0.0f;
		_mtx[12] = 0.0f;
		_mtx[13] = 0.0f;
		_mtx[14] = 0.0f;
		_mtx[15] = 1.0f;
	}

	struct Interp3f
	{
		bx::Vec3 curr;
		bx::Vec3 dest;
	};

	Interp3f m_target;
	Interp3f m_pos;
	float m_orbit[2];
};

struct Mouse
{
	Mouse()
		: m_dx(0.0f)
		, m_dy(0.0f)
		, m_prevMx(0.0f)
		, m_prevMy(0.0f)
		, m_scroll(0)
		, m_scrollPrev(0)
	{
	}

	void update(float _mx, float _my, int32_t _mz, uint32_t _width, uint32_t _height)
	{
		const float widthf  = float(int32_t(_width));
		const float heightf = float(int32_t(_height));

		// Delta movement.
		m_dx = float(_mx - m_prevMx)/widthf;
		m_dy = float(_my - m_prevMy)/heightf;

		m_prevMx = _mx;
		m_prevMy = _my;

		// Scroll.
		m_scroll = _mz - m_scrollPrev;
		m_scrollPrev = _mz;
	}

	float m_dx; // Screen space.
	float m_dy;
	float m_prevMx;
	float m_prevMy;
	int32_t m_scroll;
	int32_t m_scrollPrev;
};
	
#define MIN_APERTURE (0.5f)
#define MAX_APERTURE (64.0f)
#define MIN_SHUTTER_SPEED (1.0f / 25000.0f)
#define MAX_SHUTTER_SPEED (60.0f)
#define MIN_SENSITIVITY (10.0f)
#define MAX_SENSITIVITY (204800.0f)

struct Settings
{
	Settings()
	{
		m_lightColor[0] = 1.0f;
		m_lightColor[1] = 1.0f;
		m_lightColor[2] = 1.0f;
		m_lightIntensity = 100000.0f;
		// area light: cos(radius), sin(radius), 1.0f / (cos(radius * haloSize) - cos(radius)), haloFalloff
		m_sunRadius = 1.0f;
		m_sunHaloSize = 1.0f;
		m_sunHaloFalloff = 1.0f;
		
		m_lightElevation = 45.0f;
		m_lightAzimuth = 70.0f;
		
		m_iblLuminance = 50000.0f;
		m_cameraAperture = 16.0f;
		m_cameraShutterSpeed = 1.0f / 125.0f;
		m_cameraSensitivity = 100.0f;

		m_specularAntiAliasingVariance = 0.0f;
		m_specularAntiAliasingThreshold = 0.0f;
		m_doubleSided = 0.0f;
		
		float baseColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f};
		bx::memCopy(m_baseColor, baseColor, 4*sizeof(float));
		m_roughness = 0.0f;
		m_metallic = 0.0f;
		m_reflectance = 0.0f;
		float emissive[4] = { 0.0f, 0.0f, 0.0f, 0.0f};
		bx::memCopy(m_emissive, emissive, 4*sizeof(float));
		m_clearCoat = 0.0f;
		m_clearCoatRoughness = 0.0f;
		m_anisotropy = 0.0f;
		float anisotropyDirection[3] = { 1.0f, 1.0f, 1.0f};
		bx::memCopy(m_anisotropyDirection, anisotropyDirection, 3*sizeof(float));
		m_thickness = 1.0f;
		float subsurfaceColor[3] = { 1.0f, 1.0f, 1.0f};
		bx::memCopy(m_subsurfaceColor, subsurfaceColor, 3*sizeof(float));
		m_subsurfacePower = 1.0f;
		float sheenColor[3] = { 1.0f, 1.0f, 1.0f};
		bx::memCopy(m_sheenColor, sheenColor, 3*sizeof(float));
		float specularColor[3] = { 1.0f, 1.0f, 1.0f};
		bx::memCopy(m_specularColor, specularColor, 3*sizeof(float));
		m_glossiness = 1.0f;

		m_meshSelection = 0;
	}

	float m_lightColor[3];
	float m_lightIntensity;
	// area light: cos(radius), sin(radius), 1.0f / (cos(radius * haloSize) - cos(radius)), haloFalloff
	float m_sunRadius;
	float m_sunHaloSize;
	float m_sunHaloFalloff;
	float m_lightElevation;
	float m_lightAzimuth;
	
	float m_iblLuminance;
	float m_cameraAperture;
	float m_cameraShutterSpeed;
	float m_cameraSensitivity;

	/** Sets this camera's exposure (default is f/16, 1/125s, 100 ISO)
	 *
	 * The exposure ultimately controls the scene's brightness, just like with a real camera.
	 * The default values provide adequate exposure for a camera placed outdoors on a sunny day
	 * with the sun at the zenith.
	 *
	 * @param aperture      Aperture in f-stops, clamped between 0.5 and 64.
	 *                      A lower \p aperture value *increases* the exposure, leading to
	 *                      a brighter scene. Realistic values are between 0.95 and 32.
	 *
	 * @param shutterSpeed  Shutter speed in seconds, clamped between 1/25,000 and 60.
	 *                      A lower shutter speed increases the exposure. Realistic values are
	 *                      between 1/8000 and 30.
	 *
	 * @param sensitivity   Sensitivity in ISO, clamped between 10 and 204,800.
	 *                      A higher \p sensitivity increases the exposure. Realistic values are
	 *                      between 50 and 25600.
	 *
	 * @note
	 * With the default parameters, the scene must contain at least one Light of intensity
	 * similar to the sun (e.g.: a 100,000 lux directional light).
	 */
	
	float m_specularAntiAliasingVariance;
	float m_specularAntiAliasingThreshold;
	bool m_doubleSided;
	
	float m_baseColor[4];
	float m_roughness;
	float m_metallic;
	float m_reflectance;
	float m_emissive[4];
	float m_clearCoat;
	float m_clearCoatRoughness;
	float m_anisotropy;
	float m_anisotropyDirection[3];
	float m_thickness;
	float m_subsurfaceColor[3];
	float m_subsurfacePower;
	float m_sheenColor[3];
	float m_specularColor[3];
	float m_glossiness;
	
	int32_t m_meshSelection;
};
	
class ExamplePbr : public entry::AppI
{
public:
	ExamplePbr(const char* _name, const char* _description, const char* _url)
		: entry::AppI(_name, _description, _url)
	{
	}

	void init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height) override
	{
		Args args(_argc, _argv);

		m_width  = _width;
		m_height = _height;
		m_debug = BGFX_DEBUG_NONE;
		m_reset  = 0
			| BGFX_RESET_VSYNC
			| BGFX_RESET_MSAA_X16
			;

		bgfx::Init init;
		init.type     = args.m_type;
		init.vendorId = args.m_pciId;
		init.resolution.width  = m_width;
		init.resolution.height = m_height;
		init.resolution.reset  = m_reset;
		bgfx::init(init);

		// Enable debug text.
		bgfx::setDebug(m_debug);

		// Set views  clear state.
		bgfx::setViewClear(0
			, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
			, 0x303030ff
			, 1.0f
			, 0
			);

		// Imgui.
		imguiCreate();

		// Uniforms.
		m_uniforms.init();

		// Vertex declarations.
		PosColorTexCoord0Vertex::init();

		m_lightProbes[LightProbe::TeufelsbergLookout].load("teufelsberg_lookout_ibl", (const float*)s_teufelsberg_lookout_sh);
		m_lightProbes[LightProbe::CapeHill].load("cape_hill_ibl", (const float*)s_cape_hill_sh);
		m_lightProbes[LightProbe::SmallHangar].load("small_hangar_01_ibl", (const float*)s_small_hangar_01_sh);
		m_currentLightProbe = LightProbe::TeufelsbergLookout;

		m_texIblDFG = loadTexture("textures/dfg_ibl.dds");
		m_texSsao = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8);
		uint32_t whitePixel = 0xffffffff;
		bgfx::updateTexture2D(m_texSsao, 0, 0, 0, 0, 1, 1, bgfx::copy(&whitePixel, sizeof(whitePixel)));

		s_texIblDFG      = bgfx::createUniform("s_texIblDFG",    bgfx::UniformType::Sampler);
		s_texIblSpecular = bgfx::createUniform("s_texIblSpecular", bgfx::UniformType::Sampler);
		s_texSsao        = bgfx::createUniform("s_texSsao", bgfx::UniformType::Sampler);

		m_programMesh  = loadProgram("vs_pbr_mesh",   "fs_pbr_mesh");

		m_meshBunny = meshLoad("meshes/bunny.bin");
		m_meshOrb = meshLoad("meshes/orb.bin");
	}

	virtual int shutdown() override
	{
		meshUnload(m_meshBunny);
		meshUnload(m_meshOrb);

		// Cleanup.
		bgfx::destroy(m_programMesh);

		bgfx::destroy(s_texIblDFG);
		bgfx::destroy(s_texIblSpecular);
		bgfx::destroy(s_texSsao);

		for (uint8_t ii = 0; ii < LightProbe::Count; ++ii)
		{
			m_lightProbes[ii].destroy();
		}

		m_uniforms.destroy();

		imguiDestroy();

		// Shutdown bgfx.
		bgfx::shutdown();

		return 0;
	}

	bool update() override
	{
		if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState) )
		{
			imguiBeginFrame(m_mouseState.m_mx
				,  m_mouseState.m_my
				, (m_mouseState.m_buttons[entry::MouseButton::Left  ] ? IMGUI_MBUT_LEFT   : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Right ] ? IMGUI_MBUT_RIGHT  : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
				,  m_mouseState.m_mz
				, uint16_t(m_width)
				, uint16_t(m_height)
				);

			showExampleDialog(this);

			ImGui::SetNextWindowPos(
				  ImVec2(m_width - m_width / 4.5f - 10.0f, 10.0f)
				, ImGuiCond_FirstUseEver
				);
			ImGui::SetNextWindowSize(
				  ImVec2(m_width / 4.5f, m_height - 20.0f)
				, ImGuiCond_FirstUseEver
				);
			ImGui::Begin("Settings"
				, NULL
				, 0
				);
			ImGui::PushItemWidth(180.0f);

			ImGui::Text("Environment light:");
			ImGui::Indent();

			if (ImGui::BeginTabBar("Cubemap", ImGuiTabBarFlags_None) )
			{
				if (ImGui::BeginTabItem("TeufelsbergLookout") )
				{
					m_currentLightProbe = LightProbe::TeufelsbergLookout;
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("CapeHill") )
				{
					m_currentLightProbe = LightProbe::CapeHill;
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("SmallHangar"))
				{
					m_currentLightProbe = LightProbe::SmallHangar;
					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}

			ImGui::SliderFloat("Luminance", &m_settings.m_iblLuminance, 0.0f, 100000.0f, "%.2f", 2.0f);
			
			ImGui::Unindent();

			ImGui::Separator();
			ImGui::Text("Directional light:");
			ImGui::Indent();
			
			ImGui::SliderFloat("Elevation", &m_settings.m_lightElevation, 0.0f, 360.0f);
			ImGui::SliderFloat("Azimuth", &m_settings.m_lightAzimuth, 0.0f, 90.0f);
			ImGui::ColorEdit3("Color", m_settings.m_lightColor);
			ImGui::SliderFloat("Intensity", &m_settings.m_lightIntensity, 0.0f, 100000.0f, "%.2f", 2.0f);
			
			ImGui::SliderFloat("Radius", &m_settings.m_sunRadius, 0.0f, 100.0f);
			ImGui::SliderFloat("Halo Size", &m_settings.m_sunHaloSize, 0.0f, 100.0f);
			ImGui::SliderFloat("Halo Falloff", &m_settings.m_sunHaloFalloff, 0.0f, 100.0f);

			ImGui::Unindent();

			ImGui::Separator();
			ImGui::Text("Camera:");
			ImGui::Indent();
			
			
			ImGui::SliderFloat("Aperture",& m_settings.m_cameraAperture, MIN_APERTURE, MAX_APERTURE);
			ImGui::SliderFloat("Shutter Speed",& m_settings.m_cameraShutterSpeed, MIN_SHUTTER_SPEED, MAX_SHUTTER_SPEED, "%.3f", 2.0f);
			ImGui::SliderFloat("Sensitivity",& m_settings.m_cameraSensitivity, MIN_SENSITIVITY, MAX_SENSITIVITY, "%.3f", 2.0f);
			ImGui::Unindent();
			
			ImGui::PopItemWidth();
			ImGui::End();

			ImGui::SetNextWindowPos(
				  ImVec2(10.0f, 260.0f)
				, ImGuiCond_FirstUseEver
				);
			ImGui::SetNextWindowSize(
				  ImVec2(m_width / 5.0f, 450.0f)
				, ImGuiCond_FirstUseEver
				);
			ImGui::Begin("Mesh"
				, NULL
				, 0
				);

			ImGui::Text("Mesh:");
			ImGui::Indent();
			ImGui::RadioButton("Bunny", &m_settings.m_meshSelection, 0);
			ImGui::RadioButton("Orbs",  &m_settings.m_meshSelection, 1);
			ImGui::Unindent();

			const bool isBunny = (0 == m_settings.m_meshSelection);
			if (!isBunny)
			{
				//m_settings.m_metalOrSpec = 0;
			}
			else
			{
				ImGui::Separator();
				ImGui::Text("Material:");
				ImGui::Indent();
				ImGui::PushItemWidth(130.0f);
				
				ImGui::ColorEdit4("Base Color", m_settings.m_baseColor);
				ImGui::SliderFloat("Roughness", &m_settings.m_roughness, 0.0f, 1.0f );
				ImGui::SliderFloat("Metallic", &m_settings.m_metallic, 0.0f, 1.0f );
				ImGui::SliderFloat("Reflectance", &m_settings.m_reflectance, 0.0f, 1.0f );
				ImGui::ColorEdit4("Emissive", m_settings.m_emissive);
				ImGui::SliderFloat("Clear Coat", &m_settings.m_clearCoat, 0.0f, 1.0f );
				ImGui::SliderFloat("Clear Coat Roughness", &m_settings.m_clearCoatRoughness, 0.0f, 1.0f );
				ImGui::SliderFloat("Anisotropy", &m_settings.m_anisotropy, -1.0f, 1.0f );
				ImGui::SliderFloat("Anisotropy X", &m_settings.m_anisotropyDirection[0], -1.0f, 1.0f );
				ImGui::SliderFloat("Anisotropy Y", &m_settings.m_anisotropyDirection[1], -1.0f, 1.0f );
				ImGui::SliderFloat("Anisotropy Z", &m_settings.m_anisotropyDirection[2], -1.0f, 1.0f );
				ImGui::SliderFloat("Thickness", &m_settings.m_thickness, 0.0f, 1.0f );
				ImGui::ColorEdit3("Subsurface Color", m_settings.m_subsurfaceColor);
				ImGui::SliderFloat("Subsurface Power", &m_settings.m_subsurfacePower, 0.0f, 1.0f );
				ImGui::ColorEdit3("Sheen Color", m_settings.m_sheenColor);
				ImGui::ColorEdit3("Specular Color", m_settings.m_specularColor);
				ImGui::SliderFloat("Glossiness", &m_settings.m_glossiness, 0.0f, 1.0f );

				ImGui::SliderFloat("Specular AntiAliasing Variance", &m_settings.m_specularAntiAliasingVariance, 0.0f, 1.0f );
				ImGui::SliderFloat("Specular AntiAliasing Threshold", &m_settings.m_specularAntiAliasingThreshold, 0.0f, 1.0f );
				ImGui::Checkbox("Double Sided", &m_settings.m_doubleSided);
				
				ImGui::PopItemWidth();
				ImGui::Unindent();
			}

			ImGui::End();

			imguiEndFrame();
			
			float ev100 = bx::log2((m_settings.m_cameraAperture * m_settings.m_cameraAperture) / m_settings.m_cameraShutterSpeed * 100.0 / m_settings.m_cameraSensitivity);
			float exposure = 1.0 / (bx::pow(2.0, ev100) * 1.2);
			
			for(uint32_t ii=0; ii<Uniforms::FrameNumVec4*4; ++ii)
				m_uniforms.m_frameParams[ii] = 0.0f;
			
			m_uniforms.m_lightColorIntensity[0] = m_settings.m_lightColor[0];
			m_uniforms.m_lightColorIntensity[1] = m_settings.m_lightColor[1];
			m_uniforms.m_lightColorIntensity[2] = m_settings.m_lightColor[2];
			m_uniforms.m_lightColorIntensity[3] = m_settings.m_lightIntensity * exposure;

			m_uniforms.m_sun[0] = bx::cos(m_settings.m_sunRadius);
			m_uniforms.m_sun[1] = bx::sin(m_settings.m_sunRadius);
			m_uniforms.m_sun[2] = 1.0f / (bx::cos(m_settings.m_sunRadius * m_settings.m_sunHaloSize) - bx::cos(m_settings.m_sunRadius));
			m_uniforms.m_sun[3] = m_settings.m_sunHaloFalloff;

			float el = m_settings.m_lightElevation * (bx::kPi/180.0f);
			float az = m_settings.m_lightAzimuth   * (bx::kPi/180.0f);
			m_uniforms.m_lightDirection[0] = bx::cos(el)*bx::cos(az);
			m_uniforms.m_lightDirection[2] = bx::cos(el)*bx::sin(az);
			m_uniforms.m_lightDirection[1] = bx::sin(el);
			
			m_uniforms.m_iblLuminance = m_settings.m_iblLuminance * exposure;
			m_uniforms.m_exposure = exposure;
			m_uniforms.m_ev100 = ev100;
			m_uniforms.m_iblMaxMipLevel[0] = m_lightProbes[m_currentLightProbe].m_texNumMips;
			m_uniforms.m_iblMaxMipLevel[1] = 1 << m_lightProbes[m_currentLightProbe].m_texNumMips;
			
			bx::memCopy(m_uniforms.m_iblSH, m_lightProbes[m_currentLightProbe].m_sh, sizeof(m_lightProbes[m_currentLightProbe].m_sh));

			for(uint32_t ii=0; ii<Uniforms::ObjectNumVec4*4; ++ii)
				m_uniforms.m_objectParams[ii] = 0.0f;
			
			m_uniforms.m_specularAntiAliasingVariance = m_settings.m_specularAntiAliasingVariance;
			m_uniforms.m_specularAntiAliasingThreshold = m_settings.m_specularAntiAliasingThreshold;
			m_uniforms.m_doubleSided = m_settings.m_doubleSided ? 1.0f : 0.0f;

			for(uint32_t ii=0; ii<Uniforms::MaterialNumVec4*4; ++ii)
				m_uniforms.m_materialParams[ii] = 1.0f;

			//todo: convert colors from srgb to linear
			bx::memCopy(m_uniforms.m_baseColor, m_settings.m_baseColor, 4*sizeof(float));
			m_uniforms.m_roughness = m_settings.m_roughness;
			m_uniforms.m_metallic = m_settings.m_metallic;
			m_uniforms.m_reflectance = m_settings.m_reflectance;
			bx::memCopy(m_uniforms.m_emissive, m_settings.m_emissive, 4*sizeof(float));
			m_uniforms.m_clearCoat = m_settings.m_clearCoat;
			m_uniforms.m_clearCoatRoughness = m_settings.m_clearCoatRoughness;
			m_uniforms.m_anisotropy = m_settings.m_anisotropy;
			bx::memCopy(m_uniforms.m_anisotropyDirection, m_settings.m_anisotropyDirection, 3*sizeof(float));
			m_uniforms.m_thickness = m_settings.m_thickness;
			bx::memCopy(m_uniforms.m_subsurfaceColor, m_settings.m_subsurfaceColor, 3*sizeof(float));
			m_uniforms.m_subsurfacePower = m_settings.m_subsurfacePower;
			bx::memCopy(m_uniforms.m_sheenColor, m_settings.m_sheenColor, 3*sizeof(float));
			bx::memCopy(m_uniforms.m_specularColor, m_settings.m_specularColor, 3*sizeof(float));
			m_uniforms.m_glossiness = m_settings.m_glossiness;
			
			int64_t now = bx::getHPCounter();
			static int64_t last = now;
			const int64_t frameTime = now - last;
			last = now;
			const double freq = double(bx::getHPFrequency() );
			const float deltaTimeSec = float(double(frameTime)/freq);

			// Camera.
			const bool mouseOverGui = ImGui::MouseOverArea();
			m_mouse.update(float(m_mouseState.m_mx), float(m_mouseState.m_my), m_mouseState.m_mz, m_width, m_height);
			if (!mouseOverGui)
			{
				if (m_mouseState.m_buttons[entry::MouseButton::Left])
				{
					m_camera.orbit(m_mouse.m_dx, m_mouse.m_dy);
				}
				else if (m_mouseState.m_buttons[entry::MouseButton::Right])
				{
					m_camera.dolly(m_mouse.m_dx + m_mouse.m_dy);
				}
				else if (0 != m_mouse.m_scroll)
				{
					m_camera.dolly(float(m_mouse.m_scroll)*0.05f);
				}
			}
			m_camera.update(deltaTimeSec);
			
			bx::memCopy(m_uniforms.m_cameraPosition, &m_camera.m_pos.curr.x, 3*sizeof(float) );

			const bgfx::Caps* caps = bgfx::getCaps();

			// View Transform
			float view[16];
			m_camera.mtxLookAt(view);
			float proj[16];
			bx::mtxProj(proj, 45.0f, float(m_width)/float(m_height), 0.1f, 100.0f, caps->homogeneousDepth);
			bgfx::setViewTransform(0, view, proj);

			// View rect.
			bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height) );

			if (0 == m_settings.m_meshSelection)
			{
				// Submit bunny.
				float mtx[16];
				bx::mtxSRT(mtx, 1.0f, 1.0f, 1.0f, 0.0f, bx::kPi, 0.0f, 0.0f, -0.80f, 0.0f);
				bgfx::setTexture(3, s_texIblDFG, m_texIblDFG, BGFX_SAMPLER_UVW_CLAMP);
				bgfx::setTexture(4, s_texIblSpecular, m_lightProbes[m_currentLightProbe].m_tex, BGFX_SAMPLER_UVW_CLAMP);
				bgfx::setTexture(5, s_texSsao, m_texSsao);
				
				float mtxCof[4*4];
				bx::mtxCofactor(mtxCof, mtx);
				bx::memCopy(m_uniforms.m_worldFromModelNormalMatrix, mtxCof, 3*4*sizeof(float));

				m_uniforms.submit();
				meshSubmit(m_meshBunny, 0, m_programMesh, mtx);
			}
			else
			{
				// Submit orbs.
				for (float yy = 0, yend = 5.0f; yy < yend; yy+=1.0f)
				{
					for (float xx = 0, xend = 5.0f; xx < xend; xx+=1.0f)
					{
						const float scale   =  1.2f;
						const float spacing =  2.2f;
						const float yAdj    = -0.8f;

						float mtx[16];
						bx::mtxSRT(mtx
							, scale/xend
							, scale/xend
							, scale/xend
							, 0.0f
							, 0.0f
							, 0.0f
							, 0.0f      + (xx/xend)*spacing - (1.0f + (scale-1.0f)*0.5f - 1.0f/xend)
							, yAdj/yend + (yy/yend)*spacing - (1.0f + (scale-1.0f)*0.5f - 1.0f/yend)
							, 0.0f
							);

						m_uniforms.m_roughness   =        xx*(1.0f/xend);
						m_uniforms.m_reflectance = (yend-yy)*(1.0f/yend);

						float mtxCof[4 * 4];
						bx::mtxCofactor(mtxCof, mtx);
						bx::memCopy(m_uniforms.m_worldFromModelNormalMatrix, mtxCof, 3 * 4 * sizeof(float));

						m_uniforms.submit();

						bgfx::setTexture(3, s_texIblDFG, m_texIblDFG);
						bgfx::setTexture(4, s_texIblSpecular, m_lightProbes[m_currentLightProbe].m_tex);
						bgfx::setTexture(5, s_texSsao, m_texSsao);

						meshSubmit(m_meshOrb, 0, m_programMesh, mtx);
					}
				}
			}

			// Advance to next frame. Rendering thread will be kicked to
			// process submitted rendering primitives.
			bgfx::frame();

			return true;
		}

		return false;
	}

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_debug;
	uint32_t m_reset;
	entry::MouseState m_mouseState;

	Uniforms m_uniforms;

	LightProbe m_lightProbes[LightProbe::Count];
	LightProbe::Enum m_currentLightProbe;

	bgfx::TextureHandle m_texIblDFG;
	bgfx::TextureHandle m_texSsao;

	bgfx::UniformHandle s_texIblDFG;
	bgfx::UniformHandle s_texIblSpecular;
	bgfx::UniformHandle s_texSsao;

	bgfx::ProgramHandle m_programMesh;

	Mesh* m_meshBunny;
	Mesh* m_meshOrb;
	Camera m_camera;
	Mouse m_mouse;

	Settings m_settings;
};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	  ExamplePbr
	, "4x-pbr"
	, "Physically Based Rendering."
	, "https://bkaradzic.github.io/bgfx/examples.html#pbr"
	);
