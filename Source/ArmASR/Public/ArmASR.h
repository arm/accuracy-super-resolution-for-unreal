//
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectMacros.h"

#if WITH_EDITOR
#include "PropertyEditorModule.h"
#endif

#include "ArmASR.generated.h"

class FArmASRSceneViewExtension;
class FArmASRTemporalUpscaler;
struct ArmASRState;

UENUM()
enum class EShaderQualityPreset : int32
{
	QUALITY = 1 UMETA(DisplayName = "Quality"),
	BALANCED = 2 UMETA(DisplayName = "Balanced"),
	PERFORMANCE = 3 UMETA(DisplayName = "Performance"),
	ULTRA_PERFORMANCE = 4 UMETA(DisplayName = "Ultra Performance")
};

class FArmASRModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FArmASRTemporalUpscaler* GetTemporalUpscaler() const
	{
		return TemporalUpscaler.Get();
	}

private:
	void OnPostEngineInit();
#if WITH_EDITOR
	void OnObjectPropertyChanged(UObject* Obj, FPropertyChangedEvent& Event);
#endif

	FDelegateHandle OnPostEngineInitHandle;
	FDelegateHandle OnObjectPropertyChangedHandle;
	TSharedPtr<FArmASRSceneViewExtension> SceneViewExtension;

private:
	TUniquePtr<FArmASRTemporalUpscaler> TemporalUpscaler;
};
