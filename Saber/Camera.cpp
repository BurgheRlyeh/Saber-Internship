#include "Camera.h"

#include <algorithm>
#include <cmath>

void Camera::SetAspectRatio(float newAspectRatio) {
	GetSettings().aspectRatio = newAspectRatio;
}

DirectX::XMMATRIX Camera::GetViewMatrix() const {
	DirectX::XMFLOAT3 posPoint{ GetPosition() };
	DirectX::XMVECTOR posVector{ DirectX::XMLoadFloat3(&posPoint) };

	DirectX::XMFLOAT3 poiPoint{ GetPointOfInterest() };
	DirectX::XMVECTOR poiVector{ DirectX::XMLoadFloat3(&poiPoint) };

	DirectX::XMFLOAT3 upPoint{ GetUpDirection() };

	return DirectX::XMMatrixLookAtRH(
		DirectX::XMVectorSetW(posVector, 1.f),
		DirectX::XMVectorSetW(poiVector, 1.f),
		DirectX::XMLoadFloat3(&upPoint)
	);
}

DirectX::XMMATRIX Camera::GetPerspectiveMatrix() const {
	const Settings& settings{ GetSettings() };
	return DirectX::XMMatrixPerspectiveFovRH(
		DirectX::XMConvertToRadians(settings.fov),
		settings.aspectRatio,
		settings.farPlane,
		settings.nearPlane
	);
}

DirectX::XMMATRIX Camera::GetOrthographicMatrix() const {
	const Settings& settings{ GetSettings() };
	return DirectX::XMMatrixOrthographicRH(
		settings.orthographicViewWidth,
		settings.orthographicViewHeight,
		settings.farPlane,
		settings.nearPlane
	);
}

DirectX::XMMATRIX Camera::GetViewPerspectiveMatrix() const {
	return DirectX::XMMatrixMultiply(GetViewMatrix(), GetPerspectiveMatrix());
}

DirectX::XMMATRIX Camera::GetViewOrthographicMatrix() const {
	return DirectX::XMMatrixMultiply(GetViewMatrix(), GetOrthographicMatrix());
}

DirectX::XMMATRIX Camera::GetProjectionMatrix() const {
	return GetSettings().projectionType == ProjectionType::Perspective
		? GetPerspectiveMatrix()
		: GetOrthographicMatrix();
}

DirectX::XMMATRIX Camera::GetViewProjectionMatrix() const {
	return DirectX::XMMatrixMultiply(GetViewMatrix(), GetProjectionMatrix());
}

void Camera::BuildViewFrustumPlanes(
	DirectX::XMFLOAT4(&planes)[6],
	const DirectX::XMMATRIX* viewProjectionMatrix
) const {
	DirectX::XMMATRIX t{ DirectX::XMMatrixTranspose(
		viewProjectionMatrix ? *viewProjectionMatrix : GetViewPerspectiveMatrix()
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

// StaticCamera
StaticCamera::StaticCamera(
	const DirectX::XMFLOAT3& pos,
	const DirectX::XMFLOAT3& poi,
	const DirectX::XMFLOAT3& upDir
) {
	m_settings.pos = pos;
	m_settings.poi = poi;
	m_settings.up = upDir;
}

DirectX::XMFLOAT3 StaticCamera::GetPosition() const {
	return m_settings.pos;
}

DirectX::XMFLOAT3 StaticCamera::GetPointOfInterest() const {
	return m_settings.poi;
}

DirectX::XMFLOAT3 StaticCamera::GetUpDirection() const {
	return m_settings.up;
}

DirectX::XMFLOAT3 StaticCamera::GetViewDirection() const {
	const Settings& settings{ m_settings };
	DirectX::XMVECTOR subtractResult{ DirectX::XMVectorSubtract(
		DirectX::XMLoadFloat3(&settings.poi),
		DirectX::XMLoadFloat3(&settings.pos)
	) };

	DirectX::XMFLOAT3 result{};
	DirectX::XMStoreFloat3(&result, subtractResult);

	return result;
}

// OrbitCamera
DirectX::XMFLOAT3 OrbitCamera::GetPosition() const {
	const Settings& settings{ m_settings };
	return {
		settings.poi.x + settings.radius * std::cos(settings.phi) * std::cos(settings.theta),
		settings.poi.y + settings.radius * std::sin(settings.phi),
		settings.poi.z + settings.radius * std::cos(settings.phi) * std::sin(settings.theta)
	};
}

DirectX::XMFLOAT3 OrbitCamera::GetViewDirection() const {
	const Settings& settings{ m_settings };
	DirectX::XMFLOAT3 pos{ GetPosition() };
	return {
		settings.poi.x - pos.x,
		settings.poi.y - pos.y,
		settings.poi.z - pos.z
	};
}

void OrbitCamera::Rotate(float deltaTheta, float deltaPhi) {
	Settings& settings{ m_settings };
	settings.theta -= deltaTheta * settings.sensitivity;
	settings.phi += deltaPhi * settings.sensitivity;

	constexpr float eps{ std::numeric_limits<float>::epsilon() };
	settings.phi = std::clamp(settings.phi, eps - DirectX::XM_PIDIV2, DirectX::XM_PIDIV2 - eps);
}

void OrbitCamera::Zoom(float deltaRadius) {
	Settings& settings{ m_settings };
	settings.radius += deltaRadius;
	constexpr float minRadius = 0.1f;
	settings.radius = std::max(settings.radius, minRadius);
}

void OrbitCamera::Move(float forwardCoef, float rightCoef, float upCoef) {
	const Settings& settings{ m_settings };
	m_deltaForward += forwardCoef * settings.speed;
	m_deltaRight += rightCoef * settings.speed;
}

void OrbitCamera::Update(float deltaTime) {
	if (m_deltaForward != 0.f || m_deltaRight != 0.f) {
		Settings& settings{ m_settings };

		DirectX::XMFLOAT3 pos = GetPosition();
		DirectX::XMFLOAT3 forward{
			pos.x - settings.poi.x,
			0.f,
			pos.z - settings.poi.z
		};

		DirectX::XMVECTOR forwardVec = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&forward));

		DirectX::XMVECTOR upVec = DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f);
		DirectX::XMVECTOR rightVec = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(upVec, forwardVec));

		DirectX::XMVECTOR deltaVec =
			DirectX::XMVectorAdd(
				DirectX::XMVectorScale(forwardVec, m_deltaForward * deltaTime),
				DirectX::XMVectorScale(rightVec, m_deltaRight * deltaTime)
			);

		DirectX::XMVECTOR newPoi = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&settings.poi), deltaVec);
		DirectX::XMStoreFloat3(&settings.poi, newPoi);
	}
}

// FlyCamera
DirectX::XMFLOAT3 FlyCamera::GetPointOfInterest() const {
	const Settings& settings{ m_settings };
	DirectX::XMFLOAT3 dir = GetViewDirection();
	return { settings.position.x + dir.x, settings.position.y + dir.y, settings.position.z + dir.z };
}

DirectX::XMFLOAT3 FlyCamera::GetViewDirection() const {
	const Settings& settings{ m_settings };
	return {
		std::cos(settings.pitch) * std::sin(settings.yaw),
		std::sin(settings.pitch),
		std::cos(settings.pitch) * std::cos(settings.yaw)
	};
}

void FlyCamera::Rotate(float deltaYaw, float deltaPitch) {
	Settings& settings{ m_settings };

	settings.yaw += deltaYaw * settings.sensitivity;
	settings.pitch -= deltaPitch * settings.sensitivity;

	constexpr float eps{ std::numeric_limits<float>::epsilon() };
	settings.pitch = std::clamp(settings.pitch, -DirectX::XM_PIDIV2 + eps, DirectX::XM_PIDIV2 - eps);
}

void FlyCamera::Move(float forwardCoef, float rightCoef, float upCoef) {
	const Settings& settings{ m_settings };
	m_velocity.x += rightCoef * settings.speed;
	m_velocity.y += upCoef * settings.speed;
	m_velocity.z -= forwardCoef * settings.speed;
}

void FlyCamera::Update(float deltaTime) {
	DirectX::XMFLOAT3 forward{ GetViewDirection() };
	DirectX::XMVECTOR forwardVec{ DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&forward)) };

	DirectX::XMVECTOR upVec{ DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f) };
	DirectX::XMVECTOR rightVec{ DirectX::XMVector3Normalize(DirectX::XMVector3Cross(forwardVec, upVec)) };
	upVec = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(rightVec, forwardVec));

	DirectX::XMVECTOR deltaVec{ DirectX::XMVectorAdd(
		DirectX::XMVectorScale(forwardVec, m_velocity.z * deltaTime),
		DirectX::XMVectorAdd(
			DirectX::XMVectorScale(rightVec, m_velocity.x * deltaTime),
			DirectX::XMVectorScale(upVec, m_velocity.y * deltaTime)
		)
	) };

	Settings& settings{ m_settings };
	DirectX::XMVECTOR newPosVec{ DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&settings.position), deltaVec) };
	DirectX::XMStoreFloat3(&settings.position, newPosVec);
}

// UI

#include "imgui.h"

namespace {
	template <typename Settings>
	bool DrawSettingsInWindow(const char* title, Settings& settings) {
		bool isChanged{};
		if (ImGui::Begin(title)) {
			isChanged = DrawSettings(settings);
		}
		ImGui::End();
		return isChanged;
	}
}

bool DrawSettings(Camera::Settings& settings) {
	bool isChanged{};

	ImGui::SeparatorText("Projection");

	isChanged |= ImGui::SliderFloat("Near", &settings.nearPlane, 0.01f, 10.0f);
	isChanged |= ImGui::SliderFloat("Far", &settings.farPlane, 10.0f, 1000.0f);

	bool isPerspective{ settings.projectionType == ProjectionType::Perspective };
	if (ImGui::Checkbox("Perspective", &isPerspective)) {
		settings.projectionType = isPerspective
			? ProjectionType::Perspective
			: ProjectionType::Orthographic;
		isChanged = true;
	}

	if (settings.projectionType == ProjectionType::Perspective) {
		isChanged |= ImGui::SliderFloat("FOV", &settings.fov, 10.0f, 120.0f);
		isChanged |= ImGui::SliderFloat("Aspect ratio", &settings.aspectRatio, 0.1f, 4.0f);
	}
	else {
		isChanged |= ImGui::SliderFloat("Ortho width", &settings.orthographicViewWidth, 0.01f, 20.0f);
		isChanged |= ImGui::SliderFloat("Ortho height", &settings.orthographicViewHeight, 0.01f, 20.0f);
	}

	return isChanged;
}

bool DrawSettings(StaticCamera::Settings& settings) {
	bool isChanged{ DrawSettings(static_cast<Camera::Settings&>(settings)) };

	ImGui::SeparatorText("Placement");
	isChanged |= ImGui::SliderFloat3("Position", &settings.pos.x, -100.f, 100.f);
	isChanged |= ImGui::SliderFloat3("Target (POI)", &settings.poi.x, -100.f, 100.f);
	isChanged |= ImGui::SliderFloat3("Up", &settings.up.x, -1.f, 1.f);

	return isChanged;
}

bool DrawSettings(DynamicCamera::Settings& settings) {
	bool isChanged{ DrawSettings(static_cast<Camera::Settings&>(settings)) };

	ImGui::SeparatorText("Movement");
	isChanged |= ImGui::SliderFloat("Sensitivity", &settings.sensitivity, 0.1f, 5.0f);
	isChanged |= ImGui::SliderFloat("Move speed", &settings.speed, 0.1f, 50.0f);

	return isChanged;
}

bool DrawSettings(OrbitCamera::Settings& settings) {
	bool isChanged{ DrawSettings(static_cast<DynamicCamera::Settings&>(settings)) };

	ImGui::SeparatorText("Orbit");
	isChanged |= ImGui::SliderFloat3("Target (POI)", &settings.poi.x, -100.f, 100.f);
	isChanged |= ImGui::SliderAngle("Theta", &settings.theta);
	isChanged |= ImGui::SliderAngle("Phi", &settings.phi, -89.f, 89.f);
	isChanged |= ImGui::SliderFloat("Radius", &settings.radius, 0.1f, 100.f);

	return isChanged;
}

bool DrawSettings(FlyCamera::Settings& settings) {
	bool isChanged{ DrawSettings(static_cast<DynamicCamera::Settings&>(settings)) };

	ImGui::SeparatorText("Fly");
	isChanged |= ImGui::SliderFloat3("Position", &settings.position.x, -100.f, 100.f);
	isChanged |= ImGui::SliderAngle("Yaw", &settings.yaw);
	isChanged |= ImGui::SliderAngle("Pitch", &settings.pitch, -89.f, 89.f);

	return isChanged;
}

bool DrawSettings(Camera& camera) {
	// Most-derived first
	if (auto* pCamera{ dynamic_cast<OrbitCamera*>(&camera) }) {
		return DrawSettingsInWindow("Orbit Camera", pCamera->GetSettings());
	}
	if (auto* pCamera{ dynamic_cast<FlyCamera*>(&camera) }) {
		return DrawSettingsInWindow("Fly Camera", pCamera->GetSettings());
	}
	if (auto* pCamera{ dynamic_cast<DynamicCamera*>(&camera) }) {
		return DrawSettingsInWindow("Dynamic Camera", pCamera->GetSettings());
	}
	if (auto* pCamera{ dynamic_cast<StaticCamera*>(&camera) }) {
		return DrawSettingsInWindow("Static Camera", pCamera->GetSettings());
	}
	return DrawSettingsInWindow("Camera", camera.GetSettings());
}
