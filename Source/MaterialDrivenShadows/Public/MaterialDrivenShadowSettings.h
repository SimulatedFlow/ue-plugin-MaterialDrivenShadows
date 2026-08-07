// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "MaterialDrivenShadowTypes.h"
#include "MaterialDrivenShadowSettings.generated.h"

/**
 * Project-wide defaults for the Material-Driven Shadows subsystem.
 * Edit under Project Settings > Plugins > Material-Driven Shadows; stored in DefaultGame.ini.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Material-Driven Shadows"))
class MATERIALDRIVENSHADOWS_API UMaterialDrivenShadowSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMaterialDrivenShadowSettings();

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override;

	/** Convenience accessor. Never returns null. */
	static const UMaterialDrivenShadowSettings& Get();

	/** Master switch. When false the subsystem stays dormant and creates no components at all. */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	bool bEnableShadows = true;

	/** Quad used for shadows that do not override the mesh themselves. Must be a flat, XY-aligned plane facing +Z. */
	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	FSoftObjectPath DefaultShadowMesh;

	/** Master shadow material applied to every batch that does not override it. Leave empty to keep the mesh's own material. */
	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (AllowedClasses = "/Script/Engine.MaterialInterface"))
	FSoftObjectPath DefaultShadowMaterial;

	/**
	 * Radius of the default mesh in world units at scale 1 (the engine plane is 100x100 uu, so 50).
	 * The subsystem divides the requested shadow radius by this to derive the instance scale.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ClampMin = "0.01", UIMin = "1.0"))
	float ReferenceMeshRadius = 50.0f;

	/** Which instanced component the batches use. ISM is the right default for units that move every frame. */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	EMaterialDrivenShadowBatchMode BatchMode = EMaterialDrivenShadowBatchMode::Instanced;

	/**
	 * Seconds between shadow updates. 0 updates every frame.
	 * Values around 0.033 (30 Hz) halve the CPU cost on huge unit counts and are usually invisible.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "0.0", UIMax = "0.2"))
	float UpdateInterval = 0.0f;

	/**
	 * Upper bound on ground line traces per update, shared by every shadow in Trace Downward mode.
	 * Components are served round-robin, so 10,000 units at 256 traces per frame refresh their ground
	 * height roughly every 40 frames — plenty, since ground height changes slowly.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "0", UIMax = "2048"))
	int32 MaxGroundTracesPerUpdate = 256;

	/** Collision channel used by the ground trace. */
	UPROPERTY(Config, EditAnywhere, Category = "Performance")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_WorldStatic;

	/** Lifted this far above the detected ground, purely as a cheap first line of defence against Z-fighting. */
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "0.0", UIMax = "20.0"))
	float GroundOffset = 2.0f;

	/**
	 * Multiplier applied to the batch component's BoundsScale.
	 * The material moves vertices outside the mesh bounds, so culling needs headroom or shadows pop at
	 * screen edges. Keep this at or above the largest Max Shadow Stretch you use.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "1.0", UIMax = "16.0"))
	float BatchBoundsScale = 4.0f;

	/** Distance at which instances start fading out. 0 disables distance culling. */
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "0"))
	int32 InstanceStartCullDistance = 0;

	/** Distance at which instances are fully culled. 0 disables distance culling. */
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "0"))
	int32 InstanceEndCullDistance = 0;

	/** Seconds between re-scans for the scene's primary directional light. The cached light is re-resolved immediately if it goes away. */
	UPROPERTY(Config, EditAnywhere, Category = "Lighting", meta = (ClampMin = "0.1", UIMax = "10.0"))
	float LightRefreshInterval = 2.0f;

	/** Fallback light direction used when the level has no usable directional light (straight down). */
	UPROPERTY(Config, EditAnywhere, Category = "Lighting")
	FVector FallbackLightDirection = FVector(0.0f, 0.0f, -1.0f);
};
