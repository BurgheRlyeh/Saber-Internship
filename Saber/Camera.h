#pragma once

#include "Headers.h"

class StaticCamera {
	DirectX::XMVECTOR m_pos{ 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMVECTOR m_poi{ 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMVECTOR  m_up{ 0.0f, 1.0f, 0.0f, 0.0f };

public:
	float m_near{ 0.1f };
	float m_far{ 100.0f };
	float m_fov{ 60.0f };
	float m_aspectRatio{ 16.0f / 9.0f};

	StaticCamera(
		const DirectX::XMVECTOR& pos,
		const DirectX::XMVECTOR& poi,
		const DirectX::XMVECTOR& upDir
	);;

	virtual DirectX::XMVECTOR GetPosition() const;
	virtual DirectX::XMVECTOR GetPointOfInterest() const;
	virtual DirectX::XMVECTOR GetUpDirection() const;

	DirectX::XMMATRIX GetViewMatrixLH() const;
	DirectX::XMMATRIX GetViewMatrixRH() const;

	DirectX::XMMATRIX GetProjectionMatrixLH() const;
	DirectX::XMMATRIX GetProjectionMatrixRH() const;

	DirectX::XMMATRIX GetViewProjectionMatrixLH() const;
	DirectX::XMMATRIX GetViewProjectionMatrixRH() const;
};