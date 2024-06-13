#include "Camera.h"

StaticCamera::StaticCamera(
	const DirectX::XMVECTOR& pos,
	const DirectX::XMVECTOR& poi,
	const DirectX::XMVECTOR& upDir
) : m_pos(pos), m_poi(poi), m_up(upDir) {}

DirectX::XMVECTOR StaticCamera::GetPosition() const {
	return m_pos;
}

DirectX::XMVECTOR StaticCamera::GetPointOfInterest() const {
	return m_poi;
}

DirectX::XMVECTOR StaticCamera::GetUpDirection() const {
	return m_up;
}

DirectX::XMMATRIX StaticCamera::GetViewMatrixLH() const {
	return DirectX::XMMatrixLookAtLH(m_pos, m_poi, m_up);
}

DirectX::XMMATRIX StaticCamera::GetViewMatrixRH() const {
	return DirectX::XMMatrixLookAtRH(m_pos, m_poi, m_up);
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
