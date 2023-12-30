#include "XSPSubModelMaterialActor.h"
#include "XSPSubModelActor.h"
#include "XSPModelActor.h"
#include "XSPDataStruct.h"
#include "MeshUtils.h"
#include "XSPBatchMeshComponent.h"
#include "XSPStat.h"


extern int32 XSPMaxNumVerticesPerBatch;
extern int32 XSPMinNumVerticesPerBatch;
extern int32 XSPMinNumVerticesUnbatch;

AXSPSubModelMaterialActor::AXSPSubModelMaterialActor()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}

AXSPSubModelMaterialActor::~AXSPSubModelMaterialActor()
{
    DEC_DWORD_STAT(STAT_XSPLoader_NumSubMaterialActor);
}

void AXSPSubModelMaterialActor::Init(AXSPSubModelActor* InParent, UMaterialInstanceDynamic* Material, int32 InCustomDepthStencilValue, bool bInRenderInMainAndDepthPass)
{
    Parent = InParent;
    MaterialInstanceDynamic = Material;
    CustomDepthStencilValue = InCustomDepthStencilValue;
    bRenderInMainAndDepthPass = bInRenderInMainAndDepthPass;

    INC_DWORD_STAT(STAT_XSPLoader_NumSubMaterialActor);
}

void AXSPSubModelMaterialActor::AddNode(int32 Dbid)
{
    NodeToAddArray.Add(Dbid);
}

void AXSPSubModelMaterialActor::AddNode(const TArray<int32>& InNodeArray)
{
    NodeToAddArray.Append(InNodeArray);
}

void AXSPSubModelMaterialActor::RemoveNode(int32 Dbid)
{
    NodeToRemoveArray.Add(Dbid);
}

void AXSPSubModelMaterialActor::RemoveNode(const TArray<int32>& InNodeArray)
{
    NodeToRemoveArray.Append(InNodeArray);
}

const TArray<struct FXSPNodeData*>& AXSPSubModelMaterialActor::GetNodeDataArray() const
{
    return Parent->GetNodeDataArray();
}

bool AXSPSubModelMaterialActor::TickDynamicCombine(float& InOutSeconds, bool bAsyncBuild)
{
    int64 BeginTicks = FDateTime::Now().GetTicks();

    if (PreProcess())
    {
        ProcessBatch(bAsyncBuild);
    }

    bool bFinished = ProcessRegister();

    InOutSeconds -= (float)(FDateTime::Now().GetTicks() - BeginTicks) / ETimespan::TicksPerSecond;

    return bFinished;
}

bool AXSPSubModelMaterialActor::PreProcess()
{
    TArray<UXSPBatchMeshComponent*> ComponentsToRelease;

    //处理待移除
    for (int32 Dbid : NodeToRemoveArray)
    {
        if (!NodeComponentMap.Contains(Dbid))
            continue;

        UXSPBatchMeshComponent* Component = NodeComponentMap[Dbid];
        check(Component);
        if (!ComponentsToRelease.Contains(Component))
        {
            ComponentsToRelease.Add(Component);
            BatchMeshComponentArray.Remove(Component);

            NodeToBuildArray.Append(Component->GetNodes());
        }
        NodeToBuildArray.Remove(Dbid);
        NodeComponentMap.Remove(Dbid);
    }
    NodeToRemoveArray.Reset();

    //处理待添加
    for (int32 Dbid : NodeToAddArray)
    {
        if (NodeComponentMap.Contains(Dbid))
            continue;

        NodeComponentMap.Add(Dbid, nullptr);
        NodeToBuildArray.Add(Dbid);
    }
    NodeToAddArray.Reset();

    //打开未满的包
    if (!NodeToBuildArray.IsEmpty())
    {
        for (TArray<UXSPBatchMeshComponent*>::TIterator Itr(BatchMeshComponentArray); Itr; ++Itr)
        {
            UXSPBatchMeshComponent* Component = *Itr;
            if (Component->GetNumVertices() < XSPMinNumVerticesPerBatch) //未满包的
            {
                if (!ComponentsToRelease.Contains(Component))
                {
                    NodeToBuildArray.Append(Component->GetNodes());

                    ComponentsToRelease.Add(Component);
                    Itr.RemoveCurrent();
                }
            }
        }
    }

    //释放包
    for (UXSPBatchMeshComponent* Component : ComponentsToRelease)
    {
        ReleaseComponent(Component);
    }

    return !NodeToBuildArray.IsEmpty();
}

void AXSPSubModelMaterialActor::ProcessBatch(bool bAsyncBuild)
{
    int32 NumBatchedVertices = 0;
    TArray<int32> BatchNodeArray;
    const TArray<FXSPNodeData*>& NodeDataArray = Parent->GetNodeDataArray();
    for (int32 Dbid : NodeToBuildArray)
    {
        int32 NodeVertexNum = NodeDataArray[Dbid]->MeshPositionArray.Num();
        //独立成包的
        if (NodeVertexNum > XSPMinNumVerticesUnbatch)
        {
            AddComponent({Dbid}, bAsyncBuild);
            continue;
        }

        if (NumBatchedVertices + NodeVertexNum > XSPMaxNumVerticesPerBatch)
        {
            AddComponent(BatchNodeArray, bAsyncBuild);
            BatchNodeArray.Reset();
            NumBatchedVertices = 0;
        }

        BatchNodeArray.Add(Dbid);
        NumBatchedVertices += NodeVertexNum;
    }
    if (BatchNodeArray.Num() > 0)
    {
        AddComponent(BatchNodeArray, bAsyncBuild);
    }
    NodeToBuildArray.Reset();
}

bool AXSPSubModelMaterialActor::ProcessRegister()
{
    for (TArray<UXSPBatchMeshComponent*>::TIterator Itr(BuildingComponentArray); Itr; ++Itr)
    {
        if ((*Itr)->TryFinishBuildMesh())
        {
            if (BatchMeshComponentArray.Contains(*Itr))
            {
                RegisterComponent(*Itr);
            }

            Itr.RemoveCurrent();
        }
    }
    return BuildingComponentArray.IsEmpty();
}

void AXSPSubModelMaterialActor::AddComponent(const TArray<int32>& DbidArray, bool bAsyncBuild)
{
    UXSPBatchMeshComponent* Component = NewObject<UXSPBatchMeshComponent>(this);
    for (int32 Dbid : DbidArray)
    {
        NodeComponentMap[Dbid] = Component;
    }
    BatchMeshComponentArray.Add(Component);

    Component->SetMaterial(0, MaterialInstanceDynamic);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetRenderInMainPass(bRenderInMainAndDepthPass);
    Component->SetRenderInDepthPass(bRenderInMainAndDepthPass);
    Component->SetRenderCustomDepth(CustomDepthStencilValue >= 0);
    Component->SetCustomDepthStencilValue(CustomDepthStencilValue);

    Component->Init(this, DbidArray, bAsyncBuild);
    if (bAsyncBuild)
    {
        BuildingComponentArray.Add(Component);
    }
    else
    {
        RegisterComponent(Component);
    }
}

void AXSPSubModelMaterialActor::ReleaseComponent(UXSPBatchMeshComponent* Component)
{
    for (int32 Dbid : Component->GetNodes())
    {
        if (NodeComponentMap.Contains(Dbid))
        {
            NodeComponentMap[Dbid] = nullptr;
        }
    }

    if (!BuildingComponentArray.Contains(Component))
    {
        Component->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
        Component->DestroyComponent();
    }
}

void AXSPSubModelMaterialActor::RegisterComponent(UXSPBatchMeshComponent* Component)
{
    Component->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

    int64 TicksBeforeRegisterComponent = FDateTime::Now().GetTicks();
    Component->RegisterComponent();
    INC_FLOAT_STAT_BY(STAT_XSPLoader_RegisterComponentTime, (float)(FDateTime::Now().GetTicks() - TicksBeforeRegisterComponent) / ETimespan::TicksPerSecond);

    INC_DWORD_STAT(STAT_XSPLoader_NumRegisteredComponents);
    INC_DWORD_STAT_BY(STAT_XSPLoader_NumRegisteredVertices, Component->GetNumVertices());
}
