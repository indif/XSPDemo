// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "XSPDemoGameModeBase.generated.h"

/**
 * 
 */
UCLASS()
class XSPDEMO_API AXSPDemoGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
		
public:
	AXSPDemoGameModeBase();
	virtual void BeginPlay() override;
	virtual void Logout(AController* Exiting) override;
	virtual void Tick(float deltaSeconds) override;

	UFUNCTION(BlueprintCallable)
	void Load();

	UFUNCTION(BlueprintCallable)
	void Unload();

	UFUNCTION(BlueprintCallable)
	void EnableCorssSection();
	
	UFUNCTION(BlueprintCallable)
	void DisableCorssSection();

private:
	//UPROPERTY()
	AActor* CombinedMeshActor = nullptr;

	UPROPERTY()
	class AXSPCrossSectionActor* CrossSectionActor = nullptr;
};
