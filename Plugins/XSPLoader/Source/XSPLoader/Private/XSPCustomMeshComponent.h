#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "XSPCustomMeshComponent.generated.h"

UCLASS()
class UXSPCustomMeshComponent : public UPrimitiveComponent, public IInterface_CollisionDataProvider
{
    GENERATED_BODY()
public:
    UXSPCustomMeshComponent();
    virtual ~UXSPCustomMeshComponent();

    void Init(class AXSPSubModelMaterialActor* Parent, const TArray<int32>& DbidArray, bool bAsyncBuild);

    const TArray<int32>& GetNodes() const;

    int32 GetNumVertices() const;

    int32 GetNode(int32 FaceIndex);

    bool TryFinishBuildMesh();

public:
    // Begin UObject Interface
    virtual void BeginDestroy() override;
    virtual bool IsReadyForFinishDestroy() override;
    // End UObject Interface.

    // UPrimitiveComponent interface
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
    virtual UBodySetup* GetBodySetup() override;
    virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material) override;
    virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
    // End of UPrimitiveComponent interface

    // IInterface_CollisionDataProvider Interface
    virtual bool GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
    virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
    // End IInterface_CollisionDataProvider Interface

private:
    friend class FXSPCustomMeshSceneProxy;
    friend class FXSPBuildCustomMeshTask;
    void BuildMesh_AnyThread();
    void ReleaseResources();

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

    FBoxSphereBounds LocalBounds;

    TSharedPtr<struct FXSPCustomMesh> CustomMesh;
    bool bRenderingResourcesInitialized = false;
    FRenderCommandFence ReleaseResourcesFence;

    UPROPERTY()
    UBodySetup* BuildingBodySetup = nullptr;

    UPROPERTY()
    UBodySetup* MeshBodySetup = nullptr;

    UPROPERTY()
    UMaterialInterface* Material = nullptr;

    FAsyncTask<class FXSPBuildCustomMeshTask>* AsyncBuildTask = nullptr;
};

class FXSPBuildCustomMeshTask : public FNonAbandonableTask
{
public:
    FXSPBuildCustomMeshTask(UXSPCustomMeshComponent*);

    void DoWork();

    TStatId GetStatId() const
    {
        return TStatId();
    }

private:
    UXSPCustomMeshComponent* XSPCustomMeshComponent;
};

//typedef UXSPCustomMeshComponent MyComponentClass;
