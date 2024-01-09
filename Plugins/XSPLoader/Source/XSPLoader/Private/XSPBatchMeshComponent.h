#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "XSPBatchMeshComponent.generated.h"

UCLASS()
class UXSPBatchMeshComponent : public UStaticMeshComponent, public IInterface_CollisionDataProvider
{
    GENERATED_BODY()

public:
    UXSPBatchMeshComponent();
    virtual ~UXSPBatchMeshComponent();

    void Init(class AXSPSubModelMaterialActor* Parent, const TArray<int32>& DbidArray, bool bAsyncBuild);

    const TArray<int32>& GetNodes() const;

    int32 GetNumVertices() const;

    int32 GetNode(int32 FaceIndex);

    bool TryFinishBuildMesh();

public:
    //~ Begin UPrimitiveComponent Interface.
    virtual class UBodySetup* GetBodySetup() override;
    //~ End UPrimitiveComponent Interface.

    //~ Begin IInterface_CollisionDataProvider Interface
    virtual bool GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
    virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override { return true; }
    //~ End IInterface_CollisionDataProvider Interface

private:
    friend class FXSPBuildStaticMeshTask;
    void BuildStaticMesh_AnyThread();

    void BuildPhysicsData(bool bAsync);
    void FinishPhysicsAsyncCook(bool bSuccess);

private:
    class AXSPSubModelMaterialActor* Parent = nullptr;

    //包含的所有叶子节点
    TArray<int32> DbidArray;
    //包中各节点的最大三角形索引
    TArray<int32> EndFaceIndexArray;

    //顶点总数
    int32 NumVerticesTotal = 0;

    UPROPERTY()
    UStaticMesh* BuildingStaticMesh;

    UPROPERTY()
    UBodySetup* BuildingBodySetup;

    UPROPERTY()
    UBodySetup* MeshBodySetup;

    FAsyncTask<class FXSPBuildStaticMeshTask>* AsyncBuildTask = nullptr;
};

class FXSPBuildStaticMeshTask : public FNonAbandonableTask
{
public:
    FXSPBuildStaticMeshTask(UXSPBatchMeshComponent*);

    void DoWork();

    TStatId GetStatId() const
    {
        return TStatId();
    }

private:
    UXSPBatchMeshComponent* XSPBatchMeshComponent;
};

typedef UXSPBatchMeshComponent MyComponentClass;