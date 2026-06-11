#include "HeightMapGen.h"

void GenerateHeightMapMesh(
	uint32_t width,
	uint32_t height,
	std::vector<uint32_t>& indices,
	std::vector<DirectX::XMFLOAT3>& positions,
	std::vector<DirectX::XMFLOAT3>& normals,
	std::vector<DirectX::XMFLOAT4>& tangents,  // xyz = tangent, w = handedness
	std::vector<DirectX::XMFLOAT2>& uvs
) {
	using namespace DirectX;

	assert(width >= 2 && height >= 2);

	const uint32_t vertexCount = width * height;

	// Indices
	//
	//  (row  , col) --- (row  , col+1)
	//        |       \        |
	//  (row+1, col) --- (row+1, col+1)
	//
	indices.clear();
	indices.reserve((width - 1) * (height - 1) * 6);

	for (uint32_t row = 0; row < height - 1; ++row) {
		for (uint32_t col = 0; col < width - 1; ++col)
		{
			const uint32_t i00{ row * width + col };
			const uint32_t i10{ (row + 1) * width + col };
			const uint32_t i01{ row * width + col + 1 };
			const uint32_t i11{ (row + 1) * width + col + 1 };

			// Triangle 1: i00, i10, i01
			indices.push_back(i00);
			indices.push_back(i10);
			indices.push_back(i01);

			// Triangle 2: i01, i10, i11
			indices.push_back(i01);
			indices.push_back(i10);
			indices.push_back(i11);
		}
	}

	// Vertices

	positions.clear(); positions.reserve(vertexCount);
	normals.clear(); normals.reserve(vertexCount);
	tangents.clear(); tangents.reserve(vertexCount);
	uvs.clear(); uvs.reserve(vertexCount);

	//std::random_device rd;
	//std::mt19937 gen(rd());
	//std::uniform_real_distribution<float> heightDist(-1.0f, 1.0f);

	for (uint32_t row = 0; row < height; ++row) {
		for (uint32_t col = 0; col < width; ++col)
		{
			float cellSize = 1.0f;
			const float halfW = (width - 1) * cellSize * 0.5f;
			const float halfD = (height - 1) * cellSize * 0.5f;

			// -size / 2 ... size / 2
			//const float x = col * cellSize - halfW;
			//const float y = heightDist(gen);
			//const float z = row * cellSize - halfD;

			// 0 ... 1
			const float x = static_cast<float>(col) / (width - 1);
			//const float y = heightDist(gen) / 2.0f;
			const float y = 0.0f;
			const float z = static_cast<float>(row) / (height - 1);

			positions.push_back({ x, y, z });
			normals.push_back({ 0.0f, 1.0f, 0.0f });
			tangents.push_back({ 0.0f, 0.0f, 0.0f, 1.0f });
			uvs.push_back({
				static_cast<float>(col) / (width - 1),
				static_cast<float>(row) / (height - 1)
				});
		}
	}
}
