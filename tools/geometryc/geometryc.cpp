/*
 * Copyright 2011-2019 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include <algorithm>

#include <bx/string.h>
#include <bgfx/bgfx.h>
#include "../../src/vertexdecl.h"

#include <tinystl/allocator.h>
#include <tinystl/string.h>
#include <tinystl/unordered_map.h>
#include <tinystl/unordered_set.h>
#include <tinystl/vector.h>
namespace stl = tinystl;

#include <meshoptimizer/src/meshoptimizer.h>

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

#define BGFX_GEOMETRYC_VERSION_MAJOR 1
#define BGFX_GEOMETRYC_VERSION_MINOR 0

#if 0
#	define BX_TRACE(_format, ...) \
		do { \
			bx::printf(BX_FILE_LINE_LITERAL "BGFX " _format "\n", ##__VA_ARGS__); \
		} while(0)

#	define BX_WARN(_condition, _format, ...) \
		do { \
			if (!(_condition) ) \
			{ \
				BX_TRACE(BX_FILE_LINE_LITERAL "WARN " _format, ##__VA_ARGS__); \
			} \
		} while(0)

#	define BX_CHECK(_condition, _format, ...) \
		do { \
			if (!(_condition) ) \
			{ \
				BX_TRACE(BX_FILE_LINE_LITERAL "CHECK " _format, ##__VA_ARGS__); \
				bx::debugBreak(); \
			} \
		} while(0)
#endif // 0

#include <bx/bx.h>
#include <bx/debug.h>
#include <bx/commandline.h>
#include <bx/timer.h>
#include <bx/hash.h>
#include <bx/uint32_t.h>
#include <bx/math.h>
#include <bx/file.h>

#include "bounds.h"

typedef stl::vector<bx::Vec3> Vec3Array;

struct Index3
{
	int32_t m_position;
	int32_t m_texcoord;
	int32_t m_normal;
	int32_t m_vbc; // Barycentric ID. Holds eigher 0, 1 or 2.
};

struct TriIndices
{
	Index3 m_index[3];
};

typedef stl::vector<TriIndices> TriangleArray;

struct Group
{
	uint32_t m_startTriangle;
	uint32_t m_numTriangles;
	stl::string m_name;
	stl::string m_material;
};

typedef stl::vector<Group> GroupArray;

struct Primitive
{
	uint32_t m_startVertex;
	uint32_t m_startIndex;
	uint32_t m_numVertices;
	uint32_t m_numIndices;
	stl::string m_name;
};

typedef stl::vector<Primitive> PrimitiveArray;

static uint32_t s_obbSteps = 17;

#define BGFX_CHUNK_MAGIC_VB  BX_MAKEFOURCC('V', 'B', ' ', 0x1)
#define BGFX_CHUNK_MAGIC_VBC BX_MAKEFOURCC('V', 'B', 'C', 0x0)
#define BGFX_CHUNK_MAGIC_IB  BX_MAKEFOURCC('I', 'B', ' ', 0x0)
#define BGFX_CHUNK_MAGIC_IBC BX_MAKEFOURCC('I', 'B', 'C', 0x1)
#define BGFX_CHUNK_MAGIC_PRI BX_MAKEFOURCC('P', 'R', 'I', 0x0)

void optimizeVertexCache(uint16_t* _indices, uint32_t _numIndices, uint32_t _numVertices)
{
	uint16_t* newIndexList = new uint16_t[_numIndices];
	meshopt_optimizeVertexCache(newIndexList, _indices, _numIndices, _numVertices);
	bx::memCopy(_indices, newIndexList, _numIndices * 2);
	delete[] newIndexList;
}

uint32_t optimizeVertexFetch(uint16_t* _indices, uint32_t _numIndices, uint8_t* _vertexData, uint32_t _numVertices, uint16_t _stride)
{
	unsigned char* newVertices = (unsigned char*)malloc(_numVertices * _stride );
	size_t vertexCount = meshopt_optimizeVertexFetch(newVertices, _indices, _numIndices, _vertexData, _numVertices, _stride);
	bx::memCopy(_vertexData, newVertices, _numVertices * _stride);
	free(newVertices);

	return uint32_t(vertexCount);
}

void writeCompressedIndices(bx::WriterI* _writer, const uint16_t* _indices, uint32_t _numIndices, uint32_t _numVertices)
{
	size_t maxSize = meshopt_encodeIndexBufferBound(_numIndices, _numVertices);
	unsigned char* compressedIndices = (unsigned char*)malloc(maxSize);
	size_t compressedSize = meshopt_encodeIndexBuffer(compressedIndices, maxSize, _indices, _numIndices);
	bx::printf( "indices uncompressed: %10d, compressed: %10d, ratio: %0.2f%%\n"
		, _numIndices*2
		, (uint32_t)compressedSize
		, 100.0f - float(compressedSize ) / float(_numIndices*2)*100.0f
		);

	bx::write(_writer, (uint32_t)compressedSize);
	bx::write(_writer, compressedIndices, (uint32_t)compressedSize );
	free(compressedIndices);
}

void writeCompressedVertices(bx::WriterI* _writer,  const uint8_t* _vertices, uint32_t _numVertices, uint16_t _stride)
{
	size_t maxSize = meshopt_encodeVertexBufferBound(_numVertices, _stride);
	unsigned char* compressedVertices = (unsigned char*)malloc(maxSize);
	size_t compressedSize = meshopt_encodeVertexBuffer(compressedVertices, maxSize, _vertices, _numVertices, _stride);
	bx::printf("vertices uncompressed: %10d, compressed: %10d, ratio: %0.2f%%\n"
		, _numVertices * _stride
		, (uint32_t)compressedSize
		, 100.0f - float(compressedSize) / float(_numVertices * _stride)*100.0f
		);

	bx::write(_writer, (uint32_t)compressedSize);
	bx::write(_writer, compressedVertices, (uint32_t)compressedSize );
	free(compressedVertices);
}

void calcTangents(void* _vertices, uint16_t _numVertices, bgfx::VertexDecl _decl, const uint16_t* _indices, uint32_t _numIndices)
{
	struct PosTexcoord
	{
		float m_x;
		float m_y;
		float m_z;
		float m_pad0;
		float m_u;
		float m_v;
		float m_pad1;
		float m_pad2;
	};

	float* tangents = new float[6*_numVertices];
	bx::memSet(tangents, 0, 6*_numVertices*sizeof(float) );

	PosTexcoord v0;
	PosTexcoord v1;
	PosTexcoord v2;

	for (uint32_t ii = 0, num = _numIndices/3; ii < num; ++ii)
	{
		const uint16_t* indices = &_indices[ii*3];
		uint32_t i0 = indices[0];
		uint32_t i1 = indices[1];
		uint32_t i2 = indices[2];

		bgfx::vertexUnpack(&v0.m_x, bgfx::Attrib::Position, _decl, _vertices, i0);
		bgfx::vertexUnpack(&v0.m_u, bgfx::Attrib::TexCoord0, _decl, _vertices, i0);

		bgfx::vertexUnpack(&v1.m_x, bgfx::Attrib::Position, _decl, _vertices, i1);
		bgfx::vertexUnpack(&v1.m_u, bgfx::Attrib::TexCoord0, _decl, _vertices, i1);

		bgfx::vertexUnpack(&v2.m_x, bgfx::Attrib::Position, _decl, _vertices, i2);
		bgfx::vertexUnpack(&v2.m_u, bgfx::Attrib::TexCoord0, _decl, _vertices, i2);

		const float bax = v1.m_x - v0.m_x;
		const float bay = v1.m_y - v0.m_y;
		const float baz = v1.m_z - v0.m_z;
		const float bau = v1.m_u - v0.m_u;
		const float bav = v1.m_v - v0.m_v;

		const float cax = v2.m_x - v0.m_x;
		const float cay = v2.m_y - v0.m_y;
		const float caz = v2.m_z - v0.m_z;
		const float cau = v2.m_u - v0.m_u;
		const float cav = v2.m_v - v0.m_v;

		const float det = (bau * cav - bav * cau);
		const float invDet = 1.0f / det;

		const float tx = (bax * cav - cax * bav) * invDet;
		const float ty = (bay * cav - cay * bav) * invDet;
		const float tz = (baz * cav - caz * bav) * invDet;

		const float bx = (cax * bau - bax * cau) * invDet;
		const float by = (cay * bau - bay * cau) * invDet;
		const float bz = (caz * bau - baz * cau) * invDet;

		for (uint32_t jj = 0; jj < 3; ++jj)
		{
			float* tanu = &tangents[indices[jj]*6];
			float* tanv = &tanu[3];
			tanu[0] += tx;
			tanu[1] += ty;
			tanu[2] += tz;

			tanv[0] += bx;
			tanv[1] += by;
			tanv[2] += bz;
		}
	}

	for (uint32_t ii = 0; ii < _numVertices; ++ii)
	{
		const bx::Vec3 tanu = bx::load<bx::Vec3>(&tangents[ii*6]);
		const bx::Vec3 tanv = bx::load<bx::Vec3>(&tangents[ii*6 + 3]);

		float nxyzw[4];
		bgfx::vertexUnpack(nxyzw, bgfx::Attrib::Normal, _decl, _vertices, ii);

		const bx::Vec3 normal  = bx::load<bx::Vec3>(nxyzw);
		const float    ndt     = bx::dot(normal, tanu);
		const bx::Vec3 nxt     = bx::cross(normal, tanu);
		const bx::Vec3 tmp     = bx::sub(tanu, bx::mul(normal, ndt) );

		float tangent[4];
		bx::store(tangent, bx::normalize(tmp) );
		tangent[3] = bx::dot(nxt, tanv) < 0.0f ? -1.0f : 1.0f;

		bgfx::vertexPack(tangent, true, bgfx::Attrib::Tangent, _decl, _vertices, ii);
	}

	delete [] tangents;
}

void write(bx::WriterI* _writer, const void* _vertices, uint32_t _numVertices, uint32_t _stride)
{
	Sphere maxSphere;
	calcMaxBoundingSphere(maxSphere, _vertices, _numVertices, _stride);

	Sphere minSphere;
	calcMinBoundingSphere(minSphere, _vertices, _numVertices, _stride);

	if (minSphere.radius > maxSphere.radius)
	{
		bx::write(_writer, maxSphere);
	}
	else
	{
		bx::write(_writer, minSphere);
	}

	Aabb aabb;
	toAabb(aabb, _vertices, _numVertices, _stride);
	bx::write(_writer, aabb);

	Obb obb;
	calcObb(obb, _vertices, _numVertices, _stride, s_obbSteps);
	bx::write(_writer, obb);
}

void write(bx::WriterI* _writer
		, const uint8_t* _vertices
		, uint32_t _numVertices
		, const bgfx::VertexDecl& _decl
		, const uint16_t* _indices
		, uint32_t _numIndices
		, bool _compress
		, const stl::string& _material
		, const PrimitiveArray& _primitives
		)
{
	using namespace bx;
	using namespace bgfx;

	uint32_t stride = _decl.getStride();

	if (_compress)
	{
		write(_writer, BGFX_CHUNK_MAGIC_VBC);
		write(_writer, _vertices, _numVertices, stride);

		write(_writer, _decl);

		write(_writer, uint16_t(_numVertices) );
		writeCompressedVertices(_writer, _vertices, _numVertices, uint16_t(stride));
	}
	else
	{
		write(_writer, BGFX_CHUNK_MAGIC_VB);
		write(_writer, _vertices, _numVertices, stride);

		write(_writer, _decl);

		write(_writer, uint16_t(_numVertices) );
		write(_writer, _vertices, _numVertices*stride);
	}

	if (_compress)
	{
		write(_writer, BGFX_CHUNK_MAGIC_IBC);
		write(_writer, _numIndices);
		writeCompressedIndices(_writer, _indices, _numIndices, _numVertices);
	}
	else
	{
		write(_writer, BGFX_CHUNK_MAGIC_IB);
		write(_writer, _numIndices);
		write(_writer, _indices, _numIndices*2);
	}

	write(_writer, BGFX_CHUNK_MAGIC_PRI);
	uint16_t nameLen = uint16_t(_material.size() );
	write(_writer, nameLen);
	write(_writer, _material.c_str(), nameLen);
	write(_writer, uint16_t(_primitives.size() ) );
	for (PrimitiveArray::const_iterator primIt = _primitives.begin(); primIt != _primitives.end(); ++primIt)
	{
		const Primitive& prim = *primIt;
		nameLen = uint16_t(prim.m_name.size() );
		write(_writer, nameLen);
		write(_writer, prim.m_name.c_str(), nameLen);
		write(_writer, prim.m_startIndex);
		write(_writer, prim.m_numIndices);
		write(_writer, prim.m_startVertex);
		write(_writer, prim.m_numVertices);
		write(_writer, &_vertices[prim.m_startVertex*stride], prim.m_numVertices, stride);
	}
}

inline uint32_t rgbaToAbgr(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a)
{
	return (uint32_t(_r)<<0)
		 | (uint32_t(_g)<<8)
		 | (uint32_t(_b)<<16)
		 | (uint32_t(_a)<<24)
		 ;
}

struct GroupSortByMaterial
{
	bool operator()(const Group& _lhs, const Group& _rhs)
	{
		return 0 < bx::strCmp(_lhs.m_material.c_str(), _rhs.m_material.c_str() );
	}
};


struct ObjVertices
{
	Vec3Array m_positions;
	Vec3Array m_normals;
	Vec3Array m_texcoords;

	bool m_hasNormal;
	bool m_hasTexcoord;
	bool m_hasColor;
};

void parseObj(char* _data, uint32_t _size, ObjVertices& _vertices,
			TriangleArray& _triangles,
			GroupArray& _groups,
			uint32_t& _num,
			bool _hasBc)
{
	// Reference(s):
	// - Wavefront .obj file
	//   https://en.wikipedia.org/wiki/Wavefront_.obj_file


	Group group;
	group.m_startTriangle = 0;
	group.m_numTriangles = 0;

	char commandLine[2048];
	uint32_t len = sizeof(commandLine);
	int argc;
	char* argv[64];

	for (bx::StringView next(_data, _size); !next.isEmpty(); )
	{
		next = bx::tokenizeCommandLine(next, commandLine, len, argc, argv, BX_COUNTOF(argv), '\n');

		if (0 < argc)
		{
			if (0 == bx::strCmp(argv[0], "#"))
			{
				if (2 < argc
					&& 0 == bx::strCmp(argv[2], "polygons"))
				{
				}
			}
			else if (0 == bx::strCmp(argv[0], "f"))
			{
				TriIndices triangle;
				bx::memSet(&triangle, 0, sizeof(TriIndices));

				const int numNormals = (int)_vertices.m_normals.size();
				const int numTexcoords = (int)_vertices.m_texcoords.size();
				const int numPositions = (int)_vertices.m_positions.size();
				for (uint32_t edge = 0, numEdges = argc - 1; edge < numEdges; ++edge)
				{
					Index3 index;
					index.m_texcoord = -1;
					index.m_normal = -1;
					if (_hasBc)
					{
						index.m_vbc = edge < 3 ? edge : (1 + (edge + 1)) & 1;
					}
					else
					{
						index.m_vbc = 0;
					}

					{
						bx::StringView triplet(argv[edge + 1]);
						bx::StringView vertex(triplet);
						bx::StringView texcoord = bx::strFind(triplet, '/');
						if (!texcoord.isEmpty())
						{
							vertex.set(vertex.getPtr(), texcoord.getPtr());

							const bx::StringView normal = bx::strFind(bx::StringView(texcoord.getPtr() + 1, triplet.getTerm()), '/');
							if (!normal.isEmpty())
							{
								int32_t nn;
								bx::fromString(&nn, bx::StringView(normal.getPtr() + 1, triplet.getTerm()));
								index.m_normal = (nn < 0) ? nn + numNormals : nn - 1;
							}

							texcoord.set(texcoord.getPtr() + 1, normal.getPtr());

							// Reference(s):
							// - Wavefront .obj file / Vertex normal indices without texture coordinate indices
							//   https://en.wikipedia.org/wiki/Wavefront_.obj_file#Vertex_Normal_Indices_Without_Texture_Coordinate_Indices
							if (!texcoord.isEmpty())
							{
								int32_t tex;
								bx::fromString(&tex, texcoord);
								index.m_texcoord = (tex < 0) ? tex + numTexcoords : tex - 1;
							}
						}

						int32_t pos;
						bx::fromString(&pos, vertex);
						index.m_position = (pos < 0) ? pos + numPositions : pos - 1;
					}


					switch (edge)
					{
					case 0:	case 1:	case 2:
						triangle.m_index[edge] = index;
						if (2 == edge)
						{
							_triangles.push_back(triangle);
						}
						break;

					default:
						triangle.m_index[1] = triangle.m_index[2];
						triangle.m_index[2] = index;

						_triangles.push_back(triangle);
						break;
					}
				}
			}
			else if (0 == bx::strCmp(argv[0], "g"))
			{
				group.m_name = argv[1];
			}
			else if (*argv[0] == 'v')
			{
				group.m_numTriangles = (uint32_t)(_triangles.size()) - group.m_startTriangle;
				if (0 < group.m_numTriangles)
				{
					_groups.push_back(group);
					group.m_startTriangle = (uint32_t)(_triangles.size());
					group.m_numTriangles = 0;
				}

				if (0 == bx::strCmp(argv[0], "vn"))
				{
					bx::Vec3 normal;
					bx::fromString(&normal.x, argv[1]);
					bx::fromString(&normal.y, argv[2]);
					bx::fromString(&normal.z, argv[3]);

					_vertices.m_normals.push_back(normal);
				}
				else if (0 == bx::strCmp(argv[0], "vp"))
				{
					static bool once = true;
					if (once)
					{
						once = false;
						bx::printf("warning: 'parameter space vertices' are unsupported.\n");
					}
				}
				else if (0 == bx::strCmp(argv[0], "vt"))
				{
					bx::Vec3 texcoord;
					texcoord.y = 0.0f;
					texcoord.z = 0.0f;

					bx::fromString(&texcoord.x, argv[1]);

					switch (argc)
					{
					case 4:
						bx::fromString(&texcoord.z, argv[3]);
						BX_FALLTHROUGH;

					case 3:
						bx::fromString(&texcoord.y, argv[2]);
						break;

					default:
						break;
					}

					_vertices.m_texcoords.push_back(texcoord);
				}
				else
				{
					float px, py, pz, pw;
					bx::fromString(&px, argv[1]);
					bx::fromString(&py, argv[2]);
					bx::fromString(&pz, argv[3]);

					if (argc == 5 || argc == 8)
					{
						bx::fromString(&pw, argv[4]);
					}
					else
					{
						pw = 1.0f;
					}

					float invW = 1.0f / pw;
					px *= invW;
					py *= invW;
					pz *= invW;

					bx::Vec3 pos;
					pos.x = px;
					pos.y = py;
					pos.z = pz;

					_vertices.m_positions.push_back(pos);
				}
			}
			else if (0 == bx::strCmp(argv[0], "usemtl"))
			{
				stl::string material(argv[1]);

				if (0 != bx::strCmp(material.c_str(), group.m_material.c_str()))
				{
					group.m_numTriangles = (uint32_t)(_triangles.size()) - group.m_startTriangle;
					if (0 < group.m_numTriangles)
					{
						_groups.push_back(group);
						group.m_startTriangle = (uint32_t)(_triangles.size());
						group.m_numTriangles = 0;
					}
				}

				group.m_material = material;
			}
			// unsupported tags
			// 				else if (0 == bx::strCmp(argv[0], "mtllib") )
			// 				{
			// 				}
			// 				else if (0 == bx::strCmp(argv[0], "o") )
			// 				{
			// 				}
			// 				else if (0 == bx::strCmp(argv[0], "s") )
			// 				{
			// 				}
		}

		++_num;
	}

	group.m_numTriangles = (uint32_t)(_triangles.size()) - group.m_startTriangle;
	if (0 < group.m_numTriangles)
	{
		_groups.push_back(group);
		group.m_startTriangle = (uint32_t)(_triangles.size());
		group.m_numTriangles = 0;
	}

	bool hasNormal;
	bool hasTexcoord;

	{
		TriangleArray::const_iterator it = _triangles.begin();
		hasNormal = -1 != it->m_index[0].m_normal;
		hasTexcoord = -1 != it->m_index[0].m_texcoord;

		if (!hasTexcoord)
		{
			for (TriangleArray::iterator jt = _triangles.begin(), jtEnd = _triangles.end(); jt != jtEnd && !hasTexcoord; ++jt)
			{
				hasTexcoord |= -1 != jt->m_index[0].m_texcoord;
				hasTexcoord |= -1 != jt->m_index[1].m_texcoord;
				hasTexcoord |= -1 != jt->m_index[2].m_texcoord;
			}

			if (hasTexcoord)
			{
				for (TriangleArray::iterator jt = _triangles.begin(), jtEnd = _triangles.end(); jt != jtEnd; ++jt)
				{
					jt->m_index[0].m_texcoord = -1 == jt->m_index[0].m_texcoord ? 0 : jt->m_index[0].m_texcoord;
					jt->m_index[1].m_texcoord = -1 == jt->m_index[1].m_texcoord ? 0 : jt->m_index[1].m_texcoord;
					jt->m_index[2].m_texcoord = -1 == jt->m_index[2].m_texcoord ? 0 : jt->m_index[2].m_texcoord;
				}
			}
		}

		if (!hasNormal)
		{
			for (TriangleArray::iterator jt = _triangles.begin(), jtEnd = _triangles.end(); jt != jtEnd && !hasNormal; ++jt)
			{
				hasNormal |= -1 != jt->m_index[0].m_normal;
				hasNormal |= -1 != jt->m_index[1].m_normal;
				hasNormal |= -1 != jt->m_index[2].m_normal;
			}

			if (hasNormal)
			{
				for (TriangleArray::iterator jt = _triangles.begin(), jtEnd = _triangles.end(); jt != jtEnd; ++jt)
				{
					jt->m_index[0].m_normal = -1 == jt->m_index[0].m_normal ? 0 : jt->m_index[0].m_normal;
					jt->m_index[1].m_normal = -1 == jt->m_index[1].m_normal ? 0 : jt->m_index[1].m_normal;
					jt->m_index[2].m_normal = -1 == jt->m_index[2].m_normal ? 0 : jt->m_index[2].m_normal;
				}
			}
		}
	}

	_vertices.m_hasNormal = hasNormal;
	_vertices.m_hasTexcoord = hasTexcoord;
	_vertices.m_hasColor = false;
}


void objGetVertex(ObjVertices& _vertices, Index3& _index, bgfx::VertexDecl _decl, uint8_t* _vertexData, bool _hasBc, bool _flipV, bool _ccw, float _scale, uint32_t _numVertices, uint32_t _numIndices)
{
	uint32_t positionOffset = _decl.getOffset(bgfx::Attrib::Position);

	float* position = (float*)(_vertexData + positionOffset);
	bx::memCopy(position, &_vertices.m_positions[_index.m_position], 3 * sizeof(float));

	if (_vertices.m_hasColor)
	{
		uint32_t color0Offset = _decl.getOffset(bgfx::Attrib::Color0);

		uint32_t* color0 = (uint32_t*)(_vertexData + color0Offset);
		*color0 = rgbaToAbgr(_numVertices % 255, _numIndices % 255, 0, 0xff);
	}

	if (_hasBc)
	{
		const float bc[3] =
		{
			(_index.m_vbc == 0) ? 1.0f : 0.0f,
			(_index.m_vbc == 1) ? 1.0f : 0.0f,
			(_index.m_vbc == 2) ? 1.0f : 0.0f,
		};
		bgfx::vertexPack(bc, true, bgfx::Attrib::Color1, _decl, _vertexData);
	}

	if (_vertices.m_hasTexcoord)
	{
		float uv[2];
		bx::memCopy(uv, &_vertices.m_texcoords[_index.m_texcoord], 2 * sizeof(float));

		if (_flipV)
		{
			uv[1] = -uv[1];
		}

		bgfx::vertexPack(uv, true, bgfx::Attrib::TexCoord0, _decl, _vertexData);
	}

	if (_vertices.m_hasNormal)
	{
		float normal[4] = {};
		bx::store(normal, bx::normalize(bx::load<bx::Vec3>(&_vertices.m_normals[_index.m_normal])));
		bgfx::vertexPack(normal, true, bgfx::Attrib::Normal, _decl, _vertexData);
	}
}

// non-indexed geometry support  ?
// points/lines (list/fan/strip) ?
// merging vertices after compression ( can result in smaller number of vertices )

void parseGltf(char* _data, uint32_t _size, ObjVertices& _vertices,
	TriangleArray& _triangles,
	GroupArray& _groups,
	uint32_t& _num,
	bool _hasBc)
{
	cgltf_options options = {  };
	cgltf_data* data = NULL;
	cgltf_result result = cgltf_parse(&options, _data, _size, &data);

	if (result == cgltf_result_success)
	{
		result = cgltf_load_buffers(&options, data, "");

		if (result == cgltf_result_success)
		{
			for (cgltf_size sceneIndex = 0; sceneIndex < data->scenes_count; ++sceneIndex)
			{
				cgltf_scene* scene = &data->scenes[sceneIndex];

				for (cgltf_size nodeIndex = 0; nodeIndex < scene->nodes_count; ++nodeIndex)
				{
					cgltf_node* node = &data->nodes[nodeIndex];

					cgltf_mesh* mesh = node->mesh;
					if (NULL != mesh)
					{
						for (cgltf_size primitiveIndex = 0; primitiveIndex < mesh->primitives_count; ++primitiveIndex)
						{
							cgltf_primitive* primitive = &mesh->primitives[primitiveIndex];

							for (cgltf_size attributeIndex = 0; attributeIndex < primitive->attributes_count; ++attributeIndex)
							{
								cgltf_attribute* attribute = &primitive->attributes[attributeIndex];
								cgltf_accessor* accessor = attribute->data;

								if (attribute->type == cgltf_attribute_type_position && attribute->index == 0)
								{
									float pos[3];
									bool success = cgltf_accessor_read_float(accessor, 0, pos, 3);
									success = cgltf_accessor_read_float(accessor, 1, pos, 3);
									success = cgltf_accessor_read_float(accessor, 2, pos, 3);
									int a = 1;
								}
							}
						}
					}
				}
			}
		}
		
		cgltf_free(data);
	}
}


void help(const char* _error = NULL)
{
	if (NULL != _error)
	{
		bx::printf("Error:\n%s\n\n", _error);
	}

	bx::printf(
		"geometryc, bgfx geometry compiler tool, version %d.%d.%d.\n"
		"Copyright 2011-2019 Branimir Karadzic. All rights reserved.\n"
		"License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause\n\n"
		, BGFX_GEOMETRYC_VERSION_MAJOR
		, BGFX_GEOMETRYC_VERSION_MINOR
		, BGFX_API_VERSION
	);

	bx::printf(
		"Usage: geometryc -f <in> -o <out>\n"

		"\n"
		"Supported input file types:\n"
		"    *.obj                  Wavefront\n"
		"    *.gltf,*.glb           glTF 2.0\n"

		"\n"
		"Options:\n"
		"  -h, --help               Help.\n"
		"  -v, --version            Version information only.\n"
		"  -f <file path>           Input file path.\n"
		"  -o <file path>           Output file path.\n"
		"  -s, --scale <num>        Scale factor.\n"
		"      --ccw                Counter-clockwise winding order.\n"
		"      --flipv              Flip texture coordinate V.\n"
		"      --obb <num>          Number of steps for calculating oriented bounding box.\n"
		"           Default value is 17. Less steps less precise OBB is.\n"
		"           More steps slower calculation.\n"
		"      --packnormal <num>   Normal packing.\n"
		"           0 - unpacked 12 bytes (default).\n"
		"           1 - packed 4 bytes.\n"
		"      --packuv <num>       Texture coordinate packing.\n"
		"           0 - unpacked 8 bytes (default).\n"
		"           1 - packed 4 bytes.\n"
		"      --tangent            Calculate tangent vectors (packing mode is the same as normal).\n"
		"      --barycentric        Adds barycentric vertex attribute (packed in bgfx::Attrib::Color1).\n"
		"  -c, --compress           Compress indices.\n"

		"\n"
		"For additional information, see https://github.com/bkaradzic/bgfx\n"
	);
}

int main(int _argc, const char* _argv[])
{
	bx::CommandLine cmdLine(_argc, _argv);

	if (cmdLine.hasArg('v', "version") )
	{
		bx::printf(
			  "geometryc, bgfx geometry compiler tool, version %d.%d.%d.\n"
			, BGFX_GEOMETRYC_VERSION_MAJOR
			, BGFX_GEOMETRYC_VERSION_MINOR
			, BGFX_API_VERSION
			);
		return bx::kExitSuccess;
	}

	if (cmdLine.hasArg('h', "help") )
	{
		help();
		return bx::kExitFailure;
	}

	const char* filePath = cmdLine.findOption('f');
	if (NULL == filePath)
	{
		help("Input file name must be specified.");
		return bx::kExitFailure;
	}

	const char* outFilePath = cmdLine.findOption('o');
	if (NULL == outFilePath)
	{
		help("Output file name must be specified.");
		return bx::kExitFailure;
	}

	float scale = 1.0f;
	const char* scaleArg = cmdLine.findOption('s', "scale");
	if (NULL != scaleArg)
	{
		if (!bx::fromString(&scale, scaleArg))
		{
			scale = 1.0f;
		}
	}

	bool compress = cmdLine.hasArg('c', "compress");

	cmdLine.hasArg(s_obbSteps, '\0', "obb");
	s_obbSteps = bx::uint32_min(bx::uint32_max(s_obbSteps, 1), 90);

	uint32_t packNormal = 0;
	cmdLine.hasArg(packNormal, '\0', "packnormal");

	uint32_t packUv = 0;
	cmdLine.hasArg(packUv, '\0', "packuv");

	bool ccw = cmdLine.hasArg("ccw");
	bool flipV = cmdLine.hasArg("flipv");
	bool hasTangent = cmdLine.hasArg("tangent");
	bool hasBc = cmdLine.hasArg("barycentric");

	bx::FileReader fr;
	if (!bx::open(&fr, filePath) )
	{
		bx::printf("Unable to open input file '%s'.", filePath);
		exit(bx::kExitFailure);
	}

	int64_t parseElapsed = -bx::getHPCounter();
	int64_t triReorderElapsed = 0;

	uint32_t size = (uint32_t)bx::getSize(&fr);
	char* data = new char[size+1];
	size = bx::read(&fr, data, size);
	data[size] = '\0';
	bx::close(&fr);

	TriangleArray triangles;
	GroupArray groups;
	ObjVertices objVertices;

	uint32_t num = 0;
	bx::StringView ext = bx::FilePath(filePath).getExt();

	if (0 == bx::strCmpI(ext, ".obj"))
	{
		parseObj(data, size, objVertices, triangles, groups, num, hasBc);
	}
	else if (0 == bx::strCmpI(ext, ".gltf") || 0 == bx::strCmpI(ext, ".glb"))
	{
		parseGltf(data, size, objVertices, triangles, groups, num, hasBc);
	}
	else
	{
		bx::printf("Unsupported input file format '%s'.", filePath);
		exit(bx::kExitFailure);
	}

	delete[] data;

	int64_t now = bx::getHPCounter();
	parseElapsed += now;
	int64_t convertElapsed = -now;

	if ( scale != 1.0f )
	{
		for(uint32_t ii = 0, numPositions = objVertices.m_positions.size(); ii < numPositions; ++ii)
		{
			objVertices.m_positions[ii] = bx::mul(objVertices.m_positions[ii], scale);
		}
	}
	
	if (ccw)
	{
		for(uint32_t tri = 0, numTris = triangles.size(); tri<numTris; ++tri)
		{
			bx::swap(triangles[tri].m_index[1], triangles[tri].m_index[2]);
		}
	}

	std::sort(groups.begin(), groups.end(), GroupSortByMaterial() );

	bgfx::VertexDecl decl;
	decl.begin();
	decl.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);

	if (objVertices.m_hasColor)
	{
		decl.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true);
	}

	if (hasBc)
	{
		decl.add(bgfx::Attrib::Color1, 4, bgfx::AttribType::Uint8, true);
	}

	if (objVertices.m_hasTexcoord)
	{
		switch (packUv)
		{
		default:
		case 0:
			decl.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float);
			break;

		case 1:
			decl.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Half);
			break;
		}
	}

	if (objVertices.m_hasNormal)
	{
		hasTangent &= objVertices.m_hasTexcoord;

		switch (packNormal)
		{
		default:
		case 0:
			decl.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float);
			if (hasTangent)
			{
				decl.add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Float);
			}
			break;

		case 1:
			decl.add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true);
			if (hasTangent)
			{
				decl.add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Uint8, true, true);
			}
			break;
		}
	}

	decl.end();

	uint32_t stride = decl.getStride();
	uint8_t* vertexData = new uint8_t[triangles.size() * 3 * stride];
	//todo: remove this debug test only
	Index3* index3Data = new Index3[triangles.size() * 3];

	uint16_t* indexData = new uint16_t[triangles.size() * 3];
	int32_t numVertices = 0;
	int32_t numIndices = 0;

	int32_t writtenPrimitives = 0;
	int32_t writtenVertices = 0;
	int32_t writtenIndices = 0;

	uint8_t* vertices = vertexData;
	uint16_t* indices = indexData;
	Index3* index3s = index3Data;

	stl::string material = groups.begin()->m_material;

	PrimitiveArray primitives;

	bx::FileWriter writer;
	if (!bx::open(&writer, outFilePath) )
	{
		bx::printf("Unable to open output file '%s'.", outFilePath);
		exit(bx::kExitFailure);
	}

	Primitive prim;
	prim.m_startVertex = 0;
	prim.m_startIndex  = 0;

	Group sentinelGroup;
	sentinelGroup.m_startTriangle = 0;
	sentinelGroup.m_numTriangles = UINT32_MAX;
	groups.push_back(sentinelGroup);

	uint32_t tableSize = 65536 * 2;
	size_t hashmod = tableSize - 1;
	uint32_t* table = new uint32_t[65536*2];
	memset(table, 0xff, tableSize * sizeof(unsigned int));

	int extraMerge = 0;

	uint32_t ii = 0;
	for (GroupArray::const_iterator groupIt = groups.begin(); groupIt != groups.end(); ++groupIt, ++ii)
	{
		bool sentinel = groupIt->m_startTriangle == 0 && groupIt->m_numTriangles == UINT32_MAX;
		for (uint32_t tri = groupIt->m_startTriangle, end = tri + groupIt->m_numTriangles; tri < end; ++tri)
		{
			if (0 != bx::strCmp(material.c_str(), groupIt->m_material.c_str() )
			|| sentinel
			||  65533 <= numVertices)
			{
				prim.m_numVertices = numVertices - prim.m_startVertex;
				prim.m_numIndices  = numIndices  - prim.m_startIndex;
				if (0 < prim.m_numVertices)
				{
					primitives.push_back(prim);
				}

				if (hasTangent)
				{
					calcTangents(vertexData, uint16_t(numVertices), decl, indexData, numIndices);
				}

				triReorderElapsed -= bx::getHPCounter();
				for (PrimitiveArray::const_iterator primIt = primitives.begin(); primIt != primitives.end(); ++primIt)
				{
					const Primitive& prim1 = *primIt;
					optimizeVertexCache(indexData + prim1.m_startIndex, prim1.m_numIndices, numVertices);
				}
				numVertices = optimizeVertexFetch(indexData, numIndices, vertexData, numVertices, uint16_t(stride));

				triReorderElapsed += bx::getHPCounter();

				write(&writer
					, vertexData
					, numVertices
					, decl
					, indexData
					, numIndices
					, compress
					, material
					, primitives
					);
				primitives.clear();

				++writtenPrimitives;
				writtenVertices += numVertices;
				writtenIndices += numIndices;

				vertices = vertexData;
				indices  = indexData;
				index3s = index3Data;
				numVertices = 0;
				numIndices  = 0;
				prim.m_startVertex = 0;
				prim.m_startIndex  = 0;

				memset(table, 0xff, tableSize * sizeof(unsigned int));

				material = groupIt->m_material;

				if (sentinel)
					break;
			}

			TriIndices& triangle = triangles[tri];
			for (uint32_t edge = 0; edge < 3; ++edge)
			{
				uint8_t vertex[256];
				objGetVertex(objVertices, triangle.m_index[edge], decl, vertex, hasBc, flipV, ccw, scale,  numVertices, numIndices);

				uint32_t hash = bx::hash<bx::HashMurmur2A>(vertex, stride);
				size_t bucket = hash & hashmod;
				uint32_t index = 0;

				for (size_t probe = 0; probe <= hashmod; ++probe)
				{
					uint32_t& item = table[bucket];

					if (item == ~0)
					{
						memcpy(vertices, vertex, decl.getStride());
						vertices += stride;
						item = numVertices++;
						index = item;

						memcpy(index3s, &triangle.m_index[edge], sizeof(Index3));
						++index3s;
						break;
					}

					if (0 == bx::memCmp(vertexData + item * stride, vertex, stride))
					{
						if (0 != bx::memCmp(index3Data + item, &triangle.m_index[edge], sizeof(Index3)))
						{
							++extraMerge;
						}

						index = item;
						break;
					}

					// hash collision, quadratic probing
					bucket = (bucket + probe + 1) & hashmod;
				}

				*indices++ = (uint16_t)index;
				++numIndices;
			}
		}

		prim.m_numVertices = numVertices - prim.m_startVertex;
		if (0 < prim.m_numVertices)
		{
			prim.m_numIndices = numIndices - prim.m_startIndex;
			prim.m_name = groupIt->m_name;
			primitives.push_back(prim);
			prim.m_startVertex = numVertices;
			prim.m_startIndex  = numIndices;
		}

		BX_TRACE("%3d: s %5d, n %5d, %s\n"
			, ii
			, groupIt->m_startTriangle
			, groupIt->m_numTriangles
			, groupIt->m_material.c_str()
			);
	}

	BX_CHECK(0 == primitives.size(), "Not all primitives are written");

	bx::printf("size: %d\n", uint32_t(bx::seek(&writer) ) );
	bx::close(&writer);

	delete [] table;
	delete [] indexData;
	delete [] vertexData;

	now = bx::getHPCounter();
	convertElapsed += now;

	bx::printf("parse %f [s]\ntri reorder %f [s]\nconvert %f [s]\n# %d, g %d, p %d, v %d, i %d\n"
		, double(parseElapsed)/bx::getHPFrequency()
		, double(triReorderElapsed)/bx::getHPFrequency()
		, double(convertElapsed)/bx::getHPFrequency()
		, num
		, uint32_t(groups.size() )
		, writtenPrimitives
		, writtenVertices
		, writtenIndices
		);

	return bx::kExitSuccess;
}
