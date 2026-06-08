#pragma once

#include "Headers.h"

#include <limits>

enum class ProjectionType : uint8_t {
	Perspective,
	Orthographic
};

class Camera {
public:
	struct Settings {
		float nearPlane{ 0.1f };
		float farPlane{ 100.0f };

		ProjectionType projectionType{ ProjectionType::Perspective };

		float fov{ 60.0f };
		float aspectRatio{ 16.0f / 9.0f };

		float orthographicViewWidth{ 8.f };
		float orthographicViewHeight{ 8.f };
	};

	virtual ~Camera() = default;

	virtual Settings& GetSettings() = 0;
	virtual const Settings& GetSettings() const = 0;

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
public:
	struct Settings : Camera::Settings {
		DirectX::XMFLOAT3 pos{ 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 poi{ 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3  up{ 0.0f, 1.0f, 0.0f };
	};

	StaticCamera() = default;
	StaticCamera(
		const DirectX::XMFLOAT3& pos,
		const DirectX::XMFLOAT3& poi,
		const DirectX::XMFLOAT3& upDir
	);

	Settings& GetSettings() override { return m_settings; }
	const Settings& GetSettings() const override { return m_settings; }

	DirectX::XMFLOAT3 GetPosition() const override;
	DirectX::XMFLOAT3 GetPointOfInterest() const override;
	DirectX::XMFLOAT3 GetUpDirection() const override;
	DirectX::XMFLOAT3 GetViewDirection() const override;

private:
	Settings m_settings{};
};

class DynamicCamera : public Camera {
public:
	struct Settings : Camera::Settings {
		float sensitivity{ DirectX::XM_PI };
		float speed{ 5.f };
	};

	// Re-declare so DynamicCamera-related code can reach new settings
	Settings& GetSettings() override = 0;
	const Settings& GetSettings() const override = 0;

	virtual void Rotate(float deltaTheta, float deltaPhi) = 0;
	virtual void Move(float forwardCoef, float rightCoef, float upCoef) = 0;
	virtual void Update(float deltaTime) = 0;
};

class OrbitCamera : public DynamicCamera {
public:
	struct Settings : DynamicCamera::Settings {
		DirectX::XMFLOAT3 poi{ 0.f, 0.f, 0.f };

		float theta{ DirectX::XM_PIDIV2 };
		float phi{ DirectX::XM_PIDIV4 };

		float radius{ 5.f };
	};

	Settings& GetSettings() override { return m_settings; }
	const Settings& GetSettings() const override { return m_settings; }

	DirectX::XMFLOAT3 GetPosition() const override;
	DirectX::XMFLOAT3 GetPointOfInterest() const override { return m_settings.poi; }
	DirectX::XMFLOAT3 GetUpDirection()	const override { return { 0.f, 1.f, 0.f }; }
	DirectX::XMFLOAT3 GetViewDirection() const override;

	void Rotate(float deltaTheta, float deltaPhi) override;
	void Move(float forwardCoef, float rightCoef, float upCoef) override;
	void Update(float deltaTime) override;

	void Zoom(float deltaRadius);

private:
	Settings m_settings{};

	float m_deltaForward{};
	float m_deltaRight{};
};

class FlyCamera : public DynamicCamera {
public:
	struct Settings : DynamicCamera::Settings {
		DirectX::XMFLOAT3 position{ 0.f, 0.f, 0.f };

		float yaw{};
		float pitch{};
	};

	Settings& GetSettings() override { return m_settings; }
	const Settings& GetSettings() const override { return m_settings; }

	DirectX::XMFLOAT3 GetPosition() const override { return m_settings.position; }
	DirectX::XMFLOAT3 GetPointOfInterest() const override;
	DirectX::XMFLOAT3 GetUpDirection() const override { return { 0.f, 1.f, 0.f }; }
	DirectX::XMFLOAT3 GetViewDirection() const override;

	void Rotate(float deltaYaw, float deltaPitch) override;
	void Move(float forwardCoef, float rightCoef, float upCoef) override;
	void Update(float deltaTime) override;

private:
	Settings m_settings{};

	DirectX::XMFLOAT3 m_velocity{ 0.f, 0.f, 0.f };   // transient
};

void DrawCameraSettings(Camera& camera);
