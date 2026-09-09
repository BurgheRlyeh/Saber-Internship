#pragma once

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

struct GeometryData {
	std::vector<DirectX::XMFLOAT3> positions{};
	std::vector<DirectX::XMFLOAT3> normals{};
	std::vector<DirectX::XMFLOAT3> tangents{};
	std::vector<DirectX::XMFLOAT2> uvs{};

	std::vector<uint32_t> indices{};
};

GeometryData GenerateBox(
	float width = 1.f,
	float height = 1.f,
	float depth = 1.f
);

GeometryData GenerateSphere(
	float radius = 1.f,
	uint32_t sliceCount = 32,
	uint32_t stackCount = 16
);
