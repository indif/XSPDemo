#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "XSPCrossSectionActor.generated.h"

class AXSPModelActor;

UCLASS()
class XSPLOADER_API AXSPCrossSectionActor : public AActor
{
	GENERATED_BODY()
	
public:
	enum class EState : uint8
	{
		Default,
		DragMove
	};

	enum class EMode : uint8
	{
		Default,
		TranslateByAxis,
		TranslateByPlane,
		Rotate
	};

	enum class EProjPlane : uint8
	{
		PlaneX,
		PlaneY
	};

	static UClass* GetBPClass();

	AXSPCrossSectionActor();

	UFUNCTION(BlueprintCallable)
	void SetDir(FString Dir); //x,y,z

	UFUNCTION(BlueprintCallable)
	void SetEnable(bool bEnable);

	UFUNCTION()
	void OnPress(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);

	UFUNCTION()
	void OnRelease(UPrimitiveComponent* TouchedComponent, FKey ButtonReleased);
	
	UFUNCTION()
	void OnBeginHover(UPrimitiveComponent* TouchedComponent);
	
	UFUNCTION()
	void OnEndHover(UPrimitiveComponent* TouchedComponent);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	void ApplyCrossSectionParams();

	float GetScreenSpaceConstantScaleFactor(float InFactor);

	FVector GetMouseIntersectionPositionOnTranslatePlane();

	void HandleLeftMouseButtonPressed();

	FBox GetModelBounds();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMeshComponent* CenterSphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMeshComponent* NormalTranslate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMeshComponent* Plane;

private:
	TArray<UStaticMeshComponent*> DefaultComponents;

	bool bEnable = false;

	FVector Position = FVector(0, 0, 0);
	FRotator Rotator = FRotator(0, 0, 0);

	EState State = EState::Default;
	EMode Mode = EMode::Default;

	EProjPlane ProjPlane;
	FVector StartPoint;
	FVector StartLocation;
	FVector StartMovingAxis;
};
