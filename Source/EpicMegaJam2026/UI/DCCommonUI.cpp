#include "UI/DCCommonUI.h"
#include "CommonUITypes.h"
#include "Core/DCAudioManager.h"
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
#include "Components/WrapBox.h"
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
UDCText_Title::UDCText_Title() { SetText(this, true, 32, DCPalette::Yellow, true); }
UDCText_Heading::UDCText_Heading() { SetText(this, true, 16, DCPalette::Beige); }
UDCText_Body::UDCText_Body() { SetText(this, false, 32, DCPalette::Grey); }
UDCText_Accent::UDCText_Accent() { SetText(this, false, 32, DCPalette::Lime); }
UDCText_Caution::UDCText_Caution() { SetText(this, false, 32, DCPalette::Yellow); }
UDCText_Warn::UDCText_Warn() { SetText(this, false, 32, DCPalette::Red); }
UDCText_SkillFunny::UDCText_SkillFunny() { SetText(this, false, 32, DCPalette::SkillFunny); }
UDCText_SkillScary::UDCText_SkillScary() { SetText(this, false, 32, DCPalette::SkillScary); }
UDCText_SkillBrave::UDCText_SkillBrave() { SetText(this, false, 32, DCPalette::SkillBrave); }
UDCText_SkillThoughtful::UDCText_SkillThoughtful() { SetText(this, false, 32, DCPalette::SkillThoughtful); }
UDCText_SkillRomantic::UDCText_SkillRomantic() { SetText(this, false, 32, DCPalette::SkillRomantic); }
UDCText_Stars::UDCText_Stars() { SetText(this, true, 16, DCPalette::Yellow); }
UDCText_Button::UDCText_Button() { SetText(this, true, 16, DCPalette::Beige); }
UDCText_MenuButton::UDCText_MenuButton() { SetText(this, true, 16, FLinearColor::White, true); }
UDCText_MenuButtonDisabled::UDCText_MenuButtonDisabled() { SetText(this, true, 16, FLinearColor(1.f, 1.f, 1.f, 0.45f), true); }
UDCText_Tab::UDCText_Tab() { SetText(this, true, 10, DCPalette::Beige); }

// Frames are 10px thick (panel) and 8px (plate): content padding must be at least that
UDCBorder_Panel::UDCBorder_Panel()
{
	Background = SliceBrush(TEXT("T_UI_Panel"), FVector2D(48.f, 48.f), FMargin(20.f), FLinearColor::White, Box(DCPalette::Navy.CopyWithNewOpacity(0.94f), DCPalette::Green, 3.f, 6.f));
}
// Slanted ends don't stretch vertically well: the plate is sized for one line of title text (see DCUI::HeaderPadding)
UDCBorder_Header::UDCBorder_Header()
{
	Background = SliceBrush(TEXT("T_UI_Header"), FVector2D(100.f, 66.f), FMargin(50.f, 0.f, 34.f, 0.f), FLinearColor::White, Box(DCPalette::Blue, DCPalette::Green, 3.f));
}
UDCBorder_Inset::UDCBorder_Inset() { Background = Plate(FMath::Lerp(DCPalette::NavyLight, DCPalette::Blue, 0.2f), Box(DCPalette::NavyLight.CopyWithNewOpacity(0.6f), DCPalette::Blue)); }
UDCBorder_Transparent::UDCBorder_Transparent() { Background = Box(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f); }
UDCBorder_KeyCap::UDCBorder_KeyCap() { Background = Box(DCPalette::Navy, DCPalette::Grey, 2.f, 3.f); }
UDCBorder_LockShackle::UDCBorder_LockShackle() { Background = Box(FLinearColor::Transparent, DCPalette::Yellow, 3.f, 6.f); }
UDCBorder_LockBody::UDCBorder_LockBody() { Background = Box(DCPalette::Yellow, DCPalette::Yellow, 2.f, 3.f); }

UDCButton_Primary::UDCButton_Primary()
{
	SetButton(this, Plate(FMath::Lerp(DCPalette::Green, DCPalette::Lime, 0.3f), Box(DCPalette::Green, DCPalette::Lime)),
		Plate(FMath::Lerp(DCPalette::Green, DCPalette::Lime, 0.6f), Box(FMath::Lerp(DCPalette::Green, DCPalette::Lime, 0.35f), DCPalette::Beige)),
		Plate(DCPalette::Green, Box(FMath::Lerp(DCPalette::Green, DCPalette::Navy, 0.3f), DCPalette::Lime)),
		Plate(FMath::Lerp(DCPalette::NavyLight, DCPalette::Grey, 0.25f), Box(DCPalette::NavyLight, DCPalette::Grey.CopyWithNewOpacity(0.3f))),
		UDCText_Button::StaticClass(), FMargin(18.f, 12.f));
	MinHeight = 30; // the plate's 9-slice corners need 28px
}

UDCButton_Menu::UDCButton_Menu()
{
	SetButton(this,
		Box(FLinearColor(1.f, 1.f, 1.f, 0.50f), FLinearColor(1.f, 1.f, 1.f, 0.65f), 1.5f, 4.f),
		Box(FLinearColor(1.f, 1.f, 1.f, 0.70f), FLinearColor(1.f, 1.f, 1.f, 0.85f), 1.5f, 4.f),
		Box(FLinearColor(1.f, 1.f, 1.f, 0.80f), FLinearColor::White, 1.5f, 4.f),
		Box(FLinearColor(1.f, 1.f, 1.f, 0.22f), FLinearColor(1.f, 1.f, 1.f, 0.30f), 1.5f, 4.f),
		UDCText_MenuButton::StaticClass(), FMargin(18.f, 12.f));
	DisabledTextStyle = UDCText_MenuButtonDisabled::StaticClass();
	MinHeight = 30;
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
	Add("Back", TEXT("Back"), EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Right, TEXT("SPACE"));
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

void UDCButton::NativeOnHovered()
{
	Super::NativeOnHovered();
	ADCAudioManager::PlayCueStatic(this, TEXT("MenuHover"));
}

void UDCButton::NativeOnClicked()
{
	Super::NativeOnClicked();
	ADCAudioManager::PlayCueStatic(this, TEXT("MenuClick"));
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
	UCommonBorder* Plate = DCUI::Border(WidgetTree, nullptr, UDCBorder_Inset::StaticClass(), FMargin(10.f, 6.f)); // dark plate behind each hint
	WidgetTree->RootWidget = Plate;
	UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
	Plate->SetContent(H);
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
		// The border sits in a stack so Header() can put its plate above the border
		UVerticalBox* Stack = Tree->ConstructWidget<UVerticalBox>();
		Stack->AddChildToVerticalBox(B);
		UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Stack);
		S->SetAnchors(Anchors);
		S->SetAlignment(Alignment);
		// Centered screens sit a bit above the middle so they don't run under the key-hint bar at the bottom (UDCRootLayout)
		const bool bCentered = Anchors.Minimum.Equals(FVector2D(0.5f, 0.5f));
		S->SetPosition(bCentered ? Position - FVector2D(0.f, 50.f) : Position);
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
		// Parent is the panel's content box: its parent is the border, whose parent is the stack made by Panel()
		UVerticalBox* Stack = Parent->GetParent() ? Cast<UVerticalBox>(Parent->GetParent()->GetParent()) : nullptr;
		check(Stack);
		UCommonBorder* Plate = Border(Tree, nullptr, UDCBorder_Header::StaticClass(), HeaderPadding);
		Plate->SetContent(Text(Tree, nullptr, Title, UDCText_Title::StaticClass()));
		Stack->InsertChildAt(0, Plate);
		if (UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(Plate->Slot))
		{
			S->SetHorizontalAlignment(HAlign_Left);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
		if (!Subtitle.IsEmpty())
		{
			Text(Tree, Parent, Subtitle)->SetMargin(FMargin(4.f, 0.f, 0.f, 12.f));
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
		return FString::Printf(TEXT("★ %d"), FMath::Clamp(Rating, 0, 5)); // one star + the number, e.g. "★ 4"
	}

	void SkillTooltip(UWidgetTree* Tree, UWidget* Target, const FSkillVector& Skills)
	{
		UCommonBorder* Box = Border(Tree, nullptr, UDCBorder_Panel::StaticClass(), FMargin(12.f));
		UVerticalBox* V = Tree->ConstructWidget<UVerticalBox>();
		Box->SetContent(V);
		Text(Tree, V, TEXT("SKILLS"), UDCText_Heading::StaticClass());
		SkillList(Tree, V, Skills, 0, true);
		if (V->GetChildrenCount() == 1)
		{
			Text(Tree, V, TEXT("None"));
		}
		Target->SetToolTip(Box);
	}

	FString CleanTitle(const FText& Title)
	{
		return Title.ToString().Replace(TEXT("★"), TEXT("")).Replace(TEXT("  "), TEXT(" ")).TrimStartAndEnd();
	}

	TSubclassOf<UCommonTextStyle> SkillStyle(int32 SkillIndex)
	{
		switch ((ESkill)SkillIndex)
		{
		case ESkill::Funny: return UDCText_SkillFunny::StaticClass();
		case ESkill::Scary: return UDCText_SkillScary::StaticClass();
		case ESkill::Brave: return UDCText_SkillBrave::StaticClass();
		case ESkill::Thoughtful: return UDCText_SkillThoughtful::StaticClass();
		default: return UDCText_SkillRomantic::StaticClass();
		}
	}

	void OrderLines(UWidgetTree* Tree, UPanelWidget* Box, const FClientRuntime& Client, float Wrap)
	{
		Box->ClearChildren();
		if (Client.OrderLines.IsEmpty()) // scripted or legacy orders without per-skill lines
		{
			Text(Tree, Box, Client.DisplayName.IsEmpty() ? Client.OrderText.ToString() : FString::Printf(TEXT("%s: %s"), *Client.DisplayName.ToString(), *Client.OrderText.ToString()))->SetWrapTextAt(Wrap);
			return;
		}
		for (const FOrderLine& Line : Client.OrderLines)
		{
			const bool bGreeting = Line.Skill < 0;
			const FString Content = bGreeting && !Client.DisplayName.IsEmpty()
				? FString::Printf(TEXT("%s: %s"), *Client.DisplayName.ToString(), *Line.Text.ToString()) : Line.Text.ToString();
			// Hint colors by skill are switched off (grey Scary read as disabled); to bring them back use SkillStyle(Line.Skill) for the hints
			// Text(Tree, Box, Content, bGreeting ? TSubclassOf<UCommonTextStyle>(UDCText_Body::StaticClass()) : SkillStyle(Line.Skill))->SetWrapTextAt(Wrap);
			Text(Tree, Box, Content, UDCText_Body::StaticClass())->SetWrapTextAt(Wrap);
		}
	}

	void SkillList(UWidgetTree* Tree, UPanelWidget* Parent, const FSkillVector& Skills, int32 Cap, bool bHideZero)
	{
		for (int32 i = 0; i < DCNumSkills; ++i)
		{
			const int32 Value = Skills.Get((ESkill)i);
			if (!bHideZero || Value > 0)
			{
				const FString Name = StaticEnum<ESkill>()->GetNameStringByValue(i);
				Text(Tree, Parent, Cap > 0 ? FString::Printf(TEXT("%s %d/%d"), *Name, Value, Cap) : FString::Printf(TEXT("%s %d"), *Name, Value), SkillStyle(i));
			}
		}
	}

	void InlineSkills(UWidgetTree* Tree, UPanelWidget* Parent, const FSkillVector& Skills)
	{
		UWrapBox* Row = Tree->ConstructWidget<UWrapBox>();
		Row->SetInnerSlotPadding(FVector2D(12.f, 0.f));
		for (int32 i = 0; i < DCNumSkills; ++i)
		{
			const int32 Value = Skills.Get((ESkill)i);
			if (Value > 0)
			{
				const FString Name = StaticEnum<ESkill>()->GetNameStringByValue(i);
				Text(Tree, Row, FString::Printf(TEXT("%s %d"), *Name, Value), SkillStyle(i));
			}
		}
		if (Row->GetChildrenCount() > 0 && Parent)
		{
			Parent->AddChild(Row);
		}
	}
}
