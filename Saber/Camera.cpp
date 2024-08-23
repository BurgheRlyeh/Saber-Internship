#include "Camera.h"

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

DirectX::XMMATRIX Camera::GetProjectionMatrix() const {
	return DirectX::XMMatrixPerspectiveFovRH(
		DirectX::XMConvertToRadians(m_fov),
		m_aspectRatio,
		m_far,
		m_near
	);
}

DirectX::XMMATRIX Camera::GetViewProjectionMatrix() const {
	return DirectX::XMMatrixMultiply(GetViewMatrix(), GetProjectionMatrix());
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

DirectX::XMFLOAT3 DynamicCamera::GetPosition() const {
	DirectX::XMFLOAT3 viewDir{ GetViewDirection() };

	DirectX::XMVECTOR resVector{ DirectX::XMVectorAdd(
		DirectX::XMLoadFloat3(&m_poi),
		DirectX::XMVectorScale(
			DirectX::XMLoadFloat3(&viewDir),
			m_radius
		)
	) };

	DirectX::XMFLOAT3 resPoint{};
	DirectX::XMStoreFloat3(&resPoint, resVector);

	return resPoint;
}

DirectX::XMFLOAT3 DynamicCamera::GetPointOfInterest() const {
	return m_poi;
}

DirectX::XMFLOAT3 DynamicCamera::GetUpDirection() const {
	return {
		DirectX::XMScalarCos(m_angY + DirectX::XM_PIDIV2) * DirectX::XMScalarCos(m_angX),
		DirectX::XMScalarSin(m_angY + DirectX::XM_PIDIV2),
		DirectX::XMScalarCos(m_angY + DirectX::XM_PIDIV2) * DirectX::XMScalarSin(m_angX)
	};
}

DirectX::XMFLOAT3 DynamicCamera::GetViewDirection() const {
	return {
		DirectX::XMScalarCos(m_angY) * DirectX::XMScalarCos(m_angX),
		DirectX::XMScalarSin(m_angY),
		DirectX::XMScalarCos(m_angY) * DirectX::XMScalarSin(m_angX)
	};
}

void DynamicCamera::Move(float forwardCoef, float rightCoef) {
	m_deltaForward += forwardCoef * m_speed;
	m_deltaRight += rightCoef * m_speed;
}

void DynamicCamera::Rotate(float deltaX, float deltaY) {
	m_angX -= m_rotationSpeed * deltaX;
	m_angY += m_rotationSpeed * deltaY;

	constexpr float eps{ std::numeric_limits<float>::epsilon() };
	m_angY = DirectX::XMMin(DirectX::XM_PIDIV2 - eps, DirectX::XMMax(m_angY, eps - DirectX::XM_PIDIV2));
}

void DynamicCamera::Update(float deltaTime) {
	DirectX::XMFLOAT3 dir{ GetViewDirection() };
	DirectX::XMFLOAT3 up{ GetUpDirection() };

	DirectX::XMFLOAT3 forwardPoint{
		DirectX::XMMax(fabs(dir.x), fabs(dir.y)) <= std::numeric_limits<float>::epsilon() ? up : dir
	};
	forwardPoint.y = 0.f;
	DirectX::XMVECTOR forwardVector{ DirectX::XMLoadFloat3(&forwardPoint) };

	DirectX::XMVECTOR rightVector{ DirectX::XMVector3Cross(
		DirectX::XMLoadFloat3(&up),
		DirectX::XMLoadFloat3(&dir)
	) };

	DirectX::XMVECTOR deltaPoi{ DirectX::XMVectorAdd(
		DirectX::XMVectorScale(forwardVector, deltaTime * m_deltaForward),
		DirectX::XMVectorScale(rightVector, deltaTime * m_deltaRight)
	) };
	DirectX::XMVECTOR newPoi{ DirectX::XMVectorAdd(
		DirectX::XMLoadFloat3(&m_poi),
		deltaPoi
	) };

	DirectX::XMStoreFloat3(&m_poi, newPoi);
}
