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
// Genre tabs: eight of them have to fit the width of a list
UCLASS() class EPICMEGAJAM2026_API UDCText_Tab : public UCommonTextStyle { GENERATED_BODY() public: UDCText_Tab(); };

// ---------------------------------------------------------------- Border styles

UCLASS() class EPICMEGAJAM2026_API UDCBorder_Panel : public UCommonBorderStyle { GENERATED_BODY() public: UDCBorder_Panel(); };
UCLASS() class EPICMEGAJAM2026_API UDCBorder_Header : public UCommonBorderStyle { GENERATED_BODY() public: UDCBorder_Header(); };
UCLASS() class EPICMEGAJAM2026_API UDCBorder_Inset : public UCommonBorderStyle { GENERATED_BODY() public: UDCBorder_Inset(); };
UCLASS() class EPICMEGAJAM2026_API UDCBorder_KeyCap : public UCommonBorderStyle { GENERATED_BODY() public: UDCBorder_KeyCap(); };

// ---------------------------------------------------------------- Button styles

UCLASS() class EPICMEGAJAM2026_API UDCButton_Primary : public UCommonButtonStyle { GENERATED_BODY() public: UDCButton_Primary(); };
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
	// The 5 skills of a tape with their numbers (F S B T R, placeholders until art has icons); skills at 0 are dimmed (section 4.1)
	EPICMEGAJAM2026_API void SkillStrip(UWidgetTree* Tree, UPanelWidget* Parent, const FSkillVector& Skills);
	// One "Funny 3" line per skill; with a cap, "Funny 3/10"
	EPICMEGAJAM2026_API FString SkillLines(const FSkillVector& Skills, int32 Cap = 0);
	EPICMEGAJAM2026_API FSlateBrush Box(const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth = 2.f, float Radius = 4.f);
	// Patience/goal bar in the palette
	EPICMEGAJAM2026_API UProgressBar* Bar(UWidgetTree* Tree, UPanelWidget* Parent, float Width);
}
