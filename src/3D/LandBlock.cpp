/* openblack - A reimplementation of Lionhead's Black & White.
 *
 * openblack is the legal property of its developers, whose names
 * can be found in the AUTHORS.md file distributed with this source
 * distribution.
 *
 * openblack is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * openblack is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with openblack. If not, see <http://www.gnu.org/licenses/>.
 */

#include "LandBlock.h"

#include "Graphics/VertexBuffer.h"
#include "LandIsland.h"

#include <glm/gtc/type_ptr.hpp>

using namespace openblack;
using namespace openblack::graphics;

struct LH3DLandCell // 8 bytes
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t l;

	uint8_t altitude;
	uint8_t savecolor;

	uint8_t flags1;
	uint8_t flags2;
};

struct LH3DLandBlock
{
	LH3DLandCell cells[289];   // 17*17
	uint32_t index;            // 2312
	float mapX;                // 2320
	float mapZ;                // 2316
	uint32_t blockX;           // 2328
	uint32_t blockZ;           // 2324
	uint32_t clipped;          // 2332 0
	uint32_t frameVisibility;  // 0
	uint32_t highestAltitude;  // 0
	uint32_t useSmallBump;     // 0
	uint32_t forceLowResTex;   // 0
	uint32_t meshLOD;          // 0
	uint32_t meshBlending;     // 0
	uint32_t textureBlend;     // 0
	uint32_t meshLODType;      // 0
	uint32_t fog;              // 0
	uint32_t texPointer;       // 0 if LH3DIsland::g_b_use_always_low_texture, else new Tex
	uint32_t matPointer;       // 0 if LH3DIsland::g_b_use_always_low_texture, else
	                           // new Mat(Tex)
	uint32_t drawSomething;    // 0aaa
	uint32_t specMatBeforePtr; // 0
	uint32_t specMatAfterPtr;  // 0
	float transformUVBefore[3][4];
	float transformUVAfter[3][4];
	uint32_t nextSortingPtr;
	float valueSorting;
	float lowResTexture;
	float fu_lrs; // (iu_lrs / 256)
	float fv_lrs; // (iv_lrs / 256)
	float iu_lrs; // lowrestex x
	float iv_lrs; // lowrestex z
	uint32_t smallTextUpdated;
};

void LandBlock::Load(void* block, size_t block_size)
{
	if (block_size != sizeof(LH3DLandBlock))
	{
		throw std::runtime_error("LandBlock size does not match struct size.");
	}

	auto lhBlock = static_cast<LH3DLandBlock*>(block);

	_index = lhBlock->index;
	_blockPosition = glm::ivec2(lhBlock->blockX, lhBlock->blockZ);
	_mapPosition = glm::vec4(lhBlock->mapX, lhBlock->mapZ, 0, 0);

	// this should just work, not graceful lol
	memcpy(&_cells, lhBlock->cells, 17 * 17 * sizeof(LH3DLandCell));
}

void LandBlock::BuildMesh(LandIsland& island)
{
	if (_mesh != nullptr)
		_mesh.reset();

	VertexDecl decl;
	decl.reserve(7);
	decl.emplace_back(VertexAttrib::Attribute::Position, 3, VertexAttrib::Type::Float);
	// weight
	decl.emplace_back(VertexAttrib::Attribute::TexCoord1, 3, VertexAttrib::Type::Float);
	// first material id
	decl.emplace_back(VertexAttrib::Attribute::Color1, 3, VertexAttrib::Type::Uint8);
	// second material id
	decl.emplace_back(VertexAttrib::Attribute::Color2, 3, VertexAttrib::Type::Uint8);
	// material blend coefficient
	decl.emplace_back(VertexAttrib::Attribute::TexCoord2, 3, VertexAttrib::Type::Uint8, true);
	// light level, align to 4 bytes
	decl.emplace_back(VertexAttrib::Attribute::Color0, 4, VertexAttrib::Type::Uint8, true);
	// water alpha
	decl.emplace_back(VertexAttrib::Attribute::Color3, 1, VertexAttrib::Type::Float, true);

	auto verts = buildVertexList(island);

	auto vertexBuffer = new VertexBuffer("LandBlock", verts, decl);
	_mesh = std::make_unique<Mesh>(vertexBuffer);
}

const bgfx::Memory* LandBlock::buildVertexList(LandIsland& island)
{
	// reserve 16*16 quads of 2 tris with 3 verts = 1536
	const bgfx::Memory* verticesMem = bgfx::alloc(sizeof(LandVertex) * 1536);
	auto vertices = (LandVertex*)verticesMem->data;

	auto countries = island.GetCountries();

	auto neighbourBlockR = island.GetBlock(_blockPosition.x + 1, _blockPosition.y);
	auto neighbourBlockUp = island.GetBlock(_blockPosition.x, _blockPosition.y + 1);

	// we'll loop through each cell, 16x16
	// (the array is 17x17 but the 17th block is questionable data)

	int bx = _blockPosition.x * 16;
	int bz = _blockPosition.y * 16;

	uint16_t i = 0;
	for (int x = 0; x < 16; x++)
	{
		for (int z = 0; z < 16; z++)
		{
			LandCell tl = island.GetCell(bx + x + 0, bz + z + 0);
			LandCell tr = island.GetCell(bx + x + 1, bz + z + 0);
			LandCell bl = island.GetCell(bx + x + 0, bz + z + 1);
			LandCell br = island.GetCell(bx + x + 1, bz + z + 1);

			// construct positions from cell altitudes
			glm::vec3 pTL((x + 0) * LandIsland::CellSize, tl.Altitude() * LandIsland::HeightUnit,
			              ((z + 0) * LandIsland::CellSize));
			glm::vec3 pTR((x + 1) * LandIsland::CellSize, tr.Altitude() * LandIsland::HeightUnit,
			              ((z + 0) * LandIsland::CellSize));
			glm::vec3 pBL((x + 0) * LandIsland::CellSize, bl.Altitude() * LandIsland::HeightUnit,
			              ((z + 1) * LandIsland::CellSize));
			glm::vec3 pBR((x + 1) * LandIsland::CellSize, br.Altitude() * LandIsland::HeightUnit,
			              ((z + 1) * LandIsland::CellSize));

			auto tlMat = countries[tl.Country()].MapMaterials[tl.Altitude() + island.GetNoise(bx + x + 0, bz + z + 0)];
			auto trMat = countries[tr.Country()].MapMaterials[tr.Altitude() + island.GetNoise(bx + x + 1, bz + z + 0)];
			auto blMat = countries[bl.Country()].MapMaterials[bl.Altitude() + island.GetNoise(bx + x + 0, bz + z + 1)];
			auto brMat = countries[br.Country()].MapMaterials[br.Altitude() + island.GetNoise(bx + x + 1, bz + z + 1)];

			// use a lambda so we're not repeating ourselves
			auto make_vert = [](glm::vec3 height, glm::vec3 weight, MapMaterial m[3], LandCell cell) -> LandVertex {
				return LandVertex(height, weight, m[0].FirstMaterialIndex, m[1].FirstMaterialIndex, m[2].FirstMaterialIndex,
				                  m[0].SecondMaterialIndex, m[1].SecondMaterialIndex, m[2].SecondMaterialIndex, m[0].Coeficient,
				                  m[1].Coeficient, m[2].Coeficient, cell.Light(), cell.Alpha());
			};

			// cell splitting
			// winding order = clockwise
			if (!tl.Split())
			{
				// TR/BR/TL  # #
				//             #
				MapMaterial trbrtl[3] = {tlMat, trMat, brMat};
				vertices[i++] = make_vert(pTR, glm::vec3(0, 1, 0), trbrtl, tr);
				vertices[i++] = make_vert(pBR, glm::vec3(0, 0, 1), trbrtl, br);
				vertices[i++] = make_vert(pTL, glm::vec3(1, 0, 0), trbrtl, tl);

				// BR/BL/TL  #
				//           # #
				MapMaterial brbltl[3] = {tlMat, blMat, brMat};
				vertices[i++] = make_vert(pBR, glm::vec3(0, 0, 1), brbltl, br);
				vertices[i++] = make_vert(pBL, glm::vec3(0, 1, 0), brbltl, bl);
				vertices[i++] = make_vert(pTL, glm::vec3(1, 0, 0), brbltl, tl);
			}
			else
			{
				// BL/TL/TR  # #
				//           #
				MapMaterial bltltr[3] = {blMat, tlMat, trMat};
				vertices[i++] = make_vert(pBL, glm::vec3(1, 0, 0), bltltr, bl);
				vertices[i++] = make_vert(pTL, glm::vec3(0, 1, 0), bltltr, tl);
				vertices[i++] = make_vert(pTR, glm::vec3(0, 0, 1), bltltr, tr);

				// TR/BR/BL    #
				//           # #
				MapMaterial trbrbl[3] = {blMat, brMat, trMat};
				vertices[i++] = make_vert(pTR, glm::vec3(0, 0, 1), trbrbl, tr);
				vertices[i++] = make_vert(pBR, glm::vec3(0, 1, 0), trbrbl, br);
				vertices[i++] = make_vert(pBL, glm::vec3(1, 0, 0), trbrbl, bl);
			}
		}
	}

	return verticesMem;
}

void LandBlock::Draw(graphics::RenderPass viewId, const ShaderProgram& program, bool cullBack) const
{
	program.SetUniformValue("u_blockPosition", &_mapPosition);

	Mesh::DrawDesc desc = {
	    /*viewId =*/viewId,
	    /*program =*/program,
	    /*count =*/_mesh->GetVertexBuffer().GetCount(),
	    /*offset =*/0,
	    /*instanceBuffer =*/nullptr,
	    /*instanceStart =*/0,
	    /*instanceCount =*/1,
	    /*state =*/0u | BGFX_STATE_WRITE_MASK | BGFX_STATE_DEPTH_TEST_LESS |
	        (cullBack ? BGFX_STATE_CULL_CCW : BGFX_STATE_CULL_CW) | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA,
	    /*rgba =*/0,
	    /*skip =*/Mesh::SkipState::SkipNone,
	    /*preserveState =*/false,
	};
	_mesh->Draw(desc);
}
