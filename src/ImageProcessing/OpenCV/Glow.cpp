module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "../../../include/Define/DllExportMacro.hpp"
#include <opencv2/opencv.hpp>



module Glow;

import Image;
import Core.Parallel;
import Math.Random;
namespace ArtifactCore {

 namespace
 {
  cv::Mat convertToFloat32BGRA(const cv::Mat& input)
  {
   if (input.empty()) {
    return {};
   }

   if (input.type() == CV_32FC4) {
    return input.clone(); // CV_32FC4 inputs follow the local BGRA contract.
   }

   cv::Mat bgra;
   if (input.channels() == 4) {
    input.convertTo(bgra, CV_32FC4, input.depth() == CV_8U ? (1.0 / 255.0) : 1.0);
    return bgra;
   }

   if (input.channels() == 3) {
    cv::cvtColor(input, bgra, cv::COLOR_BGR2BGRA);
   } else if (input.channels() == 1) {
    cv::cvtColor(input, bgra, cv::COLOR_GRAY2BGRA);
   } else {
    return {};
   }

   if (bgra.depth() != CV_32F) {
    bgra.convertTo(bgra, CV_32FC4, input.depth() == CV_8U ? (1.0 / 255.0) : 1.0);
   }
   return bgra;
  }

  cv::Mat toBgr8U(const cv::Mat& src)
  {
   if (src.empty()) {
    return cv::Mat();
   }

   cv::Mat u8;
   switch (src.depth()) {
   case CV_8U:
    u8 = src;
    break;
   case CV_16U:
    src.convertTo(u8, CV_8U, 255.0 / 65535.0);
    break;
   case CV_32F:
   case CV_64F:
    src.convertTo(u8, CV_8U, 255.0);
    break;
   default:
    src.convertTo(u8, CV_8U);
    break;
   }

   cv::Mat bgr;
   switch (u8.channels()) {
   case 1:
    cv::cvtColor(u8, bgr, cv::COLOR_GRAY2BGR);
    break;
   case 3:
    bgr = u8;
    break;
   case 4:
    cv::cvtColor(u8, bgr, cv::COLOR_BGRA2BGR);
    break;
   default:
    bgr = u8;
    if (bgr.channels() == 4) {
     cv::cvtColor(bgr, bgr, cv::COLOR_BGRA2BGR);
    } else if (bgr.channels() == 1) {
     cv::cvtColor(bgr, bgr, cv::COLOR_GRAY2BGR);
    }
    break;
   }

   return bgr;
  }

  cv::Mat toMaskGray32F(const cv::Mat& mask)
  {
   if (mask.empty()) {
    return cv::Mat();
   }

   cv::Mat u8;
   switch (mask.depth()) {
   case CV_8U:
    u8 = mask;
    break;
   case CV_16U:
    mask.convertTo(u8, CV_8U, 255.0 / 65535.0);
    break;
   case CV_32F:
   case CV_64F:
    mask.convertTo(u8, CV_8U, 255.0);
    break;
   default:
    mask.convertTo(u8, CV_8U);
    break;
   }

   cv::Mat gray;
   switch (u8.channels()) {
   case 1:
    gray = u8;
    break;
   case 3:
    cv::cvtColor(u8, gray, cv::COLOR_BGR2GRAY);
    break;
   case 4:
    cv::cvtColor(u8, gray, cv::COLOR_BGRA2GRAY);
    break;
   default:
    cv::cvtColor(u8, gray, cv::COLOR_BGR2GRAY);
    break;
   }

   gray.convertTo(gray, CV_32FC1, 1.0 / 255.0);
   return gray;
  }
 } // namespace

 LIBRARY_DLL_API bool applyStillLightLeak(
  const cv::Mat& sourceLinearPremultipliedRgba,
  cv::Mat& destinationLinearPremultipliedRgba,
  const LightLeakParameters& parameters
 ) {
  if (sourceLinearPremultipliedRgba.empty() ||
      sourceLinearPremultipliedRgba.type() != CV_32FC4 ||
      (parameters.look != LightLeakLook::EdgeWash &&
       parameters.look != LightLeakLook::AnamorphicStreak &&
       parameters.look != LightLeakLook::OrganicGateLeak) ||
      (parameters.alphaPolicy != LightLeakAlphaPolicy::PreserveSourceCoverage &&
       parameters.alphaPolicy != LightLeakAlphaPolicy::ClipLeakToSourceCoverage &&
       parameters.alphaPolicy != LightLeakAlphaPolicy::ExpandFromLeakEnergy)) {
   return false;
  }

  const int width = sourceLinearPremultipliedRgba.cols;
  const int height = sourceLinearPremultipliedRgba.rows;
  const auto finiteOr = [](float value, float fallback) {
   return std::isfinite(value) ? value : fallback;
  };
  const float strength = std::clamp(finiteOr(parameters.strength, 0.55f), 0.0f, 8.0f);
  const float streakSourceX = std::clamp(
      finiteOr(parameters.streakSourceX, 0.01f), 0.0f, 1.0f);
  const float streakSourceY = std::clamp(
      finiteOr(parameters.streakSourceY, 0.105f), 0.0f, 1.0f);
  const float streakTheta = std::clamp(
      finiteOr(parameters.streakAngleDegrees, 2.5783101f), -45.0f, 45.0f) *
      (3.14159265358979323846f / 180.0f);
  const float streakCos = std::cos(streakTheta);
  const float streakSin = std::sin(streakTheta);
  const float expansionGain =
      std::clamp(finiteOr(parameters.alphaExpansionGain, 1.1f), 0.0f, 1000.0f);
  const float expansionLimit =
      std::clamp(finiteOr(parameters.alphaExpansionLimit, 0.82f), 0.0f, 1.0f);
  if (strength <= 0.0f) {
   sourceLinearPremultipliedRgba.copyTo(destinationLinearPremultipliedRgba);
   return true;
  }

  destinationLinearPremultipliedRgba.create(height, width, CV_32FC4);
  const bool edgeWash = parameters.look == LightLeakLook::EdgeWash;
  const bool anamorphicStreak = parameters.look == LightLeakLook::AnamorphicStreak;
  const float screenAmount = parameters.look == LightLeakLook::OrganicGateLeak
      ? 0.34f
      : 0.8f;
  Parallel::ForPixels(0, height, width, height, [&](int y) {
   const auto* sourceRow = sourceLinearPremultipliedRgba.ptr<cv::Vec4f>(y);
   auto* destinationRow = destinationLinearPremultipliedRgba.ptr<cv::Vec4f>(y);
   const float ynEdge = static_cast<float>(y) / static_cast<float>(std::max(1, height - 1));
   const float ynStreak = static_cast<float>(y) / static_cast<float>(height);
   for (int x = 0; x < width; ++x) {
    const float xnEdge = static_cast<float>(x) / static_cast<float>(std::max(1, width - 1));
    const float xnStreak = static_cast<float>(x) / static_cast<float>(width);
    cv::Vec3f leak;
    if (edgeWash) {
     const float topLeft = std::exp(-((xnEdge / 0.34f) * (xnEdge / 0.34f) +
                                      (ynEdge / 0.82f) * (ynEdge / 0.82f)) * 1.7f);
     const float topEdge = std::exp(-((ynEdge / 0.19f) * (ynEdge / 0.19f) +
                                      ((xnEdge - 0.24f) / 0.56f) *
                                          ((xnEdge - 0.24f) / 0.56f)) * 1.5f);
     const float sideDistance = (1.0f - xnEdge) / 0.16f;
     const float sideVertical = (ynEdge - 0.23f) / 0.58f;
     const float side = std::exp(-(sideDistance * sideDistance +
                                   sideVertical * sideVertical) * 1.4f);
     const float warm = topLeft * 0.85f + topEdge * 0.46f;
     leak = cv::Vec3f(warm * 0.80f + side * 0.12f,
                      warm * 0.36f + side * 0.12f,
                      warm * 0.10f + side * 0.32f);
    } else if (anamorphicStreak) {
     const float dx = xnStreak - streakSourceX + 0.03f;
     const float dy = ynStreak - streakSourceY;
     const float along = streakCos * dx + streakSin * dy;
     const float across = -streakSin * dx + streakCos * dy;
     const float alongTerm = (along / 0.78f) * (along / 0.78f);
     const float beam = std::exp(-0.5f * (alongTerm +
                                          (across / 0.018f) * (across / 0.018f)));
     const float sourceDx = (xnStreak - streakSourceX) / 0.11f;
     const float sourceDy = (ynStreak - streakSourceY) / 0.15f;
     const float source = std::exp(-0.5f * (sourceDx * sourceDx + sourceDy * sourceDy));
     const float redAcross = (across + 0.008f) / 0.022f;
     const float redStreak = std::exp(-0.5f * (alongTerm + redAcross * redAcross));
     const float blueAcross = (across - 0.010f) / 0.026f;
     const float blueStreak = std::exp(-0.5f * (alongTerm + blueAcross * blueAcross));
     leak = cv::Vec3f(beam * 0.86f + redStreak * 0.30f + source * 0.86f,
                      beam * 0.68f + source * 0.58f,
                      beam * 0.50f + blueStreak * 0.38f + source * 0.38f);
    } else {
     const auto lobe = [](float x, float y, float cx, float cy,
                          float rx, float ry, float weight) {
      const float dx = (x - cx) / rx;
      const float dy = (y - cy) / ry;
      return weight * std::exp(-0.5f * (dx * dx + dy * dy));
     };
     const float edge = std::exp(-((1.0f - ynEdge) / 0.30f) *
                                  ((1.0f - ynEdge) / 0.30f));
     const float pooling = 0.30f + 0.70f * std::sin(6.2831853f *
                                                    (xnEdge * 0.72f + 0.08f));
     const float amber = edge * pooling * 0.50f +
         lobe(xnEdge, ynEdge, 0.91f, 0.78f, 0.23f, 0.30f, 0.65f) +
         lobe(xnEdge, ynEdge, 0.60f, 0.98f, 0.33f, 0.12f, 0.40f) +
         lobe(xnEdge, ynEdge, 0.06f, 0.66f, 0.12f, 0.36f, 0.42f);
     const float magenta = lobe(xnEdge, ynEdge, 0.06f, 0.48f,
                                0.20f, 0.34f, 0.70f) +
                           lobe(xnEdge, ynEdge, 0.28f, 0.92f,
                                0.25f, 0.13f, 0.18f);
     const float blue = lobe(xnEdge, ynEdge, 0.98f, 0.38f,
                             0.08f, 0.27f, 0.35f);
     leak = cv::Vec3f(amber * 0.92f + magenta * 0.46f,
                      amber * 0.48f + magenta * 0.16f + blue * 0.10f,
                      amber * 0.20f + magenta * 0.58f + blue * 0.48f);
    }

    const cv::Vec4f source = sourceRow[x];
    const float sourceAlpha = std::clamp(finiteOr(source[3], 0.0f), 0.0f, 1.0f);
    const float inverseAlpha = sourceAlpha > 1.0e-6f ? 1.0f / sourceAlpha : 0.0f;
    cv::Vec3f straightSource(
        finiteOr(source[0], 0.0f) * inverseAlpha,
        finiteOr(source[1], 0.0f) * inverseAlpha,
        finiteOr(source[2], 0.0f) * inverseAlpha);
    cv::Vec3f straightResult;
    cv::Vec3f leakDelta;
    const float amount = screenAmount * strength;
    for (int channel = 0; channel < 3; ++channel) {
     const float safeLeak = std::max(0.0f, finiteOr(leak[channel], 0.0f));
     const float result = 1.0f - (1.0f - straightSource[channel]) *
                                   std::exp(-safeLeak * amount);
     straightResult[channel] = finiteOr(result, straightSource[channel]);
     leakDelta[channel] = std::max(0.0f, straightResult[channel] - straightSource[channel]);
    }

    float outputAlpha = sourceAlpha;
    cv::Vec3f outputPremultiplied;
    switch (parameters.alphaPolicy) {
    case LightLeakAlphaPolicy::PreserveSourceCoverage:
     outputPremultiplied = straightResult * sourceAlpha;
     break;
    case LightLeakAlphaPolicy::ClipLeakToSourceCoverage:
     outputPremultiplied =
         (straightSource + leakDelta * sourceAlpha) * sourceAlpha;
     break;
    case LightLeakAlphaPolicy::ExpandFromLeakEnergy: {
     const float leakLuminance = leakDelta[0] * 0.2126f + leakDelta[1] * 0.7152f +
                                 leakDelta[2] * 0.0722f;
     const float coverage = std::min(expansionLimit,
                                     1.0f - std::exp(-expansionGain * leakLuminance));
     outputAlpha = sourceAlpha + (1.0f - sourceAlpha) * coverage;
     outputPremultiplied = straightResult * outputAlpha;
     break;
    }
    }
    destinationRow[x] = cv::Vec4f(outputPremultiplied[0], outputPremultiplied[1],
                                  outputPremultiplied[2], outputAlpha);
   }
  });
  return true;
 }

 LIBRARY_DLL_API bool applySoftKneeMultiScaleGlow(
  const cv::Mat& sourceLinearPremultipliedRgba,
  cv::Mat& destinationLinearPremultipliedRgba,
  SoftKneeGlowWorkspace& workspace,
  const SoftKneeGlowParameters& parameters
 ) {
  if (sourceLinearPremultipliedRgba.empty() ||
      sourceLinearPremultipliedRgba.type() != CV_32FC4 ||
      (parameters.alphaPolicy != SoftKneeGlowAlphaPolicy::PreserveSourceCoverage &&
       parameters.alphaPolicy != SoftKneeGlowAlphaPolicy::ExpandFromGlowEnergy)) {
   return false;
  }

  const int width = sourceLinearPremultipliedRgba.cols;
  const int height = sourceLinearPremultipliedRgba.rows;
  const auto finiteOr = [](float value, float fallback) {
   return std::isfinite(value) ? value : fallback;
  };
  const float threshold = std::clamp(finiteOr(parameters.threshold, 0.45f), 0.0f, 16.0f);
  const float knee = std::clamp(finiteOr(parameters.knee, 0.30f), 1.0e-5f, 16.0f);
  const float amount = std::clamp(finiteOr(parameters.amount, 1.45f), 0.0f, 64.0f);
  const float radius = std::clamp(finiteOr(parameters.radius, 8.0f), 0.0f, 256.0f);
  const float alphaExpansionGain =
      std::clamp(finiteOr(parameters.alphaExpansionGain, 20.0f), 0.0f, 1000.0f);

  if (radius <= 0.0f || amount <= 0.0f) {
   sourceLinearPremultipliedRgba.copyTo(destinationLinearPremultipliedRgba);
   return true;
  }

  int lastActiveLevel = -1;
  for (int level = 0; level < 4; ++level) {
   if (std::clamp(finiteOr(parameters.weights[static_cast<std::size_t>(level)], 0.0f),
                  0.0f, 1.0f) > 0.0f) {
    lastActiveLevel = level;
   }
  }
  if (lastActiveLevel < 0) {
   sourceLinearPremultipliedRgba.copyTo(destinationLinearPremultipliedRgba);
   return true;
  }

  workspace.fullResolutionSeedAndUpsample.create(height, width, CV_32FC3);
  const float thresholdLow = threshold - knee;
  const float inverseKneeWidth = 1.0f / (2.0f * knee);
  Parallel::ForPixels(0, height, width, height, [&](int y) {
   const auto* sourceRow = sourceLinearPremultipliedRgba.ptr<cv::Vec4f>(y);
   auto* seedRow = workspace.fullResolutionSeedAndUpsample.ptr<cv::Vec3f>(y);
   for (int x = 0; x < width; ++x) {
    const cv::Vec4f pixel = sourceRow[x];
    const float alpha = std::clamp(finiteOr(pixel[3], 0.0f), 0.0f, 1.0f);
    const float red = alpha > 1.0e-6f ? finiteOr(pixel[0], 0.0f) : 0.0f;
    const float green = alpha > 1.0e-6f ? finiteOr(pixel[1], 0.0f) : 0.0f;
    const float blue = alpha > 1.0e-6f ? finiteOr(pixel[2], 0.0f) : 0.0f;
    const float inverseAlpha = alpha > 1.0e-6f ? 1.0f / alpha : 0.0f;
    const float luminance =
        (red * 0.2126f + green * 0.7152f + blue * 0.0722f) * inverseAlpha;
    const float kneePosition = std::clamp(
        (luminance - thresholdLow) * inverseKneeWidth, 0.0f, 1.0f);
    const float softMask = kneePosition * kneePosition * (3.0f - 2.0f * kneePosition);
    seedRow[x] = cv::Vec3f(red * softMask, green * softMask, blue * softMask);
   }
  });

  workspace.bloom.create(height, width, CV_32FC3);
  const float firstWeight = std::clamp(finiteOr(parameters.weights[0], 0.0f), 0.0f, 1.0f);
  if (firstWeight > 0.0f) {
   const float firstScale = std::clamp(finiteOr(parameters.scales[0], 0.0f), 0.0f, 16.0f);
   const float firstSigma = std::max(0.35f, radius * firstScale);
   cv::GaussianBlur(workspace.fullResolutionSeedAndUpsample, workspace.bloom,
                    cv::Size(0, 0), firstSigma, firstSigma, cv::BORDER_REPLICATE);
   workspace.bloom *= firstWeight;
  } else {
   workspace.bloom.setTo(cv::Scalar::all(0.0));
  }

  const cv::Size maximumHalfSize(std::max(1, (width + 1) / 2),
                                 std::max(1, (height + 1) / 2));
  const cv::Size maximumQuarterSize(std::max(1, (maximumHalfSize.width + 1) / 2),
                                    std::max(1, (maximumHalfSize.height + 1) / 2));
  // Level 1 and 3 share the half-resolution buffer; level 2 needs only a
  // quarter-resolution buffer. GaussianBlur supports in-place operation, so
  // there is no separate half-resolution blur scratch.
  workspace.downsampleSeeds[0].create(
      maximumQuarterSize.height, maximumQuarterSize.width, CV_32FC3);
  workspace.downsampleSeeds[1].create(
      maximumHalfSize.height, maximumHalfSize.width, CV_32FC3);

  cv::Size previousSize(width, height);
  for (int level = 1; level <= lastActiveLevel; ++level) {
   const cv::Size levelSize(std::max(1, (previousSize.width + 1) / 2),
                            std::max(1, (previousSize.height + 1) / 2));
   cv::Mat& downsample = workspace.downsampleSeeds[static_cast<std::size_t>(level % 2)];
   cv::Mat downsampleRegion = downsample(cv::Rect(0, 0, levelSize.width, levelSize.height));
   if (level == 1) {
    cv::pyrDown(workspace.fullResolutionSeedAndUpsample, downsampleRegion, levelSize);
   } else {
    const cv::Mat& previous =
        workspace.downsampleSeeds[static_cast<std::size_t>((level - 1) % 2)];
    const cv::Mat previousRegion =
        previous(cv::Rect(0, 0, previousSize.width, previousSize.height));
    cv::pyrDown(previousRegion, downsampleRegion, levelSize);
   }

   const float weight = std::clamp(
       finiteOr(parameters.weights[static_cast<std::size_t>(level)], 0.0f), 0.0f, 1.0f);
   if (weight <= 0.0f) {
    previousSize = levelSize;
    continue;
   }

   const float scale = std::clamp(
       finiteOr(parameters.scales[static_cast<std::size_t>(level)], 0.0f), 0.0f, 16.0f);
   const float levelDivisor = static_cast<float>(1u << level);
   const float sigma = std::max(0.35f, radius * scale / levelDivisor);
   cv::GaussianBlur(downsampleRegion, downsampleRegion, cv::Size(0, 0), sigma, sigma,
                    cv::BORDER_REPLICATE);
   cv::resize(downsampleRegion, workspace.fullResolutionSeedAndUpsample,
              cv::Size(width, height), 0.0, 0.0, cv::INTER_LINEAR);

   cv::addWeighted(workspace.bloom, 1.0,
                   workspace.fullResolutionSeedAndUpsample,
                   weight, 0.0, workspace.bloom);
   previousSize = levelSize;
  }

  const float tintRed = std::clamp(finiteOr(parameters.tint[0], 1.0f), 0.0f, 16.0f);
  const float tintGreen = std::clamp(finiteOr(parameters.tint[1], 1.0f), 0.0f, 16.0f);
  const float tintBlue = std::clamp(finiteOr(parameters.tint[2], 1.0f), 0.0f, 16.0f);
  destinationLinearPremultipliedRgba.create(height, width, CV_32FC4);
  Parallel::ForPixels(0, height, width, height, [&](int y) {
   const auto* sourceRow = sourceLinearPremultipliedRgba.ptr<cv::Vec4f>(y);
   const auto* bloomRow = workspace.bloom.ptr<cv::Vec3f>(y);
   auto* destinationRow = destinationLinearPremultipliedRgba.ptr<cv::Vec4f>(y);
   for (int x = 0; x < width; ++x) {
    const cv::Vec4f source = sourceRow[x];
    const cv::Vec3f bloom = bloomRow[x];
    const float sourceAlpha = std::clamp(finiteOr(source[3], 0.0f), 0.0f, 1.0f);
    const float sourceRed = sourceAlpha > 1.0e-6f ? finiteOr(source[0], 0.0f) : 0.0f;
    const float sourceGreen = sourceAlpha > 1.0e-6f ? finiteOr(source[1], 0.0f) : 0.0f;
    const float sourceBlue = sourceAlpha > 1.0e-6f ? finiteOr(source[2], 0.0f) : 0.0f;
    const float coverageScale =
        parameters.alphaPolicy == SoftKneeGlowAlphaPolicy::PreserveSourceCoverage
            ? sourceAlpha
            : 1.0f;
    const float redContribution = finiteOr(bloom[0], 0.0f) * amount * tintRed * coverageScale;
    const float greenContribution = finiteOr(bloom[1], 0.0f) * amount * tintGreen * coverageScale;
    const float blueContribution = finiteOr(bloom[2], 0.0f) * amount * tintBlue * coverageScale;
    float outputAlpha = sourceAlpha;
    if (parameters.alphaPolicy == SoftKneeGlowAlphaPolicy::ExpandFromGlowEnergy) {
     const float bloomLuminance = std::max(
         0.0f, redContribution * 0.2126f + greenContribution * 0.7152f +
                   blueContribution * 0.0722f);
     const float glowCoverage = 1.0f - std::exp(-alphaExpansionGain * bloomLuminance);
     outputAlpha = sourceAlpha + (1.0f - sourceAlpha) * glowCoverage;
    }
    destinationRow[x] = cv::Vec4f(
        finiteOr(sourceRed + redContribution, sourceRed),
        finiteOr(sourceGreen + greenContribution, sourceGreen),
        finiteOr(sourceBlue + blueContribution, sourceBlue),
        outputAlpha);
   }
  });
  return true;
 }

 LIBRARY_DLL_API void applySimpleGlow(
  const cv::Mat& src,
  const cv::Mat& mask,
  cv::Mat& dst,
  const cv::Scalar& glowColor,
  float glowGain,
  int layerCount,
  float baseSigma,
  float sigmaGrowth,
  float baseAlpha,
  float alphaFalloff,
  bool additiveBlend,
  bool linearSpace
 ) {
  if (src.empty()) return;

  try {
   const cv::Mat baseBgr = toBgr8U(src);
   if (baseBgr.empty()) {
    dst = src.clone();
    return;
   }

   const cv::Mat maskGray = toMaskGray32F(mask);

   cv::Mat glowAccum = cv::Mat::zeros(baseBgr.size(), CV_32FC3);
   cv::Mat srcFloat;
   baseBgr.convertTo(srcFloat, CV_32FC3, linearSpace ? 1.0 / 255.0 : 1.0);

   const int safeLayerCount = std::max(1, layerCount);
   const float safeBaseSigma = std::max(0.01f, baseSigma);
   const float safeSigmaGrowth = std::max(0.01f, sigmaGrowth);
   const float safeBaseAlpha = std::clamp(baseAlpha, 0.0f, 1.0f);
   const float safeAlphaFalloff = std::clamp(alphaFalloff, 0.0f, 1.0f);

   for (int i = 0; i < safeLayerCount; ++i) {
    float sigma = safeBaseSigma * std::pow(safeSigmaGrowth, i);
    float alpha = safeBaseAlpha * std::pow(safeAlphaFalloff, i);

    cv::Mat blurred;
     cv::GaussianBlur(srcFloat, blurred, cv::Size(), sigma, sigma);

     // 色乗算: ベクトル化演算でピクセルループを排除
     cv::Mat colored;
     const cv::Vec3f colorScale(
      static_cast<float>(glowColor[0] / 255.0),
      static_cast<float>(glowColor[1] / 255.0),
      static_cast<float>(glowColor[2] / 255.0));
     cv::multiply(blurred, colorScale, colored);

     // マスク適用: ベクトル化演算でピクセルループを排除
     if (!maskGray.empty()) {
      cv::multiply(colored, maskGray, colored, 1.0, CV_32FC3);
     }

     glowAccum += colored * alpha;
   }

   glowAccum *= glowGain;
   cv::Mat glowFinal;
   glowAccum.convertTo(glowFinal, CV_8UC3, linearSpace ? 255.0 : 1.0);

   if (additiveBlend) {
    cv::add(baseBgr, glowFinal, dst);
   } else {
    dst = glowFinal;
   }
  } catch (const cv::Exception& e) {
   std::cerr << "[applySimpleGlow] OpenCV exception: " << e.what() << std::endl;
   dst = src.clone();
  } catch (const std::exception& e) {
   std::cerr << "[applySimpleGlow] std::exception: " << e.what() << std::endl;
   dst = src.clone();
  } catch (...) {
   std::cerr << "[applySimpleGlow] unknown exception" << std::endl;
   dst = src.clone();
  }
 }


 LIBRARY_DLL_API cv::Mat applyGlintGlow(const cv::Mat& src, float threshold_value, int blur_radius, float intensity) {
  if (src.empty() || src.channels() != 4 || src.depth() != CV_32F) {
   std::cerr << "Input image must be CV_32FC4." << std::endl;
   return cv::Mat();
  }

  cv::Mat glow_map;
  cv::Mat bright_areas;
  cv::Mat rgb_src;
  cv::Mat alpha_src;

  // 1. RGBAからRGBとアルファチャンネルを分離
  // OCV 4.x以降ではsplit()が効率的
  std::vector<cv::Mat> channels;
  cv::split(src, channels);
  rgb_src = channels[0]; // B
  cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, rgb_src); // BGRとして結合

  alpha_src = channels[3]; // アルファチャンネル

  // 2. 輝度の抽出 (RGB画像をグレースケールに変換し、しきい値処理)
  // 輝度計算: L = 0.2126 * R + 0.7152 * G + 0.0722 * B (Rec.709)
  // OpenCVのcvtColor(BGR2GRAY)も輝度計算を使いますが、floatなので注意
  cv::Mat luminance_float;
  // BGRを直接グレースケールに変換 (float型を維持)
  cv::cvtColor(rgb_src, luminance_float, cv::COLOR_BGR2GRAY);

  // しきい値処理 (指定された閾値より明るい部分を抽出)
  // THRESH_BINARY は閾値より小さい値を0に、大きい値をmaxval(ここでは1.0)にする
  cv::threshold(luminance_float, bright_areas, threshold_value, 1.0, cv::THRESH_BINARY);

  // bright_areasをRGBチャンネルに複製して、元の画像の明るい部分だけを抽出
  std::vector<cv::Mat> bright_channels(3);
  bright_channels[0] = bright_areas.mul(channels[0]); // B
  bright_channels[1] = bright_areas.mul(channels[1]); // G
  bright_channels[2] = bright_areas.mul(channels[2]); // R
  cv::Mat bright_rgb_extract;
  cv::merge(bright_channels, bright_rgb_extract);


  // 3. ぼかし (ガウスぼかしを適用)
  // カーネルサイズは奇数である必要がある
  int kernel_size = blur_radius * 2 + 1;
  if (kernel_size < 1) kernel_size = 1; // 最小1

  cv::GaussianBlur(bright_rgb_extract, glow_map, cv::Size(kernel_size, kernel_size), 0);

  // 4. 合成 (元の画像にぼかした画像を加算)
  // AddWeighted はアルファブレンドだが、ここでは直接加算として使う
  // output = src1 * alpha + src2 * beta + gamma;
  // glow_map * intensity + src
  cv::Mat output_rgb;
  cv::addWeighted(rgb_src, 1.0, glow_map, intensity, 0.0, output_rgb);

  // 最終的な出力画像をBGRAで再結合
  std::vector<cv::Mat> output_channels(4);
  cv::split(output_rgb, output_channels); // BGRをB,G,Rに分離
  output_channels[3] = alpha_src; // 元のアルファチャンネルを再利用

  cv::Mat final_output;
  cv::merge(output_channels, final_output);

  return final_output;
 }

 LIBRARY_DLL_API void drawRandomDottedVerticalLine(cv::Mat& image, int x_center, int line_width, long num_points_to_try, unsigned int seed, const cv::Scalar& point_color, int point_size)
 {
  if (image.empty()) return;

  int width = image.cols;
  int height = image.rows;

  // 1. 縦ラインの輝度マップ（確率マップ）を生成
  cv::Mat prob_map = cv::Mat::zeros(height, width, CV_32FC1); // float型グレースケール

  // ラインの中心から離れるほど確率が低くなるようにグラデーションを作成
  Parallel::ForPixels(0, width, width, height, [&](int x) {
   float dist_from_center = std::abs(x - x_center);
   if (dist_from_center < line_width / 2.0) {
	// ラインの中心に近いほど確率を高くする
	// ガウス関数や線形減衰などで形状を調整
	prob_map.col(x).setTo(1.0f - (dist_from_center / (line_width / 2.0)) * 0.8f); // 線形減衰の例
   }
   else {
	prob_map.col(x).setTo(0.0f); // ラインの範囲外は0
   }
  });
  // 少しぼかして滑らかにする (オプション)
  cv::GaussianBlur(prob_map, prob_map, cv::Size(3, 3), 0);
  // 確率を0.0-1.0に正規化 (setToで最大1.0になるように設定していれば不要な場合も)
  double minVal, maxVal;
  cv::minMaxLoc(prob_map, &minVal, &maxVal);
  if (maxVal > 0) {
   prob_map /= maxVal;
  }


  // 2. 再現性のある乱数ジェネレータの初期化
  auto rng = ArtifactCore::makeRandomStream(
      static_cast<std::uint64_t>(seed));

  // 3. 点生成と描画
  long points_drawn = 0;
  for (long i = 0; i < num_points_to_try; ++i) {
   // ランダムな座標を選択
   int x = static_cast<int>(rng.unitFloat() * width);
   int y = static_cast<int>(rng.unitFloat() * height);

   if (x < 0 || x >= width || y < 0 || y >= height) continue;

   const float probability = prob_map.ptr<float>(y)[x];

   if (rng.unitFloat() < probability) {
	cv::circle(image, cv::Point(x, y), point_size, point_color, -1, cv::LINE_AA);
	points_drawn++;
   }
  }
  std::cout << "Total points drawn: " << points_drawn << std::endl;
 }

 LIBRARY_DLL_API  cv::Mat applyVerticalGlow(const cv::Mat& src, float threshold_value, int vertical_blur_radius, float intensity)
 {
  cv::Mat processed_src = src; // まずは入力画像をそのまま使用

  // 入力画像がCV_32FC4でない場合、変換を試みる
  if (src.empty() || src.channels() != 4 || src.depth() != CV_32F) {
   std::cerr << "Input image is not CV_32FC4. Attempting conversion..." << std::endl;
   processed_src = convertToFloat32BGRA(src); // ヘルパー関数で変換
   if (processed_src.empty()) {
	// 変換に失敗した場合
	std::cerr << "Failed to convert input image to CV_32FC4. Aborting." << std::endl;
	return cv::Mat();
   }
  }

  // ここから、以降の処理では `processed_src` を使用する
  // `processed_src` は常に CV_32FC4 であることが保証される
  cv::Mat glow_map;
  cv::Mat bright_areas;
  cv::Mat rgb_src;   // これはBGR順になる
  cv::Mat alpha_src;

  // 1. BGRAからBGRとアルファチャンネルを分離
  std::vector<cv::Mat> channels;
  cv::split(processed_src, channels); // processed_src が BGRA (B, G, R, A) の順であれば
  // channels[0]=B, channels[1]=G, channels[2]=R, channels[3]=A

  // BGRとして結合（channels[0], channels[1], channels[2] は B, G, R）
  cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, rgb_src); // これで rgb_src は BGR順になる
  alpha_src = channels[3]; // アルファチャンネル

  // 2. 輝度の抽出 (BGR画像をグレースケールに変換し、しきい値処理)
  cv::Mat luminance_float;
  // ここは元々 COLOR_BGR2GRAY なので修正不要 (rgb_src は BGR順)
  cv::cvtColor(rgb_src, luminance_float, cv::COLOR_BGR2GRAY);

  cv::threshold(luminance_float, bright_areas, threshold_value, 1.0, cv::THRESH_BINARY);

  std::vector<cv::Mat> bright_channels(3);
  // bright_areas は1チャンネル、channels[0] (B), channels[1] (G), channels[2] (R) もそれぞれ1チャンネル
  bright_channels[0] = bright_areas.mul(channels[0]); // Bチャンネルに適用
  bright_channels[1] = bright_areas.mul(channels[1]); // Gチャンネルに適用
  bright_channels[2] = bright_areas.mul(channels[2]); // Rチャンネルに適用
  cv::Mat bright_rgb_extract;
  cv::merge(bright_channels, bright_rgb_extract); // bright_rgb_extract も BGR順

  // 3. 縦方向のみのぼかし (Gaussian Blur with asymmetric kernel)
  int kernel_size_y = vertical_blur_radius * 2 + 1;
  if (kernel_size_y < 1) kernel_size_y = 1;

  int kernel_size_x = vertical_blur_radius * 2 + 1; // 縦と同じロジックで計算
  if (kernel_size_x < 1) kernel_size_x = 1;

  cv::Mat glow_map_vertical;
  cv::Mat glow_map_horizontal;

  cv::GaussianBlur(bright_rgb_extract, glow_map_vertical, cv::Size(1, kernel_size_y), 0); // bright_rgb_extract は BGR順

  cv::GaussianBlur(bright_rgb_extract, glow_map_horizontal, cv::Size(kernel_size_x, 1), 0);

  cv::addWeighted(glow_map_vertical, 1.0, glow_map_horizontal, 1.0, 0.0, glow_map); // 単純に加算

  // 4. 合成 (元の画像にぼかした画像を加算)
  cv::Mat output_rgb;
  cv::addWeighted(rgb_src, 1.0,glow_map, intensity, 0.0, output_rgb); // rgb_src は BGR順、glow_map も BGR順



  // 最終的な出力画像をBGRAで再結合
  std::vector<cv::Mat> output_channels_to_merge; // 新しいベクトルを定義

  // output_rgb は BGR順の3チャンネル画像なので、それを分割
  std::vector<cv::Mat> bgr_channels_from_output;
  cv::split(output_rgb, bgr_channels_from_output); // output_rgbをBGRの3チャンネルに分割

  // 分割したB, G, Rチャンネルを新しいベクトルに追加
  output_channels_to_merge.push_back(bgr_channels_from_output[0]); // Bチャンネル
  output_channels_to_merge.push_back(bgr_channels_from_output[1]); // Gチャンネル
  output_channels_to_merge.push_back(bgr_channels_from_output[2]); // Rチャンネル

  // 最後にアルファチャンネルを追加
  output_channels_to_merge.push_back(alpha_src); // 元のアルファチャンネルを再利用

  cv::Mat final_output;
  // output_channels_to_merge を使ってBGRAとして結合
  cv::merge(output_channels_to_merge, final_output);

  return final_output;
 }

}
