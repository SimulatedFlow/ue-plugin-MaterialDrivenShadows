// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "MaterialDrivenShadowTypes.h"
#include "MaterialDrivenShadowComponent.generated.h"

class UMaterialDrivenShadowSubsystem;
class UMaterialInterface;
class UStaticMesh;

/**
 * Attach this to anything that should drop a batched blob shadow — RTS units, horde enemies, projectiles,
 * dropped loot. The component itself never ticks and owns no primitive: on BeginPlay it claims one instance
 * slot from UMaterialDrivenShadowSubsystem, and from then on the subsystem writes that slot's transform and
 * per-instance custom data in one batched pass per frame.
 *
 * Every shadow that shares a mesh and a material ends up in the same instanced component, which means one
 * draw call for the whole army rather than one per unit.
 *
 * Changing ShadowMesh or ShadowMaterial at runtime moves the shadow into a different batch; use
 * SetShadowMeshAndMaterial() so the old slot is released cleanly.
 */
UCLASS(ClassGroup = Rendering, meta = (BlueprintSpawnableComponent), HideCategories = (Physics, Collision, Navigation))
class MATERIALDRIVENSHADOWS_API UMaterialDrivenShadowComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UMaterialDrivenShadowComponent();

	// USceneComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnregister() override;

	//~ Authoring -------------------------------------------------------------------------------------

	/** Base radius of the shadow in world units. Converted into the instance scale via the settings' Reference Mesh Radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows", meta = (ClampMin = "0.0", UIMin = "1.0", UIMax = "1000.0"))
	float ShadowRadius = 100.0f;

	/** Opacity of the shadow when the unit stands on the ground, before any height fade. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BaseOpacity = 0.8f;

	/**
	 * Upper bound on the stretch the material may apply when the sun is low.
	 * 1 keeps the shadow perfectly round; 2 lets it grow to twice its length at grazing light angles.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows", meta = (ClampMin = "1.0", UIMax = "8.0"))
	float MaxShadowStretch = 2.0f;

	/** Optional per-unit shadow quad. Leave empty to use the project's default mesh. Units sharing a mesh share a draw call. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows")
	TObjectPtr<UStaticMesh> CustomShadowMesh = nullptr;

	/** Optional per-unit shadow material. Leave empty to use the project's default master material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows")
	TObjectPtr<UMaterialInterface> CustomShadowMaterial = nullptr;

	/** Extra offset applied to the shadow's ground position, in the component's own space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows")
	FVector ShadowOffset = FVector::ZeroVector;

	/** How the ground beneath this unit is determined. Ground height drives the vertex-shader slide and the height fade. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows|Ground")
	EMaterialDrivenShadowGroundMode GroundMode = EMaterialDrivenShadowGroundMode::TraceDownward;

	/** How far down the ground trace reaches before giving up, in world units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows|Ground", meta = (ClampMin = "1.0", UIMax = "10000.0", EditCondition = "GroundMode == EMaterialDrivenShadowGroundMode::TraceDownward", EditConditionHides))
	float MaxGroundTraceDistance = 2000.0f;

	/** Height above ground at which the shadow starts fading, in world units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows|Fade", meta = (ClampMin = "0.0", UIMax = "5000.0"))
	float FadeStartHeight = 250.0f;

	/** Height above ground at which the shadow has faded out completely. Must be greater than Fade Start Height to have any effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows|Fade", meta = (ClampMin = "0.0", UIMax = "10000.0"))
	float FadeEndHeight = 1500.0f;

	/** When false the shadow is skipped entirely — its slot stays allocated but renders nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows")
	bool bShadowEnabled = true;

	/** Register with the subsystem automatically on BeginPlay. Turn off to control registration yourself. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadows")
	bool bAutoRegister = true;

	//~ Runtime API -----------------------------------------------------------------------------------

	/** Claim an instance slot. Safe to call twice; the second call is a no-op. */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows")
	void RegisterShadow();

	/** Release the instance slot. The slot is recycled by the next shadow that needs one. */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows")
	void UnregisterShadow();

	/** True while this component owns an instance slot. */
	UFUNCTION(BlueprintPure, Category = "Material-Driven Shadows")
	bool IsShadowRegistered() const { return Handle.IsValid(); }

	/** Set the base opacity. Takes effect on the next subsystem update — no render-state churn. */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows")
	void SetShadowOpacity(float NewOpacity);

	/** Set the shadow radius in world units. Takes effect on the next subsystem update. */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows")
	void SetShadowRadius(float NewRadius);

	/** Enable or disable this single shadow without releasing its slot. */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows")
	void SetShadowEnabled(bool bNewEnabled);

	/** Set the maximum stretch the material may apply at grazing light angles. */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows")
	void SetMaxShadowStretch(float NewMaxStretch);

	/**
	 * Supply the world-space Z of the ground beneath this unit.
	 * Only used in Manual ground mode; the subsystem writes this itself in Trace Downward mode.
	 */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows")
	void SetGroundHeight(float NewGroundWorldZ);

	/** Last known world-space Z of the ground beneath this unit. */
	UFUNCTION(BlueprintPure, Category = "Material-Driven Shadows")
	float GetGroundHeight() const { return GroundWorldZ; }

	/** True once a ground height has actually been established (trace hit, or manually supplied). */
	UFUNCTION(BlueprintPure, Category = "Material-Driven Shadows")
	bool HasGroundHeight() const { return bHasGroundHeight; }

	/**
	 * Swap the mesh and/or material this shadow uses, moving it into the matching batch.
	 * Pass null for either argument to fall back to the project default.
	 */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows")
	void SetShadowMeshAndMaterial(UStaticMesh* NewMesh, UMaterialInterface* NewMaterial);

	/** Handle of the instance slot this component owns, or an invalid handle when unregistered. */
	const FMaterialDrivenShadowHandle& GetShadowHandle() const { return Handle; }

	/** Used by the subsystem to record the result of a budgeted ground trace. */
	void ApplyTracedGroundHeight(float NewGroundWorldZ, bool bHit);

	/** Used by the subsystem when a slot is claimed or released. */
	void SetShadowHandle(const FMaterialDrivenShadowHandle& NewHandle) { Handle = NewHandle; }

private:
	/** Resolves the subsystem for this component's world, or null outside of a game world. */
	UMaterialDrivenShadowSubsystem* GetShadowSubsystem() const;

	/** Slot this component owns inside the subsystem. */
	FMaterialDrivenShadowHandle Handle;

	/** Last known ground Z. Seeded from the component location so the very first frame is already sensible. */
	float GroundWorldZ = 0.0f;

	/** False until a trace hit or a manual value established a real ground height. */
	bool bHasGroundHeight = false;
};
