/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 Natale Patriciello <natale.patriciello@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef TCPNEWRENOPLUS_H
#define TCPNEWRENOPLUS_H

#include "ns3/tcp-socket-state.h"
#include "ns3/tcp-congestion-ops.h"

namespace ns3 {

/**
 * \brief The NewReno implementation
 *
 * New Reno introduces partial ACKs inside the well-established Reno algorithm.
 * This and other modifications are described in RFC 6582.
 *
 * \see IncreaseWindow
 */
class TcpNewRenoPlus : public TcpNewReno
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  TcpNewRenoPlus ();

  /**
   * \brief Copy constructor.
   * \param sock object to copy.
   */
  TcpNewRenoPlus (const TcpNewRenoPlus& sock);

  ~TcpNewRenoPlus ();

  std::string GetName () const;

  virtual void IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
  virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb,
                                uint32_t bytesInFlight);

  virtual Ptr<TcpCongestionOps> Fork ();

protected:
  virtual uint32_t SlowStart (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
  virtual void CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
};

} // namespace ns3

#endif // TCPNEWRENOPLUS_H

