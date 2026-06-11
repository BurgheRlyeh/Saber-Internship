#pragma once
#include <random>

#include "Headers.h"

#include <cstdint>
#include <vector>

void GenerateHeightMapMesh(
	uint32_t width,
	uint32_t height,
	std::vector<uint32_t>& indices,
	std::vector<DirectX::XMFLOAT3>& positions,
	std::vector<DirectX::XMFLOAT3>& normals,
	std::vector<DirectX::XMFLOAT4>& tangents,
	std::vector<DirectX::XMFLOAT2>& uvs
);