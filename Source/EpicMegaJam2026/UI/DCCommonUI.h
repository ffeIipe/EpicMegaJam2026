// CommonUI building blocks, all in C++: art palette, text/border/button styles, UI input actions,
// the base button, the key-hint action bar, the base screen and the root layout that stacks screens.
#pragma once

#include "CoreMinimal.h"
#include "CommonTextBlock.h"
#include "CommonBorder.h"
#include "CommonButtonBase.h"
#include "CommonActivatableWidget.h"
#include "CommonInputBaseTypes.h"
#include "Input/CommonBoundActionBar.h"
#include "Input/CommonBoundActionButton.h"
#include "DCCommonUI.generated.h"

class UWidgetTree;
class UPanelWidget;
class UVerticalBox;
class UDataTable;
class UCommonActivatableWidgetStack;
class UProgressBar;
struct FSkillVector;
struct FClientRuntime;

// Saturated retro-store palette (menu mockup). Names are roles: Grey = body text, Beige = headings and buttons, Lime = accents.
// SourceArt/UI/gen_ui_textures.js bakes the same values into the panel and title textures: change both together.
namespace DCPalette
{
	inline const FLinearColor Green(FColor(0x2B, 0xC4, 0x68));
	inline const FLinearColor Blue(FColor(0x25, 0x63, 0xEB));
	inline const FLinearColor Lime(FColor(0xB8, 0xF2, 0x4A));
	inline const FLinearColor Yellow(FColor(0xFF, 0xD2, 0x3F));
	inline const FLinearColor Grey(FColor(0xE8, 0xEE, 0xF8));
	inline const FLinearColor Beige(FColor(0xFF, 0xF8, 0xE7));
	inline const FLinearColor Navy(FColor(0x0C, 0x18, 0x38));
	inline const FLinearColor NavyLight(FColor(0x18, 0x30, 0x6E));
	inline const FLinearColor Red(FColor(0xFF, 0x4F, 0x4F));
	// One color per skill, taken from the genre that maps to it (SystemsDesigns.md 4.1): Comedy white, Horror black, Action red, Drama violet, Romance pink.
	// Black can't be read on navy, so Scary is a mid grey.
	inline const FLinearColor SkillFunny(FColor(0xFF, 0xFF, 0xFF));
	inline const FLinearColor SkillScary(FColor(0x8E, 0x93, 0xA6));
	inline const FLinearColor SkillBrave(FColor(0xFF, 0x4F, 0x4F));
	inline const FLinearColor SkillThoughtful(FColor(0xB0, 0x7C, 0xFF));
	inline const FLinearColor SkillRomantic(FColor(0xFF, 0x8A, 0xC8));
}

// ---------------------------------------------------------------- Text styles (Press Start 2P for display, VT323 for body)

UCLASS() class EPICMEGAJAM2026_API UDCText_Title : public UCommonTextStyle { GENERATED_BODY() public: UDCText_Title(); };
UCLASS() class EPICMEGAJAM2026_API UDCText_Heading : public UCommonTextStyle { GENERATED_BODY() public: UDCText_Heading(); };
UCLASS() class EPICMEGAJAM2026_API UDCText_Body : public UCommonTextStyle { GENERATED_BODY() public: UDCText_Body(); };
UCLASS() class EPICMEGAJAM2026_API UDCText_Accent : public UCommonTextStyle { GENERATED_BODY() public: UDCText_Accent(); };
// Match-meter bands next to Accent (Great / Good): yellow for Okay, red for Poor
UCLASS() class EPICMEGAJAM2026_API UDCText_Caution : public UCommonTextStyle { GENERATED_BODY() public: UDCText_Caution(); };
UCLASS() class EPICMEGAJAM2026_API UDCText_Warn : public UCommonTextStyle { GENERATED_BODY() public: UDCText_Warn(); };
// Stars and arrow glyphs only exist in the display font
UCLASS() class EPICMEGAJAM2026_API UDCText_Stars : public UCommonTextStyle { GENERATED_BODY() public: UDCText_Stars(); };
UCLASS() class EPICMEGAJAM2026_API UDCText_Button : public UCommonTextStyle { GENERATED_BODY() public: UDCText_Button(); };
UCLASS() class EPICMEGAJAM2026_API UDCText_MenuButton : public UCommonTextStyle { GENERATED_BODY() public: UDCText_MenuButton(); };
UCLASS() class EPICMEGAJAM2026_API UDCText_MenuButtonDisabled : public UCommonTextStyle { GENERATED_BODY() public: UDCText_MenuButtonDisabled(); };
// One per skill, colored from DCPalette::Skill*; pick with DCUI::SkillStyle
UCLASS() class EPICMEGAJAM2026_API UDCText_SkillFunny : public UCommonTextStyle { GENERATED_BODY() public: UDCText_SkillFunny(); };
UCLASS() class EPICMEGAJAM2026_API UDCText_SkillScary : public UCommonTextStyle { GENERATED_BODY() public: UDCText_SkillScary(); };
UCLASS() class EPICMEGAJAM2026_API UDCText_SkillBrave : public UCommonTextStyle { GENERATED_BODY() public: UDCText_SkillBrave(); };
UCLASS() class EPICMEGAJAM2026_API UDCText_SkillThoughtful : public UCommonTextStyle { GENERATED_BODY() public: UDCText_SkillThoughtful(); };
UCLASS() class EPICMEGAJAM2026_API UDCText_SkillRomantic : public UCommonTextStyle { GENERATED_BODY() public: UDCText_SkillRomantic(); };
// Genre tabs: eight of them have to fit the width of a list
UCLASS() class EPICMEGAJAM2026_API UDCText_Tab : public UCommonTextStyle { GENERATED_BODY() public: UDCText_Tab(); };

// ---------------------------------------------------------------- Border styles

UCLASS() class EPICMEGAJAM2026_API UDCBorder_Panel : public UCommonBorderStyle { GENERATED_BODY() public: UDCBorder_Panel(); };
UCLASS() class EPICMEGAJAM2026_API UDCBorder_Header : public UCommonBorderStyle { GENERATED_BODY() public: UDCBorder_Header(); };
UCLASS() class EPICMEGAJAM2026_API UDCBorder_Inset : public UCommonBorderStyle { GENERATED_BODY() public: UDCBorder_Inset(); };
UCLASS() class EPICMEGAJAM2026_API UDCBorder_Transparent : public UCommonBorderStyle { GENERATED_BODY() public: UDCBorder_Transparent(); };
UCLASS() class EPICMEGAJAM2026_API UDCBorder_KeyCap : public UCommonBorderStyle { GENERATED_BODY() public: UDCBorder_KeyCap(); };
// The two pieces of the padlock icon on a locked slot: hollow shackle, solid body
UCLASS() class EPICMEGAJAM2026_API UDCBorder_LockShackle : public UCommonBorderStyle { GENERATED_BODY() public: UDCBorder_LockShackle(); };
UCLASS() class EPICMEGAJAM2026_API UDCBorder_LockBody : public UCommonBorderStyle { GENERATED_BODY() public: UDCBorder_LockBody(); };

// ---------------------------------------------------------------- Button styles

UCLASS() class EPICMEGAJAM2026_API UDCButton_Primary : public UCommonButtonStyle { GENERATED_BODY() public: UDCButton_Primary(); };
UCLASS() class EPICMEGAJAM2026_API UDCButton_Menu : public UCommonButtonStyle { GENERATED_BODY() public: UDCButton_Menu(); };
UCLASS() class EPICMEGAJAM2026_API UDCButton_Tab : public UCommonButtonStyle { GENERATED_BODY() public: UDCButton_Tab(); };
// List rows, cover tiles and key hints: flat until hovered/selected
UCLASS() class EPICMEGAJAM2026_API UDCButton_Row : public UCommonButtonStyle { GENERATED_BODY() public: UDCButton_Row(); };

// ---------------------------------------------------------------- Input

// CommonUI input actions; Config/DefaultGame.ini points CommonInputSettings.InputData at this class.
// The table is built here so each key sits next to its key-cap label.
UCLASS()
class EPICMEGAJAM2026_API UDCUIInputData : public UCommonUIInputData
{
	GENERATED_BODY()
public:
	UDCUIInputData();
	static FDataTableRowHandle Action(FName Name);
	static FText KeyCap(FName Name);

	UPROPERTY() TObjectPtr<UDataTable> Actions;

private:
	void Add(FName Name, const TCHAR* DisplayName, const FKey& Key, const FKey& GamepadKey, const TCHAR* Cap);
	TMap<FName, FText> KeyCaps;
};

// ---------------------------------------------------------------- Widgets

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPayloadClicked, FName, Payload);

// Code-built CommonUI button: a label in a vertical box (callers may add more content), reports a name when clicked
UCLASS()
class EPICMEGAJAM2026_API UDCButton : public UCommonButtonBase
{
	GENERATED_BODY()
public:
	UPROPERTY() FOnPayloadClicked OnPayloadClicked;
	FName Payload;
	UPROPERTY() TObjectPtr<UVerticalBox> Content;
	UPROPERTY() TObjectPtr<UCommonTextBlock> Label;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnClicked() override;
	virtual void NativeOnCurrentTextStyleChanged() override;
};

// One "[KEY] Action" hint in the action bar; clicking it runs the action
UCLASS()
class EPICMEGAJAM2026_API UDCActionButton : public UCommonBoundActionButton
{
	GENERATED_BODY()
public:
	virtual void SetRepresentedAction(FUIActionBindingHandle InBindingHandle) override;

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY() TObjectPtr<UCommonTextBlock> KeyText;
};

// A screen pushed on the root layout's stack: menu input (cursor, no look/move), Esc goes back unless bIsBackHandler is cleared
UCLASS(Abstract)
class EPICMEGAJAM2026_API UDCScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()
public:
	UDCScreen(const FObjectInitializer& ObjectInitializer);
	// Pause the game while this screen is up (menus, summary, end screens)
	UPROPERTY(EditDefaultsOnly, Category = "Screen") bool bPausesGame = false;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
};

// Added to the viewport once by the player controller: the screen stack plus the key-hint bar for the active screen
UCLASS()
class EPICMEGAJAM2026_API UDCRootLayout : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UCommonActivatableWidgetStack> Stack;

protected:
	virtual void NativeOnInitialized() override;
};

// ---------------------------------------------------------------- Tree-building helpers shared by every code-built widget

namespace DCUI
{
	// Content padding for the title plate (UDCBorder_Header): clears its slanted ends and stripe
	inline const FMargin HeaderPadding(48.f, 12.f, 36.f, 12.f);

	EPICMEGAJAM2026_API UCommonTextBlock* Text(UWidgetTree* Tree, UPanelWidget* Parent, const FString& Text, TSubclassOf<UCommonTextStyle> Style = UDCText_Body::StaticClass());
	EPICMEGAJAM2026_API UCommonBorder* Border(UWidgetTree* Tree, UPanelWidget* Parent, TSubclassOf<UCommonBorderStyle> Style, FMargin Padding);
	EPICMEGAJAM2026_API UDCButton* Button(UWidgetTree* Tree, UPanelWidget* Parent, const FString& Label, FName Payload = NAME_None, TSubclassOf<UCommonButtonStyle> Style = UDCButton_Primary::StaticClass());
	// Makes a canvas the tree root with a styled panel anchored on it; returns the panel's content box
	EPICMEGAJAM2026_API UVerticalBox* Panel(UWidgetTree* Tree, FAnchors Anchors, FVector2D Alignment, FVector2D Position, UCommonBorder** OutPanel = nullptr, TSubclassOf<UCommonBorderStyle> Style = UDCBorder_Panel::StaticClass());
	// Title bar of a screen (blue plate, display font), with an optional subtitle under it
	EPICMEGAJAM2026_API void Header(UWidgetTree* Tree, UPanelWidget* Parent, const FString& Title, const FString& Subtitle = FString());
	EPICMEGAJAM2026_API FString Stars(int32 Rating);
	// Hover tooltip on Target listing the skills that are not zero (section 4.1)
	EPICMEGAJAM2026_API void SkillTooltip(UWidgetTree* Tree, UWidget* Target, const FSkillVector& Skills);
	// Tape titles come from the table with a "★" the fonts can't draw: drop it
	EPICMEGAJAM2026_API FString CleanTitle(const FText& Title);
	EPICMEGAJAM2026_API TSubclassOf<UCommonTextStyle> SkillStyle(int32 SkillIndex);
	// The client's order as stacked lines: the greeting in body text, each hint in the color of its skill. DisplayName prefixes the greeting.
	EPICMEGAJAM2026_API void OrderLines(UWidgetTree* Tree, UPanelWidget* Box, const FClientRuntime& Client, float Wrap);
	// One line per skill in its color: "Funny 3", or "Funny 3/10" with a cap; zeros are skipped when bHideZero
	EPICMEGAJAM2026_API void SkillList(UWidgetTree* Tree, UPanelWidget* Parent, const FSkillVector& Skills, int32 Cap = 0, bool bHideZero = false);
	// Compact inline wrap row of non-zero colored skills (e.g. "Funny 3  Scary 1") for VHS rows
	EPICMEGAJAM2026_API void InlineSkills(UWidgetTree* Tree, UPanelWidget* Parent, const FSkillVector& Skills);
	EPICMEGAJAM2026_API FSlateBrush Box(const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth = 2.f, float Radius = 4.f);
	// Patience/goal bar in the palette
	EPICMEGAJAM2026_API UProgressBar* Bar(UWidgetTree* Tree, UPanelWidget* Parent, float Width);
}
