#include "Camera.h"

#include <algorithm>
#include <cmath>

void Camera::SetAspectRatio(float newAspectRatio) {
	m_aspectRatio = newAspectRatio;
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
	return DirectX::XMMatrixPerspectiveFovRH(
		DirectX::XMConvertToRadians(m_fov),
		m_aspectRatio,
		m_far,
		m_near
	);
}

DirectX::XMMATRIX Camera::GetOrthographicMatrix() const {
	return DirectX::XMMatrixOrthographicRH(
		m_orthographicViewWidth,
		m_orthographicViewHeight,
		m_far,
		m_near
	);
}

DirectX::XMMATRIX Camera::GetViewPerspectiveMatrix() const {
	return DirectX::XMMatrixMultiply(GetViewMatrix(), GetPerspectiveMatrix());
}

DirectX::XMMATRIX Camera::GetViewOrthographicMatrix() const {
	return DirectX::XMMatrixMultiply(GetViewMatrix(), GetOrthographicMatrix());
}

DirectX::XMMATRIX Camera::GetProjectionMatrix() const {
	return m_projectionType == ProjectionType::Perspective ? GetPerspectiveMatrix() : GetOrthographicMatrix();
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

StaticCamera::StaticCamera(
	const DirectX::XMFLOAT3& pos,
	const DirectX::XMFLOAT3& poi,
	const DirectX::XMFLOAT3& upDir
) : m_pos(pos), m_poi(poi), m_up(upDir) {}

DirectX::XMFLOAT3 StaticCamera::GetPosition() const {
	return m_pos;
}

DirectX::XMFLOAT3 StaticCamera::GetPointOfInterest() const {
	return m_poi;
}

DirectX::XMFLOAT3 StaticCamera::GetUpDirection() const {
	return m_up;
}

DirectX::XMFLOAT3 StaticCamera::GetViewDirection() const {
	DirectX::XMVECTOR subtractResult{ DirectX::XMVectorSubtract(
		DirectX::XMLoadFloat3(&m_poi),
		DirectX::XMLoadFloat3(&m_pos)
	) };

	DirectX::XMFLOAT3 result{};
	DirectX::XMStoreFloat3(&result, subtractResult);

	return result;
}

DirectX::XMFLOAT3 OrbitCamera::GetPosition() const {
	return {
		m_poi.x + m_radius * std::cos(m_phi) * std::cos(m_theta),
		m_poi.y + m_radius * std::sin(m_phi),
		m_poi.z + m_radius * std::cos(m_phi) * std::sin(m_theta)
	};
}

DirectX::XMFLOAT3 OrbitCamera::GetViewDirection() const {
	DirectX::XMFLOAT3 pos{ GetPosition() };
	return {
		m_poi.x - pos.x,
		m_poi.y - pos.y,
		m_poi.z - pos.z
	};
}

void OrbitCamera::Rotate(float deltaTheta, float deltaPhi) {
	m_theta -= deltaTheta * m_sensitivity;
	m_phi += deltaPhi * m_sensitivity;

	constexpr float eps{ std::numeric_limits<float>::epsilon() };
	m_phi = std::clamp(m_phi, eps - DirectX::XM_PIDIV2, DirectX::XM_PIDIV2 - eps);
}

void OrbitCamera::Zoom(float deltaRadius) {
	m_radius += deltaRadius;
	constexpr float minRadius = 0.1f;
	m_radius = std::max(m_radius, minRadius);
}

void OrbitCamera::Move(float forwardCoef, float rightCoef, float upCoef) {
	m_deltaForward += forwardCoef * m_speed;
	m_deltaRight += rightCoef * m_speed;
}

void OrbitCamera::Update(float deltaTime) {
	if (m_deltaForward != 0.f || m_deltaRight != 0.f) {
		DirectX::XMFLOAT3 pos = GetPosition();
		DirectX::XMFLOAT3 forward{
			pos.x - m_poi.x,
			0.f,
			pos.z - m_poi.z
		};

		DirectX::XMVECTOR forwardVec = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&forward));

		DirectX::XMVECTOR upVec = DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f);
		DirectX::XMVECTOR rightVec = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(upVec, forwardVec));

		DirectX::XMVECTOR deltaVec =
			DirectX::XMVectorAdd(
				DirectX::XMVectorScale(forwardVec, m_deltaForward * deltaTime),
				DirectX::XMVectorScale(rightVec, m_deltaRight * deltaTime)
			);

		DirectX::XMVECTOR newPoi = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_poi), deltaVec);
		DirectX::XMStoreFloat3(&m_poi, newPoi);
	}
}

DirectX::XMFLOAT3 FlyCamera::GetPointOfInterest() const {
	DirectX::XMFLOAT3 dir = GetViewDirection();
	return { m_position.x + dir.x, m_position.y + dir.y, m_position.z + dir.z };
}

DirectX::XMFLOAT3 FlyCamera::GetViewDirection() const {
	return {
		std::cos(m_pitch)* std::sin(m_yaw),
		std::sin(m_pitch),
		std::cos(m_pitch)* std::cos(m_yaw)
	};
}

void FlyCamera::Rotate(float deltaYaw, float deltaPitch) {
	m_yaw += deltaYaw * m_sensitivity;
	m_pitch -= deltaPitch * m_sensitivity;

	constexpr float eps{ std::numeric_limits<float>::epsilon() };
	m_pitch = std::clamp(m_pitch, -DirectX::XM_PIDIV2 + eps, DirectX::XM_PIDIV2 - eps);
}

void FlyCamera::Move(float forwardCoef, float rightCoef, float upCoef) {
	m_velocity.x += rightCoef * m_speed;
	m_velocity.y += upCoef * m_speed;
	m_velocity.z -= forwardCoef * m_speed;
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

	DirectX::XMVECTOR newPosVec{ DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&m_position), deltaVec) };
	DirectX::XMStoreFloat3(&m_position, newPosVec);
}
