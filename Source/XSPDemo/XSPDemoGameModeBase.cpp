// Copyright Epic Games, Inc. All Rights Reserved.


#include "XSPDemoGameModeBase.h"
#include "XSPModelActor.h"
#include "XSPLoaderModule.h"
#include "XSPCrossSectionActor.h"

#include <fstream>

#include "Kismet/GameplayStatics.h"


DEFINE_LOG_CATEGORY_STATIC(LogDynamicLoadDemo, Log, All);

AXSPDemoGameModeBase::AXSPDemoGameModeBase()
{
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bCanEverTick = true;
}

void AXSPDemoGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
    InputMode.SetHideCursorDuringCapture(false);
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    PlayerController->SetInputMode(InputMode);
    PlayerController->SetShowMouseCursor(true);
}

void AXSPDemoGameModeBase::Logout(AController* Exiting)
{
}

void AXSPDemoGameModeBase::Tick(float deltaSeconds)
{
}

void AXSPDemoGameModeBase::Load()
{
    if (nullptr != CombinedMeshActor)
        return;

    int32 MaxNumBatches = 4000;
    FParse::Value(FCommandLine::Get(), TEXT("-batches="), MaxNumBatches);

    bool bAsyncLoad = FParse::Param(FCommandLine::Get(), TEXT("async"));

    FString ModelName;
    FParse::Value(FCommandLine::Get(), TEXT("-Model="), ModelName);

    FModuleManager::GetModuleChecked<FXSPLoaderModule>("XSPLoader").SetAutoComputeBatchParams(true, MaxNumBatches);

    const FString ConfigFile = FPaths::Combine(FPaths::ProjectDir(), TEXT("XSPConfiguration.ini"));

    if (!ModelName.IsEmpty())
    {
        FString XSPDirectory;
        GConfig->GetString(TEXT("XSPAssetConfiguration"), TEXT("XSPDirectory"), XSPDirectory, ConfigFile);
        XSPDirectory = FPaths::Combine(XSPDirectory, ModelName, TEXT("model"));
        if (FPaths::DirectoryExists(XSPDirectory))
        {
            TArray<FString> FileArray;
            IFileManager::Get().FindFiles(FileArray, *XSPDirectory, TEXT(".xsp"));
            for (FString& FileName : FileArray)
            {
                FileName = FPaths::Combine(XSPDirectory, FileName);
            }
            if (!FileArray.IsEmpty())
            {
                AXSPModelActor* Actor = GetWorld()->SpawnActor<AXSPModelActor>();
                Actor->Load(FileArray, bAsyncLoad);
                CombinedMeshActor = Actor;
            }
        }
    }
}

void AXSPDemoGameModeBase::Unload()
{
    if (CrossSectionActor)
    {
        CrossSectionActor->SetModelActor(nullptr);
    }

    if (CombinedMeshActor)
    {
        GetWorld()->DestroyActor(CombinedMeshActor);
        CombinedMeshActor = nullptr;
        GEngine->ForceGarbageCollection(true);
    }
}

void AXSPDemoGameModeBase::EnableCorssSection()
{
    if (!CrossSectionActor)
    {
        CrossSectionActor = Cast<AXSPCrossSectionActor>(GetWorld()->SpawnActor(AXSPCrossSectionActor::GetBPClass()));
    }

    if (CombinedMeshActor)
    {
        CrossSectionActor->SetModelActor(Cast<AXSPModelActor>(CombinedMeshActor));
    }    
}

void AXSPDemoGameModeBase::DisableCorssSection()
{
    if (CrossSectionActor)
    {
        CrossSectionActor->SetModelActor(nullptr);
    }
}
