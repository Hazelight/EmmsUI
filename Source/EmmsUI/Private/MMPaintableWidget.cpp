#include "MMPaintableWidget.h"
#include "EmmsWidgetHandle.h"
#include "EmmsStatics.h"
#include "EmmsDefaultWidgetStyles.h"
#include "Fonts/FontMeasure.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateColorBrush.h"

#include "AngelscriptBinds.h"
#include "AngelscriptManager.h"

static const FName NAME_Paintable_WhiteBrush("WhiteBrush");
static const FName NAME_Paintable_Bold("Bold");
void UMMPaintableWidget::ConditionalStartDraws()
{
	if (ContainingWidget == nullptr)
		ContainingWidget = GetTypedOuter<UMMWidget>();

	if (ContainingWidget != nullptr)
	{
		if (ContainingWidget->DrawCount != LastMMWidgetDrawCount)
		{
			LastMMWidgetDrawCount = ContainingWidget->DrawCount;
			Draws.Reset();
		}
	}
	else
	{
		Draws.Reset();
	}
}

void UMMPaintableWidget::DrawBox(FVector2D Position, FVector2D Size, const FSlateBrush* Brush, FLinearColor Tint)
{
	ConditionalStartDraws();

	FSlateBrush DrawBrush;
	if (Brush != nullptr)
		DrawBrush = *Brush;
	else
		DrawBrush = *FAppStyle::Get().GetBrush(NAME_Paintable_WhiteBrush);

	Draws.Add([Size, Position, DrawBrush, Tint](FPaintContext& Context)
	{
		Context.MaxLayer++;
		FSlateDrawElement::MakeBox(
			Context.OutDrawElements,
			Context.MaxLayer,
			Context.AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)),
			&DrawBrush,
			ESlateDrawEffect::None,
			Tint);
	});
}

void UMMPaintableWidget::DrawLines(const TArray<FVector2D>& Points, FLinearColor Tint, bool bAntiAlias, float Thickness)
{
	ConditionalStartDraws();

	Draws.Add([Points, Tint, bAntiAlias, Thickness](FPaintContext& Context)
	{
		Context.MaxLayer++;
		FSlateDrawElement::MakeLines(
			Context.OutDrawElements,
			Context.MaxLayer,
			Context.AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			Tint,
			bAntiAlias,
			Thickness);
	});
}

void UMMPaintableWidget::DrawText(const FString& String, FVector2D Position, const FSlateFontInfo& Font, FLinearColor Tint, FVector2D Alignment)
{
	ConditionalStartDraws();

	Draws.Add([String, Position, Font, Tint, Alignment](FPaintContext& Context)
	{
		Context.MaxLayer++;

		FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(String, Font);
		FVector2D AlignmentOffset = TextSize * (-Alignment);

		FSlateDrawElement::MakeText(
			Context.OutDrawElements,
			Context.MaxLayer,
			Context.AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Position + AlignmentOffset)),
			String, Font, ESlateDrawEffect::None, Tint);
	});
}

void UMMPaintableWidget::DrawRing(FVector2D Position, float Radius, FLinearColor Tint, float Thickness)
{
	ConditionalStartDraws();

	Draws.Add([Position, Radius, Tint, Thickness](FPaintContext& Context)
	{
		Context.MaxLayer++;

		static FSlateColorBrush Brush = FSlateColorBrush(FColorList::White);
		FSlateResourceHandle ResourceHndl = FSlateApplication::Get().GetRenderer()->GetResourceHandle(Brush);
		auto Transform = Context.AllottedGeometry.ToPaintGeometry().GetAccumulatedRenderTransform();

		const int Resolution = 24;
		FColor Color = Tint.ToFColor(true);

		TArray<FSlateVertex> Verts;
		float Step = (PI * 2.f) / Resolution;

		// Outer ring of vertices
		for(int i = 0; i < Resolution; ++i)
		{
			float Angle = Step * i;

			FVector2D ScreenPos = Position + FVector2D(FMath::Sin(Angle), FMath::Cos(Angle)) * Radius;
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, FVector2f(ScreenPos), FVector2f(0.f), Color));
		}

		// Inner ring of vertices
		for(int i = 0; i < Resolution; ++i)
		{
			float Angle = Step * i;
			FVector2D ScreenPos = Position + FVector2D(FMath::Sin(Angle), FMath::Cos(Angle)) * (Radius - Thickness);
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, FVector2f(ScreenPos), FVector2f(0.f), Color));
		}

		TArray<SlateIndex> Indicies;
		Indicies.Reserve(Resolution * 4);
		for(int i = 0; i < Resolution; ++i)
		{
			// Outer ring
			// i				i + 1
			// x----------------x
			// |                |
			// |                |
			// x----------------x
			// Resolution + i	Resolution + i + 1
			// Inner ring

			int i_next = (i + 1) % Resolution;

			Indicies.Add(i);
			Indicies.Add(Resolution + i);
			Indicies.Add(Resolution + i_next);

			Indicies.Add(i);
			Indicies.Add(Resolution + i_next);
			Indicies.Add(i_next % Resolution);
		}

		FSlateDrawElement::MakeCustomVerts(Context.OutDrawElements, Context.MaxLayer, ResourceHndl, Verts, Indicies, nullptr, 0, 1);
	});
}

void UMMPaintableWidget::DrawCircle(FVector2D Position, float Radius, FLinearColor Tint)
{
	ConditionalStartDraws();

	Draws.Add([Position, Radius, Tint](FPaintContext& Context)
	{
		Context.MaxLayer++;

		static FSlateColorBrush Brush = FSlateColorBrush(FColorList::White);
		FSlateResourceHandle ResourceHndl = FSlateApplication::Get().GetRenderer()->GetResourceHandle(Brush);
		auto Transform = Context.AllottedGeometry.ToPaintGeometry().GetAccumulatedRenderTransform();

		const int Resolution = 24;
		FColor Color = Tint.ToFColor(true);

		TArray<FSlateVertex> Verts;
		float Step = (PI * 2.f) / Resolution;

		for(int i = 0; i < Resolution; ++i)
		{
			float Angle = Step * i;

			FVector2D ScreenPos = Position + FVector2D(FMath::Sin(Angle), FMath::Cos(Angle)) * Radius;
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, FVector2f(ScreenPos), FVector2f(0.f), Color));
		}

		TArray<SlateIndex> Indicies;
		for(int i = 0; i < Resolution - 2; ++i)
		{
			Indicies.Add(0);
			Indicies.Add(i + 1);
			Indicies.Add(i + 2);
		}

		FSlateDrawElement::MakeCustomVerts(Context.OutDrawElements, Context.MaxLayer, ResourceHndl, Verts, Indicies, nullptr, 0, 1);
	});
}

int32 UMMPaintableWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	FPaintContext Context(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (ContainingWidget != nullptr && ContainingWidget->DrawCount == LastMMWidgetDrawCount)
	{
		for (auto& Draw : Draws)
			Draw(Context);
	}

	return FMath::Max(LayerId, Context.MaxLayer);
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_EmmsPaintableWidget((int32)FAngelscriptBinds::EOrder::Late + 250, []
{
	auto mmUMMPaintableWidget_ = FAngelscriptBinds::ExistingClass("mm<UMMPaintableWidget>");

	mmUMMPaintableWidget_.Method("void DrawBox(const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color) const",
	[](FEmmsWidgetHandle* Handle, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color)
	{
		if (Handle->Element == nullptr)
			return;
		if (UMMPaintableWidget* PaintableWidget = Cast<UMMPaintableWidget>(Handle->Element->UMGWidget))
			PaintableWidget->DrawBox(Position, Size, nullptr, Color);
	});

	mmUMMPaintableWidget_.Method("void DrawBox(const FVector2D& Position, const FVector2D& Size, const FName& BrushName, const FLinearColor& Color = FLinearColor::White) const",
	[](FEmmsWidgetHandle* Handle, const FVector2D& Position, const FVector2D& Size, const FName& BrushName, const FLinearColor& Color)
	{
		if (Handle->Element == nullptr)
			return;
		if (UMMPaintableWidget* PaintableWidget = Cast<UMMPaintableWidget>(Handle->Element->UMGWidget))
			PaintableWidget->DrawBox(Position, Size, FAppStyle::Get().GetOptionalBrush(BrushName), Color);
	});

	mmUMMPaintableWidget_.Method("void DrawBox(const FVector2D& Position, const FVector2D& Size, const FSlateBrush& Brush, const FLinearColor& Color = FLinearColor::White) const",
	[](FEmmsWidgetHandle* Handle, const FVector2D& Position, const FVector2D& Size, const FSlateBrush& Brush, const FLinearColor& Color)
	{
		if (Handle->Element == nullptr)
			return;
		if (UMMPaintableWidget* PaintableWidget = Cast<UMMPaintableWidget>(Handle->Element->UMGWidget))
			PaintableWidget->DrawBox(Position, Size, &Brush, Color);
	});

	mmUMMPaintableWidget_.Method("void DrawLine(const FVector2D& PositionA, const FVector2D& PositionB, const FLinearColor& Color, float32 Thickness = 1.0, bool bAntiAlias = true) const",
	[](FEmmsWidgetHandle* Handle, const FVector2D& PositionA, const FVector2D& PositionB, const FLinearColor& Color, float Thickness, bool bAntiAlias)
	{
		if (Handle->Element == nullptr)
			return;
		if (UMMPaintableWidget* PaintableWidget = Cast<UMMPaintableWidget>(Handle->Element->UMGWidget))
			PaintableWidget->DrawLines(TArray<FVector2D>{PositionA, PositionB}, Color, bAntiAlias, Thickness);
	});

	mmUMMPaintableWidget_.Method("void DrawLines(const TArray<FVector2D>& Points, const FLinearColor& Color, float32 Thickness = 1.0, bool bAntiAlias = true) const",
	[](FEmmsWidgetHandle* Handle, const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness, bool bAntiAlias)
	{
		if (Handle->Element == nullptr)
			return;
		if (UMMPaintableWidget* PaintableWidget = Cast<UMMPaintableWidget>(Handle->Element->UMGWidget))
			PaintableWidget->DrawLines(Points, Color, bAntiAlias, Thickness);
	});

	mmUMMPaintableWidget_.Method("void DrawText(const FString& Text, const FVector2D& Position, float32 TextSize = 10, const FLinearColor& Color = FLinearColor::White, bool bBold = false, const FVector2D& Alignment = FVector2D(0, 0)) const",
	[](FEmmsWidgetHandle* Handle, const FString& Text, const FVector2D& Position, float TextSize, const FLinearColor& Color, bool bBold, const FVector2D& Alignment)
	{
		if (Handle->Element == nullptr)
			return;
		if (UMMPaintableWidget* PaintableWidget = Cast<UMMPaintableWidget>(Handle->Element->UMGWidget))
		{
			FSlateFontInfo Font = UEmmsDefaultWidgetStyles::GetDefaultFont();
			Font.Size = TextSize;
			if (bBold)
				Font.TypefaceFontName = NAME_Paintable_Bold;
			PaintableWidget->DrawText(Text, Position, Font, Color, Alignment);
		}
	});

	mmUMMPaintableWidget_.Method("void DrawText(const FString& Text, const FVector2D& Position, const FSlateFontInfo& Font, const FLinearColor& Color = FLinearColor::White, const FVector2D& Alignment = FVector2D(0, 0)) const",
	[](FEmmsWidgetHandle* Handle, const FString& Text, const FVector2D& Position, const FSlateFontInfo& Font, const FLinearColor& Color, const FVector2D& Alignment)
	{
		if (Handle->Element == nullptr)
			return;
		if (UMMPaintableWidget* PaintableWidget = Cast<UMMPaintableWidget>(Handle->Element->UMGWidget))
			PaintableWidget->DrawText(Text, Position, Font, Color, Alignment);
	});

	mmUMMPaintableWidget_.Method("void DrawCircle(const FVector2D& Position, float32 Radius, const FLinearColor& Color = FLinearColor::White) const",
	[](FEmmsWidgetHandle* Handle, const FVector2D& Position, float Radius, const FLinearColor& Color)
	{
		if (Handle->Element == nullptr)
			return;
		if (UMMPaintableWidget* PaintableWidget = Cast<UMMPaintableWidget>(Handle->Element->UMGWidget))
			PaintableWidget->DrawCircle(Position, Radius, Color);
	});

	mmUMMPaintableWidget_.Method("void DrawRing(const FVector2D& Position, float32 Radius, const FLinearColor& Color = FLinearColor::White, float32 Thickness = 1.0) const",
	[](FEmmsWidgetHandle* Handle, const FVector2D& Position, float Radius, const FLinearColor& Color, float Thickness)
	{
		if (Handle->Element == nullptr)
			return;
		if (UMMPaintableWidget* PaintableWidget = Cast<UMMPaintableWidget>(Handle->Element->UMGWidget))
			PaintableWidget->DrawRing(Position, Radius, Color, Thickness);
	});

	{
		FAngelscriptBinds::FNamespace ns("mm");
		FAngelscriptBinds::BindGlobalFunction("mm<UMMPaintableWidget> Paint()", []()
		{
			return UEmmsStatics::AddWidget(UMMPaintableWidget::StaticClass());
		});
	}
});
