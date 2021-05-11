/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright 2013,2015 Alexander Afanasyev
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/data.hpp>

#include <iostream>
#include <string>

class Client
{
public:
  explicit
  Client(ndn::Face& face, const std::string& filename, const std::string& functionName)
    : m_face(face)
    , m_baseName(ndn::Name("/my-local-prefix/simple-fetch/file").append(filename))
    , m_functionName(ndn::Function(functionName))

  {
  //std::cerr << "Base name: " << m_baseName << std::endl;
    sendInterest();
  }

private:
  void
  sendInterest()
  {
    ndn::Interest interest(m_baseName);
    interest.setFunction(m_functionName);
    std::cerr << "<< sent Interest: " << interest << std::endl;
    std::cerr << "<< Function: " << interest.getFunction() << std::endl;
    m_face.expressInterest(interest,
                           std::bind(&Client::onData, this, _2),
                           std::bind(&Client::onNack, this, _1),
                           std::bind(&Client::onTimeout, this, _1));
  }


  void
  onData(const ndn::Data& data)
  {
    std::cerr << "<< Data: "
              << std::string(reinterpret_cast<const char*>(data.getContent().value()),
                                                           data.getContent().value_size())
              << std::endl;
      return;
  }

  void
  onNack(const ndn::Interest& interest)
  {
    std::cerr << "<< got NACK for " << interest << std::endl;
  }

  void
  onTimeout(const ndn::Interest& interest)
  {
    // re-express interest
    std::cerr << "<< C++ timeout for " << interest << std::endl;
    /*m_face.expressInterest(ndn::Interest(interest.getName()),
                           std::bind(&Client::onData, this, _2),
                           std::bind(&Client::onNack, this, _1),
                           std::bind(&Client::onTimeout, this, _1));*/
  }

private:
  ndn::Face& m_face;
  ndn::Name m_baseName;
  ndn::Function m_functionName;
};

int
main(int argc, char** argv)
{
  if (argc < 2) {
    std::cerr << "Specify filename" << std::endl;
    return -1;
  }

  std::string functionName;
  if(argv[2] == NULL){
      functionName = "/";
  }

  functionName = argv[2];

  try {
    // create Face instance
    ndn::Face face;

    // create client instance
    Client client(face, argv[1], functionName);

    // start processing loop (it will block until everything is fetched)
    face.processEvents();
  }
  catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
  }

  return 0;
}
