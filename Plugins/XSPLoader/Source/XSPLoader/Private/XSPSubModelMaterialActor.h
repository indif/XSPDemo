#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "XSPSubModelMaterialActor.generated.h"

UCLASS()
class AXSPSubModelMaterialActor : public AActor
{
    GENERATED_BODY()

public:
    AXSPSubModelMaterialActor();
    virtual ~AXSPSubModelMaterialActor();

    void Init(class AXSPSubModelActor* Parent, UMaterialInstanceDynamic* Material, int32 CustomDepthStencilValue, bool bRenderInMainAndDepthPass);

    void AddNode(int32 Dbid);

    void AddNode(const TArray<int32>& NodeArray);

    void RemoveNode(int32 Dbid);

    void RemoveNode(const TArray<int32>& NodeArray);

    inline const TArray<struct FXSPNodeData*>& GetNodeDataArray() const { return Parent->GetNodeDataArray(); }

    bool TickDynamicCombine(float& InOutSeconds, bool bAsyncBuild);

private:
    bool PreProcess();
    void ProcessBatch(bool bAsyncBuild);
    bool ProcessRegister();
    void AddComponent(const TArray<int32>& DbidArray, bool bAsyncBuild);
    void ReleaseComponent(UXSPBatchMeshComponent* Component);
    void RegisterComponent(UXSPBatchMeshComponent* Component);

private:
    class AXSPSubModelActor* Parent;

    UPROPERTY()
    UMaterialInstanceDynamic* MaterialInstanceDynamic = nullptr;

    int32 CustomDepthStencilValue = -1;

    bool bRenderInMainAndDepthPass = true;

    TArray<int32> NodeArray;

    TArray<int32> NodeToAddArray;
    TArray<int32> NodeToRemoveArray;
    TArray<int32> NodeToBuildArray;

    //节点与Component对应关系表
    TMap<int32, class UXSPBatchMeshComponent*> NodeComponentMap;

    //全部Component的数组
    TArray<class UXSPBatchMeshComponent*> BatchMeshComponentArray;

    //尚在异步构建中的Component的数组
    UPROPERTY()
    TArray<class UXSPBatchMeshComponent*> BuildingComponentArray;
};