// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "MaterialDrivenShadowStatics.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "MaterialDrivenShadowSubsystem.h"

UMaterialDrivenShadowSubsystem* UMaterialDrivenShadowStatics::GetMaterialDrivenShadows(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UMaterialDrivenShadowSubsystem>() : nullptr;
}

void UMaterialDrivenShadowStatics::SetMaterialDrivenShadowsEnabled(const UObject* WorldContextObject, bool bEnabled)
{
	if (UMaterialDrivenShadowSubsystem* Subsystem = GetMaterialDrivenShadows(WorldContextObject))
	{
		Subsystem->SetShadowsEnabled(bEnabled);
	}
}

FMaterialDrivenShadowStats UMaterialDrivenShadowStatics::GetMaterialDrivenShadowStats(const UObject* WorldContextObject)
{
	if (const UMaterialDrivenShadowSubsystem* Subsystem = GetMaterialDrivenShadows(WorldContextObject))
	{
		return Subsystem->GetStats();
	}

	return FMaterialDrivenShadowStats();
}

void UMaterialDrivenShadowStatics::SetMaterialDrivenShadowLightDirection(const UObject* WorldContextObject, FVector LightDirection)
{
	if (UMaterialDrivenShadowSubsystem* Subsystem = GetMaterialDrivenShadows(WorldContextObject))
	{
		Subsystem->SetLightDirectionOverride(LightDirection);
	}
}

void UMaterialDrivenShadowStatics::ClearMaterialDrivenShadowLightDirection(const UObject* WorldContextObject)
{
	if (UMaterialDrivenShadowSubsystem* Subsystem = GetMaterialDrivenShadows(WorldContextObject))
	{
		Subsystem->ClearLightDirectionOverride();
	}
}
