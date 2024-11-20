#pragma once

#include "Headers.h"

#include <limits>

class Camera {
public:
	float m_near{ 0.1f };
	float m_far{ 100.0f };
	float m_fov{ 60.0f };
	float m_aspectRatio{ 16.0f / 9.0f };

	void SetAspectRatio(float newAspectRatio);

	virtual DirectX::XMFLOAT3 GetPosition() const = 0;
	virtual DirectX::XMFLOAT3 GetPointOfInterest() const = 0;
	virtual DirectX::XMFLOAT3 GetUpDirection() const = 0;
	virtual DirectX::XMFLOAT3 GetViewDirection() const = 0;

	DirectX::XMMATRIX GetViewMatrix() const;
	DirectX::XMMATRIX GetProjectionMatrix() const;
	DirectX::XMMATRIX GetViewProjectionMatrix() const;

	void BuildViewFrustumPlanes(
		DirectX::XMFLOAT4 (&planes)[6],
		const DirectX::XMMATRIX* viewProjectionMatrix
	) const {
		DirectX::XMMATRIX t{ DirectX::XMMatrixTranspose(
			viewProjectionMatrix ? *viewProjectionMatrix : GetViewProjectionMatrix()
		) };

		auto extractPlane{ [](DirectX::XMVECTOR v1, DirectX::XMVECTOR v2) {
			DirectX::XMVECTOR plane{
				DirectX::XMPlaneNormalize(DirectX::XMVectorAdd(v1, v2))
			};
			DirectX::XMFLOAT4 result;
			DirectX::XMStoreFloat4(&result, plane);
			return result;
		} };

		planes[0] = extractPlane(t.r[3], t.r[0]);							// left
		planes[1] = extractPlane(t.r[3], DirectX::XMVectorNegate(t.r[0]));	// right
		planes[2] = extractPlane(t.r[3], t.r[1]);							// bottom
		planes[3] = extractPlane(t.r[3], DirectX::XMVectorNegate(t.r[1]));	// up
		planes[4] = extractPlane(t.r[3], t.r[2]);							// near
		planes[5] = extractPlane(t.r[3], DirectX::XMVectorNegate(t.r[2]));	// far
	}
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
