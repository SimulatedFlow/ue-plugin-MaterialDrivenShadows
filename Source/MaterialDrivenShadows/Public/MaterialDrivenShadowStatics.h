// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MaterialDrivenShadowTypes.h"
#include "MaterialDrivenShadowStatics.generated.h"

class UMaterialDrivenShadowSubsystem;

/** Blueprint conveniences for reaching the shadow subsystem without a Get World Subsystem node. */
UCLASS()
class MATERIALDRIVENSHADOWS_API UMaterialDrivenShadowStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The shadow subsystem for the given world context, or null outside a game world. */
	UFUNCTION(BlueprintPure, Category = "Material-Driven Shadows", meta = (WorldContext = "WorldContextObject"))
	static UMaterialDrivenShadowSubsystem* GetMaterialDrivenShadows(const UObject* WorldContextObject);

	/** Turn every batched shadow in the world on or off. */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows", meta = (WorldContext = "WorldContextObject"))
	static void SetMaterialDrivenShadowsEnabled(const UObject* WorldContextObject, bool bEnabled);

	/** Batch counts, slot usage and last-update timings. Returns defaults when there is no subsystem. */
	UFUNCTION(BlueprintPure, Category = "Material-Driven Shadows", meta = (WorldContext = "WorldContextObject"))
	static FMaterialDrivenShadowStats GetMaterialDrivenShadowStats(const UObject* WorldContextObject);

	/** Cast shadows along an explicit direction instead of following the scene's directional light. */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows", meta = (WorldContext = "WorldContextObject"))
	static void SetMaterialDrivenShadowLightDirection(const UObject* WorldContextObject, FVector LightDirection);

	/** Go back to following the scene's primary directional light. */
	UFUNCTION(BlueprintCallable, Category = "Material-Driven Shadows", meta = (WorldContext = "WorldContextObject"))
	static void ClearMaterialDrivenShadowLightDirection(const UObject* WorldContextObject);
};
