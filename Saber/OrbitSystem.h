#pragma once

#include <cstddef>
#include <vector>

#include <DirectXMath.h>

#include "TransformHierarchy.h"

class OrbitSystem {
public:
	static constexpr size_t NoParent{ static_cast<size_t>(-1) };

	struct BodyDesc {
		size_t parent{ NoParent };

		float orbitRadius{};
		float orbitSpeed{};			// radians per second around the parent
		float orbitAngle{};			// starting phase

		float spinSpeed{};			// radians per second around the local Y axis
		float scale{ 1.f };
	};

private:
	struct Body {
		BodyDesc desc{};
		size_t node{};

		float orbitAngle{};
		float spinAngle{};
	};

	TransformHierarchy m_hierarchy{};
	std::vector<Body> m_bodies{};

public:
	size_t AddBody(const BodyDesc& desc);

	size_t GetBodyCount() const { return m_bodies.size(); }

	void Update(float deltaTime);

	DirectX::XMMATRIX GetWorldMatrix(size_t body) const;
};
