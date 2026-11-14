#include "UI/DCCommonUI.h"
#include "CommonUITypes.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Data/DCTypes.h"

using DCUI::Box;

namespace
{
	// Pixel fonts imported from SourceArt/Fonts (OFL). The engine font stands in until they exist (styles are read once at startup).
	FSlateFontInfo PixelFont(bool bDisplay, int32 Size)
	{
		const UObject* Font = LoadObject<UObject>(nullptr, bDisplay
			? TEXT("/Game/DirectorsCut/UI/Fonts/PressStart2P_Font.PressStart2P_Font")
			: TEXT("/Game/DirectorsCut/UI/Fonts/VT323_Font.VT323_Font"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		return FSlateFontInfo(Font ? Font : LoadObject<UObject>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")), Size);
	}

	void SetText(UCommonTextStyle* S, bool bDisplay, int32 Size, const FLinearColor& Color, bool bShadow = false)
	{
		S->Font = PixelFont(bDisplay, Size);
		S->Color = Color;
		S->bUsesDropShadow = bShadow;
		S->ShadowOffset = FVector2D(2.f, 2.f);
		S->ShadowColor = FLinearColor(0.f, 0.f, 0.f, 0.8f);
	}

	// 9-slice texture made by SourceArt/UI/gen_ui_textures.js (size and margins in texture pixels must match it).
	// Fallback stands in until the texture is imported.
	FSlateBrush SliceBrush(const TCHAR* Name, FVector2D Size, FMargin MarginPx, const FLinearColor& Tint, const FSlateBrush& Fallback)
	{
		UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/DirectorsCut/UI/Textures/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Tex)
		{
			return Fallback;
		}
		FSlateBrush Brush;
		Brush.SetResourceObject(Tex);
		Brush.ImageSize = Size;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = FMargin(MarginPx.Left / Size.X, MarginPx.Top / Size.Y, MarginPx.Right / Size.X, MarginPx.Bottom / Size.Y);
		Brush.TintColor = Tint;
		return Brush;
	}

	// Greyscale chamfered plate: the tint is the border color, the fill comes out a darker shade of it
	FSlateBrush Plate(const FLinearColor& Tint, const FSlateBrush& Fallback)
	{
		return SliceBrush(TEXT("T_UI_Plate"), FVector2D(36.f, 36.f), FMargin(14.f), Tint, Fallback);
	}

	void SetButton(UCommonButtonStyle* S, const FSlateBrush& Normal, const FSlateBrush& Hovered, const FSlateBrush& Selected, const FSlateBrush& Disabled,
		TSubclassOf<UCommonTextStyle> Text, FMargin Padding)
	{
		S->NormalBase = Normal;
		S->NormalHovered = Hovered;
		S->NormalPressed = Selected;
		S->SelectedBase = S->SelectedHovered = S->SelectedPressed = Selected;
		S->Disabled = Disabled;
		S->NormalTextStyle = S->NormalHoveredTextStyle = S->SelectedTextStyle = S->SelectedHoveredTextStyle = S->DisabledTextStyle = Text;
		S->ButtonPadding = Padding;
	}
}

// ---------------------------------------------------------------- Styles

// Sizes are pixels (FontDPI=72 in DefaultEngine.ini); Press Start 2P stays crisp on multiples of 8
UDCText_Title::UDCText_Title() { SetText(this, true, 24, DCPalette::Beige, true); }
UDCText_Heading::UDCText_Heading() { SetText(this, true, 16, DCPalette::Beige); }
UDCText_Body::UDCText_Body() { SetText(this, false, 28, DCPalette::Grey); }
UDCText_Accent::UDCText_Accent() { SetText(this, false, 28, DCPalette::Lime); }
UDCText_Stars::UDCText_Stars() { SetText(this, true, 16, DCPalette::Lime); }
UDCText_Button::UDCText_Button() { SetText(this, true, 12, DCPalette::Beige); }
UDCText_Tab::UDCText_Tab() { SetText(this, true, 8, DCPalette::Beige); }

// Frames are 10px thick (panel) and 8px (plate): content padding must be at least that
UDCBorder_Panel::UDCBorder_Panel()
{
	Background = SliceBrush(TEXT("T_UI_Panel"), FVector2D(48.f, 48.f), FMargin(20.f), FLinearColor::White, Box(DCPalette::Navy.CopyWithNewOpacity(0.94f), DCPalette::Green, 3.f, 6.f));
}
// Slanted ends don't stretch vertically well: the plate is sized for one line of title text (see DCUI::HeaderPadding)
UDCBorder_Header::UDCBorder_Header()
{
	Background = SliceBrush(TEXT("T_UI_Header"), FVector2D(88.f, 52.f), FMargin(46.f, 0.f, 28.f, 0.f), FLinearColor::White, Box(DCPalette::Blue, DCPalette::Green, 3.f));
}
UDCBorder_Inset::UDCBorder_Inset() { Background = Plate(FMath::Lerp(DCPalette::NavyLight, DCPalette::Blue, 0.5f), Box(DCPalette::NavyLight.CopyWithNewOpacity(0.6f), DCPalette::Blue)); }
UDCBorder_KeyCap::UDCBorder_KeyCap() { Background = Box(DCPalette::Navy, DCPalette::Grey, 2.f, 3.f); }

UDCButton_Primary::UDCButton_Primary()
{
	SetButton(this, Plate(FMath::Lerp(DCPalette::Green, DCPalette::Lime, 0.3f), Box(DCPalette::Green, DCPalette::Lime)),
		Plate(FMath::Lerp(DCPalette::Green, DCPalette::Lime, 0.6f), Box(FMath::Lerp(DCPalette::Green, DCPalette::Lime, 0.35f), DCPalette::Beige)),
		Plate(DCPalette::Green, Box(FMath::Lerp(DCPalette::Green, DCPalette::Navy, 0.3f), DCPalette::Lime)),
		Plate(FMath::Lerp(DCPalette::NavyLight, DCPalette::Grey, 0.25f), Box(DCPalette::NavyLight, DCPalette::Grey.CopyWithNewOpacity(0.3f))),
		UDCText_Button::StaticClass(), FMargin(18.f, 12.f));
	MinHeight = 30; // the plate's 9-slice corners need 28px
}

UDCButton_Tab::UDCButton_Tab()
{
	SetButton(this, Plate(FMath::Lerp(DCPalette::Blue, DCPalette::Grey, 0.25f), Box(DCPalette::Blue, FMath::Lerp(DCPalette::Blue, DCPalette::Grey, 0.3f), 2.f, 3.f)),
		Plate(FMath::Lerp(DCPalette::Blue, DCPalette::Grey, 0.45f), Box(FMath::Lerp(DCPalette::Blue, DCPalette::Grey, 0.2f), DCPalette::Lime, 2.f, 3.f)),
		Plate(FMath::Lerp(DCPalette::Green, DCPalette::Lime, 0.5f), Box(DCPalette::Green, DCPalette::Lime, 2.f, 3.f)),
		Plate(DCPalette::NavyLight, Box(DCPalette::NavyLight, FLinearColor::Transparent)), UDCText_Tab::StaticClass(), FMargin(10.f, 9.f));
	MinHeight = 30;
}

UDCButton_Row::UDCButton_Row()
{
	SetButton(this, Box(FLinearColor::Transparent, FLinearColor::Transparent, 0.f), Box(DCPalette::NavyLight, DCPalette::Blue), Box(DCPalette::Green.CopyWithNewOpacity(0.35f), DCPalette::Lime),
		Box(FLinearColor::Transparent, FLinearColor::Transparent, 0.f), UDCText_Body::StaticClass(), FMargin(10.f, 4.f));
}

// ---------------------------------------------------------------- Input

UDCUIInputData::UDCUIInputData()
{
	Actions = CreateDefaultSubobject<UDataTable>(TEXT("Actions"));
	Actions->RowStruct = FCommonInputActionDataBase::StaticStruct();
	Add("Confirm", TEXT("Select"), EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom, TEXT("ENTER"));
	Add("Back", TEXT("Back"), EKeys::Escape, EKeys::Gamepad_FaceButton_Right, TEXT("ESC"));
	Add("PrevTab", TEXT("Previous category"), EKeys::Left, EKeys::Gamepad_LeftShoulder, TEXT("◀"));
	// Its hint stands for both directions: PrevTab is registered hidden from the bar
	Add("NextTab", TEXT("Change category"), EKeys::Right, EKeys::Gamepad_RightShoulder, TEXT("◀ ▶"));
	DefaultClickAction.DataTable = DefaultBackAction.DataTable = Actions;
	DefaultClickAction.RowName = "Confirm";
	DefaultBackAction.RowName = "Back";
}

void UDCUIInputData::Add(FName Name, const TCHAR* DisplayName, const FKey& Key, const FKey& GamepadKey, const TCHAR* Cap)
{
	FCommonInputActionDataBase Row;
	Row.DisplayName = FText::FromString(DisplayName);
	// The per-device key infos are protected; set them through reflection like the data table editor does
	const UScriptStruct* RowType = FCommonInputActionDataBase::StaticStruct();
	FindFProperty<FStructProperty>(RowType, TEXT("KeyboardInputTypeInfo"))->ContainerPtrToValuePtr<FCommonInputTypeInfo>(&Row)->SetKey(Key);
	FindFProperty<FStructProperty>(RowType, TEXT("DefaultGamepadInputTypeInfo"))->ContainerPtrToValuePtr<FCommonInputTypeInfo>(&Row)->SetKey(GamepadKey);
	Actions->AddRow(Name, Row);
	KeyCaps.Add(Name, FText::FromString(Cap));
}

FDataTableRowHandle UDCUIInputData::Action(FName Name)
{
	FDataTableRowHandle Handle;
	Handle.DataTable = GetDefault<UDCUIInputData>()->Actions;
	Handle.RowName = Name;
	return Handle;
}

FText UDCUIInputData::KeyCap(FName Name)
{
	// The router names table actions "Legacy_<Table>_<Row>"; row names have no underscore
	FString Row = Name.ToString();
	int32 Split;
	if (Row.FindLastChar(TEXT('_'), Split))
	{
		Row.RightChopInline(Split + 1);
	}
	return GetDefault<UDCUIInputData>()->KeyCaps.FindRef(FName(*Row));
}

// ---------------------------------------------------------------- Button

void UDCButton::NativeOnInitialized()
{
	Super::NativeOnInitialized(); // runs before UCommonButtonBase wraps the root in its internal button
	Content = WidgetTree->ConstructWidget<UVerticalBox>();
	WidgetTree->RootWidget = Content;
	Label = WidgetTree->ConstructWidget<UCommonTextBlock>();
	Content->AddChildToVerticalBox(Label)->SetHorizontalAlignment(HAlign_Center);
}

void UDCButton::NativeOnClicked()
{
	Super::NativeOnClicked();
	OnPayloadClicked.Broadcast(Payload);
}

void UDCButton::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();
	if (Label)
	{
		Label->SetStyle(GetCurrentTextStyleClass());
	}
}

// ---------------------------------------------------------------- Action bar

void UDCActionButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetStyle(UDCButton_Row::StaticClass()); // applied once the base class has built its internal button
	UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
	WidgetTree->RootWidget = H;
	UCommonBorder* Cap = DCUI::Border(WidgetTree, H, UDCBorder_KeyCap::StaticClass(), FMargin(8.f, 4.f));
	Cast<UHorizontalBoxSlot>(Cap->Slot)->SetVerticalAlignment(VAlign_Center);
	KeyText = WidgetTree->ConstructWidget<UCommonTextBlock>();
	KeyText->SetStyle(UDCText_Heading::StaticClass());
	Cap->SetContent(KeyText);
	Text_ActionName = WidgetTree->ConstructWidget<UCommonTextBlock>();
	UHorizontalBoxSlot* NameSlot = H->AddChildToHorizontalBox(Text_ActionName);
	NameSlot->SetPadding(FMargin(10.f, 0.f, 4.f, 0.f));
	NameSlot->SetVerticalAlignment(VAlign_Center);
}

void UDCActionButton::SetRepresentedAction(FUIActionBindingHandle InBindingHandle)
{
	Super::SetRepresentedAction(InBindingHandle);
	// The base class only fills the name when it has an icon widget; this button shows a text key cap instead
	Text_ActionName->SetText(InBindingHandle.GetDisplayName());
	KeyText->SetText(UDCUIInputData::KeyCap(InBindingHandle.GetActionName()));
}

// ---------------------------------------------------------------- Screen and root layout

UDCScreen::UDCScreen(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	bIsBackActionDisplayedInActionBar = true;
}

TOptional<FUIInputConfig> UDCScreen::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UDCRootLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	Stack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>();
	Stack->SetTransitionDuration(0.f); // instant: lets the player controller swap screens with ClearWidgets + AddWidget in one call
	UOverlaySlot* StackSlot = Root->AddChildToOverlay(Stack);
	StackSlot->SetHorizontalAlignment(HAlign_Fill);
	StackSlot->SetVerticalAlignment(VAlign_Fill);
	// Key hints for the active screen (Back, Change category...). ActionButtonClass is private with no setter: set it like the details panel does
	UCommonBoundActionBar* Bar = WidgetTree->ConstructWidget<UCommonBoundActionBar>();
	CastFieldChecked<FClassProperty>(UCommonBoundActionBar::StaticClass()->FindPropertyByName(TEXT("ActionButtonClass")))->SetObjectPropertyValue_InContainer(Bar, UDCActionButton::StaticClass());
	Bar->SetEntrySpacing(FVector2D(16.f, 0.f));
	UOverlaySlot* BarSlot = Root->AddChildToOverlay(Bar);
	BarSlot->SetHorizontalAlignment(HAlign_Left);
	BarSlot->SetVerticalAlignment(VAlign_Bottom);
	BarSlot->SetPadding(FMargin(48.f, 0.f, 0.f, 36.f));
}

// ---------------------------------------------------------------- Helpers

namespace DCUI
{
	UCommonTextBlock* Text(UWidgetTree* Tree, UPanelWidget* Parent, const FString& Text, TSubclassOf<UCommonTextStyle> Style)
	{
		UCommonTextBlock* T = Tree->ConstructWidget<UCommonTextBlock>();
		T->SetStyle(Style);
		T->SetText(FText::FromString(Text));
		if (Parent)
		{
			Parent->AddChild(T);
		}
		return T;
	}

	UCommonBorder* Border(UWidgetTree* Tree, UPanelWidget* Parent, TSubclassOf<UCommonBorderStyle> Style, FMargin Padding)
	{
		UCommonBorder* B = Tree->ConstructWidget<UCommonBorder>();
		B->SetStyle(Style);
		B->SetPadding(Padding);
		if (Parent)
		{
			Parent->AddChild(B);
		}
		return B;
	}

	UDCButton* Button(UWidgetTree* Tree, UPanelWidget* Parent, const FString& Label, FName Payload, TSubclassOf<UCommonButtonStyle> Style)
	{
		UDCButton* Btn = Tree->ConstructWidget<UDCButton>();
		Btn->Payload = Payload;
		Btn->SetStyle(Style);
		Btn->Label->SetText(FText::FromString(Label));
		if (Parent)
		{
			Parent->AddChild(Btn);
		}
		return Btn;
	}

	UVerticalBox* Panel(UWidgetTree* Tree, FAnchors Anchors, FVector2D Alignment, FVector2D Position, UCommonBorder** OutPanel, TSubclassOf<UCommonBorderStyle> Style)
	{
		UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>();
		Tree->RootWidget = Canvas;
		UCommonBorder* B = Border(Tree, nullptr, Style, FMargin(20.f, 16.f));
		UCanvasPanelSlot* S = Canvas->AddChildToCanvas(B);
		S->SetAnchors(Anchors);
		S->SetAlignment(Alignment);
		S->SetPosition(Position);
		S->SetAutoSize(true);
		UVerticalBox* V = Tree->ConstructWidget<UVerticalBox>();
		B->SetContent(V);
		if (OutPanel)
		{
			*OutPanel = B;
		}
		return V;
	}

	void Header(UWidgetTree* Tree, UPanelWidget* Parent, const FString& Title, const FString& Subtitle)
	{
		UCommonBorder* Plate = Border(Tree, Parent, UDCBorder_Header::StaticClass(), HeaderPadding);
		Plate->SetContent(Text(Tree, nullptr, Title, UDCText_Title::StaticClass()));
		if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(Plate->Slot))
		{
			S->SetHorizontalAlignment(HAlign_Left);
		}
		if (!Subtitle.IsEmpty())
		{
			Text(Tree, Parent, Subtitle)->SetMargin(FMargin(4.f, 6.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(Parent->GetChildAt(Parent->GetChildrenCount() - 1)->Slot))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f)); // gap under the header block
		}
	}

	FSlateBrush Box(const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth, float Radius)
	{
		return FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth);
	}

	UProgressBar* Bar(UWidgetTree* Tree, UPanelWidget* Parent, float Width)
	{
		USizeBox* Size = Tree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(Width);
		Size->SetHeightOverride(14.f);
		UProgressBar* B = Tree->ConstructWidget<UProgressBar>();
		FProgressBarStyle S;
		S.SetBackgroundImage(Box(DCPalette::Navy, DCPalette::Blue, 2.f, 3.f));
		S.SetFillImage(Box(DCPalette::Lime, FLinearColor::Transparent, 0.f, 3.f));
		B->SetWidgetStyle(S);
		Size->AddChild(B);
		Parent->AddChild(Size);
		return B;
	}

	FString Stars(int32 Rating)
	{
		const int32 R = FMath::Clamp(Rating, 0, 5);
		return FString::ChrN(R, TEXT('★')) + FString::ChrN(5 - R, TEXT('☆'));
	}

	void SkillStrip(UWidgetTree* Tree, UPanelWidget* Parent, const FSkillVector& Skills)
	{
		static const TCHAR* Letters = TEXT("FSBTR");
		UHorizontalBox* Strip = Tree->ConstructWidget<UHorizontalBox>();
		Parent->AddChild(Strip);
		for (int32 i = 0; i < DCNumSkills; ++i)
		{
			const int32 Value = Skills.Get((ESkill)i);
			UCommonTextBlock* Label = Text(Tree, Strip, FString::Printf(TEXT("%c%d"), Letters[i], Value));
			Label->SetRenderOpacity(Value > 0 ? 1.f : 0.3f);
			Cast<UHorizontalBoxSlot>(Label->Slot)->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
		}
	}

	FString SkillLines(const FSkillVector& Skills, int32 Cap)
	{
		FString Out;
		for (int32 i = 0; i < DCNumSkills; ++i)
		{
			const FString Name = StaticEnum<ESkill>()->GetNameStringByValue(i);
			Out += Cap > 0 ? FString::Printf(TEXT("%s %d/%d\n"), *Name, Skills.Get((ESkill)i), Cap) : FString::Printf(TEXT("%s %d\n"), *Name, Skills.Get((ESkill)i));
		}
		return Out.TrimEnd();
	}
}
