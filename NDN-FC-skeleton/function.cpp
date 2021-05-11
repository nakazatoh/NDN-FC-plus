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

// correct way to include ndn-cxx headers
// #include <ndn-cxx/face.hpp>
// #include <ndn-cxx/security/key-chain.hpp>
//#include <Python.h>
#include <Consumer-Producer-API/producer-context.hpp>

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/data.hpp>
#include <ndn-cxx/security/key-chain.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <string>
#include <tuple>
#include <unistd.h>

class Func
{
public:
  Func():
  m_producer("/"),
  m_lastReassembledSegment(0),
  m_finalBlockNumber(std::numeric_limits<uint64_t>::max()),
  m_largerFinalBlockNumber(std::numeric_limits<uint64_t>::max())
  {}

  void
  run()
  {
    m_face = ndn::make_shared<ndn::Face>();
    m_face->setInterestFilter(m_funcName,
                             bind(&Func::onInterest, this, _1, _2),
                             ndn::RegisterPrefixSuccessCallback(),
                             bind(&Func::onRegisterFailed, this, _1, _2));

    m_face->processEvents();
  }

  void
  setFuncName(ndn::Name funcName)
  {
    m_funcName = funcName;
  }

private:
  void
  onInterest(const ndn::InterestFilter& filter, const ndn::Interest& interest)
  {
    std::cout << "<< Interest: " << interest.getName().get(-1).toSegment() << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
    interest.removeHeadFunction();
    interest.refreshNonce();

    //If new file is larger than original send Data for incoming interest
    uint64_t segment = interest.getName().get(-1).toSegment();
    m_interestSegmentCounter.insert(segment);
    if(segment > 0)
    {
      auto head = m_dataBuffer.find(segment);
      m_producer.produce(*(head->second));
      std::cout << "Sending Data for Seg.: " << segment << std::endl;
      std::cout << "--------------------------------------------" << std::endl;
      if(segment == m_largerFinalBlockNumber)
      {
        sleep(1);
//        m_dataBuffer.clear();
        m_receiveBuffer.clear();
        m_interestSegmentCounter.clear();
        m_lastReassembledSegment = 0;
        m_finalBlockNumber = std::numeric_limits<uint64_t>::max();
        m_largerFinalBlockNumber = std::numeric_limits<uint64_t>::max();
//        std::exit(0);
      }
    }
    else
    {
    m_face->expressInterest(interest,
                           bind(&Func::onData, this,  _1, _2),
                           bind(&Func::onNack, this, _1, _2),
                           bind(&Func::onTimeout, this, _1));
    }
    //std::cout << interest.getFunction() << std::endl;

  }

  void
  onData(const ndn::Interest& interest, const ndn::Data& data)
  {
    std::cout << "Data: " << data.getName() << std::endl;
    std::cout << "Prefix: " << getPrefix(data.getName()) << std::endl;
    std::cout << "Segment No.: " << data.getName().get(-1).toSegment() << std::endl;
    std::cout << "Final Block No.: " << data.getFinalBlock()->toSegment() << std::endl;

    ndn::Function executedFunction(data.getFunction());

    //Add Segments to Buffer
    m_receiveBuffer[data.getName().get(-1).toSegment()] = data.shared_from_this();
    reassembleSegments();

    if(data.getName().get(-1).toSegment() == 0)
    {
      m_prefix = getPrefix(data.getName());
      m_producer.setContextOption(PREFIX, m_prefix);
      m_filename = getFilename(data.getName());
      m_finalBlockNumber = data.getFinalBlock()->toSegment();
      std::cout << "Prefix: " << m_prefix << std::endl;
      std::cout << "Filename: " << m_filename << std::endl;

      if(m_finalBlockNumber > 0)
      {
        std::cout << "-------------------------------------------------------" << std::endl;
        for(uint64_t i = 1; i <= m_finalBlockNumber; i++)
        {
          ndn::Name name(m_prefix);
          name.append(m_filename);
          name.appendSegment(i);
          ndn::Interest newInterest(name);
          newInterest.setFunction(interest.getFunction());
          newInterest.setInterestLifetime(ndn::time::milliseconds(10000));
          std::cout << "Sending for Interests: " << name << std::endl;
          m_face->expressInterest(newInterest,
                                 bind(&Func::onData, this,  _1, _2),
                                 bind(&Func::onNack, this, _1, _2),
                                 bind(&Func::onTimeout, this, _1));
        }
      }
    }

    if(data.getName().get(-1).toSegment() == m_finalBlockNumber)
      {
        ndn::Function executedFunction(data.getFunction());
        
        std::string outputFilename = data.getName().get(-2).toUri(); //"test.png"
        createFile(outputFilename);
        m_contentBuffer.clear();
        
       	std::string s = "python3 clientyolo.py " + outputFilename;
        std::cout << "outputFilename: " << outputFilename << std::endl;

	system(s.c_str());
 
        /**APP GOES HERE**/
        
        std::string loadFilename = data.getName().get(-2).toUri(); //"test.png"
        dataSegmentation(m_filename, outputFilename, executedFunction);
	std::string rm = "rm " + outputFilename;
	system(rm.c_str());
      }
    std::cout << "-------------------------------------------------------" << std::endl;
  }

  /***********************************DATA REASSEMBLY*************************************/
  void
  reassembleSegments()
  {
    auto head = m_receiveBuffer.find(m_lastReassembledSegment);
    while(head != m_receiveBuffer.end())
    {
      addToBuffer(*(head->second));
      m_receiveBuffer.erase(head);
      m_lastReassembledSegment++;
      head = m_receiveBuffer.find(m_lastReassembledSegment);
    }
  }

  void
  addToBuffer(const ndn::Data& data)
  {
    const ndn::Block content = data.getContent();
    m_contentBuffer.insert(m_contentBuffer.end(), &content.value()[0], &content.value()[content.value_size()]);
    std::cout << "Adding to Buffer, value size: " << content.value_size() << std::endl;

    return;
  }

  void
  createFile(std::string outputFilename)
  {
    std::cout << "---------------------------------------------------" << std::endl;
    std::cout << "Creating File" << std::endl;
    std::ofstream outfile(outputFilename, std::ofstream::binary);
    outfile.write((char*)m_contentBuffer.data(), m_contentBuffer.size());
    std::cout << "Orig. BufferSize: " << m_contentBuffer.size() << std::endl;
    outfile.close();
    std::cout << "Success" << std::endl;

    return;
  }

  /*********************************************************************************/

  /************************DATA SEGMENTATION****************************************/
  void
  dataSegmentation(ndn::Name suffix, std::string loadFilename, ndn::Function executedfunction)
  {
    const uint8_t* buffer;
    size_t bufferSize;
    std::tie(buffer, bufferSize) = loadFile(loadFilename);
    std::cout << "new bufferSize: " << bufferSize << std::endl;
    uint64_t tmp_finalBlockNumber = ndn::Producer::getFinalBlockIdFromBufferSize(m_prefix.append(suffix), m_funcName, bufferSize);
    std::cout << "Final Block ID: " << tmp_finalBlockNumber << std::endl;
    m_producer.setContextOption(FUNCTION, m_funcName);
    m_producer.attach();
    if(tmp_finalBlockNumber > 0)
    {
      m_largerFinalBlockNumber = tmp_finalBlockNumber;
      m_dataBuffer = m_producer.getDataSegmentMap(suffix, buffer, bufferSize, executedfunction);
      
      auto head = m_dataBuffer.find(0);
      m_producer.produce(*(head->second));
      m_dataBuffer.erase(head);
      
    }
    else
    {
      m_producer.produce(suffix, executedfunction, buffer, bufferSize);
    }
    std::cout << "SENDING" << std::endl;
    //sleep(300);
  }

  std::tuple<const uint8_t*, size_t>
  loadFile(std::string filename)
  {
      std::ifstream infile(filename, std::ifstream::binary);
      infile.seekg(0, infile.end);
      size_t bufferSize = infile.tellg();
      infile.seekg(0);

      char* buffer = new char[bufferSize];
      infile.read(buffer, bufferSize);
      infile.close();

      return std::make_tuple((const uint8_t*)buffer, bufferSize);
  }
  /*********************************************************************************/

  /************************Callbacks************************************************/
  void
  onNack(const ndn::Interest& interest, const ndn::lp::Nack& nack)
  {
    std::cout << "received Nack with reason " << nack.getReason()
              << " for interest " << interest << std::endl;
    std::cout << "---------------------------------------------" << std::endl;
  }

  void
  onTimeout(const ndn::Interest& interest)
  {
    std::cout << "Timeout Seg No.: " << interest.getName().get(-1).toSegment() << std::endl;
    std::cout << "---------------------------------------------" << std::endl;
  }

  void
  onRegisterFailed(const ndn::Name& prefix, const std::string& reason)
  {
    std::cerr << "ERROR: Failed to register prefix \""
              << prefix << "\" in local hub's daemon (" << reason << ")"
              << std::endl;
    m_face->shutdown();
  }
/***************************************************************************************/

  ndn::Name
  getPrefix(ndn::Name name){
    std::string prefix;
    for(int i = 0; i < int(name.size())-2; i++) //Removes Segment and file name
    {
      prefix.append("/");
      prefix.append(name.get(i).toUri());
    }
    return ndn::Name(prefix);
  }

  ndn::Name
  getFilename(ndn::Name name){
    return ndn::Name(name.get(-2).toUri());
  }

private:
  ndn::shared_ptr<ndn::Face> m_face;
  ndn::Name m_funcName;
  ndn::KeyChain m_keyChain;

  ndn::Producer m_producer;
  ndn::Name m_prefix;
  ndn::Name m_filename;
  std::map<uint64_t, std::shared_ptr<const ndn::Data>> m_receiveBuffer;
  std::vector<uint8_t> m_contentBuffer;
  std::set<uint64_t> m_interestSegmentCounter;
  uint64_t m_lastReassembledSegment;
  uint64_t m_finalBlockNumber;
  uint64_t m_largerFinalBlockNumber;
  std::map<uint64_t, std::shared_ptr<ndn::Data>> m_dataBuffer; //Buffer for when file is larger

};

int
main(int argc, char** argv)
{
  if(argc < 2){
    std::cerr << "Input Prefix for Function" << std::endl;
    return 1;
  }
  try {
    Func function;
    function.setFuncName(ndn::Name(argv[1]));
    function.run();
  }
  catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
  }
  return 0;
}
