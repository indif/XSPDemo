#include "XSPSubModelActor.h"
#include "XSPModelActor.h"
#include "XSPDataStruct.h"
#include "XSPSubModelMaterialActor.h"
#include "XSPStat.h"

FXSPSubModelActor::FXSPSubModelActor()
{
}

FXSPSubModelActor::~FXSPSubModelActor()
{
}

void FXSPSubModelActor::Init(AXSPModelActor* InOwner, int32 Dbid, int32 Num)
{
    Owner = InOwner;
    StartDbid = Dbid;
    NumNodes = Num;

    const TArray<FXSPNodeData*>& NodeDataArray = Owner->GetNodeDataArray();
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
}

void FXSPSubModelActor::SetRenderCustomDepthStencil(int32 Dbid, int32 CustomDepthStencilValue)
{
    GetOrCreateStencilActor(CustomDepthStencilValue)->AddNode(GetChildLeafNodeArray(Dbid));
}

void FXSPSubModelActor::SetRenderCustomDepthStencil(const TArray<int32>& DbidArray, int32 CustomDepthStencilValue)
{
    GetOrCreateStencilActor(CustomDepthStencilValue)->AddNode(GetChildLeafNodeArray(DbidArray));
}

void FXSPSubModelActor::ClearRenderCustomDepthStencil(int32 Dbid)
{
    TArray<int32> ChildLeafNodeArray = GetChildLeafNodeArray(Dbid);
    for (auto Pair : CustomStencilActorMap)
    {
        Pair.Value->RemoveNode(ChildLeafNodeArray);
    }
}

void FXSPSubModelActor::ClearRenderCustomDepthStencil(const TArray<int32>& DbidArray)
{
    TArray<int32> ChildLeafNodeArray = GetChildLeafNodeArray(DbidArray);
    for (auto Pair : CustomStencilActorMap)
    {
        Pair.Value->RemoveNode(ChildLeafNodeArray);
    }
}

void FXSPSubModelActor::SetVisibility(int32 Dbid, bool bVisible)
{
    TArray<int32> ChildLeafNodeArray = GetChildLeafNodeArray(Dbid);
    for (int32 LeafNodeDbid : ChildLeafNodeArray)
    {
        SetLeafNodeVisibility(LeafNodeDbid, bVisible);
    }
}

void FXSPSubModelActor::SetVisibility(const TArray<int32>& DbidArray, bool bVisible)
{
    TArray<int32> ChildLeafNodeArray = GetChildLeafNodeArray(DbidArray);
    for (int32 LeafNodeDbid : ChildLeafNodeArray)
    {
        SetLeafNodeVisibility(LeafNodeDbid, bVisible);
    }
}

void FXSPSubModelActor::SetRenderColor(int32 Dbid, const FLinearColor& Color)
{
    TArray<int32> ChildLeafNodeArray = GetChildLeafNodeArray(Dbid);
    GetOrCreateHighlightActor(Color)->AddNode(ChildLeafNodeArray);
    //在原渲染Actor中隐藏高亮着色的节点
    for (int32 LeafNodeDbid : ChildLeafNodeArray)
    {
        SetLeafNodeVisibility(LeafNodeDbid, false);
    }
}

void FXSPSubModelActor::SetRenderColor(const TArray<int32>& DbidArray, const FLinearColor& Color)
{
    TArray<int32> ChildLeafNodeArray = GetChildLeafNodeArray(DbidArray);
    GetOrCreateHighlightActor(Color)->AddNode(ChildLeafNodeArray);
    //在原渲染Actor中隐藏高亮着色的节点
    for (int32 LeafNodeDbid : ChildLeafNodeArray)
    {
        SetLeafNodeVisibility(LeafNodeDbid, false);
    }
}

void FXSPSubModelActor::ClearRenderColor(int32 Dbid)
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

void FXSPSubModelActor::ClearRenderColor(const TArray<int32>& DbidArray)
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

bool FXSPSubModelActor::TickDynamicCombine(float& InOutSeconds, bool bAsyncBuild)
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

void FXSPSubModelActor::SetCrossSection(bool bEnable, const FVector& Position, const FVector& Normal)
{
    for (auto Pair : MaterialActorMap)
        Pair.Value->SetCrossSection(bEnable, Position, Normal);
    for (auto Pair : CustomStencilActorMap)
        Pair.Value->SetCrossSection(bEnable, Position, Normal);
    for (auto Pair : HighlightActorMap)
        Pair.Value->SetCrossSection(bEnable, Position, Normal);
}

FXSPSubModelMaterialActor* FXSPSubModelActor::GetOrCreateMaterialActor(const FLinearColor& Material)
{
    TSharedPtr<FXSPSubModelMaterialActor>* Found = MaterialActorMap.Find(Material);
    if (Found)
        return Found->Get();

    TSharedPtr<FXSPSubModelMaterialActor> Actor = MakeShareable(new FXSPSubModelMaterialActor);
    Actor->Init(Owner, Owner->CreateMaterialInstanceDynamic(Material, Material.A, FLinearColor::Black), -1, true);
    MaterialActorMap.Add(Material, Actor);
    return Actor.Get();
}

FXSPSubModelMaterialActor* FXSPSubModelActor::GetOrCreateStencilActor(int32 CustomDepthStencilValue)
{
    TSharedPtr<FXSPSubModelMaterialActor>* Found = CustomStencilActorMap.Find(CustomDepthStencilValue);
    if (Found)
        return Found->Get();

    TSharedPtr<FXSPSubModelMaterialActor> Actor = MakeShareable(new FXSPSubModelMaterialActor);
    Actor->Init(Owner, Owner->CreateMaterialInstanceDynamic(FLinearColor::White, 0, FLinearColor::Black), CustomDepthStencilValue, false);
    CustomStencilActorMap.Add(CustomDepthStencilValue, Actor);
    return Actor.Get();
}

FXSPSubModelMaterialActor* FXSPSubModelActor::GetOrCreateHighlightActor(const FLinearColor& Color)
{
    TSharedPtr<FXSPSubModelMaterialActor>* Found = HighlightActorMap.Find(Color);
    if (Found)
        return Found->Get();

    TSharedPtr<FXSPSubModelMaterialActor> Actor = MakeShareable(new FXSPSubModelMaterialActor);
    Actor->Init(Owner, Owner->CreateMaterialInstanceDynamic(Color, 1, Color), -1, true);
    HighlightActorMap.Add(Color, Actor);
    return Actor.Get();
}

TArray<int32> FXSPSubModelActor::GetChildLeafNodeArray(int32 Dbid)
{
    const TArray<FXSPNodeData*>& NodeDataArray = Owner->GetNodeDataArray();
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

TArray<int32> FXSPSubModelActor::GetChildLeafNodeArray(const TArray<int32>& DbidArray)
{
    const TArray<FXSPNodeData*>& NodeDataArray = Owner->GetNodeDataArray();
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

void FXSPSubModelActor::SetLeafNodeVisibility(int32 Dbid, bool bVisible)
{
    const TArray<FXSPNodeData*>& NodeDataArray = Owner->GetNodeDataArray();
    FXSPSubModelMaterialActor* Actor = GetOrCreateMaterialActor(NodeDataArray[Dbid]->MeshMaterial);
    if (bVisible)
        Actor->AddNode(Dbid);
    else
        Actor->RemoveNode(Dbid);
}
