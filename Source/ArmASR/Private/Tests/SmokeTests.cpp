//
// Copyright © 2025 Arm Limited.
// SPDX-License-Identifier: MIT
//

#if WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Tests/AutomationCommon.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Framework/Application/SlateApplication.h"

#include "ArmASR.h"

// Class to enable setting console variables as latent commands.
class FSetConsoleVariableLatentCommand : public IAutomationLatentCommand
{
public:
    // Constructor: takes the name of the console variable and the float value to set.
    FSetConsoleVariableLatentCommand(const FString& InConsoleVarName, float InValue)
        : ConsoleVarName(InConsoleVarName)
        , Value(InValue)
        , bHasSet(false)
    {}

    // Update() is called every frame until it returns true.
    virtual bool Update() override
    {
        if (!bHasSet)
        {
            // Find the console variable by name.
            IConsoleVariable* ConsoleVar = IConsoleManager::Get().FindConsoleVariable(*ConsoleVarName);
            if (ConsoleVar)
            {
                // Set the console variable to the specified value.
                ConsoleVar->Set(Value, ECVF_SetByConsole);
                UE_LOG(LogTemp, Log, TEXT("Set console variable '%s' to %f."), *ConsoleVarName, Value);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Console variable '%s' not found."), *ConsoleVarName);
            }
            bHasSet = true;
        }
        // Return true immediately once the variable is set.
        return true;
    }

private:
    FString ConsoleVarName;
    float Value;
    bool bHasSet;
};


// Custom latent command to take a screenshot in game mode.
class FTakeScreenshotLatentCommand : public IAutomationLatentCommand
{
public:
    // Constructor: ScreenshotName is the base name (without extension) 
    // and InDelay is how long to wait after requesting the screenshot.
    FTakeScreenshotLatentCommand(const FString& InScreenshotName, float InDelay = 1.0f)
        : ScreenshotName(InScreenshotName)
        , Delay(InDelay)
        , bScreenshotRequested(false)
        , StartTime(0.0)
    {}

    virtual bool Update() override
    {
        if (!bScreenshotRequested)
        {
            // Record the start time when we request the screenshot.
            StartTime = FPlatformTime::Seconds();

            // Request the screenshot.
            // This call schedules the screenshot to be taken.
            FScreenshotRequest::RequestScreenshot(*(ScreenshotName + TEXT(".png")), false, false);
            bScreenshotRequested = true;

            UE_LOG(LogTemp, Log, TEXT("Screenshot requested: %s.png"), *ScreenshotName);
        }

        // Wait for the specified delay to allow the screenshot process to complete.
        double ElapsedTime = FPlatformTime::Seconds() - StartTime;
        return (ElapsedTime > Delay);
    }

private:
    FString ScreenshotName;
    float Delay;
    bool bScreenshotRequested;
    double StartTime;
};


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArmASREnableTest,
    "ArmASR.PluginTests.EnablePluginTest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext 
    | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext
    | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FArmASREnableTest::RunTest(const FString& Parameters)
{
    // 1. Enable the temporal upscaler visualizer so we can see when Arm ASR is running.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("ShowFlag.VisualizeTemporalUpscaler"), true));

    // 2. Ensure Arm ASR is disabled to start with.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.AntiAliasingMethod"), 2));
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.Enable"), false));

    // 3. Load a test map (ensure the map exists in your project)
    const FString MapName = "/Game/_Game/ThirdPerson/ThirdPerson";
    if (!AutomationOpenMap(MapName))
    {
        AddError(FString::Printf(TEXT("Failed to open map %s"), *MapName));
        return false;
    }

    // 4. Wait for the map to load and render. Use a latent command to delay execution.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));

    // 5. Take a screenshot. This latent command will capture a screenshot and save it in your project's saved folder.
    TSharedPtr<SWindow> CurrentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
    ADD_LATENT_AUTOMATION_COMMAND(FTakeEditorScreenshotCommand({ TEXT("ArmASR_EnablePluginTest_before.png"), CurrentWindow }));
    // 6. Enable Arm ASR from the command line.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.Enable"), true));

    // 7. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeEditorScreenshotCommand({ TEXT("ArmASR_EnablePluginTest_after.png"), CurrentWindow }));

    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArmASRQualityPresetTest,
    "ArmASR.PluginTests.QualityPresetTest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext 
    | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext
    | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

    bool FArmASRQualityPresetTest::RunTest(const FString& Parameters)
{
    // 1. Enable the temporal upscaler visualizer so we can see when Arm ASR is running.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("ShowFlag.VisualizeTemporalUpscaler"), true));

    // 2. Ensure Arm ASR is enabled.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.AntiAliasingMethod"), 2));
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.Enable"), true));

    // 3. Load a test map (ensure the map exists in your project)
    const FString MapName = "/Game/_Game/ThirdPerson/ThirdPerson";
    if (!AutomationOpenMap(MapName))
    {
        AddError(FString::Printf(TEXT("Failed to open map %s"), *MapName));
        return false;
    }

    // 4. Wait for the map to load and render. Use a latent command to delay execution.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));

    // 5. Set shader quality to 1.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.ShaderQuality"), 1));

    // 6. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeActiveEditorScreenshotCommand(TEXT("ArmASR_ShaderQualityTest_Quality.png")));

    // 7. Set shader quality to 2.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.ShaderQuality"), 2));

    // 8. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeActiveEditorScreenshotCommand(TEXT("ArmASR_ShaderQualityTest_Balanced.png")));

    // 7. Set shader quality to 3.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.ShaderQuality"), 3));

    // 8. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeActiveEditorScreenshotCommand(TEXT("ArmASR_ShaderQualityTest_Performance.png")));

    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArmASRScreenPercentageTest,
    "ArmASR.PluginTests.UpscaleRatioTest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
    | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext
    | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

    bool FArmASRScreenPercentageTest::RunTest(const FString& Parameters)
{
    // 1. Enable the temporal upscaler visualizer so we can see when Arm ASR is running.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("ShowFlag.VisualizeTemporalUpscaler"), true));

    // 2. Ensure Arm ASR is enabled.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.AntiAliasingMethod"), 2));
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.Enable"), true));

    // 3. Load a test map (ensure the map exists in your project)
    const FString MapName = "/Game/_Game/ThirdPerson/ThirdPerson";
    if (!AutomationOpenMap(MapName))
    {
        AddError(FString::Printf(TEXT("Failed to open map %s"), *MapName));
        return false;
    }

    // 4. Wait for the map to load and render. Use a latent command to delay execution.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));

    // 5. Set screen percentage to 100.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ScreenPercentage"), 100));

    // 6. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeActiveEditorScreenshotCommand(
        TEXT("ArmASR_ScreenPercentageTest_100.png")));

    // 5. Set screen percentage to 50.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ScreenPercentage"), 50));

    // 8. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeActiveEditorScreenshotCommand(
        TEXT("ArmASR_ScreenPercentageTest_50.png")));

    // 7. Set screen percentage to 67.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ScreenPercentage"), 67));

    // 8. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeActiveEditorScreenshotCommand(
        TEXT("ArmASR_ScreenPercentageTest_67.png")));

    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArmASRSharpnessTest,
    "ArmASR.PluginTests.SharpnessTest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
    | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext
    | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

    bool FArmASRSharpnessTest::RunTest(const FString& Parameters)
{
    // 1. Enable the temporal upscaler visualizer so we can see when Arm ASR is running.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("ShowFlag.VisualizeTemporalUpscaler"), true));

    // 2. Ensure Arm ASR is enabled.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.AntiAliasingMethod"), 2));
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.Enable"), true));

    // 3. Load a test map (ensure the map exists in your project)
    const FString MapName = "/Game/_Game/ThirdPerson/ThirdPerson";
    if (!AutomationOpenMap(MapName))
    {
        AddError(FString::Printf(TEXT("Failed to open map %s"), *MapName));
        return false;
    }

    // 4. Wait for the map to load and render. Use a latent command to delay execution.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));

    // 5. Set sharpness to 0.0.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.Sharpness"), 0.0f));

    // 6. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeActiveEditorScreenshotCommand(TEXT("ArmASR_SharpnessTest_0_0.png")));

    // 7. Set sharpness to 0.24.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.Sharpness"), 0.24f));

    // 8. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeActiveEditorScreenshotCommand(TEXT("ArmASR_SharpnessTest_0_24.png")));

    // 7. Set sharpness to 0.24.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.Sharpness"), 0.24f));

    // 8. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeActiveEditorScreenshotCommand(TEXT("ArmASR_SharpnessTest_0_24.png")));

    // 7. Set sharpness to 0.63.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.Sharpness"), 0.63f));

    // 8. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeActiveEditorScreenshotCommand(TEXT("ArmASR_SharpnessTest_0_63.png")));

    // 7. Set sharpness to 1.0.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.Sharpness"), 1.0f));

    // 8. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeActiveEditorScreenshotCommand(TEXT("ArmASR_SharpnessTest_1_0.png")));

    return true;
}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArmASRReactiveMaskTest,
    "ArmASR.PluginTests.ReactiveMaskTest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
    | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext
    | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

    bool FArmASRReactiveMaskTest::RunTest(const FString& Parameters)
{
    // 1. Enable the temporal upscaler visualizer so we can see when Arm ASR is running.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("ShowFlag.VisualizeTemporalUpscaler"), true));

    // 2. Ensure Arm ASR is enabled.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.AntiAliasingMethod"), 2));
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.Enable"), true));

    // 3. Load a test map (ensure the map exists in your project)
    const FString MapName = "/Game/_Game/ThirdPerson/ThirdPerson";
    if (!AutomationOpenMap(MapName))
    {
        AddError(FString::Printf(TEXT("Failed to open map %s"), *MapName));
        return false;
    }

    // 4. Wait for the map to load and render. Use a latent command to delay execution.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));

    // 5. Disable reactive mask.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.CreateReactiveMask"), false));

    // 6. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeActiveEditorScreenshotCommand(TEXT("ArmASR_ReactiveMaskTest_off.png")));

    // 7. Enable reactive mask.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.CreateReactiveMask"), true));

    // 8. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeActiveEditorScreenshotCommand(TEXT("ArmASR_ReactiveMaskTest_on.png")));

    return true;
}

// Optional post-processing tests.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArmASRFilmGrainTest,
    "ArmASR.PluginTests.FilmGrainTest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext
    | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext
    | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

    bool FArmASRFilmGrainTest::RunTest(const FString &Parameters)
{
    // 1. Enable the temporal upscaler visualizer so we can see when Arm ASR is running.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("ShowFlag.VisualizeTemporalUpscaler"), true));

    // 2. Ensure Arm ASR is enabled.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.AntiAliasingMethod"), 2));
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.ArmASR.Enable"), true));

    // 3. Load a test map (ensure the map exists in your project)
    const FString MapName = "/Game/_Game/ThirdPerson/ThirdPerson";
    if (!AutomationOpenMap(MapName))
    {
        AddError(FString::Printf(TEXT("Failed to open map %s"), *MapName));
        return false;
    }

    // 4. Wait for the map to load and render. Use a latent command to delay execution.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));

    // 5. Enable film grain (post process volume should exist in the test map with this enabled).
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.FilmGrain"), true));

    // 6. Wait before taking a screenshot.
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FTakeActiveEditorScreenshotCommand(TEXT("ArmASR_FilmGrain_on.png")));

    // 7. Disable film grain.
    ADD_LATENT_AUTOMATION_COMMAND(FSetConsoleVariableLatentCommand(TEXT("r.FilmGrain"), false));

    return true;
}

#endif