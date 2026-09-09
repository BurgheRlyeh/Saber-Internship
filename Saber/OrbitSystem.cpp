#include "OrbitSystem.h"

using namespace DirectX;

size_t OrbitSystem::AddBody(const BodyDesc& desc) {
	const size_t parentNode{
		desc.parent == NoParent
			? TransformHierarchy::RootNode
			: m_bodies.at(desc.parent).node
	};

	m_bodies.push_back(Body{
		.desc{ desc },
		.node{ m_hierarchy.AddNode({}, parentNode) },
		.orbitAngle{ desc.orbitAngle }
	});

	return m_bodies.size() - 1;
}

void OrbitSystem::Update(float deltaTime) {
	for (Body& body : m_bodies) {
		body.orbitAngle += body.desc.orbitSpeed * deltaTime;
		body.spinAngle += body.desc.spinSpeed * deltaTime;

		// Only the orbital offset goes into the hierarchy, so that children
		// inherit where their parent is and nothing else.
		const XMMATRIX orbit{
			XMMatrixRotationY(body.orbitAngle)
		};
		XMStoreFloat3(
			&m_hierarchy.GetLocalTransform(body.node).position,
			XMVector3TransformCoord(
				XMVectorSet(body.desc.orbitRadius, 0.f, 0.f, 1.f),
				orbit
			)
		);
	}

	m_hierarchy.Update();
}

XMMATRIX OrbitSystem::GetWorldMatrix(size_t body) const {
	const Body& orbitingBody{ m_bodies.at(body) };

	return XMMatrixScaling(
			orbitingBody.desc.scale,
			orbitingBody.desc.scale,
			orbitingBody.desc.scale
		)
		* XMMatrixRotationY(orbitingBody.spinAngle)
		* m_hierarchy.GetWorldMatrix(orbitingBody.node);
}
