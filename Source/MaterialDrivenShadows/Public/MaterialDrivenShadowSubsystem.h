// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MaterialDrivenShadowTypes.h"
#include "MaterialDrivenShadowSubsystem.generated.h"

class ADirectionalLight;
class UInstancedStaticMeshComponent;
class UMaterialDrivenShadowComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * One batch = one unique (mesh, material) pair = one instanced component = one draw call.
 * Instance slots are allocated once and then recycled; instances are never removed while the world runs,
 * because removal would swap indices around and invalidate every handle above the removed one.
 */
USTRUCT()
struct FMaterialDrivenShadowBatch
{
	GENERATED_BODY()

	/** Mesh this batch renders. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** Material override applied to slot 0, or null to keep the mesh's own material. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> Material = nullptr;

	/** The single instanced component every shadow in this batch lives in. */
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> InstancedComponent = nullptr;

	/** Slot index -> owning component. Null entries are free slots. */
	TArray<TWeakObjectPtr<UMaterialDrivenShadowComponent>> Slots;

	/** Slot indices ready to be handed out again. */
	TArray<int32> FreeSlots;

	/** Scratch buffers reused every update so the hot path allocates nothing. */
	TArray<FTransform> ScratchTransforms;
	TArray<float> ScratchCustomData;
};

/**
 * Central manager for all material-driven shadows in a world. One instance per game/PIE world.
 *
 * Every frame it performs a single batched pass: it resolves the scene's primary directional light, walks
 * the registered shadow components, and writes one transform plus four custom-data floats per instance
 * straight into the instanced component. There is no per-unit component, no per-unit tick and no per-unit
 * draw call — 10,000 units cost one draw call per mesh/material pair.
 *
 * The CPU deliberately does the bare minimum: it positions each quad at the ground point directly beneath
 * its unit and yaws it to face away from the light. The actual projection — sliding the quad along the
 * light direction as the unit rises, stretching it at grazing sun angles, and blending it into uneven
 * ground — happens in the material's vertex and pixel shaders from the custom data. See
 * Docs/DOCUMENTATION.md for the master-material graph.
 */
UCLASS()
class MATERIALDRIVENSHADOWS_API UMaterialDrivenShadowSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Claim an instance slot for the given component. Returns an invalid handle when the shadow cannot be batched. */
	FMaterialDrivenShadowHandle RegisterShadowComponent(UMaterialDrivenShadowComponent* ShadowComponent);

	/** Release the component's instance slot and hide its instance immediately. */
	void UnregisterShadowComponent(UMaterialDrivenShadowComponent* ShadowComponent);

	/** Turn the whole system on or off at runtime. Disabling hides every instance without destroying the batches. */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows")
	void SetShadowsEnabled(bool bNewEnabled);

	/** True while the system is updating shadows. */
	UFUNCTION(BlueprintPure, Category = "Material-Driven Shadows")
	bool AreShadowsEnabled() const { return bShadowsEnabled; }

	/** Snapshot of batch counts, slot usage and last-update timings. */
	UFUNCTION(BlueprintPure, Category = "Material-Driven Shadows")
	const FMaterialDrivenShadowStats& GetStats() const { return Stats; }

	/**
	 * Ignore the scene's directional light and cast shadows along an explicit direction instead.
	 * Useful for stylised top-down games that want a fixed shadow direction regardless of the sun.
	 */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows")
	void SetLightDirectionOverride(FVector NewLightDirection);

	/** Go back to following the scene's primary directional light. */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows")
	void ClearLightDirectionOverride();

	/** Normalised direction the light travels in (pointing away from the sun). */
	UFUNCTION(BlueprintPure, Category = "Material-Driven Shadows")
	FVector GetLightDirection() const { return LightDirection; }

	/** Ground-plane direction the shadows extend towards. */
	UFUNCTION(BlueprintPure, Category = "Material-Driven Shadows")
	FVector GetShadowDirection() const { return ShadowDirection; }

	/** Number of instanced components in play. Each one is exactly one draw call. */
	UFUNCTION(BlueprintPure, Category = "Material-Driven Shadows")
	int32 GetNumBatches() const { return Batches.Num(); }

	/** Force an update on the next tick even if the update interval has not elapsed. */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows")
	void RequestImmediateUpdate() { bForceUpdate = true; }

private:
	/** Find the batch for this mesh/material pair, creating the instanced component on first use. */
	int32 FindOrCreateBatch(UStaticMesh* Mesh, UMaterialInterface* Material);

	/** Spawn the transient holder actor every batch component is attached to. */
	void EnsureBatchHolder();

	/** Build and register a fresh instanced component for a batch. */
	UInstancedStaticMeshComponent* CreateBatchComponent(UStaticMesh* Mesh, UMaterialInterface* Material);

	/** Resolve the project's default shadow mesh/material, loading them on first use. */
	UStaticMesh* ResolveDefaultMesh();
	UMaterialInterface* ResolveDefaultMaterial();

	/** Re-resolve the scene's primary directional light and derive the shadow direction and stretch factor. */
	void UpdateLightDirection(float DeltaTime);

	/** Run at most MaxGroundTracesPerUpdate downward traces, continuing where the previous update stopped. */
	void UpdateGroundTraces();

	/** Write one transform and four custom-data floats per instance, then push both arrays in a single call. */
	void UpdateBatches();

	/** Collapse an instance to zero scale so it rasterises nothing. */
	void HideInstance(FMaterialDrivenShadowBatch& Batch, int32 InstanceIndex);

	/** Every batch created so far. Indices are stable; batches live as long as the world. */
	UPROPERTY(Transient)
	TArray<FMaterialDrivenShadowBatch> Batches;

	/** Transient actor owning every batch component. Never saved, never visible in the outliner's saved state. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> BatchHolder = nullptr;

	/** Lazily loaded project defaults. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> DefaultShadowMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DefaultShadowMaterial = nullptr;

	/** Flat list of registered components, used to drive the round-robin ground trace budget. */
	TArray<TWeakObjectPtr<UMaterialDrivenShadowComponent>> RegisteredComponents;

	/** Primary directional light, re-resolved every LightRefreshInterval seconds. */
	TWeakObjectPtr<ADirectionalLight> CachedDirectionalLight;

	/** Cursor into RegisteredComponents for the round-robin trace budget. */
	int32 GroundTraceCursor = 0;

	/** Accumulated time since the last update, compared against the configured update interval. */
	float TimeSinceLastUpdate = 0.0f;

	/** Accumulated time since the last directional-light scan. */
	float TimeSinceLightRefresh = 0.0f;

	/** Direction the light travels in. */
	FVector LightDirection = FVector(0.0f, 0.0f, -1.0f);

	/** Light direction flattened onto the ground plane and normalised. */
	FVector ShadowDirection = FVector(1.0f, 0.0f, 0.0f);

	/** 1 at high noon, growing as the sun approaches the horizon. Clamped per component by MaxShadowStretch. */
	float LightStretchFactor = 1.0f;

	/** Explicit light direction set through SetLightDirectionOverride(). */
	FVector LightDirectionOverride = FVector::ZeroVector;
	bool bUseLightDirectionOverride = false;

	/** Runtime master switch, seeded from the project settings. */
	bool bShadowsEnabled = true;

	/** True when the update interval should be ignored for one tick. */
	bool bForceUpdate = false;

	/** True once every instance has been hidden after the system was disabled. */
	bool bInstancesHiddenWhileDisabled = false;

	/** Warn only once per world about a missing default mesh or material. */
	bool bLoggedMissingMesh = false;
	bool bLoggedMissingMaterial = false;

	/** Published through GetStats(). */
	FMaterialDrivenShadowStats Stats;
};
