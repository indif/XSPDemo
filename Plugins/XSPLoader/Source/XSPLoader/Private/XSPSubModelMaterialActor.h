#pragma once

#include "CoreMinimal.h"

class AXSPModelActor;

class FXSPSubModelMaterialActor
{
public:
    FXSPSubModelMaterialActor();
    virtual ~FXSPSubModelMaterialActor();

    void Init(AXSPModelActor* Owner, UMaterialInstanceDynamic* Material, int32 CustomDepthStencilValue, bool bRenderInMainAndDepthPass);

    void AddNode(int32 Dbid);

    void AddNode(const TArray<int32>& NodeArray);

    void RemoveNode(int32 Dbid);

    void RemoveNode(const TArray<int32>& NodeArray);

    bool TickDynamicCombine(float& InOutSeconds, bool bAsyncBuild);

private:
    bool PreProcess();
    void ProcessBatch(bool bAsyncBuild);
    bool ProcessRegister();
    void AddComponent(const TArray<int32>& DbidArray, bool bAsyncBuild);
    void ReleaseComponent(UPrimitiveComponent* Component);
    void RegisterComponent(UPrimitiveComponent* Component);

private:
    AXSPModelActor* Owner;

    UMaterialInstanceDynamic* MaterialInstanceDynamic = nullptr;

    int32 CustomDepthStencilValue = -1;

    bool bRenderInMainAndDepthPass = true;

    TArray<int32> NodeArray;

    TArray<int32> NodeToAddArray;
    TArray<int32> NodeToRemoveArray;
    TArray<int32> NodeToBuildArray;

    //节点与Component对应关系表
    TMap<int32, UPrimitiveComponent*> NodeComponentMap;

    //全部Component的数组
    TArray<UPrimitiveComponent*> BatchMeshComponentArray;

    //尚在异步构建中的Component的数组
    TArray<TStrongObjectPtr<UPrimitiveComponent>> BuildingComponentArray;
};