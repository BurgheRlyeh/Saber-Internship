#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

#include <DirectXMath.h>

#include "TransformHierarchy.h"

class CommandList;
class DeviceContext;
class Scene;
class Texture;

class KatamariWorld {
public:
	struct Settings {
		float moveSpeed{ 8.f };
		float pickupRatio{ 0.75f };		// Largest item the ball can pick up, in its own radius
		float growthFactor{ 0.6f };		// How much of a collected item's volume is added to the ball
		float groundHalfSize{ 40.f };	// Half-extent of the square play area
		float sinkFactor{ 0.6f };		// How far items sink in: 0 - touching, 1 - center on the surface
		bool isBobbing{};				// Recalculate resting height every frame, instead of a fixed value
	};

	explicit KatamariWorld(const Settings& settings = {});

	Settings& GetSettings() { return m_settings; }
	const Settings& GetSettings() const { return m_settings; }

	struct ItemDesc {
		DirectX::XMFLOAT3 position{};
		float yaw{};

		float radius{ 1.f };
		float scale{ 1.f };
	};
	size_t AddItem(const ItemDesc& desc);
	size_t GetItemCount() const { return m_items.size(); }

	// Accumulates key presses (press adds, release subtracts), clamped on use
	void AddMoveInput(float forward, float right);

	// Which way "forward" points, usually the camera. Only the horizontal part is
	// used; a vector without one leaves the basis alone
	void SetMoveBasis(const DirectX::XMFLOAT3& viewDirection);

	void Update(float deltaTime);

	DirectX::XMMATRIX GetBallMatrix() const;
	DirectX::XMMATRIX GetItemMatrix(size_t item) const;

	DirectX::XMFLOAT3 GetBallPosition() const;
	float GetBallRadius() const { return m_ballRadius; }
	size_t GetCollectedCount() const { return m_collectedCount; }

private:
	struct Item {
		float radius{};
		float scale{ 1.f };
		size_t node{};
		bool isCollected{};

		// Offset from the ball center once collected, in the ball's unrotated frame
		DirectX::XMFLOAT3 attachOffset{};
	};

	Settings m_settings{};

	TransformHierarchy m_hierarchy{};
	size_t m_ballNode{};
	float m_ballRadius{ 1.f };

	// Height the ball center rides at, enough to clear its longest spur
	float m_restingHeight{ 1.f };

	std::vector<Item> m_items{};
	size_t m_collectedCount{};

	std::atomic<float> m_moveForward{};
	std::atomic<float> m_moveRight{};

	// Set and read on the render thread only, unlike the input above.
	DirectX::XMFLOAT3 m_moveBasisForward{ 0.f, 0.f, -1.f };

	void MoveBall(float deltaTime);
	void CollectReachableItems();

	// Lowest the ball's center may sit without anything poking through the floor.
	float ComputeRestingHeight() const;
};

bool DrawSettings(KatamariWorld::Settings& settings);

void BuildKatamariScene(
	Scene& scene,
	std::shared_ptr<DeviceContext> pDeviceContext,
	const std::shared_ptr<CommandList>& pCommandList,
	std::shared_ptr<Texture> pGBuffer
);
