#pragma once

#include <DirectXMath.h>

// Game-side building block: a local SRT transform. Deliberately depends on
// DirectXMath only -- nothing here knows about the renderer.
struct Transform {
	DirectX::XMFLOAT3 position{ 0.f, 0.f, 0.f };
	DirectX::XMFLOAT4 rotation{ 0.f, 0.f, 0.f, 1.f };	// quaternion
	DirectX::XMFLOAT3 scale{ 1.f, 1.f, 1.f };

	DirectX::XMMATRIX GetMatrix() const {
		return DirectX::XMMatrixAffineTransformation(
			DirectX::XMLoadFloat3(&scale),
			DirectX::g_XMZero,
			DirectX::XMLoadFloat4(&rotation),
			DirectX::XMLoadFloat3(&position)
		);
	}
};
