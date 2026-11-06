#include "DraggablePanelWidget.h"

#include "Components/SizeBox.h"
#include "Components/Widget.h"
#include "Components/CanvasPanelSlot.h"
#include "Misc/ConfigCacheIni.h"

void UDraggablePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UCanvasPanelSlot* slot = RootCanvasSlot())
	{
		slot->SetAutoSize(true);
	}

	ApplyPanelSize();
	LoadLayout();
}

void UDraggablePanelWidget::NativeTick(const FGeometry& _geometry, float _deltaTime)
{
	Super::NativeTick(_geometry, _deltaTime);
	ApplyPanelSize();   
}

FVector2D UDraggablePanelWidget::ClampPanelSize(FVector2D _size) const
{
	_size.X = FMath::Max(_size.X, MinPanelSize.X);
	_size.Y = FMath::Max(_size.Y, MinPanelSize.Y);
	if (MaxPanelSize.X > 0.0f) _size.X = FMath::Min(_size.X, MaxPanelSize.X);
	if (MaxPanelSize.Y > 0.0f) _size.Y = FMath::Min(_size.Y, MaxPanelSize.Y);
	return _size;
}

void UDraggablePanelWidget::ApplyPanelSize()
{
	if (RootBox)
	{
		const FVector2D s = ClampPanelSize(PanelSize);
		RootBox->SetWidthOverride(s.X);
		RootBox->SetHeightOverride(s.Y);
	}
}

UCanvasPanelSlot* UDraggablePanelWidget::RootCanvasSlot() const
{
	return RootBox ? Cast<UCanvasPanelSlot>(RootBox->Slot) : nullptr;
}

// ============================================================================
//  Ziehen mit der Maus
// ============================================================================
FReply UDraggablePanelWidget::NativeOnPreviewMouseButtonDown(const FGeometry& _geometry, const FPointerEvent& _event)
{
	// RESIZE: an der unteren rechten Ecke der RootBox. 
	if (bResizable && RootBox && _event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FVector2D abs = _event.GetScreenSpacePosition();
		const FGeometry& rg = RootBox->GetCachedGeometry();
		const FVector2D lc  = rg.AbsoluteToLocal(abs);   // Klick in RootBox-lokalen Koordinaten
		const FVector2D sz  = rg.GetLocalSize();         // Panelgröße

		const bool bInCorner = lc.X >= sz.X - ResizeCorner && lc.Y >= sz.Y - ResizeCorner
		                     && lc.X <= sz.X + 8.0f && lc.Y <= sz.Y + 8.0f;
		const bool bOnHandle = ResizeHandle && ResizeHandle->GetCachedGeometry().IsUnderLocation(abs);

		if ((bInCorner || bOnHandle) && RootCanvasSlot())
		{
			m_resizing     = true;
			m_dragging     = false;
			m_lastLocalPos = _geometry.AbsoluteToLocal(abs);
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
	}
	return Super::NativeOnPreviewMouseButtonDown(_geometry, _event);
}

FReply UDraggablePanelWidget::NativeOnMouseButtonDown(const FGeometry& _geometry, const FPointerEvent& _event)
{
	if (bDraggable && RootCanvasSlot() && _event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (RootBox && RootBox->GetCachedGeometry().IsUnderLocation(_event.GetScreenSpacePosition()))
		{
			m_resizing     = false;
			m_dragging     = true;
			m_lastLocalPos = _geometry.AbsoluteToLocal(_event.GetScreenSpacePosition());
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
	}
	return Super::NativeOnMouseButtonDown(_geometry, _event);
}

FReply UDraggablePanelWidget::NativeOnMouseMove(const FGeometry& _geometry, const FPointerEvent& _event)
{
	if (m_dragging || m_resizing)
	{
		if (UCanvasPanelSlot* slot = RootCanvasSlot())
		{
			const FVector2D nowLocal = _geometry.AbsoluteToLocal(_event.GetScreenSpacePosition());
			const FVector2D delta    = nowLocal - m_lastLocalPos;
			m_lastLocalPos = nowLocal;

			if (m_resizing)
			{
				PanelSize = ClampPanelSize(PanelSize + delta);   // Min UND Max
				ApplyPanelSize();
			}
			else
			{
				slot->SetPosition(slot->GetPosition() + delta);
			}
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(_geometry, _event);
}

FReply UDraggablePanelWidget::NativeOnMouseButtonUp(const FGeometry& _geometry, const FPointerEvent& _event)
{
	if ((m_dragging || m_resizing) && _event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		m_dragging = false;
		m_resizing = false;
		SaveLayout();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(_geometry, _event);
}

// ============================================================================
//  Position speichern/laden (GameUserSettings.ini, Sektion [UILayout])
// ============================================================================
void UDraggablePanelWidget::SaveLayout()
{
	UCanvasPanelSlot* slot = RootCanvasSlot();
	if (!slot || PanelId.IsNone()) return;

	// Position UND Größe sichern: "posX,posY,sizeX,sizeY".
	const FVector2D p = slot->GetPosition();
	const FString val = FString::Printf(TEXT("%.1f,%.1f,%.1f,%.1f"),
		p.X, p.Y, PanelSize.X, PanelSize.Y);
	GConfig->SetString(TEXT("UILayout"), *PanelId.ToString(), *val, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UDraggablePanelWidget::LoadLayout()
{
	UCanvasPanelSlot* slot = RootCanvasSlot();
	if (!slot || PanelId.IsNone()) return;

	FString val;
	if (!GConfig->GetString(TEXT("UILayout"), *PanelId.ToString(), val, GGameUserSettingsIni))
		return;   

	TArray<FString> parts;
	val.ParseIntoArray(parts, TEXT(","));
	if (parts.Num() >= 2)
		slot->SetPosition(FVector2D(FCString::Atof(*parts[0]), FCString::Atof(*parts[1])));
	if (parts.Num() >= 4)
	{
		PanelSize = ClampPanelSize(FVector2D(FCString::Atof(*parts[2]), FCString::Atof(*parts[3])));
		ApplyPanelSize();
	}
}

void UDraggablePanelWidget::ResetPanelPosition()
{
	if (!PanelId.IsNone())
	{
		GConfig->RemoveKey(TEXT("UILayout"), *PanelId.ToString(), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}
