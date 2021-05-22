/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2017,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "functionchain-strategy.hpp"
#include "algorithm.hpp"
#include "common/logger.hpp"

namespace nfd {
namespace fw {

NFD_LOG_INIT(FunctionChainStrategy);
NFD_REGISTER_STRATEGY(FunctionChainStrategy);

const time::milliseconds FunctionChainStrategy::RETX_SUPPRESSION_INITIAL(10);
const time::milliseconds FunctionChainStrategy::RETX_SUPPRESSION_MAX(250);

FunctionChainStrategy::FunctionChainStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
  , ProcessNackTraits(this)
  , m_retxSuppression(RETX_SUPPRESSION_INITIAL,
                      RetxSuppressionExponential::DEFAULT_MULTIPLIER,
                      RETX_SUPPRESSION_MAX)
  ,m_forwarder(forwarder)
{
  ParsedInstanceName parsed = parseInstanceName(name);
  if (!parsed.parameters.empty()) {
    NDN_THROW(std::invalid_argument("FunctionChainStrategy does not accept parameters"));
  }
  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    NDN_THROW(std::invalid_argument(
      "FunctionChainStrategy does not support version " + to_string(*parsed.version)));
  }
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
}

const Name&
FunctionChainStrategy::getStrategyName()
{
  static const auto strategyName = Name("/localhost/nfd/strategy/function-chain").appendVersion(2);
  return strategyName;
}

void
FunctionChainStrategy::afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                                            const shared_ptr<pit::Entry>& pitEntry)
{
  RetxSuppressionResult suppression = m_retxSuppression.decidePerPitEntry(*pitEntry);
  if (suppression == RetxSuppressionResult::SUPPRESS) {
    NFD_LOG_DEBUG(interest << " from=" << ingress << " suppressed");
    return;
  }

  const fib::Entry& fibEntry = this->lookupFibwithFunction(*pitEntry, interest);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();
  auto it = nexthops.end();

  if (suppression == RetxSuppressionResult::NEW) {
    // forward to nexthop with lowest cost except downstream
    it = std::find_if(nexthops.begin(), nexthops.end(), [&] (const auto& nexthop) {
      return isNextHopEligible(ingress.face, interest, nexthop, pitEntry);
    });

    if (it == nexthops.end()) {
      NFD_LOG_DEBUG(interest << " from=" << ingress << " noNextHop");

      lp::NackHeader nackHeader;
      nackHeader.setReason(lp::NackReason::NO_ROUTE);
      this->sendNack(pitEntry, ingress.face, nackHeader);

      this->rejectPendingInterest(pitEntry);
      return;
    }

    Face& outFace = it->getFace();
    NFD_LOG_DEBUG(interest << " from=" << ingress << " newPitEntry-to=" << outFace.getId());
    this->sendInterest(pitEntry, outFace, interest);
    return;
  }

  // find an unused upstream with lowest cost except downstream
  it = std::find_if(nexthops.begin(), nexthops.end(),
                    [&, now = time::steady_clock::now()] (const auto& nexthop) {
                      return isNextHopEligible(ingress.face, interest, nexthop, pitEntry, true, now);
                    });

  if (it != nexthops.end()) {
    Face& outFace = it->getFace();
    this->sendInterest(pitEntry, outFace, interest);
    NFD_LOG_DEBUG(interest << " from=" << ingress << " retransmit-unused-to=" << outFace.getId());
    return;
  }

  // find an eligible upstream that is used earliest
  it = findEligibleNextHopWithEarliestOutRecord(ingress.face, interest, nexthops, pitEntry);
  if (it == nexthops.end()) {
    NFD_LOG_DEBUG(interest << " from=" << ingress << " retransmitNoNextHop");
  }
  else {
    Face& outFace = it->getFace();
    this->sendInterest(pitEntry, outFace, interest);
    NFD_LOG_DEBUG(interest << " from=" << ingress << " retransmit-retry-to=" << outFace.getId());
  }
}

void
FunctionChainStrategy::afterReceiveNack(const FaceEndpoint& ingress, const lp::Nack& nack,
                                    const shared_ptr<pit::Entry>& pitEntry)
{
  this->processNack(ingress.face, nack, pitEntry);
}

const fib::Entry&
FunctionChainStrategy::lookupFibwithFunction(const pit::Entry& pitEntry, const Interest& interest)
{
  const Fib& fib = m_forwarder.getFib();

  if(interest.hasFunction()){
    ndn::Function funcName = interest.getFunction();
    const fib::Entry& fibEntry = fib.findLongestPrefixMatch(Name(funcName.toUri()));
    return fibEntry;
  }

  return this->lookupFib(pitEntry);

}

} // namespace fw
} // namespace nfd
