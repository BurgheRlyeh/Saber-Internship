#pragma once

#include "Headers.h"

class StaticCamera {
	DirectX::XMFLOAT3 m_pos{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_poi{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3  m_up{ 0.0f, 1.0f, 0.0f };

public:
	float m_near{ 0.1f };
	float m_far{ 100.0f };
	float m_fov{ 60.0f };
	float m_aspectRatio{ 16.0f / 9.0f};

	StaticCamera(
		const DirectX::XMFLOAT3& pos,
		const DirectX::XMFLOAT3& poi,
		const DirectX::XMFLOAT3& upDir
	);;

	virtual DirectX::XMFLOAT3 GetPosition() const;
	virtual DirectX::XMFLOAT3 GetPointOfInterest() const;
	virtual DirectX::XMFLOAT3 GetUpDirection() const;
	virtual DirectX::XMFLOAT3 GetViewDirection() const;

	DirectX::XMMATRIX GetViewMatrixLH() const;
	DirectX::XMMATRIX GetViewMatrixRH() const;

	DirectX::XMMATRIX GetProjectionMatrixLH() const;
	DirectX::XMMATRIX GetProjectionMatrixRH() const;

	DirectX::XMMATRIX GetViewProjectionMatrixLH() const;
	DirectX::XMMATRIX GetViewProjectionMatrixRH() const;
};