using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;

namespace TwinKartLauncher
{
    public enum GraphicsApi { Unknown, D3D9, D3D11, D3D12, Vulkan }

    // User intent is separate from runtime evidence. No property in this file
    // edits a game INI or asserts that a graphics feature is already active.
    public sealed class GraphicsProfile
    {
        public GameEdition Edition { get; set; }
        public bool WantDlssSuperResolution { get; set; }
        public bool WantDlssFrameGeneration { get; set; }
        public bool WantDlss5NeuralRendering { get; set; }
        public int FrameMultiplier { get; set; }
        public int NeuralPasses { get; set; }

        public GraphicsProfile()
        {
            FrameMultiplier = 2;
            NeuralPasses = 1;
        }

        public GraphicsAssessment Assess(GraphicsEvidence evidence)
        {
            var result = new GraphicsAssessment();
            if (evidence == null || evidence.Edition != Edition ||
                !Regex.IsMatch(evidence.ClientSha256 ?? "", "^[0-9a-fA-F]{64}$"))
            {
                result.Issues.Add("A build-specific client SHA-256 and matching game edition are required.");
                return result;
            }
            if (WantDlssFrameGeneration && (FrameMultiplier < 2 || FrameMultiplier > 6))
                result.Issues.Add("Requested DLSS frame multiplier must be between 2 and 6.");
            if (WantDlss5NeuralRendering && (NeuralPasses < 1 || NeuralPasses > 2))
                result.Issues.Add("Requested neural-rendering pass count must be 1 or 2.");

            if (WantDlssSuperResolution)
            {
                result.SuperResolution.CanRunDiagnostic = evidence.DlssSrProviderAvailable && evidence.TemporalUpscaleIntegrationVerified;
                result.SuperResolution.ReadyForGameplayTrial = result.SuperResolution.CanRunDiagnostic &&
                    evidence.MotionVectorsVerified && evidence.SceneDepthVerified;
                result.SuperResolution.VerifiedWorking = result.SuperResolution.ReadyForGameplayTrial &&
                    evidence.SrEvaluateSucceeded && evidence.SrRenderWidth > 0 && evidence.SrRenderHeight > 0 &&
                    evidence.SrOutputWidth > evidence.SrRenderWidth && evidence.SrOutputHeight > evidence.SrRenderHeight &&
                    evidence.SrVisualComparisonPassed;
                if (!evidence.DlssSrProviderAvailable) result.SuperResolution.Missing.Add("DLSS SR runtime/provider");
                if (!evidence.TemporalUpscaleIntegrationVerified) result.SuperResolution.Missing.Add("game temporal-upscale integration");
                if (!evidence.MotionVectorsVerified) result.SuperResolution.Missing.Add("scene motion vectors");
                if (!evidence.SceneDepthVerified) result.SuperResolution.Missing.Add("scene depth");
                if (!evidence.SrEvaluateSucceeded) result.SuperResolution.Missing.Add("successful SR evaluate");
                if (evidence.SrRenderWidth <= 0 || evidence.SrRenderHeight <= 0 ||
                    evidence.SrOutputWidth <= evidence.SrRenderWidth || evidence.SrOutputHeight <= evidence.SrRenderHeight)
                    result.SuperResolution.Missing.Add("measured lower render resolution and higher output resolution");
                if (!evidence.SrVisualComparisonPassed) result.SuperResolution.Missing.Add("in-game visual comparison");
            }

            if (WantDlssFrameGeneration)
            {
                result.FrameGeneration.CanRunDiagnostic = FrameMultiplier >= 2 && FrameMultiplier <= 6 &&
                    evidence.DlssFgProviderAvailable && evidence.FrameGenerationIntegrationVerified;
                result.FrameGeneration.ReadyForGameplayTrial = result.FrameGeneration.CanRunDiagnostic &&
                    evidence.MotionVectorsVerified && evidence.SceneDepthVerified && evidence.FrameMarkersVerified &&
                    evidence.UiCompositionVerified;
                result.FrameGeneration.VerifiedWorking = result.FrameGeneration.ReadyForGameplayTrial &&
                    evidence.FgFramesActuallyPresented >= FrameMultiplier && evidence.FramePacingVerified &&
                    evidence.FgVisualComparisonPassed;
                if (!evidence.DlssFgProviderAvailable) result.FrameGeneration.Missing.Add("DLSS-G runtime/provider");
                if (!evidence.FrameGenerationIntegrationVerified) result.FrameGeneration.Missing.Add("game frame-generation integration and compatible presentation path");
                if (!evidence.MotionVectorsVerified) result.FrameGeneration.Missing.Add("scene motion vectors");
                if (!evidence.SceneDepthVerified) result.FrameGeneration.Missing.Add("scene depth");
                if (!evidence.FrameMarkersVerified) result.FrameGeneration.Missing.Add("frame markers/tokens");
                if (!evidence.UiCompositionVerified) result.FrameGeneration.Missing.Add("HUDless/UI composition evidence");
                if (evidence.FgFramesActuallyPresented < FrameMultiplier) result.FrameGeneration.Missing.Add("runtime-presented frame count at requested multiplier");
                if (!evidence.FramePacingVerified) result.FrameGeneration.Missing.Add("frame pacing measurement");
                if (!evidence.FgVisualComparisonPassed) result.FrameGeneration.Missing.Add("moving in-game visual comparison");
            }

            if (WantDlss5NeuralRendering)
            {
                result.NeuralRendering.CanRunDiagnostic = NeuralPasses >= 1 && NeuralPasses <= 2 &&
                    evidence.Dlss5NrProviderAvailable && evidence.NeuralConsumerIntegrationVerified;
                result.NeuralRendering.ReadyForGameplayTrial = result.NeuralRendering.CanRunDiagnostic &&
                    evidence.MotionVectorsVerified && evidence.SceneDepthVerified;
                result.NeuralRendering.VerifiedWorking = result.NeuralRendering.ReadyForGameplayTrial &&
                    evidence.NrFeatureCreated && evidence.NrPass1Succeeded &&
                    (NeuralPasses == 1 || evidence.NrPass2Succeeded) &&
                    evidence.NrFinalCompositeVerified && evidence.NrVisualComparisonPassed;
                if (!evidence.Dlss5NrProviderAvailable) result.NeuralRendering.Missing.Add("DLSS 5 NR runtime/provider");
                if (!evidence.NeuralConsumerIntegrationVerified) result.NeuralRendering.Missing.Add("NR consumer integration");
                if (!evidence.MotionVectorsVerified) result.NeuralRendering.Missing.Add("scene or validated estimated motion vectors");
                if (!evidence.SceneDepthVerified) result.NeuralRendering.Missing.Add("scene depth");
                if (!evidence.NrFeatureCreated) result.NeuralRendering.Missing.Add("successful NR feature creation");
                if (!evidence.NrPass1Succeeded || (NeuralPasses == 2 && !evidence.NrPass2Succeeded))
                    result.NeuralRendering.Missing.Add("successful NR vendor evaluate for each requested pass");
                if (!evidence.NrFinalCompositeVerified) result.NeuralRendering.Missing.Add("final output composition");
                if (!evidence.NrVisualComparisonPassed) result.NeuralRendering.Missing.Add("moving in-game NR off/on comparison");
            }
            return result;
        }
    }

    public sealed class GraphicsEvidence
    {
        public GameEdition Edition { get; set; }
        public string ClientSha256 { get; set; }
        public GraphicsApi Api { get; set; }
        public bool DlssSrProviderAvailable { get; set; }
        public bool TemporalUpscaleIntegrationVerified { get; set; }
        public bool MotionVectorsVerified { get; set; }
        public bool SceneDepthVerified { get; set; }
        public bool SrEvaluateSucceeded { get; set; }
        public int SrRenderWidth { get; set; }
        public int SrRenderHeight { get; set; }
        public int SrOutputWidth { get; set; }
        public int SrOutputHeight { get; set; }
        public bool SrVisualComparisonPassed { get; set; }
        public bool DlssFgProviderAvailable { get; set; }
        public bool FrameGenerationIntegrationVerified { get; set; }
        public bool FrameMarkersVerified { get; set; }
        public bool UiCompositionVerified { get; set; }
        public int FgFramesActuallyPresented { get; set; }
        public bool FramePacingVerified { get; set; }
        public bool FgVisualComparisonPassed { get; set; }
        public bool Dlss5NrProviderAvailable { get; set; }
        public bool NeuralConsumerIntegrationVerified { get; set; }
        public bool NrFeatureCreated { get; set; }
        public bool NrPass1Succeeded { get; set; }
        public bool NrPass2Succeeded { get; set; }
        public bool NrFinalCompositeVerified { get; set; }
        public bool NrVisualComparisonPassed { get; set; }
    }

    public sealed class GraphicsFeatureAssessment
    {
        public bool CanRunDiagnostic { get; internal set; }
        public bool ReadyForGameplayTrial { get; internal set; }
        public bool VerifiedWorking { get; internal set; }
        public List<string> Missing { get; private set; }
        public GraphicsFeatureAssessment() { Missing = new List<string>(); }
    }

    public sealed class GraphicsAssessment
    {
        public List<string> Issues { get; private set; }
        public GraphicsFeatureAssessment SuperResolution { get; private set; }
        public GraphicsFeatureAssessment FrameGeneration { get; private set; }
        public GraphicsFeatureAssessment NeuralRendering { get; private set; }
        public GraphicsAssessment()
        {
            Issues = new List<string>();
            SuperResolution = new GraphicsFeatureAssessment();
            FrameGeneration = new GraphicsFeatureAssessment();
            NeuralRendering = new GraphicsFeatureAssessment();
        }
    }
}
