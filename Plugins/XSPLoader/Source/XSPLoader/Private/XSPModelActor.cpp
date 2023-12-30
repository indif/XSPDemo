// Fill out your copyright notice in the Description page of Project Settings.


#include "XSPModelActor.h"
#include "XSPFileUtils.h"
#include "CombinedMeshBuilder.h"
#include "MeshUtils.h"
#include "XSPFileReader.h"
#include "XSPSubModelActor.h"
#include "XSPBatchMeshComponent.h"
#include "XSPStat.h"


extern bool bXSPAutoComputeBatchParams;
extern int32 XSPMaxNumBatches;
extern int32 XSPMaxNumVerticesPerBatch;
extern int32 XSPMinNumVerticesPerBatch;
extern int32 XSPMinNumVerticesUnbatch;

// Sets default values
AXSPModelActor::AXSPModelActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}

AXSPModelActor::~AXSPModelActor()
{
    for (FXSPNodeData* NodeData : NodeDataArray)
    {
        delete NodeData;
    }
}

bool AXSPModelActor::Load(const TArray<FString>& FilePathNameArray, bool bAsyncBuild)
{
    if (EState::Empty != State)
        return false;

    OperationBeginTicks = FDateTime::Now().GetTicks();

    SourceMaterialOpaque = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, L"/XSPLoader/M_MainOpaque"));
    SourceMaterialTranslucent = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, L"/XSPLoader/M_MainTranslucent"));
    bAsyncBuildWhenLoading = bAsyncBuild;

    return LoadToDynamicCombinedMesh(FilePathNameArray);
}

int32 AXSPModelActor::GetNode(UPrimitiveComponent* Component, int32 FaceIndex)
{
    UXSPBatchMeshComponent* XSPBatchMeshComponent = Cast<UXSPBatchMeshComponent>(Component);
    if (XSPBatchMeshComponent)
        return XSPBatchMeshComponent->GetNode(FaceIndex);

    return -1;
}

namespace
{
    //递归计算节点包围盒,计算过程中将计算完的数据写入节点数据结构
    FBox3f RecursiveComputeBoundingBox(TArray<FXSPNodeData*>& NodeDataArray, int32 Dbid)
    {
        if (!NodeDataArray[Dbid]->MeshBoundingBox.IsValid)
        {
            int32 CurrentLevel = NodeDataArray[Dbid]->Level;
            //只遍历子节点，孙节点在递归中计算
            for (int32 i = 1; i < NodeDataArray[Dbid]->NumChildren; ++i)
            {
                if (NodeDataArray[Dbid + i]->Level == CurrentLevel + 1)
                {
                    NodeDataArray[Dbid]->MeshBoundingBox += RecursiveComputeBoundingBox(NodeDataArray, Dbid + i);
                }
            }
        }
        return NodeDataArray[Dbid]->MeshBoundingBox;
    };
}

FBox3f AXSPModelActor::GetNodeBoundingBox(int32 Dbid)
{
    if (EState::Empty == State || EState::ReadingFile == State)
        return FBox3f(ForceInit);
    if (Dbid < 0 || Dbid >= NodeDataArray.Num())
        return FBox3f(ForceInit);

    return RecursiveComputeBoundingBox(NodeDataArray, Dbid);
}

void AXSPModelActor::SetRenderCustomDepthStencil(int32 Dbid, int32 CustomDepthStencilValue)
{
    if (!UpdateOperation())
        return;

    AXSPSubModelActor* XSPSubModelActor = GetSubModelActor(Dbid);
    if (nullptr != XSPSubModelActor)
    {
        XSPSubModelActor->SetRenderCustomDepthStencil(Dbid, CustomDepthStencilValue);
    }
}

void AXSPModelActor::SetRenderCustomDepthStencilArray(const TArray<int32>& DbidArray, int32 CustomDepthStencilValue)
{
    if (!UpdateOperation())
        return;

    for (auto Pair : SubModelActorMap)
    {
        TArray<int32> SubArray = GetSubArray(DbidArray, Pair.Key, NodeDataArray[Pair.Key]->NumChildren);
        if (!SubArray.IsEmpty())
            Pair.Value->SetRenderCustomDepthStencil(DbidArray, CustomDepthStencilValue);
    }
}

void AXSPModelActor::ClearRenderCustomDepthStencil(int32 Dbid)
{
    if (!UpdateOperation())
        return;

    AXSPSubModelActor* XSPSubModelActor = GetSubModelActor(Dbid);
    if (nullptr != XSPSubModelActor)
    {
        XSPSubModelActor->ClearRenderCustomDepthStencil(Dbid);
    }
}

void AXSPModelActor::ClearRenderCustomDepthStencilArray(const TArray<int32>& DbidArray)
{
    if (!UpdateOperation())
        return;

    for (auto Pair : SubModelActorMap)
    {
        TArray<int32> SubArray = GetSubArray(DbidArray, Pair.Key, NodeDataArray[Pair.Key]->NumChildren);
        if (!SubArray.IsEmpty())
            Pair.Value->ClearRenderCustomDepthStencil(DbidArray);
    }
}

void AXSPModelActor::SetVisibility(int32 Dbid, bool bVisible)
{
    if (!UpdateOperation())
        return;

    AXSPSubModelActor* XSPSubModelActor = GetSubModelActor(Dbid);
    if (nullptr != XSPSubModelActor)
    {
        XSPSubModelActor->SetVisibility(Dbid, bVisible);
    }
}

void AXSPModelActor::SetVisibilityArray(const TArray<int32>& DbidArray, bool bVisible)
{
    if (!UpdateOperation())
        return;

    for (auto Pair : SubModelActorMap)
    {
        TArray<int32> SubArray = GetSubArray(DbidArray, Pair.Key, NodeDataArray[Pair.Key]->NumChildren);
        if (!SubArray.IsEmpty())
            Pair.Value->SetVisibility(DbidArray, bVisible);
    }
}

void AXSPModelActor::SetRenderColor(int32 Dbid, const FLinearColor& Color)
{
    if (!UpdateOperation())
        return;

    AXSPSubModelActor* XSPSubModelActor = GetSubModelActor(Dbid);
    if (nullptr != XSPSubModelActor)
    {
        XSPSubModelActor->SetRenderColor(Dbid, Color);
    }
}

void AXSPModelActor::SetRenderColorArray(const TArray<int32>& DbidArray, const FLinearColor& Color)
{
    if (!UpdateOperation())
        return;

    for (auto Pair : SubModelActorMap)
    {
        TArray<int32> SubArray = GetSubArray(DbidArray, Pair.Key, NodeDataArray[Pair.Key]->NumChildren);
        if (!SubArray.IsEmpty())
            Pair.Value->SetRenderColor(DbidArray, Color);
    }
}

void AXSPModelActor::ClearRenderColor(int32 Dbid)
{
    if (!UpdateOperation())
        return;

    AXSPSubModelActor* XSPSubModelActor = GetSubModelActor(Dbid);
    if (nullptr != XSPSubModelActor)
    {
        XSPSubModelActor->ClearRenderColor(Dbid);
    }
}

void AXSPModelActor::ClearRenderColorArray(const TArray<int32>& DbidArray)
{
    if (!UpdateOperation())
        return;

    for (auto Pair : SubModelActorMap)
    {
        TArray<int32> SubArray = GetSubArray(DbidArray, Pair.Key, NodeDataArray[Pair.Key]->NumChildren);
        if (!SubArray.IsEmpty())
            Pair.Value->ClearRenderColor(DbidArray);
    }
}

void AXSPModelActor::SetMaxTickTime(float Seconds)
{
    MaxTickTime = Seconds;
}

AXSPModelActor::FOnLoadFinishDelegate& AXSPModelActor::GetOnLoadFinishDelegate()
{
    return OnLoadFinishDelegate;
}

// Called when the game starts or when spawned
void AXSPModelActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AXSPModelActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    if (EState::Empty != State && EState::Finished != State)
        TickDynamicCombine(MaxTickTime);
}

UMaterialInstanceDynamic* AXSPModelActor::CreateMaterialInstanceDynamic(const FLinearColor& BaseColor, float Roughness, const FLinearColor& EmissiveColor)
{
    UMaterialInterface* ParentMaterial = BaseColor.A < 1.f ? SourceMaterialTranslucent : SourceMaterialOpaque;
    UMaterialInstanceDynamic* MaterialInstanceDynamic = UMaterialInstanceDynamic::Create(ParentMaterial, nullptr);
    MaterialInstanceDynamic->SetVectorParameterValue(TEXT("BaseColor"), BaseColor);
    MaterialInstanceDynamic->SetScalarParameterValue(TEXT("Roughness"), Roughness);
    MaterialInstanceDynamic->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor);
    return MaterialInstanceDynamic;
}

bool AXSPModelActor::LoadToDynamicCombinedMesh(const TArray<FString>& FilePathNameArray)
{
    int32 NumFiles = FilePathNameArray.Num();
    if (NumFiles < 1)
        return false;

    int32 NumTotalNodes = 0;
    for (int32 i = 0; i < NumFiles; ++i)
    {
        TSharedPtr<FXSPFileReader> FileReader = MakeShareable(new FXSPFileReader(this));
        int32 NumNodes = FileReader->Start(FilePathNameArray[i], NumTotalNodes);
        if (NumNodes > 0)
        {
            FileReaderArray.Emplace(FileReader);
            NumTotalNodes += NumNodes;
        }
    }
    if (FileReaderArray.IsEmpty())
        return false;

    State = EState::ReadingFile;

    return true;
}

void AXSPModelActor::TickDynamicCombine(float AvailableSeconds)
{
    bool bFinished = true;
    if (EState::ReadingFile == State)
    {
        int64 BeginTicks = FDateTime::Now().GetTicks();
        if (FinishLoadNodeData())
        {
            if (bXSPAutoComputeBatchParams)
                ComputeBatchParams();

            InitSubModelActors();
            State = EState::InitLoading;
        }
        else
        {
            bFinished = false;
        }
        AvailableSeconds -= (float)(FDateTime::Now().GetTicks() - BeginTicks) / ETimespan::TicksPerSecond;
    }

    if (EState::InitLoading == State || EState::Updating == State)
    {
        //异步构建只用于初始加载阶段（动态更新阶段异步构建导致的视觉效果不好）
        bool bAsyncBuild = bAsyncBuildWhenLoading && EState::InitLoading == State;
        for (auto& Pair : SubModelActorMap)
        {
            if (AvailableSeconds < 0)
            {
                bFinished = false;
                break;
            }
            if (!Pair.Value->TickDynamicCombine(AvailableSeconds, bAsyncBuild))
                bFinished = false;
        }
    }

    if (bFinished)
    {
        int32 FinishType = EState::InitLoading == State ? 0 : 1;
        State = EState::Finished;

        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("%s加载完成，耗时%.2f秒"), 
            FinishType == 0 ? TEXT("初始") : TEXT("动态更新"),
            (float)(FDateTime::Now().GetTicks() - OperationBeginTicks) / ETimespan::TicksPerSecond));

        OnLoadFinishDelegate.Broadcast(FinishType);
    }
}

bool AXSPModelActor::FinishLoadNodeData()
{
    bool bComplete = true;
    for (auto& FileReader : FileReaderArray)
    {
        if (!FileReader->IsComplete())
        {
            bComplete = false;
            break;
        }
    }

    if (bComplete)
    {
        TArray<int32> LackParentNodeIdArray;
        for (auto& FileReader : FileReaderArray)
        {
            NodeDataArray.Append(MoveTemp(FileReader->GetNodeDataArray()));
            LevelOneNodeIdArray.Append(MoveTemp(FileReader->GetLevelOneNodeIdArray()));
            LeafNodeIdArray.Append(MoveTemp(FileReader->GetLeafNodeIdArray()));
            LackParentNodeIdArray.Append(MoveTemp(FileReader->GetLackParentNodeIdArray()));
            NumVerticesTotal += FileReader->GetNumVerticesTotal();
        }
        FileReaderArray.Empty();

        SET_DWORD_STAT(STAT_XSPLoader_NumNode, NodeDataArray.Num());
        SET_DWORD_STAT(STAT_XSPLoader_NumLevelOneNode, LevelOneNodeIdArray.Num());
        SET_DWORD_STAT(STAT_XSPLoader_NumLeafNode, LeafNodeIdArray.Num());

        if (LackParentNodeIdArray.Num() > 0)
        {
            ParallelFor(LackParentNodeIdArray.Num(), [&](int32 i) {
                int32 Dbid = LackParentNodeIdArray[i];
                FXSPNodeData* NodeData = NodeDataArray[Dbid];
                if (NodeData->ParentDbid >= 0)
                {
                    FXSPNodeData* ParentNodeData = NodeDataArray[NodeData->ParentDbid];
                    InheritMaterial(*NodeData, *ParentNodeData);
                }
            }, EParallelForFlags::None);
        }
    }

    return bComplete;
}

void AXSPModelActor::ComputeBatchParams()
{
    SET_DWORD_STAT(STAT_XSPLoader_NumTotalVertices, NumVerticesTotal);
    if (NumVerticesTotal <= 10000)
        return;

    XSPMaxNumVerticesPerBatch = NumVerticesTotal / XSPMaxNumBatches;
    SET_DWORD_STAT(STAT_XSPLoader_MaxNumVerticesPerBatch, XSPMaxNumVerticesPerBatch);

    XSPMinNumVerticesPerBatch = XSPMaxNumVerticesPerBatch / 3;
    SET_DWORD_STAT(STAT_XSPLoader_MinNumVerticesPerBatch, XSPMinNumVerticesPerBatch);

    XSPMinNumVerticesUnbatch = XSPMinNumVerticesPerBatch;
    SET_DWORD_STAT(STAT_XSPLoader_MinNumVerticesUnbatch, XSPMinNumVerticesUnbatch);
}

void AXSPModelActor::InitSubModelActors()
{
    for (int32 Dbid : LevelOneNodeIdArray)
    {
        AXSPSubModelActor* Actor = NewObject<AXSPSubModelActor>(this);
        Actor->Init(this, Dbid, NodeDataArray[Dbid]->NumChildren);
        Actor->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
        SubModelActorMap.Add(Dbid, Actor);
    }
}

bool AXSPModelActor::UpdateOperation()
{
    //未初始化和正在读取文件的阶段不允许操作
    if (EState::Empty == State || EState::ReadingFile == State)
        return false;

    //稳定状态下才更新操作起始时间(重叠操作以第一个的起始时间为准)
    if (EState::Finished == State)
        OperationBeginTicks = FDateTime::Now().GetTicks();

    State = EState::Updating;

    return true;
}

AXSPSubModelActor* AXSPModelActor::GetSubModelActor(int32 Dbid)
{
    for (auto Pair : SubModelActorMap)
    {
        int32 RootDbid = Pair.Key;
        if (Dbid >= Pair.Key && Dbid < Pair.Key + NodeDataArray[Pair.Key]->NumChildren)
        {
            return Pair.Value;
        }
    }
    return nullptr;
}

TArray<int32> AXSPModelActor::GetSubArray(const TArray<int32>& DbidArray, int32 Start, int32 Num)
{
    TArray<int32> SubArray;
    for (int32 Dbid : DbidArray)
    {
        if (Dbid >= Start && Dbid < Start + Num)
            SubArray.Emplace(Dbid);
    }

    return MoveTemp(SubArray);
}
