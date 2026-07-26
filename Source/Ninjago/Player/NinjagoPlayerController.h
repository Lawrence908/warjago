// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NinjagoPlayerController.generated.h"

class ANinjagoUnit;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Player control for v0.1: one unit = one order.
 *   LMB  - select a single unit (or deselect on empty ground)
 *   RMB  - order the selected unit: on an enemy -> attack, else -> move
 *   WASD - pan the camera        Wheel - zoom        Esc - deselect
 *
 * Enhanced Input actions and the mapping context are built as transient objects in C++ (no .uasset
 * authoring needed). Selection is shown as a ground circle under the selected unit.
 */
UCLASS()
class NINJAGO_API ANinjagoPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANinjagoPlayerController();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	UPROPERTY() TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY() TObjectPtr<UInputAction> IA_Pan;
	UPROPERTY() TObjectPtr<UInputAction> IA_Zoom;
	UPROPERTY() TObjectPtr<UInputAction> IA_Select;
	UPROPERTY() TObjectPtr<UInputAction> IA_Order;
	UPROPERTY() TObjectPtr<UInputAction> IA_Deselect;
	UPROPERTY() TObjectPtr<UInputAction> IA_Ability;
	UPROPERTY() TObjectPtr<UInputAction> IA_Restart;
	UPROPERTY() TObjectPtr<UInputAction> IA_Help;
	UPROPERTY() TArray<TObjectPtr<UInputAction>> IA_Scenarios; // keys 1-4

	bool bShowHelp = true;

	TWeakObjectPtr<ANinjagoUnit> Selected;

	/** Build the transient input actions + mapping context and register them. */
	void BuildInput();

	void OnPan(const FInputActionValue& Value);
	void OnZoom(const FInputActionValue& Value);
	void OnSelect(const FInputActionValue& Value);
	void OnOrder(const FInputActionValue& Value);
	void OnDeselect(const FInputActionValue& Value);
	void OnAbility(const FInputActionValue& Value);
	void OnRestart(const FInputActionValue& Value);
	void OnHelp(const FInputActionValue& Value);
	void OnScenario1(const FInputActionValue& Value);
	void OnScenario2(const FInputActionValue& Value);
	void OnScenario3(const FInputActionValue& Value);
	void OnScenario4(const FInputActionValue& Value);
	void LoadScenario(int32 Index);

	void SetSelected(ANinjagoUnit* Unit);
	ANinjagoUnit* TraceUnitUnderCursor() const;
	bool TraceGroundUnderCursor(FVector& OutLocation) const;
};
