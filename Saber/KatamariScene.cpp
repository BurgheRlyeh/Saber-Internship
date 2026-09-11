#include "KatamariScene.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <random>
#include <vector>

#include "Camera.h"
#include "MeshRenderObject.h"
#include "Scene.h"

#include "imgui.h"

using namespace DirectX;

namespace {
	constexpr float MinBallRadius{ 1e-3f };

	struct ItemKind {
		std::filesystem::path model{};
		std::wstring albedo{};
		std::wstring normal{};
		float minScale{};
		float maxScale{};
	};

	const std::wstring DefaultNormalMap{ L"defaultNM.dds" };

	const std::vector<ItemKind> ItemKinds{
		{ L"../../Resources/StaticModels/cube.glb",     L"cube.dds",     DefaultNormalMap, 0.35f, 0.9f },
		{ L"../../Resources/StaticModels/cube.glb",		L"nugget1k.dds", DefaultNormalMap, 0.4f,  1.1f },
		{ L"../../Resources/StaticModels/sphere.glb",	L"nugget4k.dds", DefaultNormalMap, 0.8f,  1.8f },
		{ L"../../Resources/StaticModels/sphere.glb",   L"p2.dds",       DefaultNormalMap, 0.5f,  1.4f }
	};

	constexpr size_t ItemCount{ 60 };
	constexpr float GroundHalfSize{ 40.f };

	// One slot of LIGHTS_MAX_COUNT is taken by the light Renderer adds to every scene
	constexpr size_t LightCount{ 8 };
	constexpr float LightMinHeight{ 2.5f };
	constexpr float LightMaxHeight{ 9.f };

	// Attenuation is 1/d^2, so reaching a few units costs power in the tens
	constexpr float LightMinPower{ 30.f };
	constexpr float LightMaxPower{ 90.f };

	// Dimmed from the default so the point lights are actually visible
	constexpr float AmbientPower{ 0.12f };

	// Camera distance in ball radii, so it pulls back as the ball grows
	constexpr float CameraDistancePerRadius{ 6.f };
	constexpr float CameraMinDistance{ 8.f };
}

KatamariWorld::KatamariWorld(const Settings& settings)
	: m_settings(settings)
{
	// Ball is node 0
	m_ballNode = m_hierarchy.AddNode();
	m_restingHeight = m_ballRadius;
	m_hierarchy.GetLocalTransform(m_ballNode).position = { 0.f, m_restingHeight, 0.f };
	m_hierarchy.Update();
}

size_t KatamariWorld::AddItem(const ItemDesc& desc) {
	Transform local{};
	local.position = desc.position;
	XMStoreFloat4(&local.rotation, XMQuaternionRotationRollPitchYaw(0.f, desc.yaw, 0.f));
	local.scale = { desc.scale, desc.scale, desc.scale };

	m_items.push_back(Item{
		.radius{ desc.radius },
		.scale{ desc.scale },
		.node{ m_hierarchy.AddNode(local) }
	});

	m_hierarchy.Update();

	return m_items.size() - 1;
}

void KatamariWorld::AddMoveInput(float forward, float right) {
	m_moveForward.fetch_add(forward);
	m_moveRight.fetch_add(right);
}

void KatamariWorld::SetMoveBasis(const XMFLOAT3& viewDirection) {
	const XMVECTOR horizontal{ XMVectorSet(viewDirection.x, 0.f, viewDirection.z, 0.f) };
	if (XMVector3Equal(horizontal, XMVectorZero())) {
		return;
	}

	XMStoreFloat3(&m_moveBasisForward, XMVector3Normalize(horizontal));
}

void KatamariWorld::Update(float deltaTime) {
	MoveBall(deltaTime);

	// While bobbing the height follows the rotation, so it is redone every turn
	if (m_settings.isBobbing) {
		m_restingHeight = ComputeRestingHeight();
	}
	m_hierarchy.GetLocalTransform(m_ballNode).position.y = m_restingHeight;

	m_hierarchy.Update();

	CollectReachableItems();
	m_hierarchy.Update();
}

void KatamariWorld::MoveBall(float deltaTime) {
	// Clamped, not normalized: a missed key release can leave the sum above one
	const float forward{ std::clamp(m_moveForward.load(), -1.f, 1.f) };
	const float right{ std::clamp(m_moveRight.load(), -1.f, 1.f) };

	// Movement in the camera frame, so W always rolls away from the viewer.
	// Right-handed engine (XMMatrixLookAtRH): on-screen right is cross(forward, up)
	const XMVECTOR forwardDir{ XMLoadFloat3(&m_moveBasisForward) };
	const XMVECTOR rightDir{ XMVector3Normalize(
		XMVector3Cross(forwardDir, XMVectorSet(0.f, 1.f, 0.f, 0.f))
	) };

	XMVECTOR direction{ XMVectorAdd(
		XMVectorScale(forwardDir, forward),
		XMVectorScale(rightDir, right)
	) };
	if (XMVector3Equal(direction, XMVectorZero())) {
		return;
	}
	direction = XMVector3Normalize(direction);

	const float distance{ m_settings.moveSpeed * deltaTime };

	Transform& ball{ m_hierarchy.GetLocalTransform(m_ballNode) };

	XMVECTOR position{ XMVectorAdd(
		XMLoadFloat3(&ball.position),
		XMVectorScale(direction, distance)
	) };

	// Height is left to Update, which knows the new rotation
	const float limit{ std::max(m_settings.groundHalfSize - m_ballRadius, 0.f) };
	position = XMVectorClamp(
		position,
		XMVectorSet(-limit, -FLT_MAX, -limit, 0.f),
		XMVectorSet(limit, FLT_MAX, limit, 0.f)
	);
	XMStoreFloat3(&ball.position, position);

	// Rolling without slipping: angle = distance / radius, axis across the travel
	const XMVECTOR axis{ XMVector3Cross(XMVectorSet(0.f, 1.f, 0.f, 0.f), direction) };
	const XMVECTOR delta{ XMQuaternionRotationAxis(axis, distance / std::max(m_ballRadius, MinBallRadius)) };

	XMStoreFloat4(
		&ball.rotation,
		XMQuaternionNormalize(XMQuaternionMultiply(XMLoadFloat4(&ball.rotation), delta))
	);
}

void KatamariWorld::CollectReachableItems() {
	const XMVECTOR ballPosition{ m_hierarchy.GetWorldMatrix(m_ballNode).r[3] };

	for (Item& item : m_items) {
		if (item.isCollected || item.radius > m_ballRadius * m_settings.pickupRatio) {
			continue;
		}

		const XMVECTOR itemPosition{ m_hierarchy.GetWorldMatrix(item.node).r[3] };
		const float distance{ XMVectorGetX(XMVector3Length(
			XMVectorSubtract(itemPosition, ballPosition)
		)) };

		// The bounding spheres are the whole test: touch means pick up.
		if (distance > m_ballRadius + item.radius) {
			continue;
		}

		// Sink it in before attaching: left at the touch point it sticks out whole
		// and dips through the floor once rotation brings it down
		const XMVECTOR outward{ distance > 0.f
			? XMVectorScale(XMVectorSubtract(itemPosition, ballPosition), 1.f / distance)
			: XMVectorSet(0.f, 1.f, 0.f, 0.f)
		};
		const float sunkDistance{
			(m_ballRadius + item.radius)
			- std::clamp(m_settings.sinkFactor, 0.f, 1.f) * item.radius
		};
		XMStoreFloat3(
			&m_hierarchy.GetLocalTransform(item.node).position,
			XMVectorAdd(ballPosition, XMVectorScale(outward, sunkDistance))
		);
		m_hierarchy.UpdateNode(item.node);

		// Keep the world placement, or the item snaps to the ball's origin
		m_hierarchy.SetParentKeepingWorld(item.node, m_ballNode);
		item.isCollected = true;
		++m_collectedCount;

		XMStoreFloat3(
			&item.attachOffset,
			XMLoadFloat3(&m_hierarchy.GetLocalTransform(item.node).position)
		);

		m_ballRadius = std::cbrtf(
			m_ballRadius * m_ballRadius * m_ballRadius
			+ m_settings.growthFactor * item.radius * item.radius * item.radius
		);

		m_restingHeight = ComputeRestingHeight();
	}
}

float KatamariWorld::ComputeRestingHeight() const {
	float height{ m_ballRadius };

	for (const Item& item : m_items) {
		if (!item.isCollected) {
			continue;
		}

		const XMVECTOR offset{ XMLoadFloat3(&item.attachOffset) };

		if (m_settings.isBobbing) {
			// Only the spur currently at the bottom holds the ball up
			const XMVECTOR rotated{ XMVector3Rotate(
				offset,
				XMLoadFloat4(&m_hierarchy.GetLocalTransform(m_ballNode).rotation)
			) };
			height = std::max(height, item.radius - XMVectorGetY(rotated));
		}
		else {
			// Clear the longest spur wherever it points: steady, but never rocks
			height = std::max(
				height,
				XMVectorGetX(XMVector3Length(offset)) + item.radius
			);
		}
	}

	return height;
}

XMMATRIX KatamariWorld::GetBallMatrix() const {
	return XMMatrixScaling(m_ballRadius, m_ballRadius, m_ballRadius)
		* m_hierarchy.GetWorldMatrix(m_ballNode);
}

XMMATRIX KatamariWorld::GetItemMatrix(size_t item) const {
	return m_hierarchy.GetWorldMatrix(m_items.at(item).node);
}

XMFLOAT3 KatamariWorld::GetBallPosition() const {
	XMFLOAT3 position{};
	XMStoreFloat3(&position, m_hierarchy.GetWorldMatrix(m_ballNode).r[3]);
	return position;
}


bool DrawSettings(KatamariWorld::Settings& settings) {
	bool isChanged{};

	ImGui::SeparatorText("Movement");
	isChanged |= ImGui::SliderFloat("Speed", &settings.moveSpeed, 1.f, 30.f);

	ImGui::SeparatorText("Collecting");
	isChanged |= ImGui::SliderFloat("Pickup ratio", &settings.pickupRatio, 0.1f, 1.f);
	isChanged |= ImGui::SliderFloat("Growth", &settings.growthFactor, 0.f, 3.f);
	isChanged |= ImGui::SliderFloat("Sink", &settings.sinkFactor, 0.f, 1.f);

	ImGui::SeparatorText("Rolling");
	isChanged |= ImGui::Checkbox("Bobbing", &settings.isBobbing);
	ImGui::SetItemTooltip("Rides on whichever spur is at the bottom, instead of a fixed height");

	return isChanged;
}

void BuildKatamariScene(
	Scene& scene,
	std::shared_ptr<DeviceContext> pDeviceContext,
	const std::shared_ptr<CommandList>& pCommandList,
	std::shared_ptr<Texture> pGBuffer
) {
	auto pWorld{ std::make_shared<KatamariWorld>(KatamariWorld::Settings{
		.groundHalfSize{ GroundHalfSize }
	}) };
	auto pItemHandles{ std::make_shared<std::vector<RenderObjectHandle>>() };

	// Unit plane scaled to the play area, static
	scene.AddObject(
		RenderSubsystemType::Default,
		TestTextureRenderObject::CreatePlane(
			pDeviceContext, pCommandList, pGBuffer,
			L"Brick.dds", L"BrickNM.dds",
			GroundHalfSize,
			XMMatrixScaling(2.f * GroundHalfSize, 1.f, 2.f * GroundHalfSize)
		)
	);

	// The ball is a unit sphere; KatamariWorld bakes its radius into the matrix.
	const RenderObjectHandle ballHandle{ scene.AddObject(
		RenderSubsystemType::Dynamic,
		TestTextureRenderObject::CreateSphere(
			pDeviceContext, pCommandList, pGBuffer, L"Kitty.dds", DefaultNormalMap
		)
	) };

	std::mt19937 random{ std::random_device{}() };
	std::uniform_real_distribution<float> positionDist{ -GroundHalfSize + 2.f, GroundHalfSize - 2.f };
	std::uniform_real_distribution<float> yawDist{ 0.f, XM_2PI };
	std::uniform_real_distribution<float> unitDist{ 0.f, 1.f };
	std::uniform_int_distribution<size_t> kindDist{ 0, ItemKinds.size() - 1 };

	for (size_t i{}; i < ItemCount; ++i) {
		const ItemKind& kind{ ItemKinds[kindDist(random)] };
		const float scale{ kind.minScale + unitDist(random) * (kind.maxScale - kind.minScale) };

		std::shared_ptr<MeshRenderObject<ModelBuffer>> pObject{
			TestTextureRenderObject::CreateModelFromGLTF(
				pDeviceContext, pCommandList, kind.model, pGBuffer, kind.albedo, kind.normal
			)
		};

		// The game tracks items by origin, but the sphere is centered on the AABB:
		// grow the radius by that offset so it still covers the mesh
		const BoundingSphere& bounds{ pObject->GetBoundingSphere() };
		const float centerOffset{ XMVectorGetX(XMVector3Length(XMLoadFloat3(&bounds.center))) };
		const float radius{ (bounds.radius + centerOffset) * scale };

		// Sit on the ground: the origin is not the lowest point. Yaw leaves the
		// vertical extent alone, so the box bottom is enough
		const float groundOffset{ -pObject->GetAABB().min.y * scale };

		const size_t item{ pWorld->AddItem({
			.position{ positionDist(random), groundOffset, positionDist(random) },
			.yaw{ yawDist(random) },
			.radius{ radius },
			.scale{ scale }
		}) };

		pItemHandles->push_back(scene.AddObject(RenderSubsystemType::Dynamic, pObject));
		assert(pItemHandles->size() == item + 1);	// handle index must track item index
	}

	scene.SetAmbientLight({ 1.f, 1.f, 1.f }, AmbientPower);

	std::uniform_real_distribution<float> heightDist{ LightMinHeight, LightMaxHeight };
	std::uniform_real_distribution<float> powerDist{ LightMinPower, LightMaxPower };
	// Saturated colors: one channel is left near full so lights stay distinct
	std::uniform_real_distribution<float> channelDist{ 0.15f, 1.f };

	for (size_t i{}; i < LightCount; ++i) {
		const DirectX::XMFLOAT3 color{ channelDist(random), channelDist(random), channelDist(random) };
		const float power{ powerDist(random) };

		scene.AddLightSource(
			{ positionDist(random), heightDist(random), positionDist(random), 1.f },
			color,
			color,
			power,
			0.25f * power
		);
	}

	// WASD rolls the ball. The key handler emits negative forward for W, which
	// suits an orbit camera pulling in -- the ball wants the opposite sign
	scene.SetMovementHandler([pWorld](float forwardCoef, float rightCoef) {
		pWorld->AddMoveInput(-forwardCoef, rightCoef);
	});

	scene.SetSettingsUI([pWorld] {
		if (ImGui::Begin("Katamari")) {
			ImGui::Text("Radius: %.2f", pWorld->GetBallRadius());
			ImGui::Text("Collected: %zu / %zu", pWorld->GetCollectedCount(), pWorld->GetItemCount());

			DrawSettings(pWorld->GetSettings());
		}
		ImGui::End();
	});

	scene.SetSimulation([pWorld, pItemHandles, ballHandle](float deltaTime, Scene& simulatedScene) {
		const std::shared_ptr<Camera> pCamera{ simulatedScene.GetCurrentCamera() };

		// Before the step, so this frame already moves along the current view
		if (pCamera) {
			pWorld->SetMoveBasis(pCamera->GetViewDirection());
		}

		pWorld->Update(deltaTime);

		simulatedScene.UpdateObjectMatrix(ballHandle, pWorld->GetBallMatrix());
		for (size_t item{}; item < pItemHandles->size(); ++item) {
			simulatedScene.UpdateObjectMatrix((*pItemHandles)[item], pWorld->GetItemMatrix(item));
		}

		// Follow the ball, backing off as it grows; other cameras just stay put
		if (auto pOrbitCamera{ std::dynamic_pointer_cast<OrbitCamera>(pCamera) }) {
			OrbitCamera::Settings& settings{ pOrbitCamera->GetSettings() };
			settings.poi = pWorld->GetBallPosition();
			settings.radius = std::max(
				CameraMinDistance,
				CameraDistancePerRadius * pWorld->GetBallRadius()
			);
		}
	});
}
