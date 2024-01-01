// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XSPFileUtils.h"


void EncodeNormal(const FVector3f& InNormal, FXSPNormalVector& OutNormal);
void DecodeNormal(const FXSPNormalVector& InNormal, FVector3f& OutNormal);
void InheritMaterial(FXSPNodeData& Node, FXSPNodeData& ParentNode);
void ResolveNodeData(FXSPNodeData& NodeData, FXSPNodeData* ParentNodeData);