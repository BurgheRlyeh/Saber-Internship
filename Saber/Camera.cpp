#include "Camera.h"

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

DirectX::XMMATRIX StaticCamera::GetViewMatrixLH() const {
	DirectX::XMVECTOR pos{ DirectX::XMLoadFloat3(&m_pos) };
	DirectX::XMVECTOR poi{ DirectX::XMLoadFloat3(&m_poi) };

	return DirectX::XMMatrixLookAtLH(
		DirectX::XMVectorSetW(pos, 1.f),
		DirectX::XMVectorSetW(poi, 1.f),
		DirectX::XMLoadFloat3(&m_up)
	);
}

DirectX::XMMATRIX StaticCamera::GetViewMatrixRH() const {
	DirectX::XMVECTOR pos{ DirectX::XMLoadFloat3(&m_pos) };
	DirectX::XMVECTOR poi{ DirectX::XMLoadFloat3(&m_poi) };

	return DirectX::XMMatrixLookAtRH(
		DirectX::XMVectorSetW(pos, 1.f),
		DirectX::XMVectorSetW(poi, 1.f),
		DirectX::XMLoadFloat3(&m_up)
	);
}

DirectX::XMMATRIX StaticCamera::GetProjectionMatrixLH() const {
	return DirectX::XMMatrixPerspectiveFovLH(
		DirectX::XMConvertToRadians(m_fov),
		m_aspectRatio,
		m_far,
		m_near
	);
}

DirectX::XMMATRIX StaticCamera::GetProjectionMatrixRH() const {
	return DirectX::XMMatrixPerspectiveFovRH(
		DirectX::XMConvertToRadians(m_fov),
		m_aspectRatio,
		m_far,
		m_near
	);
}

DirectX::XMMATRIX StaticCamera::GetViewProjectionMatrixLH() const {
	return DirectX::XMMatrixMultiply(GetViewMatrixLH(), GetProjectionMatrixLH());
}

DirectX::XMMATRIX StaticCamera::GetViewProjectionMatrixRH() const {
	return DirectX::XMMatrixMultiply(GetViewMatrixRH(), GetProjectionMatrixRH());
}
