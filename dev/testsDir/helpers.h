#pragma once


#include <chrono>
#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"

#include "simConfig.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Scene1_F2F_constantPos_wFading");


void
NotifyConnectionEstablishedEnb (std::string context,
                                uint64_t imsi,
                                uint16_t cellid,
                                uint16_t rnti)
{
  std::cout << context
            << " eNB CellId " << cellid
            << ": successful connection of UE with IMSI " << imsi
            << std::endl;
}

int64_t prevMeasuresTime[3] {-1, -1, -1}; // usec
int64_t maxInterval[3] {}; // us
long double intervalsSum[3] {}; // us
int updateCounter[3] {};
std::fstream measurementSink;



void updateMeasIntervals(unsigned sourceCellId, unsigned cellId, int64_t time, unsigned rsrp)
{
  if (time < 14) // us
    return;

  if (prevMeasuresTime[cellId - 1] >= 0)
    {
      auto const curInterval = time - prevMeasuresTime[cellId - 1];
      if (curInterval > 0)
        {
          if (curInterval > maxInterval[cellId - 1])
            maxInterval[cellId - 1] = curInterval;
          intervalsSum[cellId - 1] += curInterval;

          ++updateCounter[cellId - 1];
        }
    }

  //% time    srcCellId    targetCellId    RSRP
  measurementSink << time << "\t" << sourceCellId << "\t" << cellId << "\t" << rsrp << "\n";

  prevMeasuresTime[cellId - 1] = time;
}

void
RecvMeasurementReportCallback (std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti,
                               LteRrcSap::MeasurementReport measReport)
{
  static u_int64_t counter = 0;
  counter++;
  unsigned const rsrp = measReport.measResults.rsrpResult;
  double const snr = -140.0 + rsrp;
  auto const time = Simulator::Now().GetMicroSeconds();


  updateMeasIntervals(cellId, cellId, time, rsrp);


  bool  hasNeighbours = measReport.measResults.haveMeasResultNeighCells;
  if (hasNeighbours)
    for (auto measRes : measReport.measResults.measResultListEutra)
      {
        if (measRes.haveRsrpResult)
          {
            unsigned cellIdNeigh = measRes.physCellId;
            unsigned rsrpNeigh = measRes.rsrpResult;
            updateMeasIntervals(cellId, cellIdNeigh, time, rsrpNeigh);
          }
      }

//  std::cout << counter << "\ttime: " << time << "[us]" << "\tue# " << imsi
//            << "\trsrq: " << (int)measReport.measResults.rsrqResult << "\n";


//  hasNeighbours = hasNeighbours && measReport.measResults.measResultListEutra.front().haveRsrpResult;

//  std::cout << counter << "\ttime: " << time << "[us]" << "\tue# " << imsi << "\trsrp: " << rsrp << "\tSNR: " << snr;
//  if (prevMeasuresTime[cellId - 1] > 0)
//    std::cout << "\t\tmax interval: " << maxInterval[cellId - 1]
//              << " [us]\taverage: " << intervalsSum[cellId - 1] / updateCounter[cellId - 1] << " [us]\t";

//    std::cout << " " << context << "hN: " << hasNeighbours << "\n";

}

void
PrintGnuplottableUeListToFile (std::string filename)
{
  std::ofstream outFile;
  outFile.open (filename.c_str (), std::ios_base::out | std::ios_base::trunc);
  if (!outFile.is_open ())
    {
      NS_LOG_ERROR ("Can't open file " << filename);
      return;
    }

  outFile << "set label \"" << "UE start pos"
          << "\" at " << UeConfig::xPosStart << "," << UeConfig::yPosStart
          << " left font \"Helvetica,12\" textcolor rgb \"grey\" front point pt 3 ps 0.3 lc rgb \"grey\""
          << " offset -8.5,-0.9"
          << std::endl;
  outFile << "set label \"" << "UE end pos"
          << "\" at " << UeConfig::xPosEnd(simTime) << "," << UeConfig::yPosEnd(simTime)
          << " left font \"Helvetica,12\" textcolor rgb \"grey\" front point pt 3 ps 0.3 lc rgb \"grey\""
          << " offset -8,+0.7"
          << std::endl;


}

void
PrintGnuplottableEnbListToFile (std::string filename)
{
  std::ofstream outFile;
  outFile.open (filename.c_str (), std::ios_base::out | std::ios_base::trunc);
  if (!outFile.is_open ())
    {
      NS_LOG_ERROR ("Can't open file " << filename);
      return;
    }
  for (NodeList::Iterator it = NodeList::Begin (); it != NodeList::End (); ++it)
    {
      Ptr<Node> node = *it;
      int nDevs = node->GetNDevices ();
      for (int j = 0; j < nDevs; j++)
        {
          Ptr<LteEnbNetDevice> enbdev = node->GetDevice (j)->GetObject <LteEnbNetDevice> ();
          if (enbdev)
            {
              Vector pos = node->GetObject<MobilityModel> ()->GetPosition ();

              std::string offset[] {"1.1, -0.1", "-4.9, +1.9", "-4.9,-1.6", "-1.3,-3.5"};

              outFile << "set label \"" << "cell " << enbdev->GetCellId ()
                      << "\" at " << pos.x << "," << pos.y
                      << " left font \"Helvetica,12\" textcolor rgb \"white\" front  point pt 4 ps 1.3 lc"
                      << " rgb \"white\" offset " << offset[enbdev->GetCellId() - 1]
                      << std::endl;

            }
        }
    }
}

