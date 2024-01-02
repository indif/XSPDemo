#include "XSPLoader.h"
#include "XSPFileUtils.h"
#include "MeshUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogXSPLoader, Log, All);


void FStaticMeshRequest::Invalidate()
{
    bValid = false;
}

bool FStaticMeshRequest::IsRequestCurrent(uint64 FrameNumber)
{
    return bValid && (FrameNumber - LastUpdateFrameNumber < 10);
}

void FRequestQueue::Add(FStaticMeshRequest* Request)
{
    FScopeLock Lock(&RequestListCS);
    AddNoLock(Request);
}

void FRequestQueue::AddNoLock(FStaticMeshRequest* Request)
{
    RequestList.Emplace(Request);
}

void FRequestQueue::TakeFirst(FStaticMeshRequest*& Request)
{
    FScopeLock Lock(&RequestListCS);

    Request = nullptr;
    if (!RequestList.IsEmpty())
    {
        FSortRequestFunctor HighPriority;

        uint64 FrameNumber = Loader->FrameNumber.load();

        for (FRequestList::TIterator Itr(RequestList); Itr; ++Itr)
        {
            FScopeLock RequestLock(&Loader->RequestCS);
            if ((*Itr)->IsRequestCurrent(FrameNumber))
            {
                if (nullptr == Request || HighPriority(*Itr, Request))
                {
                    Request = *Itr;
                }
            }
            else
            {
                FStaticMeshRequest* TheRequest = *Itr;

                //过期请求,标记为失效,并从队列中移除
                (*Itr)->Invalidate();
                (*Itr)->SetReleasable();
                Itr.RemoveCurrent();
            }
        }

        if (Request != nullptr)
        {
            RequestList.Remove(Request);
        }
    }
}

bool FRequestQueue::IsEmpty()
{
    FScopeLock Lock(&RequestListCS);
    return RequestList.IsEmpty();
}

void FRequestQueue::Empty()
{
    FScopeLock Lock(&RequestListCS);
    RequestList.Empty();
}

void FRequestQueue::Swap(FRequestList& OutRequestList)
{
    FScopeLock Lock(&RequestListCS);
    OutRequestList = MoveTemp(RequestList);
}

void FBuildStaticMeshTask::DoWork()
{
    BuildStaticMesh(Request->StaticMesh.Get(), *NodeData);
    MergeRequestQueue.Add(Request);
}

FXSPFileLoadRunnalbe::~FXSPFileLoadRunnalbe()
{
    for (auto Pair : BodyMap)
    {
        delete Pair.Value;
    }
}

uint32 FXSPFileLoadRunnalbe::Run()
{
    //读节点头信息
    FileStream.seekg(0, std::ios::beg);
    int NumNodes;
    FileStream.read((char*)&NumNodes, sizeof(NumNodes));
    check(Count == NumNodes);
    short headlength;
    FileStream.read((char*)&headlength, sizeof(headlength));
    ReadHeaderInfo(FileStream, Count, HeaderList);
    check(HeaderList.Num() == Count);

    //循环等待并执行加载请求
    while (!bStopRequested)
    {
        FStaticMeshRequest* Request = nullptr;
        LoadRequestQueue.TakeFirst(Request);
        if (nullptr != Request)
        {
            //计算全局dbid在本文件中的局部dbid
            int32 LocalDbid = Request->Dbid - StartDbid;
            check(LocalDbid >= 0 && LocalDbid < Count);

            //读取Body数据
            bool bHasCache = true;
            Body_info* NodeDataPtr = nullptr;
            if (!BodyMap.Contains(LocalDbid))
            {
                bHasCache = false;
                //读过的节点数据就缓存在内存中
                NodeDataPtr = new Body_info;
                ReadBodyInfo(FileStream, HeaderList[LocalDbid], false, *NodeDataPtr);
                BodyMap.Emplace(LocalDbid, NodeDataPtr);
            }
            else
            {
                NodeDataPtr = BodyMap[LocalDbid];
            }

            if (CheckNode(*NodeDataPtr))
            {
                //新读入的节点需要尝试继承上级节点的材质数据
                int32 LocalParentDbid = NodeDataPtr->parentdbid < 0 ? -1 : NodeDataPtr->parentdbid - StartDbid;
                if (LocalParentDbid >= 0 && LocalParentDbid < Count)
                {
                    Body_info* ParentNodeDataPtr = nullptr;
                    if (!BodyMap.Contains(LocalParentDbid))
                    {
                        ParentNodeDataPtr = new Body_info;
                        ReadBodyInfo(FileStream, HeaderList[LocalParentDbid], false, *ParentNodeDataPtr);
                        BodyMap.Emplace(LocalParentDbid, ParentNodeDataPtr);
                    }
                    else
                    {
                        ParentNodeDataPtr = BodyMap[LocalParentDbid];
                    }
                    
                    InheritMaterial(*NodeDataPtr, *ParentNodeDataPtr);
                }

                GetMaterial(*NodeDataPtr, Request->Color, Request->Roughness);

                //分发构建网格体的任务到线程池
                (new FAutoDeleteAsyncTask<FBuildStaticMeshTask>(Request, NodeDataPtr, MergeRequestQueue))->StartBackgroundTask();
            }
            else
            {
                //无网格体的节点请求,置为无效,并加入黑名单
                {
                    FScopeLock Lock(&Loader->RequestCS);
                    Request->Invalidate();
                }
                Loader->AddToBlacklist(Request->Dbid);
                //置为可释放
                Request->SetReleasable();
            }
        }

        if (LoadRequestQueue.IsEmpty())
            FPlatformProcess::SleepNoStats(1.0f);
    }

    return 0;
}

FXSPLoader::FXSPLoader()
{
    MergeRequestQueue.Loader = this;
}

FXSPLoader::~FXSPLoader()
{
    Reset();
}

bool FXSPLoader::Init(const TArray<FString>& FilePathNameArray)
{
    if (bInitialized)
        return false;

    int32 NumFiles = FilePathNameArray.Num();
    if (NumFiles < 1)
        return false;

    int32 TotalNumNodes = 0;
    bool bFail = false;
    SourceDataList.SetNum(NumFiles);
    for (int32 i = 0; i < NumFiles; ++i)
    {
        SourceDataList[i] = new FSourceData;
        SourceDataList[i]->LoadRequestQueue.Loader = this;
        std::fstream& FileStream = SourceDataList[i]->FileStream;
        FileStream.open(std::wstring(*FilePathNameArray[i]), std::ios::in | std::ios::binary);
        if (!FileStream.is_open())
        {
            bFail = true;
            break;
        }

        //读取源文件的节点数
        FileStream.seekg(0, std::ios::beg);
        int NumNodes;
        FileStream.read((char*)&NumNodes, sizeof(NumNodes));
        
        SourceDataList[i]->StartDbid = TotalNumNodes;
        SourceDataList[i]->Count = NumNodes;
        TotalNumNodes += NumNodes;
    }
    if (bFail)
    {
        ResetInternal();
        return false;
    }

    SourceMaterial = TStrongObjectPtr(Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, L"/XSPLoader/M_MainOpaque")));
    check(SourceMaterial);

    //为每个源文件创建一个读取线程
    for (int32 i = 0; i < NumFiles; ++i)
    {
        FString ThreadName = FString::Printf(TEXT("XSPFileLoader_%d"), i);
        SourceDataList[i]->FileLoadRunnable = new FXSPFileLoadRunnalbe(this, SourceDataList[i]->FileStream, SourceDataList[i]->StartDbid, SourceDataList[i]->Count, SourceDataList[i]->LoadRequestQueue, MergeRequestQueue);
        SourceDataList[i]->LoadThread = FRunnableThread::Create(SourceDataList[i]->FileLoadRunnable, *ThreadName, 8 * 1024, TPri_Normal);
    }

    bInitialized = true;
    FrameNumber.store(0);

    return true;
}

void FXSPLoader::Reset()
{
    if (!bInitialized)
        return;

    ResetInternal();

    SourceMaterial.Reset();
    MergeRequestQueue.Empty();
    for (TMap<int32, FStaticMeshRequest*>::TIterator Itr(AllRequestMap); Itr; ++Itr)
        delete Itr.Value();
    AllRequestMap.Empty();

    bInitialized = false;
}

void FXSPLoader::RequestStaticMesh_GameThread(int32 Dbid, float Priority, UStaticMeshComponent* TargetMeshComponent)
{
    {
        FScopeLock Lock(&BlacklistCS);
        if (Blacklist.Contains(Dbid))
            return;
    }

    DispatchRequest(new FStaticMeshRequest(Dbid, Priority, TargetMeshComponent), CurrentFrameNumber);
}

void FXSPLoader::RequestStaticMesh_AnyThread(int32 Dbid, float Priority, UStaticMeshComponent* TargetMeshComponent)
{
    {
        FScopeLock Lock(&BlacklistCS);
        if (Blacklist.Contains(Dbid))
            return;
    }

    {
        FScopeLock Lock(&CachedRequestArrayCS);
        CachedRequestArray.Emplace(new FStaticMeshRequest(Dbid, Priority, TargetMeshComponent));
    }
}

void FXSPLoader::Tick(float DeltaTime)
{
    if (!bInitialized)
        return;

    CurrentFrameNumber = FrameNumber.fetch_add(1);

    //int32 NumRequests = 0;
    //{
    //    FScopeLock Lock(&CachedRequestArrayCS);
    //    NumRequests = CachedRequestArray.Num();
    //}

    //if (NumRequests > 0)
    //{
    //    UE_LOG(LogXSPLoader, Display, TEXT("本帧请求数: %d"), NumRequests);
    //    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange, FString::Printf(TEXT("本帧请求数: %d"), NumRequests));
    //}

    int64 BeginTicks = FDateTime::Now().GetTicks();
    DispatchNewRequests(CurrentFrameNumber);
    float UsedTime = (float)(FDateTime::Now().GetTicks() - BeginTicks) / ETimespan::TicksPerSecond;

    //if (NumRequests > 0)
    //{
    //    UE_LOG(LogXSPLoader, Display, TEXT("分发请求耗时: %.6f"), UsedTime);
    //    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange, FString::Printf(TEXT("分发请求耗时: %.6f"), UsedTime));
    //}

    float AvailableTime = 0.1f - UsedTime;
    ProcessMergeRequests(AvailableTime);

    ReleaseRequests();
}

void FXSPLoader::ResetInternal()
{
    for (auto SourceDataPtr : SourceDataList)
    {
        if (nullptr != SourceDataPtr->LoadThread)
        {
            SourceDataPtr->LoadThread->Kill(true);
            delete SourceDataPtr->LoadThread;
        }
        if (nullptr != SourceDataPtr->FileLoadRunnable)
        {
            delete SourceDataPtr->FileLoadRunnable;
        }
        if (SourceDataPtr->FileStream.is_open())
        {
            SourceDataPtr->FileStream.close();
        }
        delete SourceDataPtr;
    }
    SourceDataList.Empty();
}

void FXSPLoader::AddToBlacklist(int32 Dbid)
{
    FScopeLock Lock(&BlacklistCS);
    if (Blacklist.Contains(Dbid))
        return;
    Blacklist.Add(Dbid);
}

void FXSPLoader::DispatchRequest(FStaticMeshRequest* Request, uint64 InFrameNumber)
{
    auto DispatchToRequestQueue = [this](FStaticMeshRequest* Request) {
        for (auto SourceDataPtr : SourceDataList)
        {
            if (Request->Dbid >= SourceDataPtr->StartDbid && Request->Dbid < SourceDataPtr->StartDbid + SourceDataPtr->Count)
            {
                SourceDataPtr->LoadRequestQueue.Add(Request);
                break;
            }
        }
    };

    int32 Dbid = Request->Dbid;
    if (AllRequestMap.Contains(Dbid))
    {
        //已有请求
        FStaticMeshRequest* InQueueRequest = AllRequestMap[Dbid];
        {
            //更新时间戳
            FScopeLock Lock(&RequestCS);
            InQueueRequest->bValid = true;
            InQueueRequest->LastUpdateFrameNumber = InFrameNumber;
            //InQueueRequest->Priority = Request->Priority;
            //InQueueRequest->TargetComponent = Request->TargetComponent;
        }
        if (InQueueRequest->IsReleasable())
        {
            //重置并重新分发到请求队列
            InQueueRequest->ResetReleasable();
            DispatchToRequestQueue(InQueueRequest);
        }
        delete Request;
    }
    else
    {
        //新请求,为其创建静态网格对象
        Request->StaticMesh = TStrongObjectPtr<UStaticMesh>(NewObject<UStaticMesh>(Request->TargetComponent));
        Request->StaticMesh->bAllowCPUAccess = true;
        Request->LastUpdateFrameNumber = InFrameNumber;
        //根据Dbid分发到相应的请求队列
        DispatchToRequestQueue(Request);
        //加入到总Map
        AllRequestMap.Emplace(Dbid, Request);
    }
}

void FXSPLoader::DispatchNewRequests(uint64 InFrameNumber)
{
    TArray<FStaticMeshRequest*> NewRequestArray;
    {
        FScopeLock Lock(&CachedRequestArrayCS);
        NewRequestArray = MoveTemp(CachedRequestArray);
    }

    for (auto& TempRequest : NewRequestArray)
    {
        DispatchRequest(TempRequest, InFrameNumber);
    }
}

void FXSPLoader::ProcessMergeRequests(float AvailableTime)
{
    int64 BeginTicks = FDateTime::Now().GetTicks();
    while (!MergeRequestQueue.IsEmpty())
    {
        FStaticMeshRequest* Request;
        MergeRequestQueue.TakeFirst(Request);
        if (nullptr != Request)
        {
            UBodySetup* BodySetup = Request->StaticMesh->GetBodySetup();
            BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
            //BodySetup->CreatePhysicsMeshes();

            Request->TargetComponent->SetMaterial(0, CreateMaterialInstanceDynamic(SourceMaterial.Get(), Request->Color, Request->Roughness));
            Request->TargetComponent->SetStaticMesh(Request->StaticMesh.Get());
            Request->StaticMesh->RemoveFromRoot();
            Request->TargetComponent->RegisterComponent();
            
            UE_LOG(LogXSPLoader, Display, TEXT("完成加载: %d"), Request->Dbid);
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("完成加载: %d"), Request->Dbid));

            //标记为可释放
            Request->SetReleasable();
        }

        if ((float)(FDateTime::Now().GetTicks() - BeginTicks) / ETimespan::TicksPerSecond >= AvailableTime)
            break;
    }
}

void FXSPLoader::ReleaseRequests()
{
    for (TMap<int32, FStaticMeshRequest*>::TIterator Itr(AllRequestMap); Itr; ++Itr)
    {
        if (Itr.Value()->IsReleasable())
        {
            //唯一释放请求的位置
            delete Itr.Value();
            Itr.RemoveCurrent();
        }
    }
}