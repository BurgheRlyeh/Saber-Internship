/**
 * @file Vertices.h
 * @brief Defines common vertex layout structures used by the rendering pipeline.
 */
#pragma once

#include "Headers.h"

/**
 * @brief Vertex with position, normal, and RGBA color components.
 */
struct VertexPosNormCl {
    DirectX::XMFLOAT3 position{ 0.f, 0.f, 0.f }; /**< @brief World-space vertex position. */
    DirectX::XMFLOAT3 norm{ 0.f, 0.f, 0.f };     /**< @brief Surface normal vector. */
    DirectX::XMFLOAT4 color{};                    /**< @brief RGBA vertex color. */
};

/**
 * @brief Vertex with position and UV texture coordinates.
 */
struct VertexPosUV {
    DirectX::XMFLOAT3 position{ 0.f, 0.f, 0.f }; /**< @brief World-space vertex position. */
    DirectX::XMFLOAT2 uv{ 0.f, 0.f };            /**< @brief Texture UV coordinates. */
};

/**
 * @brief Vertex with position, normal, tangent, and UV texture coordinates.
 *
 * Used for normal-mapped geometry where a full TBN frame is required.
 */
struct VertexPosNormTangUV {
    DirectX::XMFLOAT3 position{ 0.f, 0.f, 0.f }; /**< @brief World-space vertex position. */
    DirectX::XMFLOAT3 norm{ 0.f, 0.f, 0.f };     /**< @brief Surface normal vector. */
    DirectX::XMFLOAT3 tang{ 0.f, 0.f, 0.f };     /**< @brief Surface tangent vector. */
    DirectX::XMFLOAT2 uv{ 0.f, 0.f };            /**< @brief Texture UV coordinates. */
};
