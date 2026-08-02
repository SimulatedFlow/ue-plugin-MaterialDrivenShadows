// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/** MaterialDrivenShadows — single-draw-call instanced blob shadows for massive unit counts (runtime module). */
class FMaterialDrivenShadowsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
