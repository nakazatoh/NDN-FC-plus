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

#include "sfc_function.hpp"

namespace ndn {
namespace sfc {


  Func::Func(Name& funcName, Face& face, KeyChain& keyChain, const Options& opts)
    :m_funcName(funcName)
    ,m_face(face)
    ,m_keyChain(keyChain)
    ,m_options(opts)
  {}

  void
  Func::run()
  {
    m_face.setInterestFilter(m_funcName,
                             bind(&Func::onInterest, this, _1, _2),
                             ndn::RegisterPrefixSuccessCallback(),
                             bind(&Func::onRegisterFailed, this, _1, _2));

    m_face.processEvents();
  }
 
  void
  Func::onInterest(const ndn::InterestFilter& filter, const Interest& interest)
  {
    if (!m_options.isQuiet) {
      std::cout << "<< Interest: " << interest.getName().get(-1).toSegment() << std::endl;
      std::cout << "--------------------------------------------" << std::endl;
    }
    interest.removeHeadFunction();
    interest.refreshNonce();

    uint64_t segment = interest.getName().get(-1).toSegment();
    m_interestSegmentCounter.insert(segment);
    if (segment > 0) {
      m_face.put(*(m_store[segment]));
      if (!m_options.isQuiet)  {
        std::cout << "Sending Data for Seg.: " << segment << std::endl;
        std::cout << "--------------------------------------------" << std::endl;
      }
      if(segment == m_outgoingFinalBlockNumber) {
        sleep(1);
        m_interestSegmentCounter.clear();
//        std::exit(0);
      }
    }
    else { // segment == 0
      m_face.expressInterest(interest,
                              bind(&Func::onData, this, _1, _2),
                              bind(&Func::onNack, this, _1, _2),
                              bind(&Func::onTimeout, this, _1));
    }
    //std::cout << interest.getFunction() << std::endl;
  }

  void
  Func::onData(const Interest& interest, const Data& data)
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
          newInterest.setInterestLifetime(interest.getInterestLifetime());
          //newInterest.setInterestLifetime(time::milliseconds(m_options.interestLifeTime));
          // newInterest.setCanBePrefix(false);
          // newInterest.setMustBeFresh(m_options.mustBeFresh);
          newInterest.setMustBeFresh(interest.getMustBeFresh());
          if (!m_options.isQuiet)
            std::cout << "Sending Interests: " << name << std::endl;
          m_face.expressInterest(newInterest,
                                 bind(&Func::onData, this,  _1, _2),
                                 bind(&Func::onNack, this, _1, _2),
                                 bind(&Func::onTimeout, this, _1));
        }
      }
    }

    //    if(data.getName().get(-1).toSegment() == m_finalBlockNumber)
    if (m_receiveBuffer.size() == m_incomingFinalBlockNumber + 1) {
        Function executedFunction(data.getFunction());

        reassembleSegments();
        std::string outputFilename = data.getName().get(-2).toUri(); //"test.png"
        createFile(outputFilename);
        m_contentBuffer.clear();
        
       	std::string s = "python3 clientyolo.py " + outputFilename;
        if (!m_options.isQuiet) 
          std::cout << "outputFilename: " << outputFilename << std::endl;

	      int status = system(s.c_str());
        if (status < 0) {
          std::cout << "Error: " << strerror(errno) << std::endl;
        }
        else if (m_options.isVerbose) {
          if (WIFEXITED(status)) {
            std::cout << "Program returned normally, exit code " << WEXITSTATUS(status) << std::endl;
          }
          else {
            std::cout << "Program exited abnormaly" << std::endl;
          }
        }
 
        /**APP GOES HERE**/
        
        std::string loadFilename = data.getName().get(-2).toUri(); //"test.png"
        populateStore(outputFilename, executedFunction);

	      std::string rm = "rm " + outputFilename;
	      status = system(rm.c_str());
        if (status < 0) {
          std::cout << "Error: " << strerror(errno) << std::endl;
        }
        else if (m_options.isVerbose) {
          if (WIFEXITED(status)) {
            std::cout << "Program returned normally, exit code " << WEXITSTATUS(status) << std::endl;
          }
          else {
            std::cout << "Program exited abnormaly" << std::endl;
          }
        }

        m_face.put(*(m_store[0]));
      }
    if (!m_options.isQuiet)
      std::cout << "-------------------------------------------------------" << std::endl;
  }

  /***********************************DATA REASSEMBLY*************************************/
  void
  Func::reassembleSegments()
  {
    for (auto i = m_receiveBuffer.cbegin(); i != m_receiveBuffer.cend(); ++i) {
      addToBuffer(*(i->second));
    }
    m_receiveBuffer.clear();
  }

  void
  Func::addToBuffer(const Data& data)
  {
    const Block content = data.getContent();
    m_contentBuffer.insert(m_contentBuffer.end(), &content.value()[0], &content.value()[content.value_size()]);
    if (m_options.isVerbose)
      std::cout << "Adding to Buffer, value size: " << content.value_size() << std::endl;

    return;
  }

  void
  Func::createFile(std::string& outputFilename)
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

  /************************DATA SEGMENTATION****************************************/
  void
  Func::populateStore(std::string& loadFilename, Function& executedFunction)
  {
    if (!m_options.isQuiet)
      std::cout << "Func::populateStore: filename: " << loadFilename << " executedFunction: " << executedFunction << std::endl;
    m_store.clear();
    Name name(m_prefix);
    name.append(m_filename);
    Block nameOnWire = name.wireEncode();
    size_t bytesOccupiedByName = nameOnWire.size();


    Function funcname(m_funcName.toUri());
    funcname.append(executedFunction);
    Block funcnameOnWire = funcname.wireEncode();
    size_t bytesOccupiedByFuncName = funcnameOnWire.size();

    if (m_options.isVerbose)
      std::cout << "content name length: " << bytesOccupiedByName << " function name length: " << bytesOccupiedByFuncName << std::endl;

    int signatureSize = 32; //SHA_256 as default

    int freeSpaceForContent = m_options.maxSegmentSize - bytesOccupiedByName
      - bytesOccupiedByFuncName - signatureSize - DEFAULT_KEY_LOCATOR_SIZE
      - DEFAULT_SAFETY_OFFSET;

    if (m_options.isVerbose)
      std::cout << "freeSpaceForContent: " << freeSpaceForContent << std::endl;

    std::vector<uint8_t> buffer(freeSpaceForContent);
    if (m_options.isVerbose)
      std::cout << "bufferSize: " << buffer.size() << std::endl;

    //    const uint8_t* buffer;
    //    size_t bufferSize;
    //    std::tie(unique_ptr<uint8_t> buffer, size_t bufferSize) = loadFile(loadFilename);
    std::ifstream infile(loadFilename, std::ifstream::binary);
    while (infile.good()) {
      Name tmp_name = name;
      infile.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
      const auto nCharsRead = infile.gcount();
      if (nCharsRead > 0) {
        auto data = make_shared<Data>(tmp_name.appendSegment(m_store.size()));
        if (m_options.isVerbose)
          std::cout << "preparing Data: " << data->getName() << " segment #: " << m_store.size() << std::endl;
        data->setFunction(funcname);
        if (m_options.isVerbose)
          std::cout << "functionName: " << funcname << std::endl;
        data->setFreshnessPeriod(m_options.freshnessPeriod);
        data->setContent(buffer.data(), static_cast<size_t>(nCharsRead));
        m_store.push_back(data);
      }
    }
    infile.close();

    m_outgoingFinalBlockNumber = m_store.size() - 1;
    if (!m_options.isQuiet)
      std::cout << "Final Block ID: " << m_outgoingFinalBlockNumber << std::endl;
    auto finalBlockId = name::Component::fromSegment(m_outgoingFinalBlockNumber);
    for (const auto& data : m_store) {
      data->setFinalBlock(finalBlockId);
      m_keyChain.sign(*data, m_options.signingInfo);
    }
    //    if (tmp_finalBlockNumber > 0) {
    //      m_outgoingFinalBlockNumber = tmp_finalBlockNumber;
      
      //      auto head = m_dataBuffer.find(0);
      //      m_producer.produce(*(head->second));
      //      m_dataBuffer.erase(head);
      
    //    }
    //    else
    //    {
      //      m_producer.produce(suffix, executedfunction, buffer, bufferSize);
    //    }
    //    std::cout << "SENDING" << std::endl;
    //sleep(300);
  }
  /*********************************************************************************/

  /************************Callbacks************************************************/
  void
  Func::onNack(const Interest& interest, const lp::Nack& nack)
  {
    if (!m_options.isQuiet) {
      std::cout << "received Nack with reason " << nack.getReason()
                << " for interest " << interest << std::endl;
      std::cout << "---------------------------------------------" << std::endl;
    }
  }

  void
  Func::onTimeout(const Interest& interest)
  {
    if (!m_options.isQuiet) {
      std::cout << "Timeout Seg No.: " << interest.getName().get(-1).toSegment() << std::endl;
      std::cout << "---------------------------------------------" << std::endl;
    }
  }

  void
  Func::onRegisterFailed(const Name& prefix, const std::string& reason)
  {
    std::cerr << "ERROR: Failed to register prefix \""
              << prefix << "\" in local hub's daemon (" << reason << ")"
              << std::endl;
    m_face.shutdown();
  }
/***************************************************************************************/

  unique_ptr<Name>
  Func::getPrefix(const Name& name){
    std::string prefix;
    for(int i = 0; i < int(name.size())-2; i++) //Removes Segment and file name
    {
      prefix.append("/");
      prefix.append(name.get(i).toUri());
    }
    return unique_ptr<Name>{new Name(prefix)};
  }
  /*
  unique_ptr<Name>
  Func::getFilename(Name& name){
    unique_ptr<Name> p {new Name(name.get(-2).toUri())};
    return p;
  }
  */
  namespace po = boost::program_options;

  static void
  usage (std::ostream& os, const std::string& programName, const po::options_description& desc)
  {
    os << "Usage: " << programName << "[options] /function-name\n"
    << "\n"
    << "Apply the specified function on Data packets.\n"
    << desc;
  }

  static int
  main(int argc, char* argv[])
  {
  const std::string programName(argv[0]);

  Func::Options opts;
  std::string funcName; //, nameConv, signingStr;

  po::options_description visibleDesc("Options");
  visibleDesc.add_options()
    ("help,h",      "print this help message and exit")
    ("freshness,f", po::value<time::milliseconds::rep>()->default_value(opts.freshnessPeriod.count()),
                    "FreshnessPeriod of the published Data packets, in milliseconds")
    ("size,s",      po::value<size_t>(&opts.maxSegmentSize)->default_value(opts.maxSegmentSize),
                    "maximum chunk size, in bytes")
    /*    ("naming-convention,N",  po::value<std::string>(&nameConv),
                             "encoding convention to use for name components, either 'marker' or 'typed'")
    ("signing-info,S",       po::value<std::string>(&signingStr), "see 'man ndnputchunks' for usage")
    ("print-data-version,p", po::bool_switch(&opts.wantShowVersion),
                             "print Data version to the standard output") */
    ("quiet,q",     po::bool_switch(&opts.isQuiet), "turn off all non-error output")
    ("verbose,v",   po::bool_switch(&opts.isVerbose), "turn on verbose output (per Interest information)")
    /*    ("version,V",   "print program version and exit") */
    ;

  po::options_description hiddenDesc;
  hiddenDesc.add_options()
    //   ("name", po::value<std::string>(&prefix), "NDN name for the served content")
    ("function", po::value<std::string>(&funcName), "name for the served function")
    ;

  po::options_description optDesc;
  optDesc.add(visibleDesc).add(hiddenDesc);

  po::positional_options_description p;
  p.add("function", -1);

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
  if (funcName.empty()) {
    usage(std::cerr, programName, visibleDesc);
    return 2;
  }
  /*
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
  opts.freshnessPeriod = time::milliseconds(vm["freshness"].as<time::milliseconds::rep>());
  if (opts.freshnessPeriod < 0_ms) {
    std::cerr << "ERROR: --freshness cannot be negative\n";
    return 2;
  }

  if (opts.maxSegmentSize < 1 || opts.maxSegmentSize > MAX_NDN_PACKET_SIZE) {
    std::cerr << "ERROR: --size must be between 1 and " << MAX_NDN_PACKET_SIZE << "\n";
    return 2;
  }
  /*
  try {
    opts.signingInfo = security::SigningInfo(signingStr);
  }
  catch (const std::invalid_argument& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 2;
  }
  */
  if (opts.isQuiet && opts.isVerbose) {
    std::cerr << "ERROR: cannot be quiet and verbose at the same time\n";
    return 2;
  }
  
  try {
    Face face;
    KeyChain keyChain;
    Name functionName(funcName);
    Func func(functionName, face, keyChain, opts);
    func.run();
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
