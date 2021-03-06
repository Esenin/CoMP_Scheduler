#pragma once

#include "itrend-indicator.h"

class WmaIndicator : public ITrendIndicator
{
public:
  enum MovingAverageAlgo
  {
    weightedMovingAverage
    , simpleMovingMedian

  };

  WmaIndicator(CsiJournalPtr j);

  bool isLastOutlier(CellId cellId, size_t lPointer = 0);

private:
  WmaIndicator(const WmaIndicator&) = delete;
  WmaIndicator& operator=(const WmaIndicator&) = delete;

  std::function<double (const CsiArray&, size_t)> mCalcMaFunc;

  double updateHook(CellId cellId) override;

  bool haveTooLittleValues();

  static double calcWMA(const CsiArray &csiArray, size_t lPointer);
  static double calcSMM(const CsiArray &csiDeque, size_t lPointer);
};

using UniqWmaIndicator = std::unique_ptr<WmaIndicator>;

