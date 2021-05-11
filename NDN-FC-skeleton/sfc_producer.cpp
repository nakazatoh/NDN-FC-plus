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

#include "sfc_producer.hpp"

namespace ndn {
namespace sfc {


  Producer::Producer(Name& prefix, const std::string& loadFilename, Face& face, KeyChain& keyChain, const Options& opts)
    :m_prefix(prefix)
    ,m_face(face)
    ,m_keyChain(keyChain)
    ,m_options(opts)
  {
    m_fullContentName = prefix;
    m_fullContentName.append(loadFilename);
    m_filename = Name(loadFilename);
    populateStore(loadFilename);
  }

  void
  Producer::run()
  {
    m_face.setInterestFilter(m_fullContentName,
                             bind(&Producer::onInterest, this, _1, _2),
                             ndn::RegisterPrefixSuccessCallback(),
                             bind(&Producer::onRegisterFailed, this, _1, _2));

    m_face.processEvents();
  }
 
  void
  Producer::onInterest(const ndn::InterestFilter& filter, const Interest& interest)
  {
    if (!m_options.isQuiet) {
      std::cout << "<< Interest: " << interest.getName().get(-1).toSegment() << std::endl;
      std::cout << "--------------------------------------------" << std::endl;
    }

    Name name(interest.getName());
    if (name.size() == m_fullContentName.size() + 1 && name[-1].isSegment()) {
      uint64_t segment = interest.getName().get(-1).toSegment();
      if (segment < m_store.size()) {
        m_face.put(*(m_store[segment]));
        if (!m_options.isQuiet)  {
          std::cout << "Sending Data for Seg.: " << segment << std::endl;
          std::cout << "--------------------------------------------" << std::endl;
        }  
      }
    }
  }

  /*********************************************************************************/
  /************************DATA SEGMENTATION****************************************/
  void
  Producer::populateStore(const std::string& loadFilename)
  {
    if (!m_options.isQuiet)
      std::cout << "Producer::populateStore: filename: " << loadFilename << std::endl;
    m_store.clear();
    Name name(m_prefix);
    name.append(m_filename);
    Block nameOnWire = name.wireEncode();
    size_t bytesOccupiedByName = nameOnWire.size();

    if (m_options.isVerbose)
      std::cout << "content name length: " << bytesOccupiedByName << std::endl;

    int signatureSize = 32; //SHA_256 as default

    int freeSpaceForContent = m_options.maxSegmentSize - bytesOccupiedByName
      - signatureSize - DEFAULT_KEY_LOCATOR_SIZE - DEFAULT_SAFETY_OFFSET;

    if (m_options.isVerbose)
      std::cout << "freeSpaceForContent: " << freeSpaceForContent << std::endl;

    std::vector<uint8_t> buffer(freeSpaceForContent);
    if (m_options.isVerbose)
      std::cout << "bufferSize: " << buffer.size() << std::endl;

    std::ifstream infile;
    std::ios_base::iostate exceptionMask = infile.exceptions() | std::ios::failbit;
    infile.exceptions(exceptionMask);
    try {
      infile.open(loadFilename, std::ifstream::binary);
      //infile.exceptions(std::ios_base::failbit);
    }
    catch (const std::exception& e) {
      std::cerr << "file could not be opened" << std::endl;
    }
/*
    catch (std::ios_base::failure& e) {
      if (e.code() == std::make_error_contion(std::io_errc:stream)) {
        std::cerr << "Stream error!\n";
      }
      else {
        std::cerr << "Unknown failure in opening file.\n";
      }
    }
*/
    infile.exceptions(std::ios::goodbit);
    while (infile.good()) {
      Name tmp_name = name;
      try {
        infile.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
      }
      catch (std::ios_base::failure e) {
        std::cerr << e.what() << std::endl;
      }
      const auto nCharsRead = infile.gcount();
      if (nCharsRead > 0) {
        auto data = make_shared<Data>(tmp_name.appendSegment(m_store.size()));
        if (m_options.isVerbose)
          std::cout << "preparing Data: " << data->getName() << " segment #: " << m_store.size() << std::endl;
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
  }
  /*********************************************************************************/

  /************************Callbacks************************************************/

  void
  Producer::onRegisterFailed(const Name& prefix, const std::string& reason)
  {
    std::cerr << "ERROR: Failed to register prefix \""
              << prefix << "\" in local hub's daemon (" << reason << ")"
              << std::endl;
    m_face.shutdown();
  }
/***************************************************************************************/
/*
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
    os << "Usage: " << programName << "[options] ndn:/name filename\n"
    << "\n"
    << "Publish data in file under the speciafied prefix.\n"
    << desc;
  }

  static int
  main(int argc, char* argv[])
  {
  const std::string programName(argv[0]);

  Producer::Options opts;
  std::string prefix, fname; //, nameConv, signingStr;

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
    ("name", po::value<std::string>(&prefix), "NDN name for the served content")
    ("filename", po::value<std::string>(&fname), "name for the served file")
    ;

  po::options_description optDesc;
  optDesc.add(visibleDesc).add(hiddenDesc);

  po::positional_options_description p;
  p.add("name", 1).add("filename", 1);

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
  if (prefix.empty()) {
    std::cerr << "ERROR: '" << prefix << "' is not a valid naming convention\n";
    return 2;
  }
  if (fname.empty()) {
    std::cerr << "ERROR: '" << fname << "' is not a proper filename\n";
    return 2;
  }

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
    Name prefixName(prefix);
    Producer prod(prefixName, fname, face, keyChain, opts);
    prod.run();
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
