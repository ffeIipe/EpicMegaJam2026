// Player options saved per user (Saved/Config/.../GameUserSettings.ini): key bindings, master volume, mouse sensitivity.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "DCSettings.generated.h"

// Game actions the player can rebind; the controller polls their keys every tick
enum class EDCAction : uint8 { Interact, BackRoom, Count };

UCLASS(config = GameUserSettings)
class EPICMEGAJAM2026_API UDCSettings : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Config) FKey Interact = EKeys::E;
	UPROPERTY(Config) FKey BackRoom = EKeys::SpaceBar;
	// Multiplies PDA_GameConfig.MouseSensitivity
	UPROPERTY(Config) float MouseSensitivityScale = 1.f;
	UPROPERTY(Config) float MasterVolume = 1.f;

	static UDCSettings* Get() { return GetMutableDefault<UDCSettings>(); }
	FKey& Key(EDCAction Action);
	static const TCHAR* ActionLabel(EDCAction Action);
	// Pushes the volume to FMOD's master bus (no-op when FMOD isn't running)
	void Apply() const;
	void Save();
};
