// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MaterialDrivenShadowTypes.generated.h"

/**
 * How a shadow component determines the ground height it should project onto.
 *
 * The ground height feeds CustomData[2] (height above ground), which the material uses to slide and
 * stretch the shadow quad. It is intentionally decoupled from the shadow's X/Y position: the CPU only
 * ever writes the point directly beneath the unit, the projection itself happens in the vertex shader.
 */
UENUM(BlueprintType)
enum class EMaterialDrivenShadowGroundMode : uint8
{
	/** Cheapest. The owner's own Z is treated as the ground, so height above ground is always 0. Use for units that never leave the floor. */
	OwnerLocation UMETA(DisplayName = "Owner Location (No Trace)"),

	/** A budgeted, round-robin downward line trace finds the ground. Only a fixed number of traces run per frame across the whole world. */
	TraceDownward UMETA(DisplayName = "Trace Downward (Budgeted)"),

	/** The game supplies the ground height itself via SetGroundHeight() — free, and exact if you already know your terrain height. */
	Manual UMETA(DisplayName = "Manual (SetGroundHeight)")
};

/** Which instanced-static-mesh flavour the subsystem batches shadows into. */
UENUM(BlueprintType)
enum class EMaterialDrivenShadowBatchMode : uint8
{
	/**
	 * Plain instanced static mesh. Recommended default for moving units: instance culling happens on the
	 * GPU, and transform updates are a straight memcpy with no CPU-side acceleration structure to maintain.
	 */
	Instanced UMETA(DisplayName = "Instanced (ISM) — best for moving units"),

	/**
	 * Hierarchical instanced static mesh. Adds a CPU cluster tree for per-instance frustum culling and LOD.
	 * Only worth it when shadows barely move (props, wreckage, buildings): every instance that changes its
	 * translation flags the cluster tree as out of date and schedules an async rebuild.
	 */
	Hierarchical UMETA(DisplayName = "Hierarchical (HISM) — best for static props")
};

/**
 * Opaque handle identifying one shadow instance inside the subsystem.
 * Slots are never removed while the world is running, so a handle stays valid until it is released.
 */
USTRUCT(BlueprintType)
struct FMaterialDrivenShadowHandle
{
	GENERATED_BODY()

	/** Index of the batch (one batch per unique mesh + material pair). */
	UPROPERTY(BlueprintReadOnly, Category = "Material-Driven Shadows")
	int32 BatchIndex = INDEX_NONE;

	/** Index of the instance slot inside that batch. */
	UPROPERTY(BlueprintReadOnly, Category = "Material-Driven Shadows")
	int32 InstanceIndex = INDEX_NONE;

	bool IsValid() const { return BatchIndex != INDEX_NONE && InstanceIndex != INDEX_NONE; }

	void Reset()
	{
		BatchIndex = INDEX_NONE;
		InstanceIndex = INDEX_NONE;
	}
};

/** Snapshot of what the shadow subsystem is currently doing. Cheap to read; useful for HUDs and automated perf tests. */
USTRUCT(BlueprintType)
struct FMaterialDrivenShadowStats
{
	GENERATED_BODY()

	/** Number of instanced components in play. Each one is a single draw call. */
	UPROPERTY(BlueprintReadOnly, Category = "Material-Driven Shadows")
	int32 NumBatches = 0;

	/** Shadow components currently registered. */
	UPROPERTY(BlueprintReadOnly, Category = "Material-Driven Shadows")
	int32 NumRegisteredShadows = 0;

	/** Instance slots allocated across all batches (registered shadows + recycled free slots). */
	UPROPERTY(BlueprintReadOnly, Category = "Material-Driven Shadows")
	int32 NumInstanceSlots = 0;

	/** Slots that are allocated but currently unused and waiting to be recycled. */
	UPROPERTY(BlueprintReadOnly, Category = "Material-Driven Shadows")
	int32 NumFreeSlots = 0;

	/** Ground traces performed during the last update. Never exceeds the configured per-frame budget. */
	UPROPERTY(BlueprintReadOnly, Category = "Material-Driven Shadows")
	int32 NumGroundTracesLastUpdate = 0;

	/** Wall-clock cost of the last subsystem update, in milliseconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Material-Driven Shadows")
	float LastUpdateMilliseconds = 0.0f;

	/** Normalised ground direction the shadows are currently cast towards (light direction flattened to XY). */
	UPROPERTY(BlueprintReadOnly, Category = "Material-Driven Shadows")
	FVector ShadowDirection = FVector(1.0f, 0.0f, 0.0f);

	/** Stretch factor derived from the sun elevation: 1 at high noon, rising towards the per-component maximum at grazing angles. */
	UPROPERTY(BlueprintReadOnly, Category = "Material-Driven Shadows")
	float LightStretchFactor = 1.0f;
};

/**
 * Layout of the four per-instance custom-data floats the subsystem writes for every shadow.
 * The master material reads these through the PerInstanceCustomData node using exactly these indices.
 */
namespace MaterialDrivenShadows
{
	/** Number of per-instance custom data floats the subsystem allocates on every batch. */
	inline constexpr int32 NumCustomDataFloats = 4;

	/** CustomData[0] — shadow radius in world units. Lets the material scale its soft edge independently of the mesh. */
	inline constexpr int32 CustomDataIndex_Radius = 0;

	/** CustomData[1] — final opacity in 0..1, already including the height fade and any runtime scaling. */
	inline constexpr int32 CustomDataIndex_Opacity = 1;

	/** CustomData[2] — height of the unit above the ground in world units. Drives the vertex-shader slide. */
	inline constexpr int32 CustomDataIndex_HeightAboveGround = 2;

	/** CustomData[3] — light stretch factor (>= 1). Drives the vertex-shader stretch along the instance's local +X. */
	inline constexpr int32 CustomDataIndex_StretchFactor = 3;
}
