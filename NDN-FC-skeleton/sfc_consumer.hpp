/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013-2018 Regents of the University of California.
 *
 * This file is part of ndn-cxx library (NDN C++ library with eXperimental eXtensions).
 *
 * ndn-cxx library is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * ndn-cxx library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received copies of the GNU General Public License and GNU Lesser
 * General Public License along with ndn-cxx, e.g., in COPYING.md file.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndn-cxx authors and contributors.
 *
 * @author Alexander Afanasyev <http://lasr.cs.ucla.edu/afanasyev/index.html>
 */

//#include <Python.h>
      
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/data.hpp>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/function.hpp>
#include <ndn-cxx/encoding/block.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-info.hpp>
#include <ndn-cxx/util/time.hpp>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#define DEFAULT_KEY_LOCATOR_SIZE 256 // of bytes
#define DEFAULT_SAFETY_OFFSET 10 // of bytes

namespace ndn {
namespace sfc {

class Consumer:noncopyable
{
public:
  struct Options
  {
    // security::SigningInfo signingInfo;
    // time::milliseconds freshnessPeriod = 10_s;
    // std::size_t maxSegmentSize = 2048;
    time::milliseconds interestLifetime = 10000_s; //DEFAULT_INTEREST_LIFETIME;
    // int maxRetriesOnTimeoutOrNack = 15;
    //bool disableVersionDiscovery = true;
    bool mustBeFresh = false;
    bool isQuiet = false;
    bool isVerbose = false;
    // bool wantShowVersion = false;
  };

  /**
   * @brief Create the network processing part of function execution
   */
  Consumer(Name& contentName, Function& funcName, Face& face, const Options& opts);
    

  /**
   * @brief Run the network processing part
   */
  void
  run();
  
private:

  /**
   * @brief Respond with a metadata packet containing the versioned content name
   */
  //  void
  //  processDiscoveryInterest(const Interest& interest);

  void
  onData(const Interest& interest, const Data& data);

  void
  reassembleSegments();

  void 
  addToBuffer(const Data& data);

  void
  createFile(std::string& outputFilename);

  void
  onNack(const Interest& interest, const lp::Nack& nack);

  void
  onTimeout(const Interest& interest);

  void
  onRegisterFailed(const Name& prefix, const std::string& reason);

private:
  unique_ptr<Name>
  getPrefix(const Name& name);

//  unique_ptr<Name>
//  getFilename(Name& name);

private:

//  Name m_versionedPrefix;
  Name& m_contentName;
  Function& m_funcName;
  Face& m_face;
  const Options m_options;

  Name m_prefix;
  Name m_filename;
  std::map<uint64_t, std::shared_ptr<const Data>> m_receiveBuffer;
  std::vector<uint8_t> m_contentBuffer;

  std::set<uint64_t> m_interestSegmentCounter;
  uint64_t m_incomingFinalBlockNumber;
};

} // namespace sfc
} // namespace ndn
