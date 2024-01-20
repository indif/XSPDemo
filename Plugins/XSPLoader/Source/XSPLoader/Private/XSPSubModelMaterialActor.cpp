#include "XSPSubModelMaterialActor.h"
#include "XSPSubModelActor.h"
#include "XSPModelActor.h"
#include "XSPDataStruct.h"
#include "MeshUtils.h"
#include "XSPStat.h"
#include "XSPBatchMeshComponent.h"
#include "XSPCustomMeshComponent.h"

extern int32 XSPMaxNumVerticesPerBatch;
extern int32 XSPMinNumVerticesPerBatch;
extern int32 XSPMinNumVerticesUnbatch;


FXSPSubModelMaterialActor::FXSPSubModelMaterialActor()
{
    INC_DWORD_STAT(STAT_XSPLoader_NumSubMaterialActor);
}

FXSPSubModelMaterialActor::~FXSPSubModelMaterialActor()
{
    DEC_DWORD_STAT(STAT_XSPLoader_NumSubMaterialActor);
}

void FXSPSubModelMaterialActor::Init(AXSPModelActor* InOwner, UMaterialInstanceDynamic* Material, int32 InCustomDepthStencilValue, bool bInRenderInMainAndDepthPass)
{
    Owner = InOwner;
    MaterialInstanceDynamic = Material;
    CustomDepthStencilValue = InCustomDepthStencilValue;
    bRenderInMainAndDepthPass = bInRenderInMainAndDepthPass;
}

void FXSPSubModelMaterialActor::AddNode(int32 Dbid)
{
    NodeToAddArray.Add(Dbid);
}

void FXSPSubModelMaterialActor::AddNode(const TArray<int32>& InNodeArray)
{
    NodeToAddArray.Append(InNodeArray);
}

void FXSPSubModelMaterialActor::RemoveNode(int32 Dbid)
{
    NodeToRemoveArray.Add(Dbid);
}

void FXSPSubModelMaterialActor::RemoveNode(const TArray<int32>& InNodeArray)
{
    NodeToRemoveArray.Append(InNodeArray);
}

bool FXSPSubModelMaterialActor::TickDynamicCombine(float& InOutSeconds, bool bAsyncBuild)
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

bool FXSPSubModelMaterialActor::PreProcess()
{
    TArray<UPrimitiveComponent*> ComponentsToRelease;

    //处理待移除
    for (int32 Dbid : NodeToRemoveArray)
    {
        if (!NodeComponentMap.Contains(Dbid))
            continue;

        MyComponentClass* Component = Cast<MyComponentClass>(NodeComponentMap[Dbid]);
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
        for (TArray<UPrimitiveComponent*>::TIterator Itr(BatchMeshComponentArray); Itr; ++Itr)
        {
            MyComponentClass* Component = Cast<MyComponentClass>(*Itr);
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
    for (UPrimitiveComponent* Component : ComponentsToRelease)
    {
        ReleaseComponent(Component);
    }

    return !NodeToBuildArray.IsEmpty();
}

void FXSPSubModelMaterialActor::ProcessBatch(bool bAsyncBuild)
{
    int32 NumBatchedVertices = 0;
    TArray<int32> BatchNodeArray;
    const TArray<FXSPNodeData*>& NodeDataArray = Owner->GetNodeDataArray();
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

bool FXSPSubModelMaterialActor::ProcessRegister()
{
    for (TArray<TStrongObjectPtr<UPrimitiveComponent>>::TIterator Itr(BuildingComponentArray); Itr; ++Itr)
    {
        MyComponentClass* Component = Cast<MyComponentClass>(Itr->Get());
        if (Component->TryFinishBuildMesh())
        {
            if (BatchMeshComponentArray.Contains(Component))
            {
                RegisterComponent(Component);
            }

            Itr.RemoveCurrent();
        }
    }
    return BuildingComponentArray.IsEmpty();
}

void FXSPSubModelMaterialActor::AddComponent(const TArray<int32>& DbidArray, bool bAsyncBuild)
{
    MyComponentClass* Component = NewObject<MyComponentClass>(Owner);
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

    Component->Init(Owner, DbidArray, bAsyncBuild);
    if (bAsyncBuild)
    {
        BuildingComponentArray.Add(TStrongObjectPtr<UPrimitiveComponent>(Component));
    }
    else
    {
        RegisterComponent(Component);
    }
}

void FXSPSubModelMaterialActor::ReleaseComponent(UPrimitiveComponent* Component)
{
    MyComponentClass* XSPCustomMeshComponent = Cast<MyComponentClass>(Component);
    const TArray<int32>& NodeIdArray = XSPCustomMeshComponent->GetNodes();
    for (int32 Dbid : NodeIdArray)
    {
        if (NodeComponentMap.Contains(Dbid))
        {
            NodeComponentMap[Dbid] = nullptr;
        }
    }

    //if (!BuildingComponentArray.Contains(Component))
    {
        Component->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
        Component->DestroyComponent();
    }
}

void FXSPSubModelMaterialActor::RegisterComponent(UPrimitiveComponent* Component)
{
    Component->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

    int64 TicksBeforeRegisterComponent = FDateTime::Now().GetTicks();
    Component->RegisterComponent();
    INC_FLOAT_STAT_BY(STAT_XSPLoader_RegisterComponentTime, (float)(FDateTime::Now().GetTicks() - TicksBeforeRegisterComponent) / ETimespan::TicksPerSecond);

    INC_DWORD_STAT(STAT_XSPLoader_NumRegisteredComponents);
    INC_DWORD_STAT_BY(STAT_XSPLoader_NumRegisteredVertices, Cast<MyComponentClass>(Component)->GetNumVertices());
}
