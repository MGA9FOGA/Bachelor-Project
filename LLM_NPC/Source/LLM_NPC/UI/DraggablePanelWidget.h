#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DraggablePanelWidget.generated.h"

class USizeBox;
class UWidget;
class UCanvasPanelSlot;

/**
 * UDraggablePanelWidget – Basis für verschiebbare UI-Panels mit einstellbarer Größe
 * und gespeicherter Position.
 *
 * - PanelSize  → Größe der SizeBox (live im Details-Panel/PIE änderbar)
 * - Ziehen     → mit der Maus auf einer freien Fläche (Buttons/Zeilen fangen ihre
 *                Klicks selbst ab, stören also nicht)
 * - PanelId    → Schlüssel, unter dem die Position gespeichert wird (pro Panel EINDEUTIG!)
 */
UCLASS()
class LLM_NPC_API UDraggablePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Panel")
	FVector2D PanelSize = FVector2D(460.0f, 840.0f);

	// Darf der Spieler das Panel mit der Maus verschieben?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Panel")
	bool bDraggable = true;

	// Darf der Spieler das Panel an der unteren rechten Ecke größer/kleiner ziehen?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Panel")
	bool bResizable = true;

	// Mindestgröße beim Resizen.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Panel")
	FVector2D MinPanelSize = FVector2D(180.0f, 140.0f);

	// Maximalgröße beim Resizen. 0 pro Achse = unbegrenzt.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Panel")
	FVector2D MaxPanelSize = FVector2D(1000.0f, 1200.0f);

	// Größe der Greifzone unten rechts (px), in der Ziehen = Resizen statt Verschieben.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Panel")
	float ResizeCorner = 28.0f;

	// Eindeutiger Name zum Speichern der Position. PRO Panel unterschiedlich setzen,.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Panel")
	FName PanelId = TEXT("Panel");

	UFUNCTION(BlueprintCallable, Category="Panel")
	void ResetPanelPosition();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& _geometry, float _deltaTime) override;

	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& _geometry, const FPointerEvent& _event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& _geometry, const FPointerEvent& _event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& _geometry, const FPointerEvent& _event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& _geometry, const FPointerEvent& _event) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> RootBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ResizeHandle;

	void ApplyPanelSize();

	FVector2D ClampPanelSize(FVector2D _size) const;
	UCanvasPanelSlot* RootCanvasSlot() const;

	void SaveLayout();
	void LoadLayout();

private:
	bool      m_dragging     = false;
	bool      m_resizing     = false;
	FVector2D m_lastLocalPos = FVector2D::ZeroVector;
};
