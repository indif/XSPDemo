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

void AXSPCrossSectionActor::SetModelActor(AXSPModelActor* InModelActor)
{
    if (ModelActor.Get() == InModelActor)
        return;

    if (ModelActor.IsValid())
    {
        ModelActor->SetCrossSection(false);
        if (ModelLoadFinishDelegateHandle.IsValid())
        {
            ModelActor->GetOnLoadFinishDelegate().Remove(ModelLoadFinishDelegateHandle);
            ModelLoadFinishDelegateHandle.Reset();
        }
    }

    ModelActor = InModelActor;
    if (ModelActor.IsValid())
    {
        if (!InitCrossSectionParams())
        {
            ModelLoadFinishDelegateHandle = ModelActor->GetOnLoadFinishDelegate().AddUObject(this, &AXSPCrossSectionActor::OnModelLoadFinish);
        }
        else
        {
            ApplyCrossSectionParams();
            SetActorLocationAndRotation(Position, Rotator);
        }
    }
}

void AXSPCrossSectionActor::OnPress(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed)
{
    if (TouchedComponent == NormalTranslate)
    {
        State = EState::DragMove;
        Mode = EMode::TranslateByAxis; //沿Z轴移动

        APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

        FVector MouseWorldLocation, MouseWorldDirection;
        PlayerController->DeprojectMousePositionToWorld(MouseWorldLocation, MouseWorldDirection);
        ProjPlane = FMath::Abs(FVector::DotProduct(MouseWorldDirection, FVector::XAxisVector)) > FMath::Abs(FVector::DotProduct(MouseWorldDirection, FVector::YAxisVector)) ? EProjPlane::PlaneX : EProjPlane::PlaneY;
        StartMovingAxis = FVector::ZAxisVector;
        StartLocation = GetActorLocation();
        StartPoint = FVector::DotProduct((GetMouseIntersectionPositionOnTranslatePlane() - StartLocation), StartMovingAxis) * StartMovingAxis + StartLocation;

        PlayerController->SetIgnoreMoveInput(true);
        PlayerController->SetIgnoreLookInput(true);
    }
    //GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("AXSPCrossSectionActor::OnPress"));
}

void AXSPCrossSectionActor::OnRelease(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed)
{
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
    //GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("AXSPCrossSectionActor::OnBeginHover"));

    //UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Plane->GetMaterial(0));
    //Material->SetScalarParameterValue(TEXT("Opacity"), 0.5);

}

void AXSPCrossSectionActor::OnEndHover(UPrimitiveComponent* TouchedComponent)
{
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
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

    float Scale = GetScreenSpaceConstantScaleFactor(0.00077);
    SetActorScale3D(FVector(Scale));

    FVector2D PlaneScale = GetPlaneScale(1.1);
    Plane->SetWorldScale3D(FVector(PlaneScale, 1));

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
            //修改Actor位置
            SetActorLocation(StartLocation + Offset);

            Position = GetActorLocation();
            ApplyCrossSectionParams();
        }
    }
}

void AXSPCrossSectionActor::OnModelLoadFinish(int32 Type)
{
    if (Type == 0)
    {
        if (InitCrossSectionParams())
        {
            ApplyCrossSectionParams();
            SetActorLocationAndRotation(Position, Rotator);
        }
    }
}

bool AXSPCrossSectionActor::InitCrossSectionParams()
{
    if (ModelActor.IsValid())
    {
        ModelActorBox = ModelActor->GetNodeBoundingBox(0);
        if (ModelActorBox.IsValid)
        {
            Position = FVector(ModelActorBox.GetCenter());
            Rotator = FRotator(0, 0, 0);
            return true;
        }
    }
    return false;
}

void AXSPCrossSectionActor::ApplyCrossSectionParams()
{
    FVector Normal = Rotator.RotateVector(FVector(0, 0, -1));
    ModelActor->SetCrossSection(true, Position, Normal);

    SetActorLocationAndRotation(Position, Rotator);
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

FVector2D AXSPCrossSectionActor::GetPlaneScale(float InFactor)
{
    double PlaneRadius = Plane->GetStaticMesh()->GetBounds().SphereRadius;

    //根据ModelActorBox与切面Position和Rotator计算合适的缩放
    double BoxRadius = ModelActorBox.GetExtent().Length();
    double Scale = BoxRadius / PlaneRadius;

    return FVector2D(InFactor* Scale, InFactor* Scale);
}

FVector AXSPCrossSectionActor::GetMouseIntersectionPositionOnTranslatePlane()
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    FVector MouseWorldLocation, MouseWorldDirection;
    PlayerController->DeprojectMousePositionToWorld(MouseWorldLocation, MouseWorldDirection);
    FVector normal;
    if (ProjPlane == EProjPlane::PlaneX)
    {
        normal = FVector::XAxisVector;

    }
    else if (ProjPlane == EProjPlane::PlaneY)
    {
        normal = FVector::YAxisVector;
    }
    float Factor = (FVector::DotProduct(normal, GetActorLocation()) - FVector::DotProduct(normal, MouseWorldLocation)) / FVector::DotProduct(normal, MouseWorldDirection);
    FVector result = MouseWorldLocation + MouseWorldDirection * Factor;

    return result;
}

void AXSPCrossSectionActor::HandleLeftMouseButtonPressed()
{
    if (State == EState::DragMove)
    {
        APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        PlayerController->ResetIgnoreLookInput();
        PlayerController->ResetIgnoreMoveInput();
        State = EState::Default;
    }

    //GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("AXSPCrossSectionActor::HandleLeftMouseButtonPressed"));
}
