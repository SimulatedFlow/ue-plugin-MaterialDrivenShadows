// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "MaterialDrivenShadowSettings.h"

UMaterialDrivenShadowSettings::UMaterialDrivenShadowSettings()
{
	// The engine's built-in 100x100 uu plane is a sane out-of-the-box shadow quad.
	DefaultShadowMesh = FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane"));

	// Ships with the plugin's content. If it is missing the batch falls back to the mesh's own material.
	DefaultShadowMaterial = FSoftObjectPath(TEXT("/MaterialDrivenShadows/MaterialDrivenShadows/Materials/M_MaterialDrivenShadow.M_MaterialDrivenShadow"));
}

FName UMaterialDrivenShadowSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

const UMaterialDrivenShadowSettings& UMaterialDrivenShadowSettings::Get()
{
	const UMaterialDrivenShadowSettings* Settings = GetDefault<UMaterialDrivenShadowSettings>();
	check(Settings);
	return *Settings;
}
