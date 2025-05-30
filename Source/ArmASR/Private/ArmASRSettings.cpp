//
// Copyright © 2025 Arm Limited.
// SPDX-License-Identifier: MIT
//

#if WITH_EDITOR

#include "ArmASRSettings.h"
#include "HAL/IConsoleManager.h"

const FString UArmASRSettings::GeneralSettings = TEXT("General Settings");
const FString UArmASRSettings::QualitySettings = TEXT("Quality Settings");
const FString UArmASRSettings::ReactiveMaskSettings = TEXT("Reactive Mask Settings");

FName UArmASRSettings::GetContainerName() const
{
	static const FName ContainerName("Project");
	return ContainerName;
}

FName UArmASRSettings::GetCategoryName() const
{
	static const FName EditorCategoryName("Plugins");
	return EditorCategoryName;
}

FName UArmASRSettings::GetSectionName() const
{
	static const FName EditorSectionName("Arm ASR");
	return EditorSectionName;
}

float UArmASRSettings::GetClampedFloatPropertyValue(FFloatProperty *FloatProp, float OrigCVarValue) const
{
	float ClampedCVarValue = OrigCVarValue;

	if (FloatProp->HasMetaData("ClampMin"))
	{
		float min = FloatProp->GetFloatMetaData("ClampMin");
		ClampedCVarValue = FMath::Max(ClampedCVarValue, min);
	}
	if (FloatProp->HasMetaData("ClampMax"))
	{
		float max = FloatProp->GetFloatMetaData("ClampMax");
		ClampedCVarValue = FMath::Min(ClampedCVarValue, max);
	}

	return ClampedCVarValue;
}

int32 UArmASRSettings::GetClampedEnumPropertyValue(FEnumProperty *EnumProp, int32 OrigCVarValue) const
{
	int32 ClampedCVarValue = OrigCVarValue;

	if (EnumProp->HasMetaData("ClampMin"))
	{
		int32 min = EnumProp->GetIntMetaData("ClampMin");
		ClampedCVarValue = FMath::Max(ClampedCVarValue, min);
	}
	if (EnumProp->HasMetaData("ClampMax"))
	{
		float max = EnumProp->GetIntMetaData("ClampMax");
		ClampedCVarValue = FMath::Min(ClampedCVarValue, max);
	}

	return ClampedCVarValue;
}

// Sync the values of all the console variables with the UI on init
// Or sync only a single changed value
void UArmASRSettings::SyncConsoleVariablesWithUI(bool SetConsoleVars, IConsoleVariable* UpdatedCVar)
{
	// Iterate through all the properties in UArmASRSettings
	for (TFieldIterator<FProperty> PropertyIterator(GetClass()); PropertyIterator; ++PropertyIterator)
	{
		FProperty* Property = *PropertyIterator;

		// Check if the current property is a console variable and get the name
		FString CVarName = Property->GetMetaData("ConsoleVariable");

		// Get the associated console variable for the name
		IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);

		if (CVar)
		{
			// If only updating a single console variable,
			// and it isn't a match, continue to the next
			if (UpdatedCVar && CVar != UpdatedCVar) continue;

			// Get a pointer to the property's data
			void* Data = Property->ContainerPtrToValuePtr<void>(this);

			// Check the property type
			// Either set the console variable after changing the UI
			// Or update the UI to match the console variable
			// So both are always in sync
			if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
			{
				if (SetConsoleVars)
				{
					CVSetFromUI = CVar;
					CVar->Set(BoolProp->GetPropertyValue(Data), ECVF_SetByConsole);
				}
				else
				{
					int32 CVarValue = CVar->GetInt();
					BoolProp->SetPropertyValue_InContainer(this, CVarValue != 0);
				}
			}

			else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
			{
				if (SetConsoleVars)
				{
					CVSetFromUI = CVar;
					CVar->Set(FloatProp->GetPropertyValue(Data), ECVF_SetByConsole);
				}
				else
				{
					float CVarValue = CVar->GetFloat();
					float ClampedCVarValue = GetClampedFloatPropertyValue(FloatProp, CVarValue);

					FloatProp->SetPropertyValue_InContainer(this, ClampedCVarValue);

					// If value was clamped, update original value with clamped one
					if (ClampedCVarValue != CVarValue)
					{
						CVSetFromUI = CVar;
						CVar->Set(ClampedCVarValue, ECVF_SetByConsole);
					}
				}
			}

			else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
			{
				if (FNumericProperty* UnderlyingProp =
						CastField<FNumericProperty>(EnumProp->GetUnderlyingProperty()))
				{
					if (SetConsoleVars)
					{
						CVSetFromUI = CVar;

						int32 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(Data);
						CVar->Set(EnumValue, ECVF_SetByConsole);
					}
					else
					{
						int32 CVarValue = CVar->GetInt();
						int32 ClampedCVarValue = GetClampedEnumPropertyValue(EnumProp, CVarValue);

						UnderlyingProp->SetIntPropertyValue(Data, static_cast<int64>(ClampedCVarValue));

						// If value was clamped, update original value with clamped one
						if (ClampedCVarValue != CVarValue)
						{
							CVSetFromUI = CVar;
							CVar->Set(ClampedCVarValue, ECVF_SetByConsole);
						}
					}
				}
			}

			CVSetFromUI = nullptr;

			// Break if only updating a single console variable, after it has been set
			if (UpdatedCVar) break;
		}
	}
}

void UArmASRSettings::OnConsoleVariablesUpdated(IConsoleVariable* CVar)
{
	if (CVar)
	{
		// Sync console variable with UI if it was not set by the UI in the first place
		if (CVar != CVSetFromUI)
		{
			SyncConsoleVariablesWithUI(false, CVar);
		}
		SaveConfig();
	}
}

void UArmASRSettings::PostInitProperties()
{
	Super::PostInitProperties();
	// Apply values to the UI settings to match the console variables
	SyncConsoleVariablesWithUI(false);

	// Add change listeners for console variables
	for (TFieldIterator<FProperty> PropertyIterator(GetClass()); PropertyIterator; ++PropertyIterator)
	{
		FProperty *Property = *PropertyIterator;
		FString CVarName = Property->GetMetaData("ConsoleVariable");

		IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);

		if (CVar)
		{
			CVar->SetOnChangedCallback(
				FConsoleVariableDelegate::CreateUObject(this, &UArmASRSettings::OnConsoleVariablesUpdated));
		}
	}
}

void UArmASRSettings::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// Update the console variables whenever the settings UI is changed
	SyncConsoleVariablesWithUI(true);
}

#endif