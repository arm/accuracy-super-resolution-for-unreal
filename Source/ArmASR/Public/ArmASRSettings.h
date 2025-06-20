//
// Copyright © 2025 Arm Limited.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ArmASR.h"
#include "ArmASRSettings.generated.h"

// Plugin settings for Arm ASR
UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "Arm ASR"))
class ARMASR_API UArmASRSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const FString GeneralSettings;
	static const FString QualitySettings;
	static const FString ReactiveMaskSettings;

#if WITH_EDITOR
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	void SyncConsoleVariablesWithUI(bool SetConsoleVars, IConsoleVariable* UpdatedCVar = nullptr);
	virtual void OnConsoleVariablesUpdated(IConsoleVariable* CVar);

	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, Config, Category = GeneralSettings, meta = (
		ConsoleVariable = "r.ArmASR.Enable",
		DisplayName = "Enable",
		ToolTip = "Turn on Arm ASR."))
	bool EnableArmASR;

	UPROPERTY(EditAnywhere, Config, Category = GeneralSettings, meta = (
		ConsoleVariable = "r.ArmASR.AutoExposure",
		DisplayName = "Auto Exposure",
		ToolTip = "True to use Arm ASR's own auto-exposure, otherwise the engine's auto-exposure value is used.",
		EditCondition = "EnableArmASR"))
	bool ArmASRAutoExposure;

	UPROPERTY(EditAnywhere, Config, Category = QualitySettings, meta = (
		ConsoleVariable = "r.ArmASR.Sharpness",
		DisplayName = "Sharpness",
		ClampMin = 0, ClampMax = 1,
		ToolTip = "Sharpening Filter to sharpen the output image. Enables Robust Contrast Adaptive Sharpening Filter when greater than 0.",
		EditCondition = "EnableArmASR"))
	float ArmASRSharpness;

	UPROPERTY(EditAnywhere, Config, Category = QualitySettings, meta = (
		ConsoleVariable = "r.ArmASR.ShaderQuality",
		DisplayName = "Quality Mode",
		ClampMin = 1, ClampMax = 4,
		ToolTip = "Select shader quality preset. 1: Quality / 2: Balanced / 3: Performance / 4:Ultra Performance.",
		EditCondition = "EnableArmASR"))
	EShaderQualityPreset ArmASRShaderQualityMode;

	UPROPERTY(EditAnywhere, Config, Category = QualitySettings, meta = (
		ConsoleVariable = "r.ArmASR.CreateReactiveMask",
		DisplayName = "Create Reactive Mask",
		ToolTip = "Create the reactive mask.",
		EditCondition = "EnableArmASR"))
	bool ArmASRCreateReactiveMask;

	UPROPERTY(EditAnywhere, Config, Category = ReactiveMaskSettings, meta = (
		ConsoleVariable = "r.ArmASR.ReactiveMaskReflectionScale",
		DisplayName = "Reflection Scale",
		ClampMin = 0, ClampMax = 1,
		ToolTip = "Scales the Unreal engine reflection contribution to the reactive mask, which can be used to control the amount of aliasing on reflective surfaces.",
		EditCondition = "EnableArmASR"))
	float ArmASRReflectionScale;

	UPROPERTY(EditAnywhere, Config, Category = ReactiveMaskSettings, meta = (
		ConsoleVariable = "r.ArmASR.ReactiveMaskRoughnessScale",
		DisplayName = "Roughness Scale",
		ClampMin = 0, ClampMax = 1,
		ToolTip = "Scales the GBuffer roughness to provide a fallback value for the reactive mask when screenspace & planar reflections are disabled or don't affect a pixel.",
		EditCondition = "EnableArmASR"))
	float ArmASRRoughnessScale;

	UPROPERTY(EditAnywhere, Config, Category = ReactiveMaskSettings, meta = (
		ConsoleVariable = "r.ArmASR.ReactiveMaskRoughnessBias",
		DisplayName = "Roughness Bias",
		ClampMin = 0, ClampMax = 1,
		ToolTip = "Biases the reactive mask value when screenspace / planar reflections are weak with the GBuffer roughness to account for reflection environment captures.",
		EditCondition = "EnableArmASR"))
	float ArmASRRoughnessBias;

	UPROPERTY(EditAnywhere, Config, Category = ReactiveMaskSettings, meta = (
		ConsoleVariable = "r.ArmASR.ReactiveMaskRoughnessMaxDistance",
		DisplayName = "Roughness Max Distance",
		ClampMin = 0,
		ToolTip = "Maximum distance in world units for using material roughness to contribute to the reactive mask, the maximum of this value and View.FurthestReflectionCaptureDistance will be used.",
		EditCondition = "EnableArmASR"))
	float ArmASRRoughnessMaxDistance;

	UPROPERTY(EditAnywhere, Config, Category = ReactiveMaskSettings, meta = (
		ConsoleVariable = "r.ArmASR.ReactiveMaskRoughnessForceMaxDistance",
		DisplayName = "Force Roughness Max Distance",
		ToolTip = "Enable to force the maximum distance in world units for using material roughness to contribute to the reactive mask rather than using View.FurthestReflectionCaptureDistance.",
		EditCondition = "EnableArmASR"))
	bool ArmASRRoughnessForceMaxDistance;

	UPROPERTY(EditAnywhere, Config, Category = ReactiveMaskSettings, meta = (
		ConsoleVariable = "r.ArmASR.ReactiveMaskReflectionLumaBias",
		DisplayName = "Reflection Luminance Bias",
		ClampMin = 0, ClampMax = 1,
		ToolTip = "Biases the reactive mask by the luminance of the reflection. Use to balance aliasing against ghosting on brightly lit reflective surfaces.",
		EditCondition = "EnableArmASR"))
	float ArmASRReflectionLuminanceBias;

	UPROPERTY(EditAnywhere, Config, Category = ReactiveMaskSettings, meta = (
		ConsoleVariable = "r.ArmASR.ReactiveHistoryTranslucencyBias",
		DisplayName = "Reactive History Translucency Bias",
		ClampMin = 0, ClampMax = 1,
		ToolTip = "Scales how much translucency suppresses history via the reactive mask. Higher values will make translucent materials more reactive which can reduce smearing.",
		EditCondition = "EnableArmASR"))
	float ArmASRReactiveHistoryTranslucencyBias;

	UPROPERTY(EditAnywhere, Config, Category = ReactiveMaskSettings, meta = (
		ConsoleVariable = "r.ArmASR.ReactiveHistoryTranslucencyLumaBias",
		DisplayName = "Reactive History Translucency Luminance Bias",
		ClampMin = 0, ClampMax = 1,
		ToolTip = "Biases how much the translucency suppresses history via the reactive mask by the luminance of the transparency. Higher values will make bright translucent materials more reactive which can reduce smearing.",
		EditCondition = "EnableArmASR"))
	float ArmASRReactiveHistoryTranslucencyLumaBias;

	UPROPERTY(EditAnywhere, Config, Category = ReactiveMaskSettings, meta = (
		ConsoleVariable = "r.ArmASR.ReactiveMaskTranslucencyBias",
		DisplayName = "Translucency Bias",
		ClampMin = 0, ClampMax = 1,
		ToolTip = "Scales how much contribution translucency makes to the reactive mask. Higher values will make translucent materials more reactive which can reduce smearing.",
		EditCondition = "EnableArmASR"))
	float ArmASRTranslucencyBias;

	UPROPERTY(EditAnywhere, Config, Category = ReactiveMaskSettings, meta = (
		ConsoleVariable = "r.ArmASR.ReactiveMaskTranslucencyLumaBias",
		DisplayName = "Translucency Luminance Bias",
		ClampMin = 0, ClampMax = 1,
		ToolTip = "Biases the translucency contribution to the reactive mask by the luminance of the transparency. Higher values will make bright translucent materials more reactive which can reduce smearing.",
		EditCondition = "EnableArmASR"))
	float ArmASRTranslucencyLuminanceBias;

	UPROPERTY(EditAnywhere, Config, Category = ReactiveMaskSettings, meta = (
		ConsoleVariable = "r.ArmASR.ReactiveMaskTranslucencyMaxDistance",
		DisplayName = "Translucency Max Distance",
		ClampMin = 0,
		ToolTip = "Maximum distance in world units for using translucency to contribute to the reactive mask. This is a way to remove sky-boxes and other back-planes from the reactive mask, at the expense of nearer translucency not being reactive.",
		EditCondition = "EnableArmASR"))
	float ArmASRTranslucencyMaxDistance;

	UPROPERTY(EditAnywhere, Config, Category = ReactiveMaskSettings, meta = (
		ConsoleVariable = "r.ArmASR.ReactiveMaskForceReactiveMaterialValue",
		DisplayName = "Force value for Reactive Shading Model",
		ClampMin = 0, ClampMax = 1,
		ToolTip = "Force the reactive mask value for Reactive Shading Model materials, when > 0 this value can be used to override the value supplied in the Material Graph.",
		EditCondition = "EnableArmASR"))
	float ArmASRForceReactiveMaterialValue;

	UPROPERTY(EditAnywhere, Config, Category = ReactiveMaskSettings, meta = (
		ConsoleVariable = "r.ArmASR.ReactiveMaskReactiveShadingModelID",
		DisplayName = "Reactive Shading Model",
		ToolTip = "Treat the specified shading model as reactive, taking the CustomData0.x value as the reactive value to write into the mask.",
		EditCondition = "EnableArmASR"))
	TEnumAsByte<enum EMaterialShadingModel> ArmASRReactiveShadingModelID;

private:
	IConsoleVariable *CVSetFromUI = nullptr;

	float GetClampedFloatPropertyValue(FFloatProperty *FloatProp, float OrigCVarValue) const;

	int32 GetClampedEnumPropertyValue(FEnumProperty *EnumProp, int32 OrigCVarValue) const;
};