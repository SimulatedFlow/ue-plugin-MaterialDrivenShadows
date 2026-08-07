// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "MaterialDrivenShadowComponent.h"

#include "Engine/World.h"
#include "MaterialDrivenShadowSubsystem.h"
#include "MaterialDrivenShadowsLog.h"

UMaterialDrivenShadowComponent::UMaterialDrivenShadowComponent()
{
	// The subsystem drives everything in one batched pass; a per-unit tick would defeat the purpose.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	bWantsInitializeComponent = false;
}

void UMaterialDrivenShadowComponent::BeginPlay()
{
	Super::BeginPlay();

	// Seed the ground height with the component's own Z so the very first frame renders sensibly even
	// before the round-robin trace budget gets around to this unit.
	GroundWorldZ = GetComponentLocation().Z;

	if (bAutoRegister)
	{
		RegisterShadow();
	}
}

void UMaterialDrivenShadowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterShadow();

	Super::EndPlay(EndPlayReason);
}

void UMaterialDrivenShadowComponent::OnUnregister()
{
	// Covers components torn down without an EndPlay (level streaming, actor component removal at runtime).
	UnregisterShadow();

	Super::OnUnregister();
}

UMaterialDrivenShadowSubsystem* UMaterialDrivenShadowComponent::GetShadowSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UMaterialDrivenShadowSubsystem>() : nullptr;
}

void UMaterialDrivenShadowComponent::RegisterShadow()
{
	if (Handle.IsValid())
	{
		return;
	}

	if (UMaterialDrivenShadowSubsystem* Subsystem = GetShadowSubsystem())
	{
		Handle = Subsystem->RegisterShadowComponent(this);
	}
}

void UMaterialDrivenShadowComponent::UnregisterShadow()
{
	if (!Handle.IsValid())
	{
		return;
	}

	if (UMaterialDrivenShadowSubsystem* Subsystem = GetShadowSubsystem())
	{
		Subsystem->UnregisterShadowComponent(this);
	}

	Handle.Reset();
}

void UMaterialDrivenShadowComponent::SetShadowOpacity(float NewOpacity)
{
	BaseOpacity = FMath::Clamp(NewOpacity, 0.0f, 1.0f);
}

void UMaterialDrivenShadowComponent::SetShadowRadius(float NewRadius)
{
	ShadowRadius = FMath::Max(0.0f, NewRadius);
}

void UMaterialDrivenShadowComponent::SetShadowEnabled(bool bNewEnabled)
{
	bShadowEnabled = bNewEnabled;
}

void UMaterialDrivenShadowComponent::SetMaxShadowStretch(float NewMaxStretch)
{
	MaxShadowStretch = FMath::Max(1.0f, NewMaxStretch);
}

void UMaterialDrivenShadowComponent::SetGroundHeight(float NewGroundWorldZ)
{
	GroundWorldZ = NewGroundWorldZ;
	bHasGroundHeight = true;
}

void UMaterialDrivenShadowComponent::ApplyTracedGroundHeight(float NewGroundWorldZ, bool bHit)
{
	GroundWorldZ = NewGroundWorldZ;
	bHasGroundHeight = bHit;
}

void UMaterialDrivenShadowComponent::SetShadowMeshAndMaterial(UStaticMesh* NewMesh, UMaterialInterface* NewMaterial)
{
	if (CustomShadowMesh == NewMesh && CustomShadowMaterial == NewMaterial)
	{
		return;
	}

	const bool bWasRegistered = Handle.IsValid();
	if (bWasRegistered)
	{
		UnregisterShadow();
	}

	CustomShadowMesh = NewMesh;
	CustomShadowMaterial = NewMaterial;

	if (bWasRegistered)
	{
		RegisterShadow();
	}
}
