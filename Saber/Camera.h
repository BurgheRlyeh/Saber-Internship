#pragma once

#include "Headers.h"

#include <limits>

enum class ProjectionType : uint8_t {
	Perspective,
	Orthographic
};

class Camera {
public:
	float m_near{ 0.1f };
	float m_far{ 100.0f };
	float m_fov{ 60.0f };
	float m_aspectRatio{ 16.0f / 9.0f };

	ProjectionType m_projectionType{ ProjectionType::Perspective };
	float m_orthographicViewWidth{ 8.f };
	float m_orthographicViewHeight{ 8.f };

	void SetAspectRatio(float newAspectRatio);

	virtual DirectX::XMFLOAT3 GetPosition() const = 0;
	virtual DirectX::XMFLOAT3 GetPointOfInterest() const = 0;
	virtual DirectX::XMFLOAT3 GetUpDirection() const = 0;
	virtual DirectX::XMFLOAT3 GetViewDirection() const = 0;

	DirectX::XMMATRIX GetPerspectiveMatrix() const;
	DirectX::XMMATRIX GetOrthographicMatrix() const;
	DirectX::XMMATRIX GetViewPerspectiveMatrix() const;
	DirectX::XMMATRIX GetViewOrthographicMatrix() const;

	DirectX::XMMATRIX GetViewMatrix() const;
	DirectX::XMMATRIX GetProjectionMatrix() const;
	DirectX::XMMATRIX GetViewProjectionMatrix() const;

	void BuildViewFrustumPlanes(
		DirectX::XMFLOAT4 (&planes)[6],
		const DirectX::XMMATRIX* viewProjectionMatrix
	) const;
};

class StaticCamera : public Camera {
	DirectX::XMFLOAT3 m_pos{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_poi{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3  m_up{ 0.0f, 1.0f, 0.0f };

public:
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
protected:
	float m_sensitivity{ DirectX::XM_PI };
	float m_speed{ 5.f };

public:
	virtual void Rotate(float deltaTheta, float deltaPhi) = 0;
	virtual void Move(float forwardCoef, float rightCoef, float upCoef) = 0;
	virtual void Update(float deltaTime) = 0;
};

class OrbitCamera : public DynamicCamera {
	DirectX::XMFLOAT3 m_poi{ 0.f, 0.f, 0.f };

	float m_theta{ DirectX::XM_PIDIV2 };
	float m_phi{ DirectX::XM_PIDIV4 };

	float m_deltaForward{};
	float m_deltaRight{};

	float m_radius{ 5.f };

public:
	DirectX::XMFLOAT3 GetPosition() const override;
	DirectX::XMFLOAT3 GetPointOfInterest() const override { return m_poi; }
	DirectX::XMFLOAT3 GetUpDirection()	const override { return { 0.f, 1.f, 0.f }; }
	DirectX::XMFLOAT3 GetViewDirection() const override;

	void Rotate(float deltaTheta, float deltaPhi) override;
	void Move(float forwardCoef, float rightCoef, float upCoef) override;
	void Update(float deltaTime) override;

	void Zoom(float deltaRadius);
};

class FlyCamera : public DynamicCamera {
	DirectX::XMFLOAT3 m_position{ 0.f, 0.f, 0.f };

	float m_yaw{};
	float m_pitch{};

	DirectX::XMFLOAT3 m_velocity{ 0.f, 0.f, 0.f };

public:
	DirectX::XMFLOAT3 GetPosition() const override { return m_position; }
	DirectX::XMFLOAT3 GetPointOfInterest() const override;
	DirectX::XMFLOAT3 GetUpDirection() const override { return { 0.f, 1.f, 0.f }; }
	DirectX::XMFLOAT3 GetViewDirection() const override;

	void Rotate(float deltaYaw, float deltaPitch) override;
	void Move(float forwardCoef, float rightCoef, float upCoef) override;
	void Update(float deltaTime) override;
};