#include <Consumer-Producer-API/producer-context.hpp>
#include <ndn-cxx/face.hpp>
#include <iostream>
#include <fstream>
#include <tuple>
#include <set>

using namespace ndn;

class CallbackContainer
{
public:
  CallbackContainer()
  : m_interestCounter(0)
  , m_dataCounter(0)
  , m_finalBlockId(0)
  {}

  void
  onNewInterest(Producer& p, const Interest& interest)
  {
    std::cout << "COMES IN " << interest.getName() << std::endl;
  }

  void
  interestDropRcvBuf(Producer& p, const Interest& interest)
  {
    std::cout << "Dropped from Snd Buffer: " << interest.getName() << std::endl;
  }

  void
  cacheHit(Producer& p, const Interest& interest)
  {
    std::cout << "Cache hit: " << interest.getName() << std::endl;
  }

  void
  interestPassedToSendBuffer(Producer& p, const Interest& interest)
  {
    std::cout << "Passed to Snd Buffer: " << interest.getName() << std::endl;
  }

  void
  processContentInterest(Producer& contentProducer, const Interest& interest)
  {
    std::cout << "Cacche Miss" << std::endl;
    std::string filenameString = interest.getName().get(-2).toUri(); //get FileName
    Name fileName(filenameString);

    uint64_t segment = interest.getName().get(-1). toSegment();
    std::cout << "Segment: " << segment << std::endl;
    m_segmentBuffer.insert(segment);

    const uint8_t* m_buffer;
    size_t m_bufferSize;
    std::tie(m_buffer, m_bufferSize) = loadFile(filenameString);
    contentProducer.produce(fileName, m_buffer, m_bufferSize);
    
    std::cout << "Retruned from produce()" << std::endl;
  	if (m_segmentBuffer.size() == m_finalBlockId + 1) {
    std::cout << "bufferSize: " << m_bufferSize << std::endl;
    std::cout << "Final Block ID: " << Producer::getFinalBlockIdFromBufferSize(m_contentPrefix.append(Name("test.png")), Name("/"), m_bufferSize) << std::endl;
    std::cout << "SENT PNG FILE" << std::endl;
    }
    return;
  }

  void
  newDataSegment(Producer& producer, const Data& data)
  {
    std::cout << "New Data Segmnet: " << data.getName() << std::endl;
  }

  void
  leavingContentData(Producer& producer, const Data& data)
  {
    std::cout << "Leaving Data: " << data.getName() << std::endl;
  }

  void
  sendBufferPopulation(Producer& producer, const Data& data)
  {
    std::cout << "New Data in Send Buffer: " << data.getName() << std::endl;
  }

  Name m_contentPrefix;

private:
  int m_interestCounter;
  int m_dataCounter;
  std::set<uint64_t> m_segmentBuffer;
  uint64_t m_finalBlockId;

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
};

int main(int argc, char* argv[])
{
    CallbackContainer callback;

//    ndn::shared_ptr<ndn::Face> m_face = ndn::make_shared<ndn::Face>();

    /*Name pilotProducerName("/test/producer/info");
    Producer pilotProducer(pilotProducerName);
    pilotProducer.setContextOption(CACHE_MISS, (ProducerInterestCallback)bind(&CallbackContainer::processInfoInterest, &callback, _1, _2));
    pilotProducer.setContextOption(DATA_LEAVE_CNTX, (ProducerDataCallback)bind(&CallbackContainer::leavingInfoData, &callback, _1, _2));
    pilotProducer.attach();*/

    Name contentProducerName("/test/producer/content");
    callback.m_contentPrefix = contentProducerName;
    Producer contentProducer(contentProducerName);
    contentProducer.setContextOption(FUNCTION, Name("/A/B/C"));
    contentProducer.setContextOption(INTEREST_DROP_RCV_BUF, (ProducerInterestCallback)bind(&CallbackContainer::interestDropRcvBuf, &callback, _1, _2));
    contentProducer.setContextOption(CACHE_MISS, (ProducerInterestCallback)bind(&CallbackContainer::processContentInterest, &callback, _1, _2));
    contentProducer.setContextOption(CACHE_HIT, (ProducerInterestCallback)bind(&CallbackContainer::cacheHit, &callback, _1, _2));
    contentProducer.setContextOption(INTEREST_ENTER_CNTX, (ProducerInterestCallback)bind(&CallbackContainer::onNewInterest, &callback, _1, _2));
    contentProducer.setContextOption(INTEREST_PASS_RCV_BUF, (ProducerInterestCallback)bind(&CallbackContainer::interestPassedToSendBuffer, &callback, _1, _2));
    contentProducer.setContextOption(NEW_DATA_SEGMENT, (ProducerDataCallback)bind(&CallbackContainer::newDataSegment, &callback, _1, _2));
    contentProducer.setContextOption(DATA_IN_SND_BUF, (ProducerDataCallback)bind(&CallbackContainer::sendBufferPopulation, &callback, _1, _2));
    contentProducer.setContextOption(DATA_LEAVE_CNTX, (ProducerDataCallback)bind(&CallbackContainer::leavingContentData, &callback, _1, _2));
    contentProducer.attach();
    sleep(30000);
//    m_face->processEvents();

    return 0;
}
