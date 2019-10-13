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
		m_tex = loadTexture(filePath, BGFX_SAMPLER_U_CLAMP|BGFX_SAMPLER_V_CLAMP|BGFX_SAMPLER_W_CLAMP);

		bx::memCopy(m_sh, _sh, sizeof(m_sh));
	}

	void destroy()
	{
		bgfx::destroy(m_tex);
	}

	bgfx::TextureHandle m_tex;
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

struct Settings
{
	Settings()
	{
		m_envRotCurr = 0.0f;
		m_envRotDest = 0.0f;
		m_lightDir[0] = -0.8f;
		m_lightDir[1] = 0.2f;
		m_lightDir[2] = -0.5f;
		m_lightCol[0] = 1.0f;
		m_lightCol[1] = 1.0f;
		m_lightCol[2] = 1.0f;
		m_glossiness = 0.7f;
		m_exposure = 0.0f;
		m_bgType = 3.0f;
		m_radianceSlider = 2.0f;
		m_reflectivity = 0.85f;
		m_rgbDiff[0] = 1.0f;
		m_rgbDiff[1] = 1.0f;
		m_rgbDiff[2] = 1.0f;
		m_rgbSpec[0] = 1.0f;
		m_rgbSpec[1] = 1.0f;
		m_rgbSpec[2] = 1.0f;
		m_lod = 0.0f;
		m_doDiffuse = false;
		m_doSpecular = false;
		m_doDiffuseIbl = true;
		m_doSpecularIbl = true;
		m_showLightColorWheel = true;
		m_showDiffColorWheel = true;
		m_showSpecColorWheel = true;
		m_metalOrSpec = 0;
		m_meshSelection = 0;
	}

	float m_envRotCurr;
	float m_envRotDest;
	float m_lightDir[3];
	float m_lightCol[3];
	float m_glossiness;
	float m_exposure;
	float m_radianceSlider;
	float m_bgType;
	float m_reflectivity;
	float m_rgbDiff[3];
	float m_rgbSpec[3];
	float m_lod;
	bool  m_doDiffuse;
	bool  m_doSpecular;
	bool  m_doDiffuseIbl;
	bool  m_doSpecularIbl;
	bool  m_showLightColorWheel;
	bool  m_showDiffColorWheel;
	bool  m_showSpecColorWheel;
	int32_t m_metalOrSpec;
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
				  ImVec2(m_width - m_width / 5.0f - 10.0f, 10.0f)
				, ImGuiCond_FirstUseEver
				);
			ImGui::SetNextWindowSize(
				  ImVec2(m_width / 5.0f, m_height - 20.0f)
				, ImGuiCond_FirstUseEver
				);
			ImGui::Begin("Settings"
				, NULL
				, 0
				);
			ImGui::PushItemWidth(180.0f);

			ImGui::Text("Environment light:");
			ImGui::Indent();
			ImGui::Checkbox("IBL Diffuse",  &m_settings.m_doDiffuseIbl);
			ImGui::Checkbox("IBL Specular", &m_settings.m_doSpecularIbl);

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

			ImGui::SliderFloat("Texture LOD", &m_settings.m_lod, 0.0f, 10.1f);
			ImGui::Unindent();

			ImGui::Separator();
			ImGui::Text("Directional light:");
			ImGui::Indent();
			ImGui::Checkbox("Diffuse",  &m_settings.m_doDiffuse);
			ImGui::Checkbox("Specular", &m_settings.m_doSpecular);
			const bool doDirectLighting = m_settings.m_doDiffuse || m_settings.m_doSpecular;
			if (doDirectLighting)
			{
				ImGui::SliderFloat("Light direction X", &m_settings.m_lightDir[0], -1.0f, 1.0f);
				ImGui::SliderFloat("Light direction Y", &m_settings.m_lightDir[1], -1.0f, 1.0f);
				ImGui::SliderFloat("Light direction Z", &m_settings.m_lightDir[2], -1.0f, 1.0f);
				ImGui::ColorWheel("Color:", m_settings.m_lightCol, 0.6f);
			}
			ImGui::Unindent();

			ImGui::Separator();
			ImGui::Text("Post processing:");
			ImGui::Indent();
			ImGui::SliderFloat("Exposure",& m_settings.m_exposure, -4.0f, 4.0f);
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
				m_settings.m_metalOrSpec = 0;
			}
			else
			{
				ImGui::Separator();
				ImGui::Text("Workflow:");
				ImGui::Indent();
				ImGui::RadioButton("Metalness", &m_settings.m_metalOrSpec, 0);
				ImGui::RadioButton("Specular", &m_settings.m_metalOrSpec, 1);
				ImGui::Unindent();

				ImGui::Separator();
				ImGui::Text("Material:");
				ImGui::Indent();
				ImGui::PushItemWidth(130.0f);
				ImGui::SliderFloat("Glossiness", &m_settings.m_glossiness, 0.0f, 1.0f);
				ImGui::SliderFloat(0 == m_settings.m_metalOrSpec ? "Metalness" : "Diffuse - Specular", &m_settings.m_reflectivity, 0.0f, 1.0f);
				ImGui::PopItemWidth();
				ImGui::Unindent();
			}


			ImGui::ColorWheel("Diffuse:", &m_settings.m_rgbDiff[0], 0.7f);
			ImGui::Separator();
			if ( (1 == m_settings.m_metalOrSpec) && isBunny )
			{
				ImGui::ColorWheel("Specular:", &m_settings.m_rgbSpec[0], 0.7f);
			}

			ImGui::End();

			imguiEndFrame();
			
			
			for(uint32_t ii=0; ii<Uniforms::FrameNumVec4*4; ++ii)
				m_uniforms.m_frameParams[ii] = 0.0f;
			
			//xyz - directional light color, .w - light intensity premultiplied with exposure
			m_uniforms.m_lightColorIntensity[0] = 1.0f;
			m_uniforms.m_lightColorIntensity[1] = 1.0f;
			m_uniforms.m_lightColorIntensity[2] = 1.0f;
			m_uniforms.m_lightColorIntensity[3] = 1.0f;
			// area light: cos(radius), sin(radius), 1.0f / (cos(radius * haloSize) - cos(radius)), haloFalloff
			m_uniforms.m_sun[0] = bx::cos(1.0f);
			m_uniforms.m_sun[1] = bx::sin(1.0f);
			m_uniforms.m_sun[2] = 1.0f / (bx::cos(1.0f*1.0f) - bx::cos(1.0f));
			m_uniforms.m_sun[3] = 1.0f;

			m_uniforms.m_lightDirection[0] = 0.0f;
			m_uniforms.m_lightDirection[1] = -1.0f;
			m_uniforms.m_lightDirection[2] = 0.0f;

			m_uniforms.m_iblLuminance = 1.0f;
			m_uniforms.m_exposure = 1.0f;
			m_uniforms.m_ev100 = 1.0f;
			m_uniforms.m_iblMaxMipLevel[0] = 10.0f;
			m_uniforms.m_iblMaxMipLevel[1] = 1.0f;

			for(uint32_t ii=0; ii<Uniforms::ObjectNumVec4*4; ++ii)
				m_uniforms.m_objectParams[ii] = 0.0f;
			
			m_uniforms.m_skinningEnabled = 0.0f;
			m_uniforms.m_morphingEnabled = 0.0f;
	
			m_uniforms.m_specularAntiAliasingVariance = 0.0f;
			m_uniforms.m_specularAntiAliasingThreshold = 0.0f;
			m_uniforms.m_maskThreshold = 0.0f;
			m_uniforms.m_doubleSided = 0.0f;

			for(uint32_t ii=0; ii<Uniforms::MaterialNumVec4*4; ++ii)
				m_uniforms.m_materialParams[ii] = 1.0f;

			float baseColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f};
			bx::memCopy(m_uniforms.m_baseColor, baseColor, 4*sizeof(float));
			m_uniforms.m_roughness = 0.0f;
			m_uniforms.m_metallic = 0.0f;
			m_uniforms.m_reflectance = 0.0f;
			float emissive[4] = { 1.0f, 1.0f, 1.0f, 1.0f};
			bx::memCopy(m_uniforms.m_emissive, emissive, 4*sizeof(float));
			m_uniforms.m_clearCoat = 0.0f;
			m_uniforms.m_clearCoatRoughness = 0.0f;
			m_uniforms.m_anisotropy = 0.0f;
			float anisotropyDirection[3] = { 1.0f, 1.0f, 1.0f};
			bx::memCopy(m_uniforms.m_anisotropyDirection, anisotropyDirection, 3*sizeof(float));
			m_uniforms.m_thickness = 1.0f;
			float subsurfaceColor[3] = { 1.0f, 1.0f, 1.0f};
			bx::memCopy(m_uniforms.m_subsurfaceColor, subsurfaceColor, 3*sizeof(float));
			m_uniforms.m_subsurfacePower = 1.0f;
			float sheenColor[3] = { 1.0f, 1.0f, 1.0f};
			bx::memCopy(m_uniforms.m_sheenColor, sheenColor, 3*sizeof(float));
			float specularColor[3] = { 1.0f, 1.0f, 1.0f};
			bx::memCopy(m_uniforms.m_specularColor, specularColor, 3*sizeof(float));
			m_uniforms.m_glossiness = 1.0f;

#if 0
			m_uniforms.m_glossiness   = m_settings.m_glossiness;
			m_uniforms.m_reflectivity = m_settings.m_reflectivity;
			m_uniforms.m_exposure     = m_settings.m_exposure;
			m_uniforms.m_bgType       = m_settings.m_bgType;
			m_uniforms.m_metalOrSpec   = float(m_settings.m_metalOrSpec);
			m_uniforms.m_doDiffuse     = float(m_settings.m_doDiffuse);
			m_uniforms.m_doSpecular    = float(m_settings.m_doSpecular);
			m_uniforms.m_doDiffuseIbl  = float(m_settings.m_doDiffuseIbl);
			m_uniforms.m_doSpecularIbl = float(m_settings.m_doSpecularIbl);
			bx::memCopy(m_uniforms.m_rgbDiff,  m_settings.m_rgbDiff,  3*sizeof(float) );
			bx::memCopy(m_uniforms.m_rgbSpec,  m_settings.m_rgbSpec,  3*sizeof(float) );
			bx::memCopy(m_uniforms.m_lightDir, m_settings.m_lightDir, 3*sizeof(float) );
			bx::memCopy(m_uniforms.m_lightCol, m_settings.m_lightCol, 3*sizeof(float) );
#endif
			
			bx::memCopy(m_uniforms.m_iblSH, m_lightProbes[m_currentLightProbe].m_sh, sizeof(m_lightProbes[m_currentLightProbe].m_sh));
			
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
				else if (m_mouseState.m_buttons[entry::MouseButton::Middle])
				{
					m_settings.m_envRotDest += m_mouse.m_dx*2.0f;
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

			// Env rotation.
			const float amount = bx::min(deltaTimeSec/0.12f, 1.0f);
			m_settings.m_envRotCurr = bx::lerp(m_settings.m_envRotCurr, m_settings.m_envRotDest, amount);

			if (0 == m_settings.m_meshSelection)
			{
				// Submit bunny.
				float mtx[16];
				bx::mtxSRT(mtx, 1.0f, 1.0f, 1.0f, 0.0f, bx::kPi, 0.0f, 0.0f, -0.80f, 0.0f);
				bgfx::setTexture(3, s_texIblDFG, m_texIblDFG);
				bgfx::setTexture(4, s_texIblSpecular, m_lightProbes[m_currentLightProbe].m_tex);
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

#if 0
						m_uniforms.m_glossiness   =        xx*(1.0f/xend);
						m_uniforms.m_reflectivity = (yend-yy)*(1.0f/yend);
						m_uniforms.m_metalOrSpec = 0.0f;
#endif
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
