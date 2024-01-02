// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XSPFileUtils.h"


void InheritMaterial(FXSPNodeData& Node, FXSPNodeData& ParentNode);
void ResolveNodeData(FXSPNodeData& NodeData, FXSPNodeData* ParentNodeData);