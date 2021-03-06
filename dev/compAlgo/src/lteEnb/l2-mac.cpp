#include "l2-mac.h"

#include "x2-channel.h"
#include "../simulator.h"

L2Mac::L2Mac()
{
  X2Channel::instance()->configurate(compMembersCount);
  mMacSapUser = new FfMacSchedSapUser;
  for (int i = 0; i < compMembersCount; i++)
    {
      mSchedulers.push_back(FfMacScheduler(i + 1));
      mSchedulers.back().setFfMacSchedSapUser(mMacSapUser);
    }

  std::string resultMacLocation = "./output/DlRlcStats.txt";
  mResultRlcStats.open(resultMacLocation, std::ios_base::out | std::ios_base::trunc);
  mResultRlcStats << "% start	end	CellId	IMSI	RNTI	LCID	nTxPDUs	TxBytes	nRxPDUs	RxBytes	delay"
                  << "	stdDev	min	max	PduSize	stdDev	min	max\n";

  std::string resultRsrpLocation = "./output/measurements.log";
  mResultMeasurements.open(resultRsrpLocation, std::ios_base::out | std::ios_base::trunc);
  mResultMeasurements << "% time[usec]	srcCellId	targetCellId	RSRP\n";

}

L2Mac::~L2Mac()
{
  delete mMacSapUser;
  mResultRlcStats.flush();
  mResultRlcStats.close();

  mResultMeasurements.flush();
  mResultMeasurements.close();

  printMacTimings();
  LOG("Not used timeframes: " << mMissedFrameCounter << "\t(about " << mMissedFrameCounter / 1000.0 << " [s])\n");
}

void L2Mac::activateDlCompFeature()
{
  mSchedulers.front().setLeader(true);
  mSchedulers.front().setCompGroup({1, 2, 3});

  l2Timeout(-1);
}

void L2Mac::makeScheduleDecision(int cellId, const DlRlcPacket &packet)
{
  static Time subframeTime = Converter::milliseconds(0);
  const Time curTime = SimTimeProvider::getTime();
  if (curTime > subframeTime)
    {
      if (mMacSapUser->getDirectCellId() == -1)
        {
          mMissedFrameCounter += 1;
          LOG(">" << subframeTime << "  frame miss");
        }
      subframeTime = curTime;
    }


  if (mMacSapUser->getDciDecision(cellId))
    {
      mResultRlcStats << packet.dlRlcStatLine << "\n";
    }
}

void L2Mac::recvMeasurementsReport(int cellId, const CSIMeasurementReport &report)
{
  const std::string fname = "recvMeasurementsReport" + std::to_string(cellId);
  mTimeMeasurement.start(fname);

  mSchedulers[cellId - 1].schedDlCqiInfoReq(report.targetCellId, report.csi);

  mTimeMeasurement.stop(fname);

  if (mMacSapUser->peekDciDecision(cellId))
    {
      mResultMeasurements << report.csi.first << "\t" << cellId  << "\t"
                          << report.targetCellId << "\t" << report.csi.second << "\n";
    }
}

void L2Mac::recvX2Message(int cellId, const X2Message &message)
{
  switch (message.type)
    {
    case X2Message::changeScheduleModeInd:
      {
        mSchedulers[cellId - 1].setTrafficActivity(message.mustSendTraffic, message.applyDirectMembership);
        break;
      }
    case X2Message::measuresInd:
    {
        const CSIMeasurementReport& report = message.report;
        mSchedulers[cellId - 1].schedDlCqiInfoReq(report.targetCellId, report.csi);
        break;
      }
    case X2Message::leadershipInd:
      {
        mSchedulers[cellId - 1].setLeader(message.leaderCellId);
        break;
      }
    }
}

void L2Mac::l2Timeout(int cellId)
{
  size_t begin = 1;
  size_t end = mSchedulers.size();
  if (cellId >= 0)
    {
      begin = cellId;
      end = begin + 1;
    }

  for (size_t i = begin; i < end; i++)
    {
      mSchedulers[i - 1].onTimeout();
    }

  if (cellId == -1)
    {
      Event event { EventType::l2Timeout, SimTimeProvider::getTime() + Converter::microseconds(999) };
      event.cellId = -1;
      Simulator::instance()->scheduleEvent(event);
    }

}

void L2Mac::printMacTimings()
{
  LOG("Mac simulation statistics:");
  LOG("\tScheduler decisions timings [us]:");
  for (int i = 0; i < compMembersCount; i++)
    {
      const std::string index = "recvMeasurementsReport" + std::to_string(i + 1);
      LOG("\tcellId = " << i + 1
          << "\tave: " << mTimeMeasurement.average(index) << "\tmin: "<< mTimeMeasurement.minimum(index)
          << "\tmax: " << mTimeMeasurement.maximum(index));
    }
  LOG("\n");
}
