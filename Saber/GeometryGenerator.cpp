#include "GeometryGenerator.h"

#include <cassert>
#include <cmath>

GeometryData GenerateBox(float width, float height, float depth) {
	const float x{ 0.5f * width };
	const float y{ 0.5f * height };
	const float z{ 0.5f * depth };

	GeometryData geometry{
		.positions = {
			{ -x, -y,  z }, {  x, -y,  z }, {  x, -y, -z }, { -x, -y, -z },	// -Y
			{ -x,  y, -z }, {  x,  y, -z }, {  x,  y,  z }, { -x,  y,  z },	// +Y
			{  x, -y, -z }, {  x, -y,  z }, {  x,  y,  z }, {  x,  y, -z },	// +X
			{ -x, -y,  z }, { -x, -y, -z }, { -x,  y, -z }, { -x,  y,  z },	// -X
			{  x, -y,  z }, { -x, -y,  z }, { -x,  y,  z }, {  x,  y,  z },	// +Z
			{ -x, -y, -z }, {  x, -y, -z }, {  x,  y, -z }, { -x,  y, -z }	// -Z
		},
		.normals = {
			{  0.f, -1.f,  0.f }, {  0.f, -1.f,  0.f }, {  0.f, -1.f,  0.f }, {  0.f, -1.f,  0.f },
			{  0.f,  1.f,  0.f }, {  0.f,  1.f,  0.f }, {  0.f,  1.f,  0.f }, {  0.f,  1.f,  0.f },
			{  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f },
			{ -1.f,  0.f,  0.f }, { -1.f,  0.f,  0.f }, { -1.f,  0.f,  0.f }, { -1.f,  0.f,  0.f },
			{  0.f,  0.f,  1.f }, {  0.f,  0.f,  1.f }, {  0.f,  0.f,  1.f }, {  0.f,  0.f,  1.f },
			{  0.f,  0.f, -1.f }, {  0.f,  0.f, -1.f }, {  0.f,  0.f, -1.f }, {  0.f,  0.f, -1.f }
		},
		.tangents = {
			{  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f },
			{  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f },
			{  0.f,  0.f,  1.f }, {  0.f,  0.f,  1.f }, {  0.f,  0.f,  1.f }, {  0.f,  0.f,  1.f },
			{  0.f,  0.f, -1.f }, {  0.f,  0.f, -1.f }, {  0.f,  0.f, -1.f }, {  0.f,  0.f, -1.f },
			{ -1.f,  0.f,  0.f }, { -1.f,  0.f,  0.f }, { -1.f,  0.f,  0.f }, { -1.f,  0.f,  0.f },
			{  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }, {  1.f,  0.f,  0.f }
		},
		.uvs = {
			{ 0.f, 1.f }, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f },
			{ 0.f, 1.f }, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f },
			{ 0.f, 1.f }, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f },
			{ 0.f, 1.f }, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f },
			{ 0.f, 1.f }, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f },
			{ 0.f, 1.f }, { 1.f, 1.f }, { 1.f, 0.f }, { 0.f, 0.f }
		},
		.indices = {
			 0,	 2,  1,  0,  3,  2,
			 4,	 6,  5,  4,  7,  6,
			 8,	10,  9,  8, 11, 10,
			12, 14, 13, 12, 15, 14,
			16, 18, 17, 16, 19, 18,
			20, 22, 21, 20, 23, 22
		}
	};

	return geometry;
}

GeometryData GenerateSphere(float radius, uint32_t sliceCount, uint32_t stackCount) {
	assert(sliceCount >= 3 && stackCount >= 2);

	GeometryData geometry{};

	const uint32_t ringCount{ sliceCount + 1 };	// the seam column is duplicated
	const uint32_t vertexCount{ ringCount * (stackCount + 1) };

	geometry.positions.reserve(vertexCount);
	geometry.normals.reserve(vertexCount);
	geometry.tangents.reserve(vertexCount);
	geometry.uvs.reserve(vertexCount);

	for (uint32_t stack{}; stack <= stackCount; ++stack) {
		const float v{ static_cast<float>(stack) / stackCount };
		const float phi{ v * DirectX::XM_PI };			// 0 at +Y, PI at -Y
		const float sinPhi{ std::sinf(phi) };
		const float cosPhi{ std::cosf(phi) };

		for (uint32_t slice{}; slice < ringCount; ++slice) {
			const float u{ static_cast<float>(slice) / sliceCount };
			const float theta{ u * DirectX::XM_2PI };
			const float sinTheta{ std::sinf(theta) };
			const float cosTheta{ std::cosf(theta) };

			const DirectX::XMFLOAT3 unit{ sinPhi * cosTheta, cosPhi, sinPhi * sinTheta };

			geometry.positions.push_back({ radius * unit.x, radius * unit.y, radius * unit.z });
			geometry.normals.push_back(unit);
			geometry.tangents.push_back({ -sinTheta, 0.f, cosTheta });
			geometry.uvs.push_back({ u, v });
		}
	}

	geometry.indices.reserve(6ull * sliceCount * stackCount);
	for (uint32_t stack{}; stack < stackCount; ++stack) {
		for (uint32_t slice{}; slice < sliceCount; ++slice) {
			const uint32_t topLeft{ stack * ringCount + slice };
			const uint32_t topRight{ topLeft + 1 };
			const uint32_t bottomLeft{ topLeft + ringCount };
			const uint32_t bottomRight{ bottomLeft + 1 };

			geometry.indices.insert(geometry.indices.end(), {
				topLeft, topRight, bottomLeft,
				topRight, bottomRight, bottomLeft
			});
		}
	}

	return geometry;
}
