/**
 * @file Camera.h
 * @brief Camera hierarchy: abstract base, static, and interactive (orbit/fly) cameras.
 */
#pragma once

#include "Headers.h"

#include <limits>

/**
 * @brief Selects between perspective and orthographic projection.
 */
enum class ProjectionType : uint8_t {
    Perspective,   /**< @brief Standard perspective projection. */
    Orthographic   /**< @brief Parallel orthographic projection. */
};

/**
 * @brief Abstract base class providing projection and view matrices for a camera.
 *
 * Subclasses supply the world-space position, point-of-interest, and up direction;
 * this class computes all matrices and frustum planes from those primitives.
 */
class Camera {
public:
    float m_near{ 0.1f };                      /**< @brief Near clip-plane distance. */
    float m_far{ 100.0f };                     /**< @brief Far clip-plane distance. */
    float m_fov{ 60.0f };                      /**< @brief Vertical field of view in degrees (perspective only). */
    float m_aspectRatio{ 16.0f / 9.0f };       /**< @brief Viewport aspect ratio (width / height). */

    ProjectionType m_projectionType{ ProjectionType::Perspective }; /**< @brief Active projection mode. */
    float m_orthographicViewWidth{ 8.f };      /**< @brief Orthographic view half-width. */
    float m_orthographicViewHeight{ 8.f };     /**< @brief Orthographic view half-height. */

    /**
     * @brief Updates the stored aspect ratio.
     * @param newAspectRatio New width/height ratio.
     */
    void SetAspectRatio(float newAspectRatio);

    /** @brief Returns the world-space camera position. */
    virtual DirectX::XMFLOAT3 GetPosition() const = 0;

    /** @brief Returns the world-space point the camera is looking at. */
    virtual DirectX::XMFLOAT3 GetPointOfInterest() const = 0;

    /** @brief Returns the camera's up direction in world space. */
    virtual DirectX::XMFLOAT3 GetUpDirection() const = 0;

    /** @brief Returns the normalized view direction in world space. */
    virtual DirectX::XMFLOAT3 GetViewDirection() const = 0;

    /** @brief Builds and returns the perspective projection matrix. */
    DirectX::XMMATRIX GetPerspectiveMatrix() const;

    /** @brief Builds and returns the orthographic projection matrix. */
    DirectX::XMMATRIX GetOrthographicMatrix() const;

    /** @brief Returns view * perspective. */
    DirectX::XMMATRIX GetViewPerspectiveMatrix() const;

    /** @brief Returns view * orthographic. */
    DirectX::XMMATRIX GetViewOrthographicMatrix() const;

    /** @brief Returns the view (look-at) matrix. */
    DirectX::XMMATRIX GetViewMatrix() const;

    /** @brief Returns either the perspective or orthographic matrix depending on @ref m_projectionType. */
    DirectX::XMMATRIX GetProjectionMatrix() const;

    /** @brief Returns view * projection (respects @ref m_projectionType). */
    DirectX::XMMATRIX GetViewProjectionMatrix() const;

    /**
     * @brief Extracts the six view-frustum planes from the given view-projection matrix.
     * @param[out] planes            Array of six planes to populate (world space).
     * @param      viewProjectionMatrix Pointer to the matrix used for extraction;
     *             defaults to @ref GetViewProjectionMatrix() if @c nullptr.
     */
    void BuildViewFrustumPlanes(
        DirectX::XMFLOAT4 (&planes)[6],
        const DirectX::XMMATRIX* viewProjectionMatrix
    ) const;
};

/**
 * @brief A camera with a fixed, immutable world position and look target.
 */
class StaticCamera : public Camera {
    DirectX::XMFLOAT3 m_pos{ 0.0f, 0.0f, 0.0f }; /**< @brief World-space camera position. */
    DirectX::XMFLOAT3 m_poi{ 0.0f, 0.0f, 0.0f }; /**< @brief World-space point of interest. */
    DirectX::XMFLOAT3  m_up{ 0.0f, 1.0f, 0.0f }; /**< @brief Up direction. */

public:
    /**
     * @brief Constructs a static camera from explicit position, target, and up vectors.
     * @param pos    Camera world position.
     * @param poi    Look-at target.
     * @param upDir  Up direction (need not be normalised).
     */
    StaticCamera(
        const DirectX::XMFLOAT3& pos,
        const DirectX::XMFLOAT3& poi,
        const DirectX::XMFLOAT3& upDir
    );

    virtual DirectX::XMFLOAT3 GetPosition() const override;
    virtual DirectX::XMFLOAT3 GetPointOfInterest() const override;
    virtual DirectX::XMFLOAT3 GetUpDirection() const override;
    virtual DirectX::XMFLOAT3 GetViewDirection() const override;
};

/**
 * @brief Abstract base for cameras that respond to user input.
 *
 * Adds sensitivity and movement-speed parameters and mandates @c Rotate,
 * @c Move, and @c Update overrides.
 */
class DynamicCamera : public Camera {
protected:
    float m_sensitivity{ DirectX::XM_PI }; /**< @brief Angular sensitivity (radians per unit input). */
    float m_speed{ 5.f };                  /**< @brief Translation speed (world units per second). */

public:
    /**
     * @brief Applies a yaw/pitch (or theta/phi) rotation delta.
     * @param deltaTheta Horizontal rotation delta.
     * @param deltaPhi   Vertical rotation delta.
     */
    virtual void Rotate(float deltaTheta, float deltaPhi) = 0;

    /**
     * @brief Applies a movement impulse along forward, right, and up axes.
     * @param forwardCoef Forward/backward component.
     * @param rightCoef   Left/right component.
     * @param upCoef      Up/down component.
     */
    virtual void Move(float forwardCoef, float rightCoef, float upCoef) = 0;

    /**
     * @brief Integrates velocity/position by the elapsed frame time.
     * @param deltaTime Elapsed time since the last update, in seconds.
     */
    virtual void Update(float deltaTime) = 0;
};

/**
 * @brief Spherical-coordinate camera that orbits a fixed point of interest.
 *
 * The camera position is described by (radius, theta, phi) in spherical
 * coordinates centred on @c m_poi.  Scroll-wheel input adjusts the radius.
 */
class OrbitCamera : public DynamicCamera {
    DirectX::XMFLOAT3 m_poi{ 0.f, 0.f, 0.f }; /**< @brief World-space orbit centre. */

    float m_theta{ DirectX::XM_PIDIV2 }; /**< @brief Azimuthal angle (horizontal). */
    float m_phi{ DirectX::XM_PIDIV4 };   /**< @brief Elevation angle (vertical). */

    float m_deltaForward{}; /**< @brief Accumulated forward movement to apply on next Update. */
    float m_deltaRight{};   /**< @brief Accumulated lateral movement to apply on next Update. */

    float m_radius{ 5.f }; /**< @brief Distance from the orbit centre. */

public:
    virtual DirectX::XMFLOAT3 GetPosition() const override;
    virtual DirectX::XMFLOAT3 GetPointOfInterest() const override { return m_poi; }
    virtual DirectX::XMFLOAT3 GetUpDirection()	const override { return { 0.f, 1.f, 0.f }; }
    virtual DirectX::XMFLOAT3 GetViewDirection() const override;

    virtual void Rotate(float deltaTheta, float deltaPhi) override;
    virtual void Move(float forwardCoef, float rightCoef, float upCoef) override;
    virtual void Update(float deltaTime) override;

    /**
     * @brief Adjusts the orbit radius (scroll-wheel zoom).
     * @param deltaRadius Positive values zoom out, negative values zoom in.
     */
    void Zoom(float deltaRadius);
};

/**
 * @brief First-person free-flight camera controlled by yaw and pitch.
 *
 * Velocity integrates movement input and decays each frame, giving a
 * smooth, inertia-like feel.
 */
class FlyCamera : public DynamicCamera {
    DirectX::XMFLOAT3 m_position{ 0.f, 0.f, 0.f }; /**< @brief World-space camera position. */

    float m_yaw{};   /**< @brief Horizontal rotation angle in radians. */
    float m_pitch{}; /**< @brief Vertical rotation angle in radians. */

    DirectX::XMFLOAT3 m_velocity{ 0.f, 0.f, 0.f }; /**< @brief Current velocity in world space. */

public:
    virtual DirectX::XMFLOAT3 GetPosition() const override { return m_position; }
    virtual DirectX::XMFLOAT3 GetPointOfInterest() const override;
    virtual DirectX::XMFLOAT3 GetUpDirection() const override { return { 0.f, 1.f, 0.f }; }
    virtual DirectX::XMFLOAT3 GetViewDirection() const override;

    virtual void Rotate(float deltaYaw, float deltaPitch) override;
    virtual void Move(float forwardCoef, float rightCoef, float upCoef) override;
    virtual void Update(float deltaTime) override;
};
