#include "TransformHierarchy.h"

#include <cassert>

size_t TransformHierarchy::AddNode(const Transform& local, size_t parent) {
	assert(parent == RootNode || parent < m_nodes.size());

	m_nodes.push_back(Node{ .local{ local }, .parent{ parent } });
	m_worldMatrices.emplace_back();

	return m_nodes.size() - 1;
}

Transform& TransformHierarchy::GetLocalTransform(size_t node) {
	return m_nodes.at(node).local;
}

const Transform& TransformHierarchy::GetLocalTransform(size_t node) const {
	return m_nodes.at(node).local;
}

void TransformHierarchy::SetParent(size_t node, size_t parent) {
	assert(parent == RootNode || parent < node);
	m_nodes.at(node).parent = parent;
}

void TransformHierarchy::SetParentKeepingWorld(size_t node, size_t parent) {
	assert(parent == RootNode || parent < node);

	const DirectX::XMMATRIX world{ GetWorldMatrix(node) };
	const DirectX::XMMATRIX local{
		parent == RootNode
			? world
			: world * DirectX::XMMatrixInverse(nullptr, GetWorldMatrix(parent))
	};

	DirectX::XMVECTOR scale{};
	DirectX::XMVECTOR rotation{};
	DirectX::XMVECTOR translation{};
	[[maybe_unused]] const bool isDecomposed{
		DirectX::XMMatrixDecompose(&scale, &rotation, &translation, local)
	};
	assert(isDecomposed);

	Transform& transform{ m_nodes.at(node).local };
	DirectX::XMStoreFloat3(&transform.scale, scale);
	DirectX::XMStoreFloat4(&transform.rotation, rotation);
	DirectX::XMStoreFloat3(&transform.position, translation);

	m_nodes[node].parent = parent;
}

size_t TransformHierarchy::GetParent(size_t node) const {
	return m_nodes.at(node).parent;
}

DirectX::XMMATRIX TransformHierarchy::GetWorldMatrix(size_t node) const {
	return DirectX::XMLoadFloat4x4(&m_worldMatrices.at(node));
}

void TransformHierarchy::UpdateNode(size_t node) {
	const DirectX::XMMATRIX local{ m_nodes.at(node).local.GetMatrix() };
	const size_t parent{ m_nodes[node].parent };

	DirectX::XMStoreFloat4x4(
		&m_worldMatrices[node],
		parent == RootNode
			? local
			: local * DirectX::XMLoadFloat4x4(&m_worldMatrices[parent])
	);
}

void TransformHierarchy::Update() {
	for (size_t i{}; i < m_nodes.size(); ++i) {
		const DirectX::XMMATRIX local{ m_nodes[i].local.GetMatrix() };
		const size_t parent{ m_nodes[i].parent };

		DirectX::XMStoreFloat4x4(
			&m_worldMatrices[i],
			parent == RootNode
				? local
				: local * DirectX::XMLoadFloat4x4(&m_worldMatrices[parent])
		);
	}
}
