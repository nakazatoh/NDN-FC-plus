/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013-2017 Regents of the University of California.
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
 * @author Jeff Thompson <jefft0@remap.ucla.edu>
 * @author Alexander Afanasyev <http://lasr.cs.ucla.edu/afanasyev/index.html>
 * @author Zhenkai Zhu <http://irl.cs.ucla.edu/~zhenkai/>
 */

#include "function.hpp"

#include "encoding/block.hpp"
#include "encoding/encoding-buffer.hpp"
#include "util/time.hpp"

#include <sstream>
#include <boost/algorithm/string/trim.hpp>
#include <boost/functional/hash.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/concepts.hpp>

namespace ndn {

BOOST_CONCEPT_ASSERT((boost::EqualityComparable<Function>));
BOOST_CONCEPT_ASSERT((WireEncodable<Function>));
BOOST_CONCEPT_ASSERT((WireEncodableWithEncodingBuffer<Function>));
BOOST_CONCEPT_ASSERT((WireDecodable<Function>));
BOOST_CONCEPT_ASSERT((boost::RandomAccessIterator<Function::iterator>));
BOOST_CONCEPT_ASSERT((boost::RandomAccessIterator<Function::const_iterator>));
BOOST_CONCEPT_ASSERT((boost::RandomAccessIterator<Function::reverse_iterator>));
BOOST_CONCEPT_ASSERT((boost::RandomAccessIterator<Function::const_reverse_iterator>));
BOOST_CONCEPT_ASSERT((boost::RandomAccessRangeConcept<Function>));
static_assert(std::is_base_of<tlv::Error, Function::Error>::value,
              "Function::Error must inherit from tlv::Error");

const size_t Function::npos = std::numeric_limits<size_t>::max();

// ---- constructors, encoding, decoding ----

Function::Function()
  : m_wire(tlv::Function)
{
}

Function::Function(const Block& wire)
  : m_wire(wire)
{
  m_wire.parse();
}

Function::Function(const char* uri)
  : Function(std::string(uri))
{
}

Function::Function(std::string uri)
{
  boost::algorithm::trim(uri);
  if (uri.empty())
    return;

  size_t iColon = uri.find(':');
  if (iColon != std::string::npos) {
    // Make sure the colon came before a '/'.
    size_t iFirstSlash = uri.find('/');
    if (iFirstSlash == std::string::npos || iColon < iFirstSlash) {
      // Omit the leading protocol such as ndn:
      uri.erase(0, iColon + 1);
      boost::algorithm::trim(uri);
    }
  }

  // Trim the leading slash and possibly the authority.
  if (uri[0] == '/') {
    if (uri.size() >= 2 && uri[1] == '/') {
      // Strip the authority following "//".
      size_t iAfterAuthority = uri.find('/', 2);
      if (iAfterAuthority == std::string::npos)
        // Unusual case: there was only an authority.
        return;
      else {
        uri.erase(0, iAfterAuthority + 1);
        boost::algorithm::trim(uri);
      }
    }
    else {
      uri.erase(0, 1);
      boost::algorithm::trim(uri);
    }
  }

  size_t iComponentStart = 0;

  // Unescape the components.
  while (iComponentStart < uri.size()) {
    size_t iComponentEnd = uri.find("/", iComponentStart);
    if (iComponentEnd == std::string::npos)
      iComponentEnd = uri.size();

    append(Component::fromEscapedString(&uri[0], iComponentStart, iComponentEnd));
    iComponentStart = iComponentEnd + 1;
  }
}

std::string
Function::toUri() const
{
  std::ostringstream os;
  os << *this;
  return os.str();
}

template<encoding::Tag TAG>
size_t
Function::wireEncode(EncodingImpl<TAG>& encoder) const
{
  size_t totalLength = 0;
  for (const Component& comp : *this | boost::adaptors::reversed) {
    totalLength += comp.wireEncode(encoder);
  }

  totalLength += encoder.prependVarNumber(totalLength);
  totalLength += encoder.prependVarNumber(tlv::Function);
  return totalLength;
}

NDN_CXX_DEFINE_WIRE_ENCODE_INSTANTIATIONS(Function);

const Block&
Function::wireEncode() const
{
  if (m_wire.hasWire())
    return m_wire;

  EncodingEstimator estimator;
  size_t estimatedSize = wireEncode(estimator);

  EncodingBuffer buffer(estimatedSize, 0);
  wireEncode(buffer);

  m_wire = buffer.block();
  m_wire.parse();

  return m_wire;
}

void
Function::wireDecode(const Block& wire)
{
  if (wire.type() != tlv::Function)
    BOOST_THROW_EXCEPTION(tlv::Error("Unexpected TLV type when decoding Function"));

  m_wire = wire;
  m_wire.parse();
}

Function
Function::deepCopy() const
{
  Function copiedFunction(*this);
  copiedFunction.m_wire.resetWire();
  copiedFunction.wireEncode(); // "compress" the underlying buffer
  return copiedFunction;
}

// ---- accessors ----

const name::Component&
Function::at(ssize_t i) const
{
  if (i < 0) {
    i = size() + i;
  }

  if (i < 0 || static_cast<size_t>(i) >= size()) {
    BOOST_THROW_EXCEPTION(Error("Requested component does not exist (out of bounds)"));
  }

  return reinterpret_cast<const Component&>(m_wire.elements()[i]);
}

PartialFunction
Function::getSubFunction(ssize_t iStartComponent, size_t nComponents) const
{
  PartialFunction result;

  ssize_t iStart = iStartComponent < 0 ? this->size() + iStartComponent : iStartComponent;
  size_t iEnd = this->size();

  iStart = std::max(iStart, static_cast<ssize_t>(0));

  if (nComponents != npos)
    iEnd = std::min(this->size(), iStart + nComponents);

  for (size_t i = iStart; i < iEnd; ++i)
    result.append(at(i));

  return result;
}

// ---- modifiers ----

Function&
Function::appendVersion()
{
  return appendVersion(time::toUnixTimestamp(time::system_clock::now()).count());
}

Function&
Function::appendTimestamp()
{
  return appendTimestamp(time::system_clock::now());
}

Function&
Function::append(const PartialFunction& name)
{
  if (&name == this)
    // Copying from this name, so need to make a copy first.
    return append(PartialFunction(name));

  for (size_t i = 0; i < name.size(); ++i)
    append(name.at(i));

  return *this;
}

// ---- algorithms ----

Function
Function::getSuccessor() const
{
  if (empty()) {
    static uint8_t firstValue[] {0};
    Function firstFunction;
    firstFunction.append(firstValue, 1);
    return firstFunction;
  }

  return getPrefix(-1).append(get(-1).getSuccessor());
}

bool
Function::isPrefixOf(const Function& other) const
{
  // This name is longer than the name we are checking against.
  if (size() > other.size())
    return false;

  // Check if at least one of given components doesn't match.
  for (size_t i = 0; i < size(); ++i) {
    if (get(i) != other.get(i))
      return false;
  }

  return true;
}

bool
Function::equals(const Function& other) const
{
  if (size() != other.size())
    return false;

  for (size_t i = 0; i < size(); ++i) {
    if (get(i) != other.get(i))
      return false;
  }

  return true;
}

int
Function::compare(size_t pos1, size_t count1, const Function& other, size_t pos2, size_t count2) const
{
  count1 = std::min(count1, this->size() - pos1);
  count2 = std::min(count2, other.size() - pos2);
  size_t count = std::min(count1, count2);

  for (size_t i = 0; i < count; ++i) {
    int comp = get(pos1 + i).compare(other.get(pos2 + i));
    if (comp != 0) { // i-th component differs
      return comp;
    }
  }
  // [pos1, pos1+count) of this Function equals [pos2, pos2+count) of other Function
  return count1 - count2;
}

// ---- stream operators ----

std::ostream&
operator<<(std::ostream& os, const Function& name)
{
  if (name.empty()) {
    os << "/";
  }
  else {
    for (const auto& component : name) {
      os << "/";
      component.toUri(os);
    }
  }
  return os;
}

std::istream&
operator>>(std::istream& is, Function& name)
{
  std::string inputString;
  is >> inputString;
  name = Function(inputString);

  return is;
}

} // namespace ndn

namespace std {

size_t
hash<ndn::Function>::operator()(const ndn::Function& name) const
{
  return boost::hash_range(name.wireEncode().wire(),
                           name.wireEncode().wire() + name.wireEncode().size());
}

} // namespace std
