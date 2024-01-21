#include "XSPCustomMeshComponent.h"
#include "XSPModelActor.h"
#include "XSPDataStruct.h"
#include "XSPStat.h"
#include "MeshUtils.h"
#include "XSPVertexFactory.h"
#include "RHI.h"
#include "XSPPositionVertexBuffer.h"

struct FXSPCustomMesh
{
	FStaticMeshVertexBuffer StaticMeshVertexBuffer;
	FXSPPositionVertexBuffer PositionVertexBuffer;
	FRawStaticIndexBuffer IndexBuffer;
    FXSPVertexFactory VertexFactory;

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

                FXSPDataType Data;
                Self->PositionVertexBuffer.BindPositionVertexBuffer(&Self->VertexFactory, Data);
                Self->StaticMeshVertexBuffer.BindTangentVertexBuffer(&Self->VertexFactory, Data);
                //Self->StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&Self->VertexFactory, Data);
                //Self->StaticMeshVertexBuffer.BindLightMapVertexBuffer(&Self->VertexFactory, Data, 0);
                Self->VertexFactory.SetData(Data);
                Self->VertexFactory.InitResource();

                Self->IndexBuffer.InitResource();
            });
    }

    void ReleaseResources()
    {
        BeginReleaseResource(&StaticMeshVertexBuffer);
        BeginReleaseResource(&PositionVertexBuffer);
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
        , CustomMesh(Component->CustomMesh.Get())
        , Material(Component->GetMaterial(0))
        , MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
    {
        NumVertices = Component->NumVerticesTotal;
        if (Material == NULL)
        {
            Material = UMaterial::GetDefaultMaterial(MD_Surface);
        }
    }
    ~FXSPCustomMeshSceneProxy()
    {
    }

    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
    {
        FPrimitiveViewRelevance Result;
        Result.bDrawRelevance = true;
        Result.bShadowRelevance = true;
        Result.bStaticRelevance = true;
        Result.bDynamicRelevance = false;
        Result.bRenderInMainPass = ShouldRenderInMainPass();
        Result.bRenderInDepthPass = ShouldRenderInDepthPass();
        Result.bUsesLightingChannels = false;
        Result.bRenderCustomDepth = ShouldRenderCustomDepth();
        Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
        MaterialRelevance.SetPrimitiveViewRelevance(Result);

        if (!View->Family->EngineShowFlags.Materials)
        {
            Result.bOpaque = true;
        }

        Result.bVelocityRelevance = IsMovable() & Result.bOpaque & Result.bRenderInMainPass;
        return Result;
    }

    virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override 
    {
        FMeshBatch MeshBatch;
        GetMeshElement(MeshBatch);
        PDI->DrawMesh(MeshBatch, FLT_MAX);
    }

    void GetMeshElement(FMeshBatch& OutMeshBatch) const
    {
        OutMeshBatch.bWireframe = false;
        OutMeshBatch.VertexFactory = &CustomMesh->VertexFactory;
        OutMeshBatch.MaterialRenderProxy = Material->GetRenderProxy();
        OutMeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
        OutMeshBatch.Type = PT_TriangleList;
        OutMeshBatch.DepthPriorityGroup = SDPG_World;
        OutMeshBatch.bCanApplyViewModeOverrides = false;
        OutMeshBatch.LODIndex = 0;
        OutMeshBatch.SegmentIndex = 0;
        OutMeshBatch.CastShadow = true;

        FMeshBatchElement& BatchElement = OutMeshBatch.Elements[0];
        BatchElement.IndexBuffer = &CustomMesh->IndexBuffer;
        BatchElement.FirstIndex = 0;
        BatchElement.NumPrimitives = NumVertices / 3;
        BatchElement.MinVertexIndex = 0;
        BatchElement.MaxVertexIndex = NumVertices-1;
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
    uint32 NumVertices;
    UMaterialInterface* Material;
    FMaterialRelevance MaterialRelevance;
};

UXSPCustomMeshComponent::UXSPCustomMeshComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

UXSPCustomMeshComponent::~UXSPCustomMeshComponent()
{
}

void UXSPCustomMeshComponent::Init(AXSPModelActor* InOwnerActor, const TArray<int32>& InDbidArray, bool bAsyncBuild)
{
    OwnerActor = InOwnerActor;
    DbidArray = InDbidArray;
    NumVerticesTotal = 0;

    bHasNoStreamableTextures = true;

    if (bAsyncBuild)
    {
        AsyncBuildTask = new FAsyncTask<FXSPBuildCustomMeshTask>(this);
        AsyncBuildTask->StartBackgroundTask();
    }
    else
    {
        BuildMesh_AnyThread();
    }
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

bool UXSPCustomMeshComponent::GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
    CollisionData->Vertices.SetNum(NumVerticesTotal);
    CollisionData->Indices.SetNum(NumVerticesTotal / 3);

    const TArray<FXSPNodeData*>& NodeDataArray = OwnerActor->GetNodeDataArray();
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
    const TArray<FXSPNodeData*>& NodeDataArray = OwnerActor->GetNodeDataArray();
    for (int32 Dbid : DbidArray)
    {
        NumVerticesTotal += NodeDataArray[Dbid]->MeshPositionArray.Num();
        EndFaceIndexArray.Add(NumVerticesTotal / 3 - 1);
    }

    CustomMesh = MakeShareable(new FXSPCustomMesh(GMaxRHIFeatureLevel));

    TArray<uint32> IndexArray;
    IndexArray.SetNum(NumVerticesTotal);

    CustomMesh->PositionVertexBuffer.Init(NumVerticesTotal, false);
    void* PositionData = CustomMesh->PositionVertexBuffer.GetVertexData();
    uint32 PositionStride = CustomMesh->PositionVertexBuffer.GetStride();

    CustomMesh->StaticMeshVertexBuffer.Init(NumVerticesTotal, 0, false);

    FBox3f BoundingBox;
    BoundingBox.Init();
    uint32 Index = 0;
    uint32 Offset = 0;
    for (int32 Dbid : DbidArray)
    {
        int32 NumVertices = NodeDataArray[Dbid]->MeshPositionArray.Num();

        FMemory::Memcpy(&((uint8*)PositionData)[Offset * PositionStride], NodeDataArray[Dbid]->MeshPositionArray.GetData(), PositionStride * NumVertices);
        Offset += NumVertices;

        for (int32 i = 0; i < NumVertices; i++)
        {
            BoundingBox += NodeDataArray[Dbid]->MeshPositionArray[i];

            CustomMesh->StaticMeshVertexBuffer.SetVertexTangents(Index, FVector3f::ZeroVector, FVector3f::ZeroVector, NodeDataArray[Dbid]->MeshNormalArray[i].ToFVector3f());

            IndexArray[Index] = Index;
            Index++;
        }

    }
    CustomMesh->IndexBuffer.SetIndices(IndexArray, (IndexArray.Num() <= (int32)MAX_uint16 + 1) ? EIndexBufferStride::Type::Force16Bit : EIndexBufferStride::Type::Force32Bit);
    
    CustomMesh->InitResources();
    bRenderingResourcesInitialized = true;

    LocalBounds = FBoxSphereBounds(FBox(BoundingBox));
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