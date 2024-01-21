// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XSPFileUtils.h"


void AppendRawMesh(const std::vector<float>& vertices, TArray<FVector3f>& VertexList, TArray<FVector3f>* NormalList);

void AppendEllipticalMesh(const std::vector<float>& vertices, TArray<FVector3f>& VertexList, TArray<FVector3f>* NormalList);

bool AppendCylinderMesh(const std::vector<float>& vertices, TArray<FVector3f>& VertexList, TArray<FVector3f>* NormalList);

void AppendNodeMesh(const Body_info& Node, TArray<FVector3f>& VertexList, TArray<FVector3f>* NormalList);

void BuildStaticMesh(UStaticMesh* StaticMesh, const TArray<FVector3f>& VertexList, const TArray<FVector3f>* NormalList);

void BuildStaticMesh(UStaticMesh* StaticMesh, const Body_info& Node);

bool IsValidMaterial(float material[4]);

void GetMaterial(Body_info* ParentNode, Body_info& Node, FLinearColor& Color, float& Roughness);

void GetMaterial(Body_info& Node, FLinearColor& Color, float& Roughness);

void InheritMaterial(Body_info& Node, Body_info& Parent);

UMaterialInstanceDynamic* CreateMaterialInstanceDynamic(UMaterialInterface* SourceMaterial, const FLinearColor& Color, float Roughness);

bool CheckNode(const Body_info& Node);

void InheritMaterial(FXSPNodeData& Node, FXSPNodeData& ParentNode);
void ResolveNodeData(FXSPNodeData& NodeData, FXSPNodeData* ParentNodeData);