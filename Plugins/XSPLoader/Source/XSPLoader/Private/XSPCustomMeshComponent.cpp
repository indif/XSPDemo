#include "XSPCustomMeshComponent.h"
#include "XSPSubModelMaterialActor.h"
#include "XSPDataStruct.h"
#include "XSPStat.h"
#include "MeshUtils.h"
#include "RHI.h"

//Mesh渲染资源，在Component::Init时（异步）初始化资源，在Component::BeginDestroy时释放资源
struct FXSPCustomMesh
{
	FStaticMeshVertexBuffer StaticMeshVertexBuffer;
	FPositionVertexBuffer PositionVertexBuffer;
	FColorVertexBuffer ColorVertexBuffer;

	FRawStaticIndexBuffer IndexBuffer;

    FLocalVertexFactory VertexFactory;

    FXSPCustomMesh(ERHIFeatureLevel::Type InFeatureLevel)
        : VertexFactory(InFeatureLevel, "FXSPCustomMesh")
    {}

    void InitResources()
    {
        FXSPCustomMesh* Self = this;
        ENQUEUE_RENDER_COMMAND(XSPCustomMeshInit)(
            [Self](FRHICommandListImmediate& RHICmdList)
            {
                Self->PositionVertexBuffer.InitResource();
                Self->StaticMeshVertexBuffer.InitResource();
                Self->ColorVertexBuffer.InitResource();

                FLocalVertexFactory::FDataType Data;
                Self->PositionVertexBuffer.BindPositionVertexBuffer(&Self->VertexFactory, Data);
                Self->StaticMeshVertexBuffer.BindTangentVertexBuffer(&Self->VertexFactory, Data);
                Self->StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&Self->VertexFactory, Data);
                Self->StaticMeshVertexBuffer.BindLightMapVertexBuffer(&Self->VertexFactory, Data, 0);
                Self->ColorVertexBuffer.BindColorVertexBuffer(&Self->VertexFactory, Data);
                Self->VertexFactory.SetData(Data);
                Self->VertexFactory.InitResource();

                Self->IndexBuffer.InitResource();
            });
    }

    void ReleaseResources()
    {
        BeginReleaseResource(&StaticMeshVertexBuffer);
        BeginReleaseResource(&PositionVertexBuffer);
        BeginReleaseResource(&ColorVertexBuffer);
        BeginReleaseResource(&IndexBuffer);
        BeginReleaseResource(&VertexFactory);
    }
};

class FXSPCustomMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
    SIZE_T GetTypeHash() const override
    {
        static size_t UniquePointer;
        return reinterpret_cast<size_t>(&UniquePointer);
    }

    FXSPCustomMeshSceneProxy(UXSPCustomMeshComponent* Component)
        : FPrimitiveSceneProxy(Component)
        , XSPCustomMeshComponent(Component)
        , CustomMesh(Component->CustomMesh.Get())
        , MaterialRelevance(Component->Material->GetRelevance_Concurrent(GetScene().GetFeatureLevel()))
    {
    }

    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
    {
        FPrimitiveViewRelevance Result;
        Result.bDrawRelevance = IsShown(View);
        Result.bShadowRelevance = true;//IsShadowCast(View);
        Result.bStaticRelevance = true;
        Result.bDynamicRelevance = false;
        Result.bRenderInMainPass = ShouldRenderInMainPass();
        Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
        Result.bRenderCustomDepth = ShouldRenderCustomDepth();
        Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
        MaterialRelevance.SetPrimitiveViewRelevance(Result);
        Result.bVelocityRelevance = IsMovable() & Result.bOpaque & Result.bRenderInMainPass;
        return Result;
    }

    virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override 
    {
        FMeshBatch MeshBatch;
        GetMeshElement(MeshBatch);
        PDI->DrawMesh(MeshBatch, FLT_MAX);
    }

    virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
    {
        for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
        {
            const FSceneView* View = Views[ViewIndex];

            if (IsShown(View) && (VisibilityMap & (1 << ViewIndex)))
            {
                FMeshBatch MeshBatch;
                GetMeshElement(MeshBatch);
                Collector.AddMesh(ViewIndex, MeshBatch);
            }
        }
    }

    void GetMeshElement(FMeshBatch& OutMeshBatch) const
    {
        OutMeshBatch.bWireframe = false;
        OutMeshBatch.VertexFactory = &CustomMesh->VertexFactory;
        OutMeshBatch.MaterialRenderProxy = XSPCustomMeshComponent->Material->GetRenderProxy();
        OutMeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
        OutMeshBatch.Type = PT_TriangleList;
        OutMeshBatch.DepthPriorityGroup = SDPG_World;
        OutMeshBatch.bCanApplyViewModeOverrides = false;
        OutMeshBatch.LODIndex = 0;
        OutMeshBatch.SegmentIndex = 0;
        OutMeshBatch.CastShadow = true;// XSPCustomMeshComponent->CastShadow;

        FMeshBatchElement& BatchElement = OutMeshBatch.Elements[0];
        BatchElement.IndexBuffer = &CustomMesh->IndexBuffer;
        BatchElement.FirstIndex = 0;
        BatchElement.NumPrimitives = XSPCustomMeshComponent->NumVerticesTotal / 3;
        BatchElement.MinVertexIndex = 0;
        BatchElement.MaxVertexIndex = XSPCustomMeshComponent->NumVerticesTotal;
    }
    
    virtual bool CanBeOccluded() const override
    {
        return !MaterialRelevance.bDisableDepthTest;
    }

    virtual uint32 GetMemoryFootprint(void) const
    {
        return(sizeof(*this) + GetAllocatedSize());
    }

    uint32 GetAllocatedSize(void) const
    {
        return(FPrimitiveSceneProxy::GetAllocatedSize());
    }

private:
    UXSPCustomMeshComponent* XSPCustomMeshComponent;
    FXSPCustomMesh* CustomMesh;
    FMaterialRelevance MaterialRelevance;
};

UXSPCustomMeshComponent::UXSPCustomMeshComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

UXSPCustomMeshComponent::~UXSPCustomMeshComponent()
{
    DEC_DWORD_STAT(STAT_XSPLoader_NumBatchComponent);
}

void UXSPCustomMeshComponent::Init(AXSPSubModelMaterialActor* InParent, const TArray<int32>& InDbidArray, bool bAsyncBuild)
{
    Parent = InParent;
    DbidArray = InDbidArray;
    NumVerticesTotal = 0;

    bHasNoStreamableTextures = true;

    if (bAsyncBuild)
    {
        AsyncBuildTask = new FAsyncTask<FXSPBuildCustomMeshTask>(this);
        AsyncBuildTask->StartBackgroundTask();
        INC_DWORD_STAT(STAT_XSPLoader_NumBuildingComponents);
    }
    else
    {
        BuildMesh_AnyThread();
    }

    INC_DWORD_STAT(STAT_XSPLoader_NumBatchComponent);
}

const TArray<int32>& UXSPCustomMeshComponent::GetNodes() const
{
    return DbidArray;
}

int32 UXSPCustomMeshComponent::GetNumVertices() const
{
    return NumVerticesTotal;
}

int32 UXSPCustomMeshComponent::GetNode(int32 FaceIndex)
{
    for (int32 i = 0; i < EndFaceIndexArray.Num(); i++)
    {
        if (FaceIndex <= EndFaceIndexArray[i])
            return DbidArray[i];
    }
    return -1;
}

bool UXSPCustomMeshComponent::TryFinishBuildMesh()
{
    if (nullptr != AsyncBuildTask)
    {
        if (AsyncBuildTask->IsDone())
        {
            delete AsyncBuildTask;
            AsyncBuildTask = nullptr;

            DEC_DWORD_STAT(STAT_XSPLoader_NumBuildingComponents);
            return true;
        }
        return false;
    }

    return true;
}

void UXSPCustomMeshComponent::BeginDestroy()
{
    Super::BeginDestroy();

    if (bRenderingResourcesInitialized)
    {
        ReleaseResources();
    }
}

bool UXSPCustomMeshComponent::IsReadyForFinishDestroy()
{
    if (!Super::IsReadyForFinishDestroy())
    {
        return false;
    }

    if (bRenderingResourcesInitialized)
    {
        ReleaseResources();
    }
    return ReleaseResourcesFence.IsFenceComplete();
}

FPrimitiveSceneProxy* UXSPCustomMeshComponent::CreateSceneProxy()
{
    return new FXSPCustomMeshSceneProxy(this);
}

FBoxSphereBounds UXSPCustomMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    FBoxSphereBounds Ret(LocalBounds.TransformBy(LocalToWorld));

    Ret.BoxExtent *= BoundsScale;
    Ret.SphereRadius *= BoundsScale;

    return Ret;
}

UBodySetup* UXSPCustomMeshComponent::GetBodySetup()
{
    if (nullptr == MeshBodySetup && nullptr == BuildingBodySetup)
    {
        BuildPhysicsData(true);
    }

    return MeshBodySetup;
}

void UXSPCustomMeshComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial)
{
    Material = InMaterial;
}

void UXSPCustomMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
    OutMaterials.Add(Material);
}

bool UXSPCustomMeshComponent::GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
    CollisionData->Vertices.SetNum(NumVerticesTotal);
    CollisionData->Indices.SetNum(NumVerticesTotal / 3);

    const TArray<FXSPNodeData*>& NodeDataArray = Parent->GetNodeDataArray();
    int32 VerticesIndex = 0, IndicesIndex = 0;
    int32 VertexBase = 0;
    for (int32 Dbid : DbidArray)
    {
        int32 NumVertices = NodeDataArray[Dbid]->MeshPositionArray.Num();
        for (int32 i = 0; i < NumVertices; i++)
        {
            CollisionData->Vertices[VerticesIndex++] = FVector3f(NodeDataArray[Dbid]->MeshPositionArray[i]);
        }
        int32 NumTriangles = NumVertices / 3;
        for (int32 j = 0; j < NumTriangles; j++)
        {
            CollisionData->Indices[IndicesIndex].v0 = VertexBase + j * 3 + 0;
            CollisionData->Indices[IndicesIndex].v1 = VertexBase + j * 3 + 1;
            CollisionData->Indices[IndicesIndex].v2 = VertexBase + j * 3 + 2;
            IndicesIndex++;
        }
        VertexBase += NumVertices;
    }

    //CollisionData->bFlipNormals = true;
    //CollisionData->bDeformableMesh = true;
    //CollisionData->bFastCook = true;

    return true;
}

bool UXSPCustomMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
    return true;
}

void UXSPCustomMeshComponent::BuildMesh_AnyThread()
{
    int64 Ticks1 = FDateTime::Now().GetTicks();

    const TArray<FXSPNodeData*>& NodeDataArray = Parent->GetNodeDataArray();
    for (int32 Dbid : DbidArray)
    {
        NumVerticesTotal += NodeDataArray[Dbid]->MeshPositionArray.Num();
        EndFaceIndexArray.Add(NumVerticesTotal / 3 - 1);
    }

    CustomMesh = MakeShareable(new FXSPCustomMesh(GMaxRHIFeatureLevel));

    TArray<FStaticMeshBuildVertex> StaticMeshBuildVertices;
    StaticMeshBuildVertices.SetNum(NumVerticesTotal);
    TArray<uint32> IndexArray;
    IndexArray.SetNum(NumVerticesTotal);
    FBox3f BoundingBox;
    BoundingBox.Init();
    int32 Index = 0;
    for (int32 Dbid : DbidArray)
    {
        int32 NumVertices = NodeDataArray[Dbid]->MeshPositionArray.Num();
        for (int32 i = 0; i < NumVertices; i++)
        {
            BoundingBox += NodeDataArray[Dbid]->MeshPositionArray[i];
            StaticMeshBuildVertices[Index].Position = NodeDataArray[Dbid]->MeshPositionArray[i];
            StaticMeshBuildVertices[Index].TangentZ = NodeDataArray[Dbid]->MeshNormalArray[i].ToFVector3f();
            StaticMeshBuildVertices[Index].Color = FColor::White;
            StaticMeshBuildVertices[Index].UVs[0].Set(0, 0);
            IndexArray[Index] = Index;
            Index++;
        }
    }
    CustomMesh->PositionVertexBuffer.Init(StaticMeshBuildVertices, false);
    CustomMesh->StaticMeshVertexBuffer.Init(StaticMeshBuildVertices, 1, false);
    CustomMesh->IndexBuffer.SetIndices(IndexArray, (IndexArray.Num() <= (int32)MAX_uint16 + 1) ? EIndexBufferStride::Type::Force16Bit : EIndexBufferStride::Type::Force32Bit);
    
    CustomMesh->InitResources();
    bRenderingResourcesInitialized = true;

    LocalBounds = FBoxSphereBounds(FBox(BoundingBox));

    INC_FLOAT_STAT_BY(STAT_XSPLoader_BuildStaticMeshTime, (float)(FDateTime::Now().GetTicks() - Ticks1) / ETimespan::TicksPerSecond);
}

void UXSPCustomMeshComponent::ReleaseResources()
{
    if (CustomMesh.IsValid())
    {
        CustomMesh->ReleaseResources();

        ReleaseResourcesFence.BeginFence();
    }

    bRenderingResourcesInitialized = false;
}

void UXSPCustomMeshComponent::BuildPhysicsData(bool bAsync)
{
    if (MeshBodySetup || BuildingBodySetup)
        return;

    {
        //FGCScopeGuard Scope;
        BuildingBodySetup = NewObject<UBodySetup>(this);
    }
    BuildingBodySetup->BodySetupGuid = FGuid::NewGuid();
    BuildingBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
    BuildingBodySetup->bGenerateMirroredCollision = false;
    BuildingBodySetup->bSupportUVsAndFaceRemap = false;
    BuildingBodySetup->bDoubleSidedGeometry = false;

    if (bAsync)
    {
        BuildingBodySetup->CreatePhysicsMeshesAsync(FOnAsyncPhysicsCookFinished::CreateUObject(this, &UXSPCustomMeshComponent::FinishPhysicsAsyncCook));
    }
    else
    {
        BuildingBodySetup->CreatePhysicsMeshes();
        MeshBodySetup = BuildingBodySetup;
        BuildingBodySetup = nullptr;
        RecreatePhysicsState();
        INC_DWORD_STAT(STAT_XSPLoader_NumCreatedPhysicsState);
    }
}

void UXSPCustomMeshComponent::FinishPhysicsAsyncCook(bool bSuccess)
{
    if (bSuccess)
    {
        MeshBodySetup = BuildingBodySetup;
        BuildingBodySetup = nullptr;
        RecreatePhysicsState();
    }
    else
    {
        checkNoEntry();
    }
    INC_DWORD_STAT(STAT_XSPLoader_NumCreatedPhysicsState);
}

FXSPBuildCustomMeshTask::FXSPBuildCustomMeshTask(UXSPCustomMeshComponent* InComponent)
    : XSPCustomMeshComponent(InComponent)
{
}

void FXSPBuildCustomMeshTask::DoWork()
{
    XSPCustomMeshComponent->BuildMesh_AnyThread();
}