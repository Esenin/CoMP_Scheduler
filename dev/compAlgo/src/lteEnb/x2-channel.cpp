#include "x2-channel.h"

#include "../simulator.h"

X2Channel* X2Channel::mInstance = nullptr;

X2Channel *X2Channel::instance()
{
  if (!mInstance)
    mInstance = new X2Channel;
  return mInstance;
}

void X2Channel::destroy()
{
  delete mInstance;
  mInstance = nullptr;
}

void X2Channel::configurate(int compGroupSize)
{
  mCompGroupSize = compGroupSize;
}

Time X2Channel::getLatency() const
{
  return delay;
}

void X2Channel::send(int tCellId, X2Message msg)
{
  int beginMulticastId = tCellId;
  int endMulticastId = beginMulticastId + 1;
  if (tCellId == -1)
    {
      beginMulticastId = 1;
      endMulticastId = mCompGroupSize + 1;
    }

  for (int i = beginMulticastId; i < endMulticastId; i++)
    {
      const Time constArrivalPart = SimTimeProvider::getTime() + getLatency();
      Time variativePart = Converter::microseconds(tCellId);

      if (constArrivalPart + variativePart == mLastSentTime[tCellId])
        variativePart += Converter::microseconds(1);
      mLastSentTime[tCellId] = constArrivalPart + variativePart;

      Event msgEvent(EventType::x2Message, constArrivalPart + variativePart);
      msgEvent.cellId = i;
      msgEvent.message = msg;
      Simulator::instance()->scheduleEvent(msgEvent);
    }
}

