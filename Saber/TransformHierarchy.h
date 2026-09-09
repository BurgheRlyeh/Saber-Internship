#pragma once

#include <cstddef>
#include <vector>

#include <DirectXMath.h>

#include "Transform.h"

class TransformHierarchy {
public:
	static constexpr size_t RootNode{ static_cast<size_t>(-1) };

private:
	struct Node {
		Transform local{};
		size_t parent{ RootNode };
	};

	std::vector<Node> m_nodes{};
	std::vector<DirectX::XMFLOAT4X4> m_worldMatrices{};

public:
	size_t AddNode(const Transform& local = {}, size_t parent = RootNode);

	size_t GetNodeCount() const { return m_nodes.size(); }

	Transform& GetLocalTransform(size_t node);
	const Transform& GetLocalTransform(size_t node) const;

	void SetParent(size_t node, size_t parent);
	size_t GetParent(size_t node) const;

	DirectX::XMMATRIX GetWorldMatrix(size_t node) const;

	void Update();
};
