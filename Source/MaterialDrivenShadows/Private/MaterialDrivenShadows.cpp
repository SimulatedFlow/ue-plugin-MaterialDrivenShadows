// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "MaterialDrivenShadows.h"
#include "MaterialDrivenShadowsLog.h"

DEFINE_LOG_CATEGORY(LogMaterialDrivenShadows);

#define LOCTEXT_NAMESPACE "FMaterialDrivenShadowsModule"

void FMaterialDrivenShadowsModule::StartupModule()
{
	UE_LOG(LogMaterialDrivenShadows, Log, TEXT("MaterialDrivenShadows started."));
}

void FMaterialDrivenShadowsModule::ShutdownModule()
{
	UE_LOG(LogMaterialDrivenShadows, Log, TEXT("MaterialDrivenShadows shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMaterialDrivenShadowsModule, MaterialDrivenShadows)
