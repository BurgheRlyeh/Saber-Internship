#pragma once

#include "Headers.h"

#include <limits>

class Camera {
protected:
	float m_near{ 0.1f };
	float m_far{ 100.0f };
	float m_fov{ 60.0f };
	float m_aspectRatio{ 16.0f / 9.0f };

public:
	void SetAspectRatio(float newAspectRatio);

	virtual DirectX::XMFLOAT3 GetPosition() const = 0;
	virtual DirectX::XMFLOAT3 GetPointOfInterest() const = 0;
	virtual DirectX::XMFLOAT3 GetUpDirection() const = 0;
	virtual DirectX::XMFLOAT3 GetViewDirection() const = 0;

	DirectX::XMMATRIX GetViewMatrix() const;
	DirectX::XMMATRIX GetProjectionMatrix() const;
	DirectX::XMMATRIX GetViewProjectionMatrix() const;
};

class StaticCamera : public Camera {
	DirectX::XMFLOAT3 m_pos{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_poi{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3  m_up{ 0.0f, 1.0f, 0.0f };

public:
	float m_near{ 0.1f };
	float m_far{ 100.0f };
	float m_fov{ 60.0f };

	StaticCamera(
		const DirectX::XMFLOAT3& pos,
		const DirectX::XMFLOAT3& poi,
		const DirectX::XMFLOAT3& upDir
	);

	virtual DirectX::XMFLOAT3 GetPosition() const override;
	virtual DirectX::XMFLOAT3 GetPointOfInterest() const override;
	virtual DirectX::XMFLOAT3 GetUpDirection() const override;
	virtual DirectX::XMFLOAT3 GetViewDirection() const override;
};

class DynamicCamera : public Camera {
	DirectX::XMFLOAT3 m_poi{ 0.0f, 0.0f, 0.0f };

	float m_radius{ 3.f };
	float m_angX{ DirectX::XM_PIDIV2 * 3 / 4 };
	float m_angY{};
	float m_rotationSpeed{ DirectX::XM_PI };

	float m_deltaForward{};
	float m_deltaRight{};
	float m_speed{ 2.f };

public:
	DirectX::XMFLOAT3 GetPosition() const override;
	DirectX::XMFLOAT3 GetPointOfInterest() const override;
	DirectX::XMFLOAT3 GetUpDirection() const override;
	DirectX::XMFLOAT3 GetViewDirection() const override;

	void Move(float forwardCoef, float rightCoef);

	void Rotate(float deltaX, float deltaY);

	void Update(float deltaTime);
};
