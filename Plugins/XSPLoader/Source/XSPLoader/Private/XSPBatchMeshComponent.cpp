#include "XSPBatchMeshComponent.h"
#include "XSPModelActor.h"
#include "XSPDataStruct.h"
#include "XSPStat.h"
#include "MeshUtils.h"

#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"

bool bXSPBuildStaticMesh = true;
FAutoConsoleVariableRef CVarXSPBuildStaticMesh(
    TEXT("xsp.BuildStaticMesh"),
    bXSPBuildStaticMesh,
    TEXT("是否生成StaticMesh，缺省为true")
);

bool bXSPBuildPhysicsData = true;
FAutoConsoleVariableRef CVarXSPBuildPhysicsData(
    TEXT("xsp.BuildPhysicsData"),
    bXSPBuildPhysicsData,
    TEXT("是否生成碰撞体，缺省为true")
);

bool bXSPDiscardCPUDataAfterUpload = true;
FAutoConsoleVariableRef CVarXSPDiscardCPUDataAfterUpload(
    TEXT("xsp.DiscardCPUDataAfterUpload"),
    bXSPDiscardCPUDataAfterUpload,
    TEXT("是否在上传资源到GPU后释放其CPU数据，缺省为true")
);

UXSPBatchMeshComponent::UXSPBatchMeshComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

UXSPBatchMeshComponent::~UXSPBatchMeshComponent()
{
    DEC_DWORD_STAT(STAT_XSPLoader_NumBatchComponent);
}

void UXSPBatchMeshComponent::Init(AXSPModelActor* InOwnerActor, const TArray<int32>& InDbidArray, bool bAsyncBuild)
{
    OwnerActor = InOwnerActor;
    DbidArray = InDbidArray;
    NumVerticesTotal = 0;

    bHasNoStreamableTextures = true;

    if (bXSPBuildStaticMesh)
    {
        BuildingStaticMesh = NewObject<UStaticMesh>(this);

        if (bAsyncBuild)
        {
            AsyncBuildTask = new FAsyncTask<FXSPBuildStaticMeshTask>(this);
            AsyncBuildTask->StartBackgroundTask();
            INC_DWORD_STAT(STAT_XSPLoader_NumBuildingComponents);
        }
        else
        {
            BuildStaticMesh_AnyThread();

            SetStaticMesh(BuildingStaticMesh);
            BuildingStaticMesh = nullptr;
        }
    }

    INC_DWORD_STAT(STAT_XSPLoader_NumBatchComponent);
}

const TArray<int32>& UXSPBatchMeshComponent::GetNodes() const
{
    return DbidArray;
}

int32 UXSPBatchMeshComponent::GetNumVertices() const
{
    return NumVerticesTotal;
}

int32 UXSPBatchMeshComponent::GetNode(int32 FaceIndex)
{
    for (int32 i = 0; i < EndFaceIndexArray.Num(); i++)
    {
        if (FaceIndex <= EndFaceIndexArray[i])
            return DbidArray[i];
    }
    return -1;
}

bool UXSPBatchMeshComponent::TryFinishBuildMesh()
{
    if (bXSPBuildStaticMesh && nullptr != AsyncBuildTask && AsyncBuildTask->IsDone())
    {
        SetStaticMesh(BuildingStaticMesh);
        BuildingStaticMesh = nullptr;

        delete AsyncBuildTask;
        AsyncBuildTask = nullptr;

        DEC_DWORD_STAT(STAT_XSPLoader_NumBuildingComponents);

        return true;
    }

    return false;
}

UBodySetup* UXSPBatchMeshComponent::GetBodySetup()
{
    if (bXSPBuildPhysicsData && !MeshBodySetup && !BuildingBodySetup)
    {
        BuildPhysicsData(true);
    }

    return MeshBodySetup;
}

bool UXSPBatchMeshComponent::GetPhysicsTriMeshData(FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
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

void UXSPBatchMeshComponent::BuildStaticMesh_AnyThread()
{
    int64 Ticks1 = FDateTime::Now().GetTicks();

    const TArray<FXSPNodeData*>& NodeDataArray = OwnerActor->GetNodeDataArray();
    for (int32 Dbid : DbidArray)
    {
        NumVerticesTotal += NodeDataArray[Dbid]->MeshPositionArray.Num();
        EndFaceIndexArray.Add(NumVerticesTotal / 3 - 1);
    }

    BuildingStaticMesh->SetFlags(RF_Transient | RF_DuplicateTransient | RF_TextExportTransient);
    BuildingStaticMesh->NeverStream = true;
    BuildingStaticMesh->bAllowCPUAccess = !bXSPDiscardCPUDataAfterUpload;
    BuildingStaticMesh->GetStaticMaterials().Add(FStaticMaterial());

    TUniquePtr<FStaticMeshRenderData> StaticMeshRenderData = MakeUnique<FStaticMeshRenderData>();
    StaticMeshRenderData->ScreenSize[0].Default = 1.0f;
    StaticMeshRenderData->AllocateLODResources(1);

    FStaticMeshLODResources& StaticMeshLODResources = StaticMeshRenderData->LODResources[0];

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
            //StaticMeshBuildVertices[Index].Color = FColor::White;
            StaticMeshBuildVertices[Index].UVs[0].Set(0, 0);
            IndexArray[Index] = Index;
            Index++;
        }
    }
    StaticMeshLODResources.VertexBuffers.PositionVertexBuffer.Init(StaticMeshBuildVertices, !bXSPDiscardCPUDataAfterUpload);
    StaticMeshLODResources.VertexBuffers.StaticMeshVertexBuffer.Init(StaticMeshBuildVertices, 1, !bXSPDiscardCPUDataAfterUpload);
    StaticMeshLODResources.IndexBuffer.SetIndices(IndexArray, (IndexArray.Num() <= (int32)MAX_uint16 + 1) ? EIndexBufferStride::Type::Force16Bit : EIndexBufferStride::Type::Force32Bit);
    StaticMeshLODResources.bHasDepthOnlyIndices = false;
    StaticMeshLODResources.bHasReversedIndices = false;
    StaticMeshLODResources.bHasReversedDepthOnlyIndices = false;

    FStaticMeshSection& Section = StaticMeshLODResources.Sections.AddDefaulted_GetRef();
    Section.bEnableCollision = false;
    Section.NumTriangles = NumVerticesTotal / 3;
    Section.FirstIndex = 0;
    Section.MinVertexIndex = 0;
    Section.MaxVertexIndex = NumVerticesTotal - 1;
    Section.MaterialIndex = 0;
    Section.bForceOpaque = false;

    StaticMeshRenderData->Bounds = FBoxSphereBounds(FBox(BoundingBox));

    BuildingStaticMesh->SetRenderData(MoveTemp(StaticMeshRenderData));
    BuildingStaticMesh->InitResources();

    BuildingStaticMesh->CalculateExtendedBounds();

    INC_FLOAT_STAT_BY(STAT_XSPLoader_BuildStaticMeshTime, (float)(FDateTime::Now().GetTicks() - Ticks1) / ETimespan::TicksPerSecond);
}

void UXSPBatchMeshComponent::BuildPhysicsData(bool bAsync)
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
        BuildingBodySetup->CreatePhysicsMeshesAsync(FOnAsyncPhysicsCookFinished::CreateUObject(this, &UXSPBatchMeshComponent::FinishPhysicsAsyncCook));
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

void UXSPBatchMeshComponent::FinishPhysicsAsyncCook(bool bSuccess)
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

FXSPBuildStaticMeshTask::FXSPBuildStaticMeshTask(UXSPBatchMeshComponent* InComponent)
    : XSPBatchMeshComponent(InComponent)
{
}

void FXSPBuildStaticMeshTask::DoWork()
{
    XSPBatchMeshComponent->BuildStaticMesh_AnyThread();
}