using System;
using TwinKartLauncher;

internal static class GraphicsProfileTests
{
    const string DriftHash = "5041d9f5d45adb4b1c932d8056a89f70db7fa98f93614ac30685cd1adeb9d9c2";

    static int Main()
    {
        try
        {
            GraphicsProfile requested = new GraphicsProfile {
                Edition = GameEdition.Drift, WantDlssSuperResolution = true,
                WantDlssFrameGeneration = true, FrameMultiplier = 4,
                WantDlss5NeuralRendering = true, NeuralPasses = 2
            };
            GraphicsEvidence baseline = new GraphicsEvidence {
                Edition = GameEdition.Drift, ClientSha256 = DriftHash, Api = GraphicsApi.D3D11,
                Dlss5NrProviderAvailable = true // Copied DLL is present; actual evaluate count is zero.
            };
            GraphicsAssessment result = requested.Assess(baseline);
            Check(result.Issues.Count == 0, "valid baseline rejected");
            Check(!result.SuperResolution.CanRunDiagnostic && !result.SuperResolution.VerifiedWorking, "DLSS DLL presence claimed SR");
            Check(!result.FrameGeneration.CanRunDiagnostic && !result.FrameGeneration.VerifiedWorking, "DLSS DLL presence claimed FG");
            Check(!result.NeuralRendering.CanRunDiagnostic && !result.NeuralRendering.VerifiedWorking, "NR DLL presence claimed NR");

            baseline.NeuralConsumerIntegrationVerified = true;
            result = requested.Assess(baseline);
            Check(result.NeuralRendering.CanRunDiagnostic, "NR diagnostic gate missed");
            Check(!result.NeuralRendering.ReadyForGameplayTrial && !result.NeuralRendering.VerifiedWorking, "NR addon load claimed gameplay quality");
            baseline.MotionVectorsVerified = true; baseline.SceneDepthVerified = true;
            baseline.NrFeatureCreated = true; baseline.NrPass1Succeeded = true; baseline.NrPass2Succeeded = true;
            result = requested.Assess(baseline);
            Check(result.NeuralRendering.ReadyForGameplayTrial, "NR input gate missed");
            Check(!result.NeuralRendering.VerifiedWorking, "NR evaluate without final output claimed working");
            baseline.NrFinalCompositeVerified = true; baseline.NrVisualComparisonPassed = true;
            Check(requested.Assess(baseline).NeuralRendering.VerifiedWorking, "complete NR evidence not recognized");

            baseline.DlssSrProviderAvailable = true; baseline.TemporalUpscaleIntegrationVerified = true;
            baseline.SrEvaluateSucceeded = true;
            baseline.SrRenderWidth = 1920; baseline.SrRenderHeight = 1200;
            baseline.SrOutputWidth = 1920; baseline.SrOutputHeight = 1200;
            baseline.SrVisualComparisonPassed = true;
            Check(!requested.Assess(baseline).SuperResolution.VerifiedWorking, "1:1 DLAA mislabeled as SR");
            baseline.SrOutputWidth = 3840; baseline.SrOutputHeight = 2400;
            Check(requested.Assess(baseline).SuperResolution.VerifiedWorking, "measured SR not recognized");

            baseline.DlssFgProviderAvailable = true; baseline.FrameGenerationIntegrationVerified = true;
            baseline.FrameMarkersVerified = true; baseline.UiCompositionVerified = true;
            baseline.FgFramesActuallyPresented = 4;
            Check(!requested.Assess(baseline).FrameGeneration.VerifiedWorking, "FG count without pacing/visual check claimed working");
            baseline.FramePacingVerified = true; baseline.FgVisualComparisonPassed = true;
            Check(requested.Assess(baseline).FrameGeneration.VerifiedWorking, "complete FG evidence not recognized");
            requested.FrameMultiplier = 6;
            Check(!requested.Assess(baseline).FrameGeneration.VerifiedWorking, "4x runtime count claimed 6x");
            requested.FrameMultiplier = 7;
            Check(requested.Assess(baseline).Issues.Count > 0, "unsupported requested multiplier accepted");
            baseline.ClientSha256 = "unknown";
            Check(requested.Assess(baseline).Issues.Count > 0, "unidentified client build accepted");
            Console.WriteLine("PASS graphics evidence gates and 1:1 DLAA distinction");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine("FAIL " + error);
            return 1;
        }
    }

    static void Check(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
    }
}
