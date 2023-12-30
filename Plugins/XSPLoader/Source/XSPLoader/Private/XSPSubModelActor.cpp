#include "XSPSubModelActor.h"
#include "XSPModelActor.h"
#include "XSPDataStruct.h"
#include "XSPSubModelMaterialActor.h"
#include "XSPStat.h"

AXSPSubModelActor::AXSPSubModelActor()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}

AXSPSubModelActor::~AXSPSubModelActor()
{
    DEC_DWORD_STAT(STAT_XSPLoader_NumSubActor);
}

void AXSPSubModelActor::Init(AXSPModelActor* InParent, int32 Dbid, int32 Num)
{
    Parent = InParent;
    StartDbid = Dbid;
    NumNodes = Num;

    const TArray<FXSPNodeData*>& NodeDataArray = Parent->GetNodeDataArray();
    TMap<FLinearColor, TArray<int32>> MaterialNodesMap;
    for (int32 Index = 0; Index < Num; Index++)
    {
        if (NodeDataArray[StartDbid + Index]->MeshPositionArray.IsEmpty())
            continue;

        FLinearColor& Material = NodeDataArray[StartDbid + Index]->MeshMaterial;
        if (!MaterialNodesMap.Contains(Material))
            MaterialNodesMap.Add(Material, TArray<int32>());
        MaterialNodesMap[Material].Add(StartDbid + Index);
    }

    for (auto& Pair : MaterialNodesMap)
    {
        GetOrCreateMaterialActor(Pair.Key)->AddNode(MoveTemp(Pair.Value));
    }

    INC_DWORD_STAT(STAT_XSPLoader_NumSubActor);
}

void AXSPSubModelActor::SetRenderCustomDepthStencil(int32 Dbid, int32 CustomDepthStencilValue)
{
    GetOrCreateStencilActor(CustomDepthStencilValue)->AddNode(GetChildLeafNodeArray(Dbid));
}

void AXSPSubModelActor::SetRenderCustomDepthStencil(const TArray<int32>& DbidArray, int32 CustomDepthStencilValue)
{
    GetOrCreateStencilActor(CustomDepthStencilValue)->AddNode(GetChildLeafNodeArray(DbidArray));
}

void AXSPSubModelActor::ClearRenderCustomDepthStencil(int32 Dbid)
{
    TArray<int32> ChildLeafNodeArray = GetChildLeafNodeArray(Dbid);
    for (auto Pair : CustomStencilActorMap)
    {
        Pair.Value->RemoveNode(ChildLeafNodeArray);
    }
}

void AXSPSubModelActor::ClearRenderCustomDepthStencil(const TArray<int32>& DbidArray)
{
    TArray<int32> ChildLeafNodeArray = GetChildLeafNodeArray(DbidArray);
    for (auto Pair : CustomStencilActorMap)
    {
        Pair.Value->RemoveNode(ChildLeafNodeArray);
    }
}

void AXSPSubModelActor::SetVisibility(int32 Dbid, bool bVisible)
{
    TArray<int32> ChildLeafNodeArray = GetChildLeafNodeArray(Dbid);
    for (int32 LeafNodeDbid : ChildLeafNodeArray)
    {
        SetLeafNodeVisibility(LeafNodeDbid, bVisible);
    }
}

void AXSPSubModelActor::SetVisibility(const TArray<int32>& DbidArray, bool bVisible)
{
    TArray<int32> ChildLeafNodeArray = GetChildLeafNodeArray(DbidArray);
    for (int32 LeafNodeDbid : ChildLeafNodeArray)
    {
        SetLeafNodeVisibility(LeafNodeDbid, bVisible);
    }
}

void AXSPSubModelActor::SetRenderColor(int32 Dbid, const FLinearColor& Color)
{
    TArray<int32> ChildLeafNodeArray = GetChildLeafNodeArray(Dbid);
    GetOrCreateHighlightActor(Color)->AddNode(ChildLeafNodeArray);
    //在原渲染Actor中隐藏高亮着色的节点
    for (int32 LeafNodeDbid : ChildLeafNodeArray)
    {
        SetLeafNodeVisibility(LeafNodeDbid, false);
    }
}

void AXSPSubModelActor::SetRenderColor(const TArray<int32>& DbidArray, const FLinearColor& Color)
{
    TArray<int32> ChildLeafNodeArray = GetChildLeafNodeArray(DbidArray);
    GetOrCreateHighlightActor(Color)->AddNode(ChildLeafNodeArray);
    //在原渲染Actor中隐藏高亮着色的节点
    for (int32 LeafNodeDbid : ChildLeafNodeArray)
    {
        SetLeafNodeVisibility(LeafNodeDbid, false);
    }
}

void AXSPSubModelActor::ClearRenderColor(int32 Dbid)
{
    TArray<int32> ChildLeafNodeArray = GetChildLeafNodeArray(Dbid);
    for (auto Pair : HighlightActorMap)
    {
        Pair.Value->RemoveNode(ChildLeafNodeArray);
    }
    //恢复节点在原渲染Actor中的显示
    for (int32 LeafNodeDbid : ChildLeafNodeArray)
    {
        SetLeafNodeVisibility(LeafNodeDbid, true);
    }
}

void AXSPSubModelActor::ClearRenderColor(const TArray<int32>& DbidArray)
{
    TArray<int32> ChildLeafNodeArray = GetChildLeafNodeArray(DbidArray);
    for (auto Pair : HighlightActorMap)
    {
        Pair.Value->RemoveNode(ChildLeafNodeArray);
    }
    //恢复节点在原渲染Actor中的显示
    for (int32 LeafNodeDbid : ChildLeafNodeArray)
    {
        SetLeafNodeVisibility(LeafNodeDbid, true);
    }
}

const TArray<struct FXSPNodeData*>& AXSPSubModelActor::GetNodeDataArray() const
{
    return Parent->GetNodeDataArray();
}

bool AXSPSubModelActor::TickDynamicCombine(float& InOutSeconds, bool bAsyncBuild)
{
    bool bFinished = true;

    for (auto Pair : MaterialActorMap)
    {
        if (InOutSeconds < 0)
        {
            bFinished = false;
            break;
        }
        if (!Pair.Value->TickDynamicCombine(InOutSeconds, bAsyncBuild))
            bFinished = false;
    }

    for (auto Pair : CustomStencilActorMap)
    {
        if (InOutSeconds < 0)
        {
            bFinished = false;
            break;
        }
        if (!Pair.Value->TickDynamicCombine(InOutSeconds, bAsyncBuild))
            bFinished = false;
    }

    for (auto Pair : HighlightActorMap)
    {
        if (InOutSeconds < 0)
        {
            bFinished = false;
            break;
        }
        if (!Pair.Value->TickDynamicCombine(InOutSeconds, bAsyncBuild))
            bFinished = false;
    }

    return bFinished;
}

AXSPSubModelMaterialActor* AXSPSubModelActor::GetOrCreateMaterialActor(const FLinearColor& Material)
{
    AXSPSubModelMaterialActor* const* Found = MaterialActorMap.Find(Material);
    if (Found)
        return *Found;

    AXSPSubModelMaterialActor* Actor = NewObject<AXSPSubModelMaterialActor>(this);
    Actor->Init(this, Parent->CreateMaterialInstanceDynamic(Material, Material.A, FLinearColor::Black), -1, true);
    Actor->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
    MaterialActorMap.Add(Material, Actor);
    return Actor;
}

AXSPSubModelMaterialActor* AXSPSubModelActor::GetOrCreateStencilActor(int32 CustomDepthStencilValue)
{
    AXSPSubModelMaterialActor* const* Found = CustomStencilActorMap.Find(CustomDepthStencilValue);
    if (Found)
        return *Found;

    AXSPSubModelMaterialActor* Actor = NewObject<AXSPSubModelMaterialActor>(this);
    Actor->Init(this, Parent->CreateMaterialInstanceDynamic(FLinearColor::White, 0, FLinearColor::Black), CustomDepthStencilValue, false);
    Actor->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
    CustomStencilActorMap.Add(CustomDepthStencilValue, Actor);
    return Actor;
}

AXSPSubModelMaterialActor* AXSPSubModelActor::GetOrCreateHighlightActor(const FLinearColor& Color)
{
    AXSPSubModelMaterialActor* const* Found = HighlightActorMap.Find(Color);
    if (Found)
        return *Found;

    AXSPSubModelMaterialActor* Actor = NewObject<AXSPSubModelMaterialActor>(this);
    Actor->Init(this, Parent->CreateMaterialInstanceDynamic(Color, 1, Color), -1, true);
    Actor->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
    HighlightActorMap.Add(Color, Actor);
    return Actor;
}

TArray<int32> AXSPSubModelActor::GetChildLeafNodeArray(int32 Dbid)
{
    const TArray<FXSPNodeData*>& NodeDataArray = Parent->GetNodeDataArray();
    TArray<int32> ChildLeafNodeArray;
    for (int32 Index = 0; Index < NodeDataArray[Dbid]->NumChildren; Index++)
    {
        if (!NodeDataArray[Dbid + Index]->MeshPositionArray.IsEmpty())
        {
            ChildLeafNodeArray.Add(Dbid + Index);
        }
    }

    return MoveTemp(ChildLeafNodeArray);
}

TArray<int32> AXSPSubModelActor::GetChildLeafNodeArray(const TArray<int32>& DbidArray)
{
    const TArray<FXSPNodeData*>& NodeDataArray = Parent->GetNodeDataArray();
    TArray<int32> ChildLeafNodeArray;
    for (int32 Dbid : DbidArray)
    {
        for (int32 Index = 0; Index < NodeDataArray[Dbid]->NumChildren; Index++)
        {
            if (!NodeDataArray[Dbid + Index]->MeshPositionArray.IsEmpty() &&
                !ChildLeafNodeArray.Contains(Dbid + Index))
            {
                ChildLeafNodeArray.Add(Dbid + Index);
            }
        }
    }

    return MoveTemp(ChildLeafNodeArray);
}

void AXSPSubModelActor::SetLeafNodeVisibility(int32 Dbid, bool bVisible)
{
    const TArray<FXSPNodeData*>& NodeDataArray = Parent->GetNodeDataArray();
    AXSPSubModelMaterialActor* Actor = GetOrCreateMaterialActor(NodeDataArray[Dbid]->MeshMaterial);
    if (bVisible)
        Actor->AddNode(Dbid);
    else
        Actor->RemoveNode(Dbid);
}
