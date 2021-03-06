#include "comp-decision-algo.h"

#include <cmath>
#include <algorithm>

CompSchedulingAlgo::CompSchedulingAlgo(CsiJournalPtr j, CellIdVectorPtr compGroup)
  : mCsiJournal(j)
  , mCompGroup(compGroup)
  , mWmaIndicator(new WmaIndicator(j))
  , mKamaIndicator(new KamaIndicator(j))
  , mInterpolation(new InterpolationIndicator(j))
  , mApproxIndicator(new ApproximationIndicator(j))
{
  assert(j && compGroup);

  mMovingScoreLogger.open("output/moving_score.log", std::ios_base::out | std::ios_base::trunc);
  assert(mMovingScoreLogger.is_open());
  mMovingScoreLogger << "% time [us]\tcellId\tcellId\tvalue\n";

//  mKamaIndicator->setPreventiveAnalysis(true);
//  mWmaIndicator->setPreventiveAnalysis(true);
}

void CompSchedulingAlgo::setJournal(CsiJournalPtr j)
{
  mCsiJournal = j;
  assert(j);
}

void CompSchedulingAlgo::setCompGroup(CellIdVectorPtr cg)
{
  mCompGroup = cg;
  assert(cg);
}

CompSchedulingAlgo::~CompSchedulingAlgo()
{
  mMovingScoreLogger.flush();
  mMovingScoreLogger.close();
}

void CompSchedulingAlgo::update(CellId cellId)
{
  removeOldValues();
  mWmaIndicator->update(cellId);
  mKamaIndicator->update(cellId);
  mInterpolation->update(cellId);
//  writeScore(cellId, mInterpolation->forecast(cellId), mCsiJournal->at(cellId).back().second);
  writeScore(cellId, mApproxIndicator->forecast(cellId), mCsiJournal->at(cellId).back().second);
//  writeScore(cellId, mKamaIndicator->lastValueFor(cellId), mCsiJournal->at(cellId).back().second);
//  writeScore(cellId, mWmaIndicator->lastValueFor(cellId), mCsiJournal->at(cellId).back().second);
//  writeScore(cellId, weightedLastValue(cellId), mCsiJournal->at(cellId).back().second);
}

CellId CompSchedulingAlgo::redefineBestCell(CellId lastScheduled)
{
  switch (SimConfig::algoType)
    {
    case SimConfig::naive:
      return predictorSimpleMaxValue(lastScheduled);
    case SimConfig::interpolation:
      return predictorInterpolationForecast(lastScheduled);
    case SimConfig::leastSquaresRegression:
    case SimConfig::chebyshevApprx:
      return predictorApproximationForecast(lastScheduled);
    case SimConfig::wmaRaw:
    case SimConfig::smmRaw:
      return predictorPureRawForecastWMA(lastScheduled);
    case SimConfig::kamaRaw:
      return predictorPureRawForecastKama(lastScheduled);
    case SimConfig::kamaPure:
      return predictorMAForecast(lastScheduled);
    case SimConfig::hybrid:
      return predictorWeightedForecast(lastScheduled);
    default:
      ERR("no link to impl");
      break;
    }
  return predictorSimpleMaxValue(lastScheduled);

}

CellId CompSchedulingAlgo::predictorPureRawForecastWMA(CellId lastScheduled)
{
  const double hysteresis = .2;

  std::map<CellId, double> signalForecast;

  CellId nextDecision = lastScheduled;
  double estimatedBestSignal = signalForecast[lastScheduled];
  bool choosed = false;

  for (auto cellId : *mCompGroup)
    {
      const CsiArray &csiArray = mCsiJournal->at(cellId);
      if (csiArray.size() <= 1)
        continue;


      const int diffCount = (csiArray.size() > 2)? 2 : 1;

      for (int k = 0; k < diffCount; k++)
        {
          signalForecast[cellId] += csiArray[csiArray.size() - 1 - k].second
              - csiArray[csiArray.size() - 1 - k - 1].second;
        }
      signalForecast[cellId] = signalForecast[cellId] / double(diffCount) + csiArray.back().second;
      if (mWmaIndicator->isLastOutlier(cellId))
        signalForecast[cellId] = mWmaIndicator->lastValueFor(cellId);

      bool isQualityRises = mWmaIndicator->isUpgoingTrend(cellId);
      bool isWmaBetter = mWmaIndicator->forecast(cellId) > mWmaIndicator->forecast(lastScheduled);
      bool isCurForecastBetterScheduledSignificantly =
          signalForecast[cellId] > signalForecast[lastScheduled] + hysteresis * 9;
      bool isCurForecastBetterScheduled = signalForecast[cellId] > signalForecast[lastScheduled] + hysteresis;

      if (isQualityRises
          && isCurForecastBetterScheduled
          && (isWmaBetter || isCurForecastBetterScheduledSignificantly)
          && signalForecast[cellId] > estimatedBestSignal)
        {
          nextDecision = cellId;
          estimatedBestSignal = signalForecast[cellId];
          choosed = true;
        }
    }

  if (!choosed)
    {
      double maxSignal = 0;
      for (auto cellId : *mCompGroup)
        {
          if (mWmaIndicator->forecast(cellId) > maxSignal)
            {
              maxSignal = mWmaIndicator->forecast(cellId);
              nextDecision = cellId;
            }
        }
    }

  return nextDecision;
}

CellId CompSchedulingAlgo::predictorPureRawForecastKama(CellId lastScheduled)
{
  const double hysteresis = .2;

  std::map<CellId, double> signalForecast;

  CellId nextDecision = lastScheduled;
  double estimatedBestSignal = signalForecast[lastScheduled];
  bool choosed = false;

  for (auto cellId : *mCompGroup)
    {
      const CsiArray &csiArray = mCsiJournal->at(cellId);
      if (csiArray.size() <= 1)
        continue;


      const int diffCount = (csiArray.size() > 2)? 2 : 1;

      for (int k = 0; k < diffCount; k++)
        {
          signalForecast[cellId] += csiArray[csiArray.size() - 1 - k].second
              - csiArray[csiArray.size() - 1 - k - 1].second;
        }
      signalForecast[cellId] = 2.0 * (signalForecast[cellId] / double(diffCount)) + csiArray.back().second;
      if (mWmaIndicator->isLastOutlier(cellId))
        signalForecast[cellId] = mKamaIndicator->lastValueFor(cellId);

      bool isQualityRises = mKamaIndicator->isUpgoingTrend(cellId);
      bool isWmaBetter = mKamaIndicator->forecast(cellId) > mKamaIndicator->forecast(lastScheduled);
      bool isCurForecastBetterScheduledSignificantly =
          signalForecast[cellId] > signalForecast[lastScheduled] + hysteresis * 9;
      bool isCurForecastBetterScheduled = signalForecast[cellId] > signalForecast[lastScheduled] + hysteresis;

      if (isQualityRises
          && isCurForecastBetterScheduled
          && (isWmaBetter || isCurForecastBetterScheduledSignificantly)
          && signalForecast[cellId] > estimatedBestSignal)
        {
          nextDecision = cellId;
          estimatedBestSignal = signalForecast[cellId];
          choosed = true;
        }
    }

  if (!choosed)
    {
      double maxSignal = 0;
      for (auto cellId : *mCompGroup)
        {
          if (mKamaIndicator->forecast(cellId) > maxSignal)
            {
              maxSignal = mKamaIndicator->forecast(cellId);
              nextDecision = cellId;
            }
        }
    }

  return nextDecision;
}

CellId CompSchedulingAlgo::predictorMAForecast(CellId lastScheduled)
{
  const double hysteresis = .2;

  std::map<CellId, double> signalForecast;
  signalForecast[lastScheduled] = mKamaIndicator->forecast(lastScheduled);

  CellId nextDecision = lastScheduled;
  double estimatedBestSignal = signalForecast[lastScheduled];
  bool choosed = false;

  for (auto cellId : *mCompGroup)
    {
      const CsiArray &csiArray = mCsiJournal->at(cellId);
      if (csiArray.size() <= 1)
        continue;


      signalForecast[cellId] = mKamaIndicator->forecast(cellId);


      bool isQualityRises = mKamaIndicator->isUpgoingTrend(cellId); //kama
      bool isWmaBetter = mKamaIndicator->forecast(cellId) > mKamaIndicator->forecast(lastScheduled); //kamakama
      bool isCurForecastBetterScheduled = signalForecast[cellId] > signalForecast[lastScheduled] + hysteresis;

      if (isQualityRises
          && isCurForecastBetterScheduled
          && (isWmaBetter)
          && signalForecast[cellId] > estimatedBestSignal)
        {
          nextDecision = cellId;
          estimatedBestSignal = signalForecast[cellId];
          choosed = true;
        }
    }

  if (!choosed)
    {
      double maxSignal = 0;
      for (auto cellId : *mCompGroup)
        {
          if (mKamaIndicator->forecast(cellId) > maxSignal) //kama
            {
              maxSignal = mKamaIndicator->forecast(cellId); //kama
              nextDecision = cellId;
            }
        }
    }

  return nextDecision;
}

CellId CompSchedulingAlgo::predictorWeightedForecast(CellId lastScheduled)
{
//  mKamaIndicator->setPreventiveAnalysis(false); // keep false
//  mWmaIndicator->setPreventiveAnalysis(false);  // keep false
  const double hysteresis = .2;

  CellId nextDecision = lastScheduled;
  double estimatedBestSignal = 0.0;

  bool choosed = false;

  for (auto cellId : *mCompGroup)
    {
      const CsiArray &csiArray = mCsiJournal->at(cellId);
      if (csiArray.size() <= 1)
        continue;

      const auto wforecast = weightedForecast(cellId);
      const auto wLastScheduledForecast = weightedForecast(lastScheduled);

      bool isQualityRises = mKamaIndicator->isCurrentBreaksUpwards(cellId);
      bool isWForecastBetter = wforecast > wLastScheduledForecast + hysteresis;
      bool isCrossSituation = std::abs(wforecast - wLastScheduledForecast) < 0.7
                              && mKamaIndicator->isUpgoingTrend(cellId)
                              && mKamaIndicator->isDescendingTrend(lastScheduled);

      if (((isQualityRises && isWForecastBetter)
          || isCrossSituation)
          && (wforecast > estimatedBestSignal))
        {
          nextDecision = cellId;
          estimatedBestSignal = wforecast;
          choosed = true;
        }
    }

  if (!choosed)
    {
      double maxSignal = 0;
      for (auto cellId : *mCompGroup)
        {
          if (mWmaIndicator->forecast(cellId) > maxSignal)
            {
              maxSignal = mWmaIndicator->forecast(cellId);
              nextDecision = cellId;
            }
        }
    }

  return nextDecision;
}

CellId CompSchedulingAlgo::predictorInterpolationForecast(CellId lastScheduled)
{
  CellId nextDecision = lastScheduled;
  double maxSignal = 0;
  bool hasChoosed = false;
  for (auto cellId : *mCompGroup)
    {
      auto forecast = mInterpolation->forecast(cellId);
      if (forecast > maxSignal && mKamaIndicator->isUpgoingTrend(cellId))
        {
          maxSignal = forecast;
          nextDecision = cellId;
          hasChoosed = true;
        }
    }
  if (!hasChoosed)
    for (auto cellId : *mCompGroup)
      {
        auto forecast = mInterpolation->forecast(cellId);
        if (forecast > maxSignal)
          {
            maxSignal = forecast;
            nextDecision = cellId;
            hasChoosed = true;
          }
      }
  return nextDecision;
}

CellId CompSchedulingAlgo::predictorApproximationForecast(CellId lastScheduled)
{
  CellId nextDecision = lastScheduled;
  double maxSignal = 0;
  bool hasChoosed = false;
  for (auto cellId : *mCompGroup)
    {
      auto forecast = mApproxIndicator->forecast(cellId);
      if (forecast > maxSignal && mKamaIndicator->isUpgoingTrend(cellId))
        {
          maxSignal = forecast;
          nextDecision = cellId;
          hasChoosed = true;
        }
    }
  if (!hasChoosed)
    for (auto cellId : *mCompGroup)
      {
        auto forecast = mApproxIndicator->forecast(cellId);
        if (forecast > maxSignal)
          {
            maxSignal = forecast;
            nextDecision = cellId;
          }
      }
  return nextDecision;
}

CellId CompSchedulingAlgo::predictorSimpleMaxValue(CellId lastScheduled)
{
  CellId nextDecision = lastScheduled;
  double maxSignal = 0;
  for (auto cellId : *mCompGroup)
    {
      if (mCsiJournal->at(cellId).back().second > maxSignal)
        {
          maxSignal = mCsiJournal->at(cellId).back().second;
          nextDecision = cellId;
        }
    }
  return nextDecision;
}

double CompSchedulingAlgo::weightedLastValue(CellId cellId)
{
  const auto effectRatio = mKamaIndicator->efficiencyRatio();
  return (mWmaIndicator->isLastOutlier(cellId))? mKamaIndicator->lastValueFor(cellId)
                                               : effectRatio * mKamaIndicator->lastValueFor(cellId)
                                                 + (1 - effectRatio) * mWmaIndicator->lastValueFor(cellId);
}

double CompSchedulingAlgo::weightedForecast(CellId cellId)
{
  const auto effectRatio = mKamaIndicator->efficiencyRatio();
  return (mWmaIndicator->isLastOutlier(cellId))? mKamaIndicator->forecast(cellId)
                                               : effectRatio * mKamaIndicator->forecast(cellId)
                                                 + (1 - effectRatio) * mWmaIndicator->forecast(cellId);
}


bool CompSchedulingAlgo::haveTooLittleValues()
{
  double probesCount = 0.0;
  for (auto &pair : *mCsiJournal)
    probesCount += pair.second.size();
  probesCount /= mCsiJournal->size();

  return probesCount < 2.0;
}


void CompSchedulingAlgo::writeScore(CellId cellId, double aveValue, double rawValue)
{
  mMovingScoreLogger << SimTimeProvider::getTime() << "\t" << cellId << "\t" << cellId << "\t"
                     << rawValue << "\n";
  mMovingScoreLogger << SimTimeProvider::getTime() << "\t" << cellId + 10 << "\t" << cellId + 10
                     << "\t" << aveValue << "\n";
}

void CompSchedulingAlgo::removeOldValues()
{
  std::vector<Time> winDurations {
      mInterpolation->windowDuration()
      , mKamaIndicator->windowDuration()
      , mWmaIndicator->windowDuration()
  };
  const auto windowDuration = *std::max_element(winDurations.begin(), winDurations.end());
  assert(windowDuration);
  const auto barrier = SimTimeProvider::getTime() - windowDuration;
  for (auto &csiPair : *mCsiJournal)
    {
      CsiArray &array = csiPair.second;
      while (array.size() > 1 && array.front().first < barrier)
        array.pop_front();
    }
}
