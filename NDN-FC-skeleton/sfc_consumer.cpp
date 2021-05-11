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

#include "sfc_consumer.hpp"

namespace ndn {
namespace sfc {


  Consumer::Consumer(Name& contentName, Function& funcName, Face& face, const Options& opts)
    :m_contentName(contentName)
    ,m_funcName(funcName)
    ,m_face(face)
    ,m_options(opts)
  {}

  void
  Consumer::run()
  {
    if (m_options.isVerbose) {
      std::cout << "Consumer::run() contentName: " << m_contentName << " function: " << m_funcName << std::endl;
    }
    Interest interest = Interest()
                  .setName(m_contentName.appendSegment(0))
                  .setMustBeFresh(m_options.mustBeFresh)
                  .setInterestLifetime(m_options.interestLifetime);
    interest.setFunction(m_funcName);
    
    m_face.expressInterest(interest,
                          bind(&Consumer::onData, this, _1, _2),
                          bind(&Consumer::onNack, this, _1, _2),
                          bind(&Consumer::onTimeout, this, _1));
    m_face.processEvents();
  }

  void
  Consumer::onData(const Interest& interest, const Data& data)
  {
    if (!m_options.isQuiet) {
      std::cout << "Data: " << data.getName() << std::endl;
      std::cout << "Prefix: " << *(getPrefix(data.getName())) << std::endl;
      std::cout << "Segment No.: " << data.getName().get(-1).toSegment() << std::endl;
      std::cout << "Final Block No.: " << data.getFinalBlock()->toSegment() << std::endl;
    }

    Function executedFunction(data.getFunction());

    //Add Segments to Buffer
    if (m_options.isVerbose)
      std::cout << "Loading receiveBuffer: " << data.getName().get(-1).toSegment() << std::endl;
    
    m_receiveBuffer[data.getName().get(-1).toSegment()] = data.shared_from_this();
    //    reassembleSegments();

    if (data.getName().get(-1).toSegment() == 0) {
      //    if (m_receiveBuffer.empty()) {
      m_prefix = *(getPrefix(data.getName()));
      m_filename = Name(data.getName().get(-2).toUri());
      m_incomingFinalBlockNumber = data.getFinalBlock()->toSegment();
      if (!m_options.isQuiet) {
        std::cout << "Prefix: " << m_prefix << std::endl;
        std::cout << "Filename: " << m_filename << std::endl;
      }

      if(m_incomingFinalBlockNumber > 0) {
        if (!m_options.isQuiet)
          std::cout << "-------------------------------------------------------" << std::endl;
        for(uint64_t i = 1; i <= m_incomingFinalBlockNumber; i++)
        {
          Name name(m_prefix);
          name.append(m_filename);
          name.appendSegment(i);
          Interest newInterest(name);
          newInterest.setFunction(interest.getFunction());
          newInterest.setInterestLifetime(m_options.interestLifetime);
          // newInterest.setCanBePrefix(false);
          newInterest.setMustBeFresh(m_options.mustBeFresh);
          if (!m_options.isQuiet)
            std::cout << "Sending Interests: " << name << std::endl;
          m_face.expressInterest(newInterest,
                                 bind(&Consumer::onData, this,  _1, _2),
                                 bind(&Consumer::onNack, this, _1, _2),
                                 bind(&Consumer::onTimeout, this, _1));
        }
      }
    }

    //    if(data.getName().get(-1).toSegment() == m_finalBlockNumber)
    if (m_receiveBuffer.size() == m_incomingFinalBlockNumber + 1) {
        reassembleSegments();
        std::string outputFilename = data.getName().get(-2).toUri(); //"test.png"
        createFile(outputFilename);
        m_contentBuffer.clear();
      }
    if (!m_options.isQuiet)
      std::cout << "-------------------------------------------------------" << std::endl;
  }

  /***********************************DATA REASSEMBLY*************************************/
  void
  Consumer::reassembleSegments()
  {
    for (auto i = m_receiveBuffer.cbegin(); i != m_receiveBuffer.cend(); ++i) {
      addToBuffer(*(i->second));
    }
    m_receiveBuffer.clear();
  }

  void
  Consumer::addToBuffer(const Data& data)
  {
    const Block content = data.getContent();
    m_contentBuffer.insert(m_contentBuffer.end(), &content.value()[0], &content.value()[content.value_size()]);
    if (m_options.isVerbose)
      std::cout << "Adding to Buffer, value size: " << content.value_size() << std::endl;

    return;
  }

  void
  Consumer::createFile(std::string& outputFilename)
  {
    if (!m_options.isQuiet) {
      std::cout << "---------------------------------------------------" << std::endl;
      std::cout << "Creating File" << std::endl;
    }
    std::ofstream outfile(outputFilename, std::ofstream::binary);
    outfile.write((char*)m_contentBuffer.data(), m_contentBuffer.size());
    if (!m_options.isQuiet)
      std::cout << "Orig. BufferSize: " << m_contentBuffer.size() << std::endl;
    outfile.close();
    if (!m_options.isQuiet)
      std::cout << "Success" << std::endl;

    return;
  }
  /*********************************************************************************/

  /************************Callbacks************************************************/
  void
  Consumer::onNack(const Interest& interest, const lp::Nack& nack)
  {
    if (!m_options.isQuiet) {
      std::cout << "received Nack with reason " << nack.getReason()
                << " for interest " << interest << std::endl;
      std::cout << "---------------------------------------------" << std::endl;
    }
  }

  void
  Consumer::onTimeout(const Interest& interest)
  {
    if (!m_options.isQuiet) {
      std::cout << "Timeout Seg No.: " << interest.getName().get(-1).toSegment() << std::endl;
      std::cout << "---------------------------------------------" << std::endl;
    }
  }

  void
  Consumer::onRegisterFailed(const Name& prefix, const std::string& reason)
  {
    std::cerr << "ERROR: Failed to register prefix \""
              << prefix << "\" in local hub's daemon (" << reason << ")"
              << std::endl;
    m_face.shutdown();
  }
/***************************************************************************************/

  unique_ptr<Name>
  Consumer::getPrefix(const Name& name){
    std::string prefix;
    for(int i = 0; i < int(name.size())-2; i++) //Removes Segment and file name
    {
      prefix.append("/");
      prefix.append(name.get(i).toUri());
    }
    return unique_ptr<Name>{new Name(prefix)};
  }

  namespace po = boost::program_options;

  static void
  usage (std::ostream& os, const std::string& programName, const po::options_description& desc)
  {
    os << "Usage: " << programName << "[options] /contetn-name /function-name\n"
    << "\n"
    << "Apply the specified function on Data packets.\n"
    << desc;
  }

  static int
  main(int argc, char* argv[])
  {
  const std::string programName(argv[0]);

  Consumer::Options opts;
  std::string prefix, funcName; //, nameConv, signingStr;

  po::options_description visibleDesc("Options");
  visibleDesc.add_options()
    ("help,h",      "print this help message and exit")
    ("fresh,f",     po::bool_switch(&opts.mustBeFresh),
                    "only return fresh content (set MustBeFresh on all outgoing Interests)")
    ("lifetime,l",  po::value<time::milliseconds::rep>()->default_value(opts.interestLifetime.count()),
                    "lifetime of expressed Interests, in milliseconds")
    //("pipeline-type,p", po::value<std::string>(&pipelineType)->default_value(pipelineType),
    //                    "type of Interest pipeline to use; valid values are: 'fixed', 'aimd', 'cubic'")
    //("no-version-discovery,D", po::bool_switch(&opts.disableVersionDiscovery),
    //                           "skip version discovery even if the name does not end with a version component")
    ("quiet,q",     po::bool_switch(&opts.isQuiet), "turn off all non-error output")
    ("verbose,v",   po::bool_switch(&opts.isVerbose), "turn on verbose output (per Interest information)")
    /*    ("version,V",   "print program version and exit") */
    ;

  po::options_description hiddenDesc;
  hiddenDesc.add_options()
    ("name", po::value<std::string>(&prefix), "NDN name for the served content")
    ("function", po::value<std::string>(&funcName), "Function names to be appried to the content")
    ;

  po::options_description optDesc;
  optDesc.add(visibleDesc).add(hiddenDesc);

  po::positional_options_description p;
  p.add("name", 1).add("function", 1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).options(optDesc).positional(p).run(), vm);
    po::notify(vm);
  }
  catch (const po::error& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 2;
  }
  catch (const boost::bad_any_cast& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 2;
  }

  if (vm.count("help") > 0) {
    usage(std::cout, programName, visibleDesc);
    return 0;
  }
  /*
  if (vm.count("version") > 0) {
    std::cout << "ndnputchunks " << tools::VERSION << "\n";
    return 0;
  }
  */
  if (prefix.empty()) {
    usage(std::cerr, programName, visibleDesc);
    return 2;
  }
  /*
  if (funcName.empty()) {
    usage(std::cerr, programName, visibleDesc);
    return 2;
  }
  if (nameConv == "marker" || nameConv == "m" || nameConv == "1") {
    name::setConventionEncoding(name::Convention::MARKER);
  }
  else if (nameConv == "typed" || nameConv == "t" || nameConv == "2") {
    name::setConventionEncoding(name::Convention::TYPED);
  }
  else if (!nameConv.empty()) {
    std::cerr << "ERROR: '" << nameConv << "' is not a valid naming convention\n";
    return 2;
  }
  */
  if (opts.isQuiet && opts.isVerbose) {
    std::cerr << "ERROR: cannot be quiet and verbose at the same time\n";
    return 2;
  }
  
  try {
    Name contentName(prefix);
    Face face;
    Function functionName(funcName);
    Consumer cons(contentName, functionName, face, opts);
    cons.run();
  }
  catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }

  return 0;
  }
} // namespace sfc
} // namespace ndn

int
main(int argc, char** argv)
{
  return ndn::sfc::main(argc, argv);
}
