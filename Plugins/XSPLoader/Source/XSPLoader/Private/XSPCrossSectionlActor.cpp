// Fill out your copyright notice in the Description page of Project Settings.


#include "XSPCrossSectionActor.h"
#include "XSPModelActor.h"

#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"

UClass* AXSPCrossSectionActor::GetBPClass()
{
    return LoadClass<AXSPCrossSectionActor>(nullptr, TEXT("/XSPLoader/CrossSection/BP_CrossSection.BP_CrossSection_C"));
}

AXSPCrossSectionActor::AXSPCrossSectionActor()
{
    PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));

    CenterSphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CenterSphere"));
    CenterSphere->SetupAttachment(RootComponent);
    NormalTranslate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowNormal"));
    NormalTranslate->SetupAttachment(RootComponent);
    Plane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plane"));
    Plane->SetupAttachment(RootComponent);

    DefaultComponents.Add(CenterSphere);
    DefaultComponents.Add(NormalTranslate);
    DefaultComponents.Add(Plane);
}

void AXSPCrossSectionActor::SetDir(FString Dir)
{
    if (Dir == TEXT("x")) //X
    {
        Rotator = FRotator(90, 0, 0);
    }
    else if (Dir == TEXT("y")) //Y
    {
        Rotator = FRotator(0, 0, 90);
    }
    else if (Dir == TEXT("z"))//Z
    {
        Rotator = FRotator(0, 0, 0);
    }

    if (bEnable)
    {
        SetActorLocationAndRotation(Position, Rotator);
        ApplyCrossSectionParams();
    }
}

void AXSPCrossSectionActor::SetEnable(bool bInEnable)
{
    if (bInEnable != bEnable)
    {
        bEnable = bInEnable;
        SetActorHiddenInGame(!bEnable);
        if (bEnable)
        {
            Position = FVector(0, 0, 0);
            FBox Box = GetModelBounds();
            if (Box.IsValid)
            {
                Position = Box.GetCenter();
            }

            SetActorLocationAndRotation(Position, Rotator);
            ApplyCrossSectionParams();
        }
    }
}

void AXSPCrossSectionActor::OnPress(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed)
{
    if (!bEnable)
        return;

    if (TouchedComponent == NormalTranslate)
    {
        State = EState::DragMove;
        Mode = EMode::TranslateByAxis; //沿Z轴移动

        APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

        FVector MouseWorldLocation, MouseWorldDirection;
        PlayerController->DeprojectMousePositionToWorld(MouseWorldLocation, MouseWorldDirection);
        ProjPlane = FMath::Abs(FVector::DotProduct(MouseWorldDirection, GetActorForwardVector())) > FMath::Abs(FVector::DotProduct(MouseWorldDirection, GetActorRightVector())) ? EProjPlane::PlaneX : EProjPlane::PlaneY;
        StartMovingAxis = GetActorUpVector();
        StartLocation = GetActorLocation();
        StartPoint = FVector::DotProduct((GetMouseIntersectionPositionOnTranslatePlane() - StartLocation), StartMovingAxis) * StartMovingAxis + StartLocation;

        PlayerController->SetIgnoreMoveInput(true);
        PlayerController->SetIgnoreLookInput(true);
    }
    GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("AXSPCrossSectionActor::OnPress"));
}

void AXSPCrossSectionActor::OnRelease(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed)
{
    if (!bEnable)
        return;

    if (State == EState::DragMove)
    {
        APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        PlayerController->ResetIgnoreLookInput();
        PlayerController->ResetIgnoreMoveInput();
        State = EState::Default;
    }
    
    //GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("AXSPCrossSectionActor::OnRelease"));
}

void AXSPCrossSectionActor::OnBeginHover(UPrimitiveComponent* TouchedComponent)
{
    if (!bEnable)
        return;

    //GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("AXSPCrossSectionActor::OnBeginHover"));

    //UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Plane->GetMaterial(0));
    //Material->SetScalarParameterValue(TEXT("Opacity"), 0.5);

}

void AXSPCrossSectionActor::OnEndHover(UPrimitiveComponent* TouchedComponent)
{
    if (!bEnable)
        return;

    //GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("AXSPCrossSectionActor::OnEndHover"));

    //UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Plane->GetMaterial(0));
    //Material->SetScalarParameterValue(TEXT("Opacity"), 0.01);
}

void AXSPCrossSectionActor::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    PlayerController->bEnableMouseOverEvents = true;
    PlayerController->bEnableClickEvents = true;

    EnableInput(PlayerController);
    InputComponent->BindKey(EKeys::LeftMouseButton, EInputEvent::IE_Released, this, &AXSPCrossSectionActor::HandleLeftMouseButtonPressed);
    for (auto Component : DefaultComponents)
    {
        Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Component->OnBeginCursorOver.AddDynamic(this, &AXSPCrossSectionActor::OnBeginHover);
        Component->OnEndCursorOver.AddDynamic(this, &AXSPCrossSectionActor::OnEndHover);
        Component->OnClicked.AddDynamic(this, &AXSPCrossSectionActor::OnPress);
        Component->OnReleased.AddDynamic(this, &AXSPCrossSectionActor::OnRelease);
    }

    {
        UMaterialInterface* PlaneMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, L"/XSPLoader/CrossSection/Res/M_Plane"));
        UMaterialInstanceDynamic* PlaneMaterialInstanceDynamic = UMaterialInstanceDynamic::Create(PlaneMaterial, nullptr);
        Plane->SetMaterial(0, PlaneMaterialInstanceDynamic);
        PlaneMaterialInstanceDynamic->SetVectorParameterValue(TEXT("Color"), FVector4f(0.f, 0.5f, 0.5f, 1.f));
        PlaneMaterialInstanceDynamic->SetScalarParameterValue(TEXT("Opacity"), 0.5);

        UMaterialInterface* GizmoMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, L"/XSPLoader/CrossSection/Res/M_Gizmo"));
        UMaterialInstanceDynamic* SphereMaterialInstanceDynamic = UMaterialInstanceDynamic::Create(GizmoMaterial, nullptr);
        CenterSphere->SetMaterial(0, SphereMaterialInstanceDynamic);
        SphereMaterialInstanceDynamic->SetVectorParameterValue(TEXT("Color"), FVector4f(0.5f, 0.5f, 0.f, 1.f));
        UMaterialInstanceDynamic* AxisMaterialInstanceDynamic = UMaterialInstanceDynamic::Create(GizmoMaterial, nullptr);
        NormalTranslate->SetMaterial(0, AxisMaterialInstanceDynamic);
        AxisMaterialInstanceDynamic->SetVectorParameterValue(TEXT("Color"), FVector4f(1.f, 0.f, 0.f, 1.f));
    }
}

void AXSPCrossSectionActor::Tick(float DeltaTime)
{
    if (!bEnable)
        return;

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

    double Scale = GetScreenSpaceConstantScaleFactor(0.00077);
    SetActorScale3D(FVector(Scale));

    FBox Box = GetModelBounds();
    if (Box.IsValid)
    {
        Position = Box.GetCenter();
        FVector Size = Box.GetSize();
        double R = FMath::Max3(Size.X, Size.Y, Size.Z);
        Plane->SetWorldScale3D(FVector(R * 0.1, R * 0.1, 1));
    }

    if (State == EState::DragMove)
    {
        if (Mode == EMode::TranslateByAxis)
        {
            FVector CurrentMousePosition, CurrentMouseDirection;
            PlayerController->DeprojectMousePositionToWorld(CurrentMousePosition, CurrentMouseDirection);
            //获取鼠标方向与目标投影平面交点在世界坐标系下的位置
            FVector IntersectPoint = GetMouseIntersectionPositionOnTranslatePlane();
            //获取目标投影平面上的鼠标交点位置在当前移动轴上的投影
            FVector TargetPositionOnAxis = FVector::DotProduct((IntersectPoint - StartLocation), StartMovingAxis) * StartMovingAxis + StartLocation;
            FVector Offset = TargetPositionOnAxis - StartPoint;
            Position = StartLocation + Offset;

            SetActorLocationAndRotation(Position, Rotator);
            ApplyCrossSectionParams();
        }
    }
}

void AXSPCrossSectionActor::ApplyCrossSectionParams()
{
    TArray<AActor*> ModelActors; 
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AXSPModelActor::StaticClass(), ModelActors);
    for (AActor* Actor : ModelActors)
    {
        if (Actor->IsPendingKill())
            continue;

        AXSPModelActor* ModelActor = Cast<AXSPModelActor>(Actor);
        ModelActor->SetCrossSection(true, Position, -GetActorUpVector());
    }
}

float AXSPCrossSectionActor::GetScreenSpaceConstantScaleFactor(float InFactor)
{
    float OutScaleFactor = InFactor;

    FVector ActorLocation = GetActorLocation();

    UWorld* World = GetWorld();
    //处理屏幕尺寸因素
    FVector2D ViewportSize;
    World->GetGameViewport()->GetViewportSize(ViewportSize);
    OutScaleFactor *= (2048 / ViewportSize.X);
    //处理距离因素
    APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(World, 0);
    FVector CamLoc = CameraManager->GetCameraLocation();
    OutScaleFactor *= UKismetMathLibrary::Vector_Distance(CamLoc, ActorLocation);
    //处理相机FOV因素
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
    UCameraComponent* CamComp = Cast<UCameraComponent>(PlayerPawn->GetComponentByClass(UCameraComponent::StaticClass()));
    OutScaleFactor *= UKismetMathLibrary::DegTan(CamComp ? CamComp->FieldOfView * 0.5f : 45);

    return OutScaleFactor;
}

FVector AXSPCrossSectionActor::GetMouseIntersectionPositionOnTranslatePlane()
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    FVector MouseWorldLocation, MouseWorldDirection;
    PlayerController->DeprojectMousePositionToWorld(MouseWorldLocation, MouseWorldDirection);
    FVector normal;
    if (ProjPlane == EProjPlane::PlaneX)
    {
        normal = GetActorForwardVector();

    }
    else if (ProjPlane == EProjPlane::PlaneY)
    {
        normal = GetActorRightVector();
    }
    float Factor = (FVector::DotProduct(normal, GetActorLocation()) - FVector::DotProduct(normal, MouseWorldLocation)) / FVector::DotProduct(normal, MouseWorldDirection);
    FVector result = MouseWorldLocation + MouseWorldDirection * Factor;

    return result;
}

void AXSPCrossSectionActor::HandleLeftMouseButtonPressed()
{
    if (!bEnable)
        return;

    if (State == EState::DragMove)
    {
        APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        PlayerController->ResetIgnoreLookInput();
        PlayerController->ResetIgnoreMoveInput();
        State = EState::Default;
    }

    //GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("AXSPCrossSectionActor::HandleLeftMouseButtonPressed"));
}

FBox AXSPCrossSectionActor::GetModelBounds()
{
    FBox Bounds = FBox(ForceInit);

    TArray<AActor*> ModelActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AXSPModelActor::StaticClass(), ModelActors);
    for (AActor* Actor : ModelActors)
    {
        if (Actor->IsPendingKill())
            continue;
        FVector Origin, Extent;
        Actor->GetActorBounds(false, Origin, Extent, true);
        FBox Box = FBox::BuildAABB(Origin, Extent);
        Bounds += Box;
    }

    return Bounds;
}
