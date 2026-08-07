// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "MaterialDrivenShadowSubsystem.h"

#include "CollisionQueryParams.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "MaterialDrivenShadowComponent.h"
#include "MaterialDrivenShadowSettings.h"
#include "MaterialDrivenShadowsLog.h"

namespace MaterialDrivenShadowsPrivate
{
	/** Scale written to instances that must not rasterise. Not exactly zero, so no transform math can divide by it. */
	static constexpr float HiddenInstanceScale = 1.0e-4f;

	/** Guards against a division blow-up when the sun sits exactly on the horizon. */
	static constexpr float MinVerticalLightAmount = 0.05f;

	static TAutoConsoleVariable<int32> CVarEnabled(
		TEXT("MDS.Enabled"),
		1,
		TEXT("Enable the Material-Driven Shadows subsystem. 0 hides every batched shadow without destroying the batches."),
		ECVF_Default);
}

bool UMaterialDrivenShadowSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

bool UMaterialDrivenShadowSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UMaterialDrivenShadowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UMaterialDrivenShadowSettings& Settings = UMaterialDrivenShadowSettings::Get();
	bShadowsEnabled = Settings.bEnableShadows;
	LightDirection = Settings.FallbackLightDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector(0.0f, 0.0f, -1.0f));

	UE_LOG(LogMaterialDrivenShadows, Verbose, TEXT("Shadow subsystem initialised (enabled=%d)."), bShadowsEnabled ? 1 : 0);
}

void UMaterialDrivenShadowSubsystem::Deinitialize()
{
	for (FMaterialDrivenShadowBatch& Batch : Batches)
	{
		if (Batch.InstancedComponent)
		{
			Batch.InstancedComponent->DestroyComponent();
			Batch.InstancedComponent = nullptr;
		}
	}
	Batches.Empty();
	RegisteredComponents.Empty();

	if (IsValid(BatchHolder))
	{
		BatchHolder->Destroy();
	}
	BatchHolder = nullptr;

	DefaultShadowMesh = nullptr;
	DefaultShadowMaterial = nullptr;
	CachedDirectionalLight.Reset();

	Super::Deinitialize();
}

TStatId UMaterialDrivenShadowSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMaterialDrivenShadowSubsystem, STATGROUP_Tickables);
}

void UMaterialDrivenShadowSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const bool bRuntimeEnabled = bShadowsEnabled && MaterialDrivenShadowsPrivate::CVarEnabled.GetValueOnGameThread() != 0;
	if (!bRuntimeEnabled)
	{
		// Collapse every instance exactly once, then stay idle until the system is switched back on.
		if (!bInstancesHiddenWhileDisabled)
		{
			for (FMaterialDrivenShadowBatch& Batch : Batches)
			{
				for (int32 SlotIndex = 0; SlotIndex < Batch.Slots.Num(); ++SlotIndex)
				{
					HideInstance(Batch, SlotIndex);
				}
			}
			bInstancesHiddenWhileDisabled = true;
		}
		return;
	}

	bInstancesHiddenWhileDisabled = false;

	const UMaterialDrivenShadowSettings& Settings = UMaterialDrivenShadowSettings::Get();

	TimeSinceLastUpdate += DeltaTime;
	if (!bForceUpdate && Settings.UpdateInterval > 0.0f && TimeSinceLastUpdate < Settings.UpdateInterval)
	{
		return;
	}

	const float StepTime = TimeSinceLastUpdate;
	TimeSinceLastUpdate = 0.0f;
	bForceUpdate = false;

	const double UpdateStartSeconds = FPlatformTime::Seconds();

	UpdateLightDirection(StepTime);
	UpdateGroundTraces();
	UpdateBatches();

	Stats.LastUpdateMilliseconds = static_cast<float>((FPlatformTime::Seconds() - UpdateStartSeconds) * 1000.0);
	Stats.ShadowDirection = ShadowDirection;
	Stats.LightStretchFactor = LightStretchFactor;
	Stats.NumBatches = Batches.Num();
	Stats.NumRegisteredShadows = RegisteredComponents.Num();
}

void UMaterialDrivenShadowSubsystem::SetShadowsEnabled(bool bNewEnabled)
{
	if (bShadowsEnabled == bNewEnabled)
	{
		return;
	}

	bShadowsEnabled = bNewEnabled;
	bForceUpdate = true;
}

void UMaterialDrivenShadowSubsystem::SetLightDirectionOverride(FVector NewLightDirection)
{
	const FVector Normalised = NewLightDirection.GetSafeNormal();
	if (Normalised.IsNearlyZero())
	{
		UE_LOG(LogMaterialDrivenShadows, Warning, TEXT("SetLightDirectionOverride ignored: direction is a zero vector."));
		return;
	}

	LightDirectionOverride = Normalised;
	bUseLightDirectionOverride = true;
	bForceUpdate = true;
}

void UMaterialDrivenShadowSubsystem::ClearLightDirectionOverride()
{
	bUseLightDirectionOverride = false;
	// Dropping the cached light forces a rescan on the next update.
	CachedDirectionalLight.Reset();
	bForceUpdate = true;
}

//~ Registration ---------------------------------------------------------------------------------------

FMaterialDrivenShadowHandle UMaterialDrivenShadowSubsystem::RegisterShadowComponent(UMaterialDrivenShadowComponent* ShadowComponent)
{
	FMaterialDrivenShadowHandle Handle;

	if (!IsValid(ShadowComponent))
	{
		return Handle;
	}

	UStaticMesh* Mesh = ShadowComponent->CustomShadowMesh ? ShadowComponent->CustomShadowMesh.Get() : ResolveDefaultMesh();
	if (!Mesh)
	{
		if (!bLoggedMissingMesh)
		{
			bLoggedMissingMesh = true;
			UE_LOG(LogMaterialDrivenShadows, Error,
				TEXT("No shadow mesh available. Set a Custom Shadow Mesh on the component, or a valid Default Shadow Mesh under Project Settings > Plugins > Material-Driven Shadows."));
		}
		return Handle;
	}

	UMaterialInterface* Material = ShadowComponent->CustomShadowMaterial ? ShadowComponent->CustomShadowMaterial.Get() : ResolveDefaultMaterial();

	const int32 BatchIndex = FindOrCreateBatch(Mesh, Material);
	if (!Batches.IsValidIndex(BatchIndex))
	{
		return Handle;
	}

	FMaterialDrivenShadowBatch& Batch = Batches[BatchIndex];
	UInstancedStaticMeshComponent* InstancedComponent = Batch.InstancedComponent;
	if (!InstancedComponent)
	{
		return Handle;
	}

	int32 SlotIndex = INDEX_NONE;
	if (Batch.FreeSlots.Num() > 0)
	{
		// Recycle. Slots are never removed from the instanced component, so no handle is ever invalidated.
		SlotIndex = Batch.FreeSlots.Pop(EAllowShrinking::No);
		Batch.Slots[SlotIndex] = ShadowComponent;
	}
	else
	{
		const FTransform InitialTransform(
			FQuat::Identity,
			ShadowComponent->GetComponentLocation(),
			FVector(MaterialDrivenShadowsPrivate::HiddenInstanceScale));

		SlotIndex = InstancedComponent->AddInstance(InitialTransform, /*bWorldSpace*/ true);
		if (SlotIndex == INDEX_NONE)
		{
			return Handle;
		}

		if (SlotIndex == Batch.Slots.Num())
		{
			Batch.Slots.Add(ShadowComponent);
		}
		else
		{
			// Defensive: keep the slot table the same length as the instance buffer no matter what.
			Batch.Slots.SetNum(InstancedComponent->GetInstanceCount());
			if (!Batch.Slots.IsValidIndex(SlotIndex))
			{
				return Handle;
			}
			Batch.Slots[SlotIndex] = ShadowComponent;
		}
	}

	RegisteredComponents.Add(ShadowComponent);

	Handle.BatchIndex = BatchIndex;
	Handle.InstanceIndex = SlotIndex;
	return Handle;
}

void UMaterialDrivenShadowSubsystem::UnregisterShadowComponent(UMaterialDrivenShadowComponent* ShadowComponent)
{
	if (!ShadowComponent)
	{
		return;
	}

	const FMaterialDrivenShadowHandle& Handle = ShadowComponent->GetShadowHandle();
	if (Batches.IsValidIndex(Handle.BatchIndex))
	{
		FMaterialDrivenShadowBatch& Batch = Batches[Handle.BatchIndex];
		if (Batch.Slots.IsValidIndex(Handle.InstanceIndex) && Batch.Slots[Handle.InstanceIndex] == ShadowComponent)
		{
			Batch.Slots[Handle.InstanceIndex] = nullptr;
			Batch.FreeSlots.Add(Handle.InstanceIndex);
			HideInstance(Batch, Handle.InstanceIndex);
		}
	}

	RegisteredComponents.RemoveSingleSwap(ShadowComponent, EAllowShrinking::No);
	if (GroundTraceCursor >= RegisteredComponents.Num())
	{
		GroundTraceCursor = 0;
	}
}

//~ Batch management -----------------------------------------------------------------------------------

int32 UMaterialDrivenShadowSubsystem::FindOrCreateBatch(UStaticMesh* Mesh, UMaterialInterface* Material)
{
	// Batch counts are tiny in practice (one per unique mesh/material pair), so a linear scan beats a hash map.
	for (int32 BatchIndex = 0; BatchIndex < Batches.Num(); ++BatchIndex)
	{
		const FMaterialDrivenShadowBatch& Batch = Batches[BatchIndex];
		if (Batch.Mesh == Mesh && Batch.Material == Material && Batch.InstancedComponent)
		{
			return BatchIndex;
		}
	}

	UInstancedStaticMeshComponent* NewComponent = CreateBatchComponent(Mesh, Material);
	if (!NewComponent)
	{
		return INDEX_NONE;
	}

	FMaterialDrivenShadowBatch NewBatch;
	NewBatch.Mesh = Mesh;
	NewBatch.Material = Material;
	NewBatch.InstancedComponent = NewComponent;

	const int32 NewIndex = Batches.Add(MoveTemp(NewBatch));
	UE_LOG(LogMaterialDrivenShadows, Log, TEXT("Created shadow batch %d for mesh '%s' (one draw call)."), NewIndex, *GetNameSafe(Mesh));
	return NewIndex;
}

void UMaterialDrivenShadowSubsystem::EnsureBatchHolder()
{
	if (IsValid(BatchHolder))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	BatchHolder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!BatchHolder)
	{
		UE_LOG(LogMaterialDrivenShadows, Error, TEXT("Failed to spawn the shadow batch holder actor; no shadows will be rendered."));
		return;
	}

	USceneComponent* Root = NewObject<USceneComponent>(BatchHolder, TEXT("ShadowBatchRoot"), RF_Transient);
	BatchHolder->SetRootComponent(Root);
	Root->RegisterComponent();

	BatchHolder->SetActorEnableCollision(false);
	BatchHolder->SetCanBeDamaged(false);
}

UInstancedStaticMeshComponent* UMaterialDrivenShadowSubsystem::CreateBatchComponent(UStaticMesh* Mesh, UMaterialInterface* Material)
{
	EnsureBatchHolder();
	if (!IsValid(BatchHolder) || !Mesh)
	{
		return nullptr;
	}

	const UMaterialDrivenShadowSettings& Settings = UMaterialDrivenShadowSettings::Get();

	UInstancedStaticMeshComponent* Component = nullptr;
	if (Settings.BatchMode == EMaterialDrivenShadowBatchMode::Hierarchical)
	{
		Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(BatchHolder, NAME_None, RF_Transient);
	}
	else
	{
		Component = NewObject<UInstancedStaticMeshComponent>(BatchHolder, NAME_None, RF_Transient);
	}

	if (!Component)
	{
		return nullptr;
	}

	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	if (Material)
	{
		Component->SetMaterial(0, Material);
	}

	// A shadow that casts a shadow would be absurd, and lighting/decal work on these quads is pure waste.
	Component->SetCastShadow(false);
	Component->bCastDynamicShadow = false;
	Component->bCastStaticShadow = false;
	Component->bCastVolumetricTranslucentShadow = false;
	Component->bCastContactShadow = false;
	Component->bAffectDynamicIndirectLighting = false;
	Component->bAffectDistanceFieldLighting = false;
	Component->bUseAsOccluder = false;
	Component->SetReceivesDecals(false);

	// No collision, no navigation, no overlap bookkeeping — these are render-only proxies.
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);

	// The material pushes vertices well outside the mesh bounds, so culling needs headroom.
	Component->BoundsScale = FMath::Max(1.0f, Settings.BatchBoundsScale);
	Component->SetCullDistances(FMath::Max(0, Settings.InstanceStartCullDistance), FMath::Max(0, Settings.InstanceEndCullDistance));

	Component->SetNumCustomDataFloats(MaterialDrivenShadows::NumCustomDataFloats);

	Component->SetupAttachment(BatchHolder->GetRootComponent());
	Component->RegisterComponent();
	BatchHolder->AddInstanceComponent(Component);

	return Component;
}

UStaticMesh* UMaterialDrivenShadowSubsystem::ResolveDefaultMesh()
{
	if (DefaultShadowMesh)
	{
		return DefaultShadowMesh;
	}

	const UMaterialDrivenShadowSettings& Settings = UMaterialDrivenShadowSettings::Get();
	if (Settings.DefaultShadowMesh.IsNull())
	{
		return nullptr;
	}

	DefaultShadowMesh = Cast<UStaticMesh>(Settings.DefaultShadowMesh.TryLoad());
	if (!DefaultShadowMesh && !bLoggedMissingMesh)
	{
		bLoggedMissingMesh = true;
		UE_LOG(LogMaterialDrivenShadows, Error, TEXT("Default shadow mesh '%s' could not be loaded."), *Settings.DefaultShadowMesh.ToString());
	}

	return DefaultShadowMesh;
}

UMaterialInterface* UMaterialDrivenShadowSubsystem::ResolveDefaultMaterial()
{
	if (DefaultShadowMaterial)
	{
		return DefaultShadowMaterial;
	}

	const UMaterialDrivenShadowSettings& Settings = UMaterialDrivenShadowSettings::Get();
	if (Settings.DefaultShadowMaterial.IsNull())
	{
		return nullptr;
	}

	DefaultShadowMaterial = Cast<UMaterialInterface>(Settings.DefaultShadowMaterial.TryLoad());
	if (!DefaultShadowMaterial && !bLoggedMissingMaterial)
	{
		bLoggedMissingMaterial = true;
		UE_LOG(LogMaterialDrivenShadows, Warning,
			TEXT("Default shadow material '%s' could not be loaded; batches will fall back to the mesh's own material. ")
			TEXT("Assign a master shadow material under Project Settings > Plugins > Material-Driven Shadows."),
			*Settings.DefaultShadowMaterial.ToString());
	}

	return DefaultShadowMaterial;
}

//~ Per-frame update -----------------------------------------------------------------------------------

void UMaterialDrivenShadowSubsystem::UpdateLightDirection(float DeltaTime)
{
	const UMaterialDrivenShadowSettings& Settings = UMaterialDrivenShadowSettings::Get();

	if (bUseLightDirectionOverride)
	{
		LightDirection = LightDirectionOverride;
	}
	else
	{
		TimeSinceLightRefresh += DeltaTime;

		const bool bNeedsRescan = !CachedDirectionalLight.IsValid() || TimeSinceLightRefresh >= Settings.LightRefreshInterval;
		if (bNeedsRescan)
		{
			TimeSinceLightRefresh = 0.0f;
			CachedDirectionalLight.Reset();

			if (UWorld* World = GetWorld())
			{
				ADirectionalLight* BestLight = nullptr;
				float BestScore = -1.0f;

				for (TActorIterator<ADirectionalLight> It(World); It; ++It)
				{
					ADirectionalLight* Light = *It;
					if (!IsValid(Light) || Light->IsHidden())
					{
						continue;
					}

					const ULightComponent* LightComponent = Light->GetLightComponent();
					if (!LightComponent || !LightComponent->IsVisible())
					{
						continue;
					}

					// The atmosphere sun always wins; otherwise the brightest light is the one casting the shadows.
					const float Score = LightComponent->Intensity + (LightComponent->IsUsedAsAtmosphereSunLight() ? 1.0e6f : 0.0f);
					if (Score > BestScore)
					{
						BestScore = Score;
						BestLight = Light;
					}
				}

				CachedDirectionalLight = BestLight;
			}
		}

		if (const ADirectionalLight* Light = CachedDirectionalLight.Get())
		{
			LightDirection = Light->GetActorForwardVector();
		}
		else
		{
			LightDirection = Settings.FallbackLightDirection;
		}
	}

	LightDirection = LightDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector(0.0f, 0.0f, -1.0f));

	// Flatten the light onto the ground plane: that is the direction the shadow runs away from its unit.
	const FVector FlatDirection(LightDirection.X, LightDirection.Y, 0.0f);
	ShadowDirection = FlatDirection.IsNearlyZero()
		? FVector(1.0f, 0.0f, 0.0f)
		: FlatDirection.GetSafeNormal();

	// Straight overhead -> no stretch. Grazing -> long shadows, capped per component by MaxShadowStretch.
	const float VerticalAmount = FMath::Max(FMath::Abs(LightDirection.Z), MaterialDrivenShadowsPrivate::MinVerticalLightAmount);
	LightStretchFactor = 1.0f / VerticalAmount;
}

void UMaterialDrivenShadowSubsystem::UpdateGroundTraces()
{
	Stats.NumGroundTracesLastUpdate = 0;

	const UMaterialDrivenShadowSettings& Settings = UMaterialDrivenShadowSettings::Get();
	const int32 Budget = Settings.MaxGroundTracesPerUpdate;
	const int32 NumComponents = RegisteredComponents.Num();

	UWorld* World = GetWorld();
	if (Budget <= 0 || NumComponents <= 0 || !World)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MaterialDrivenShadowGround), /*bTraceComplex*/ false);

	if (!RegisteredComponents.IsValidIndex(GroundTraceCursor))
	{
		GroundTraceCursor = 0;
	}

	int32 NumExamined = 0;
	int32 NumTraced = 0;

	// Round-robin: every registered shadow gets its ground refreshed eventually, but the per-frame cost
	// is bounded no matter how many units exist.
	while (NumExamined < NumComponents && NumTraced < Budget)
	{
		const int32 Index = GroundTraceCursor;
		GroundTraceCursor = (GroundTraceCursor + 1) % NumComponents;
		++NumExamined;

		UMaterialDrivenShadowComponent* ShadowComponent = RegisteredComponents[Index].Get();
		if (!ShadowComponent
			|| !ShadowComponent->bShadowEnabled
			|| ShadowComponent->GroundMode != EMaterialDrivenShadowGroundMode::TraceDownward)
		{
			continue;
		}

		const FTransform ComponentTransform = ShadowComponent->GetComponentTransform();
		const FVector Start = ComponentTransform.GetLocation() + ComponentTransform.TransformVector(ShadowComponent->ShadowOffset);
		const FVector End = Start - FVector(0.0f, 0.0f, FMath::Max(1.0f, ShadowComponent->MaxGroundTraceDistance));

		QueryParams.ClearIgnoredSourceObjects();
		if (const AActor* Owner = ShadowComponent->GetOwner())
		{
			QueryParams.AddIgnoredActor(Owner);
		}

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, Settings.GroundTraceChannel, QueryParams);
		ShadowComponent->ApplyTracedGroundHeight(bHit ? Hit.ImpactPoint.Z : Start.Z, bHit);

		++NumTraced;
	}

	Stats.NumGroundTracesLastUpdate = NumTraced;
}

void UMaterialDrivenShadowSubsystem::UpdateBatches()
{
	const UMaterialDrivenShadowSettings& Settings = UMaterialDrivenShadowSettings::Get();
	const float ReferenceRadius = FMath::Max(UE_KINDA_SMALL_NUMBER, Settings.ReferenceMeshRadius);
	const float GroundOffset = Settings.GroundOffset;

	// The quad is yawed so its local +X points along the shadow direction; the vertex shader can then
	// stretch purely along X without needing the light vector itself.
	const float ShadowYawDegrees = FMath::RadiansToDegrees(FMath::Atan2(ShadowDirection.Y, ShadowDirection.X));
	const FQuat ShadowRotation = FRotator(0.0f, ShadowYawDegrees, 0.0f).Quaternion();

	int32 TotalSlots = 0;
	int32 TotalFreeSlots = 0;

	for (FMaterialDrivenShadowBatch& Batch : Batches)
	{
		UInstancedStaticMeshComponent* InstancedComponent = Batch.InstancedComponent;
		if (!IsValid(InstancedComponent))
		{
			continue;
		}

		const int32 NumSlots = Batch.Slots.Num();
		TotalSlots += NumSlots;
		TotalFreeSlots += Batch.FreeSlots.Num();

		if (NumSlots <= 0)
		{
			continue;
		}

		Batch.ScratchTransforms.SetNum(NumSlots, EAllowShrinking::No);
		Batch.ScratchCustomData.SetNumUninitialized(NumSlots * MaterialDrivenShadows::NumCustomDataFloats, EAllowShrinking::No);

		for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
		{
			float* CustomData = &Batch.ScratchCustomData[SlotIndex * MaterialDrivenShadows::NumCustomDataFloats];

			UMaterialDrivenShadowComponent* ShadowComponent = Batch.Slots[SlotIndex].Get();
			if (!ShadowComponent || !ShadowComponent->bShadowEnabled)
			{
				// Free or disabled slot: keep it where it is (so batch bounds stay tight) and collapse it.
				FTransform HiddenTransform;
				InstancedComponent->GetInstanceTransform(SlotIndex, HiddenTransform, /*bWorldSpace*/ true);
				HiddenTransform.SetScale3D(FVector(MaterialDrivenShadowsPrivate::HiddenInstanceScale));
				Batch.ScratchTransforms[SlotIndex] = HiddenTransform;

				CustomData[MaterialDrivenShadows::CustomDataIndex_Radius] = 0.0f;
				CustomData[MaterialDrivenShadows::CustomDataIndex_Opacity] = 0.0f;
				CustomData[MaterialDrivenShadows::CustomDataIndex_HeightAboveGround] = 0.0f;
				CustomData[MaterialDrivenShadows::CustomDataIndex_StretchFactor] = 1.0f;
				continue;
			}

			const FTransform ComponentTransform = ShadowComponent->GetComponentTransform();
			const FVector ShadowLocation = ComponentTransform.GetLocation() + ComponentTransform.TransformVector(ShadowComponent->ShadowOffset);

			float GroundZ = ShadowLocation.Z;
			if (ShadowComponent->GroundMode != EMaterialDrivenShadowGroundMode::OwnerLocation && ShadowComponent->HasGroundHeight())
			{
				GroundZ = ShadowComponent->GetGroundHeight();
			}

			const float HeightAboveGround = FMath::Max(0.0f, static_cast<float>(ShadowLocation.Z - GroundZ));

			float Opacity = FMath::Clamp(ShadowComponent->BaseOpacity, 0.0f, 1.0f);
			if (ShadowComponent->FadeEndHeight > ShadowComponent->FadeStartHeight)
			{
				const float FadeAlpha = FMath::Clamp(
					(HeightAboveGround - ShadowComponent->FadeStartHeight) / (ShadowComponent->FadeEndHeight - ShadowComponent->FadeStartHeight),
					0.0f, 1.0f);
				Opacity *= 1.0f - FadeAlpha;
			}

			const float MaxStretch = FMath::Max(1.0f, ShadowComponent->MaxShadowStretch);
			const float Stretch = FMath::Clamp(LightStretchFactor, 1.0f, MaxStretch);
			const float InstanceScale = FMath::Max(0.0f, ShadowComponent->ShadowRadius) / ReferenceRadius;

			Batch.ScratchTransforms[SlotIndex] = FTransform(
				ShadowRotation,
				FVector(ShadowLocation.X, ShadowLocation.Y, GroundZ + GroundOffset),
				FVector(InstanceScale));

			CustomData[MaterialDrivenShadows::CustomDataIndex_Radius] = ShadowComponent->ShadowRadius;
			CustomData[MaterialDrivenShadows::CustomDataIndex_Opacity] = Opacity;
			CustomData[MaterialDrivenShadows::CustomDataIndex_HeightAboveGround] = HeightAboveGround;
			CustomData[MaterialDrivenShadows::CustomDataIndex_StretchFactor] = Stretch;
		}

		// Two bulk writes per batch, no render-state recreation: the instance data manager streams the
		// deltas to the GPU scene by itself.
		InstancedComponent->BatchUpdateInstancesTransforms(
			0, Batch.ScratchTransforms, /*bWorldSpace*/ true, /*bMarkRenderStateDirty*/ false, /*bTeleport*/ true);

		if (InstancedComponent->NumCustomDataFloats == MaterialDrivenShadows::NumCustomDataFloats
			&& InstancedComponent->PerInstanceSMCustomData.Num() >= Batch.ScratchCustomData.Num())
		{
			InstancedComponent->SetCustomData(0, NumSlots - 1, Batch.ScratchCustomData, /*bMarkRenderStateDirty*/ false);
		}
	}

	Stats.NumInstanceSlots = TotalSlots;
	Stats.NumFreeSlots = TotalFreeSlots;
}

void UMaterialDrivenShadowSubsystem::HideInstance(FMaterialDrivenShadowBatch& Batch, int32 InstanceIndex)
{
	UInstancedStaticMeshComponent* InstancedComponent = Batch.InstancedComponent;
	if (!IsValid(InstancedComponent) || !Batch.Slots.IsValidIndex(InstanceIndex))
	{
		return;
	}

	FTransform HiddenTransform;
	if (!InstancedComponent->GetInstanceTransform(InstanceIndex, HiddenTransform, /*bWorldSpace*/ true))
	{
		return;
	}

	HiddenTransform.SetScale3D(FVector(MaterialDrivenShadowsPrivate::HiddenInstanceScale));
	InstancedComponent->UpdateInstanceTransform(
		InstanceIndex, HiddenTransform, /*bWorldSpace*/ true, /*bMarkRenderStateDirty*/ false, /*bTeleport*/ true);

	if (InstancedComponent->NumCustomDataFloats == MaterialDrivenShadows::NumCustomDataFloats
		&& InstancedComponent->PerInstanceSMCustomData.IsValidIndex(InstanceIndex * MaterialDrivenShadows::NumCustomDataFloats + MaterialDrivenShadows::NumCustomDataFloats - 1))
	{
		const float ClearedData[MaterialDrivenShadows::NumCustomDataFloats] = { 0.0f, 0.0f, 0.0f, 1.0f };
		InstancedComponent->SetCustomData(InstanceIndex, MakeArrayView(ClearedData), /*bMarkRenderStateDirty*/ false);
	}
}
