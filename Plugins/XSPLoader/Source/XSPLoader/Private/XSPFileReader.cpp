#include "XSPFileReader.h"
#include "XSPModelActor.h"
#include "XSPFileUtils.h"
#include "MeshUtils.h"
#include "XSPStat.h"


FXSPFileReader::FXSPFileReader(AXSPModelActor* InOwner)
    : Owner(InOwner)
    , Offset(0)
    , NumNodes(0)
    , bRunning(true)
    , bComplete(false)
    , Thread(nullptr)
{
}

FXSPFileReader::~FXSPFileReader()
{
    Stop();

    if (Thread)
    {
        Thread->Kill(true);
        Thread = nullptr;
    }

    if (FileStream.is_open())
    {
        FileStream.close();
    }
}

int32 FXSPFileReader::Start(const FString& FilePathName, int32 InOffset)
{
    Offset = InOffset;

    FileStream.open(std::wstring(*FilePathName), std::ios::in | std::ios::binary);
    if (!FileStream.is_open())
        return false;

    //读取源文件的节点数
    FileStream.seekg(0, std::ios::beg);
    FileStream.read((char*)&NumNodes, sizeof(NumNodes));

    if (NumNodes > 0)
    {
        FString ThreadName = FString::Printf(TEXT("XSPFileReaderThread_%d"), Offset);
        Thread = FRunnableThread::Create(this, *ThreadName);
    }

    return NumNodes;
}

bool FXSPFileReader::Init()
{
    return true;
}

uint32 FXSPFileReader::Run()
{
    int64 Ticks1 = FDateTime::Now().GetTicks();

    short headlength;
    FileStream.read((char*)&headlength, sizeof(headlength));
    TArray<Header_info> HeaderList;
    ReadHeaderInfo(FileStream, NumNodes, HeaderList);

    TArray<FAsyncTask<FResolveNodeDataTask>*> ResolveNodeDataTaskArray;
    NodeDataArray.SetNumUninitialized(NumNodes);
    for (int32 j = 0; bRunning && j < NumNodes; j++)
    {
        FXSPNodeData* NodeData = NodeDataArray[j] = new FXSPNodeData;
        NodeData->Dbid = Offset + j;
        ReadNodeData(FileStream, HeaderList[j], *NodeData);

        if (NodeData->Level == 1)
            LevelOneNodeIdArray.Emplace(NodeData->Dbid);

        if (NodeData->PrimitiveArray.Num() > 0)
        {
            LeafNodeIdArray.Emplace(NodeData->Dbid);

            FXSPNodeData* ParentNodeData = nullptr;
            if (NodeData->ParentDbid >= Offset)
                ParentNodeData = NodeDataArray[NodeData->ParentDbid - Offset];
            else
                LackParentNodeIdArray.Emplace(NodeData->Dbid);

            FAsyncTask<FResolveNodeDataTask>* Task = new FAsyncTask<FResolveNodeDataTask>(NodeData, ParentNodeData);
            Task->StartBackgroundTask();
            ResolveNodeDataTaskArray.Emplace(Task);
        }
    }
    TArray<FAsyncTask<FResolveNodeDataTask>*> RemainTasks;
    for (auto Task : ResolveNodeDataTaskArray)
    {
        if (Task->IsDone())
        {
            NumVerticesTotal += Task->GetTask().NodeData->MeshPositionArray.Num();
            delete Task;
        }
        else
        {
            RemainTasks.Emplace(Task);
        }
    }
    ResolveNodeDataTaskArray.Empty();

    //等待所有计算任务完成
    for (auto Task : RemainTasks)
    {
        Task->EnsureCompletion();
        NumVerticesTotal += Task->GetTask().NodeData->MeshPositionArray.Num();
        delete Task;
    }
    RemainTasks.Empty();

    bComplete = true;
    INC_FLOAT_STAT_BY(STAT_XSPLoader_ReadFileTime, (float)(FDateTime::Now().GetTicks() - Ticks1) / ETimespan::TicksPerSecond);

    return 0;
}

void FXSPFileReader::Stop()
{
    bRunning = false;
}

void FXSPFileReader::Exit()
{
}

bool FXSPFileReader::IsRunning() const
{
    return bRunning;
}

bool FXSPFileReader::IsComplete() const
{
    return bComplete;
}

TArray<FXSPNodeData*>& FXSPFileReader::GetNodeDataArray()
{
    return NodeDataArray;
}

TArray<int32>& FXSPFileReader::GetLevelOneNodeIdArray()
{
    return LevelOneNodeIdArray;
}

TArray<int32>& FXSPFileReader::GetLeafNodeIdArray()
{
    return LeafNodeIdArray;
}

TArray<int32>& FXSPFileReader::GetLackParentNodeIdArray()
{
    return LackParentNodeIdArray;
}

int32 FXSPFileReader::GetNumVerticesTotal()
{
    return NumVerticesTotal;
}

FResolveNodeDataTask::FResolveNodeDataTask(FXSPNodeData* InNodeData, FXSPNodeData* InParentNodeData)
    : NodeData(InNodeData)
    , ParentNodeData(InParentNodeData)
{
}

void FResolveNodeDataTask::DoWork()
{
    ResolveNodeData(*NodeData, ParentNodeData);
}
