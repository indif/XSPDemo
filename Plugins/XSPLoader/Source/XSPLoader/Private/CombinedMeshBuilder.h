// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FCombinedMeshBuilder
{
public:
    FCombinedMeshBuilder();
    ~FCombinedMeshBuilder();

    void SetSourceMaterial(UMaterialInterface* InSourceMaterial);

    void AddBodyData(FLinearColor Material, TSharedPtr<struct Body_info> BodyData);

    void BuildMeshes(UObject* Outer, TArray<UStaticMesh*>& StaticMeshArray, TArray<UMaterialInterface*>& MaterialArray, bool bBuildNormal);

private:
    UMaterialInterface* SourceMaterial = nullptr;

    TMap<FLinearColor, TArray<TSharedPtr<struct Body_info>>> DataMap;
};
