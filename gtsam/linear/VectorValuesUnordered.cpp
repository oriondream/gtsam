/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file VectorValues.cpp
 * @brief Implementations for VectorValues
 * @author Richard Roberts
 * @author Alex Cunningham
 */

#include <gtsam/base/FastVector.h>
#include <gtsam/linear/VectorValuesUnordered.h>

#include <boost/iterator/counting_iterator.hpp>

using namespace std;

namespace gtsam {

/* ************************************************************************* */
VectorValuesUnordered VectorValuesUnordered::Zero(const VectorValuesUnordered& x) {
  VectorValuesUnordered result;
  result.values_.resize(x.size());
  for(size_t j=0; j<x.size(); ++j)
    result.values_[j] = Vector::Zero(x.values_[j].size());
  return result;
}

/* ************************************************************************* */
vector<size_t> VectorValuesUnordered::dims() const {
  std::vector<size_t> result(this->size());
  for(Index j = 0; j < this->size(); ++j)
    result[j] = this->dim(j);
  return result;
}

/* ************************************************************************* */
void VectorValuesUnordered::insert(Index j, const Vector& value) {
  // Make sure j does not already exist
  if(exists(j))
    throw invalid_argument("VectorValues: requested variable index to insert already exists.");

  // If this adds variables at the end, insert zero-length entries up to j
  if(j >= size())
    values_.resize(j+1);

  // Assign value
  values_[j] = value;
}

/* ************************************************************************* */
void VectorValuesUnordered::print(const std::string& str, const IndexFormatter& formatter) const {
  std::cout << str << ": " << size() << " elements\n";
  for (Index var = 0; var < size(); ++var)
    std::cout << "  " << formatter(var) << ": \n" << (*this)[var] << "\n";
  std::cout.flush();
}

/* ************************************************************************* */
bool VectorValuesUnordered::equals(const VectorValuesUnordered& x, double tol) const {
  if(this->size() != x.size())
    return false;
  for(Index j=0; j < size(); ++j)
    if(!equal_with_abs_tol(values_[j], x.values_[j], tol))
      return false;
  return true;
}

/* ************************************************************************* */
void VectorValuesUnordered::resize(Index nVars, size_t varDim) {
  values_.resize(nVars);
  for(Index j = 0; j < nVars; ++j)
    values_[j] = Vector(varDim);
}

/* ************************************************************************* */
void VectorValuesUnordered::resizeLike(const VectorValuesUnordered& other) {
  values_.resize(other.size());
  for(Index j = 0; j < other.size(); ++j)
    values_[j].resize(other.values_[j].size());
}

/* ************************************************************************* */
VectorValuesUnordered VectorValuesUnordered::SameStructure(const VectorValuesUnordered& other) {
  VectorValuesUnordered ret;
  ret.resizeLike(other);
  return ret;
}

/* ************************************************************************* */
VectorValuesUnordered VectorValuesUnordered::Zero(Index nVars, size_t varDim) {
  VectorValuesUnordered ret(nVars, varDim);
  ret.setZero();
  return ret;
}

/* ************************************************************************* */
void VectorValuesUnordered::setZero() {
  BOOST_FOREACH(Vector& v, *this) {
    v.setZero();
  }
}

/* ************************************************************************* */
const Vector VectorValuesUnordered::asVector() const {
  return internal::extractVectorValuesSlices(*this,
    boost::make_counting_iterator(size_t(0)), boost::make_counting_iterator(this->size()), true);
}

/* ************************************************************************* */
const Vector VectorValuesUnordered::vector(const std::vector<Index>& indices) const {
  return internal::extractVectorValuesSlices(*this, indices.begin(), indices.end());
}

/* ************************************************************************* */
bool VectorValuesUnordered::hasSameStructure(const VectorValuesUnordered& other) const {
  if(this->size() != other.size())
    return false;
  for(size_t j = 0; j < size(); ++j)
    // Directly accessing maps instead of using VV::dim in case some values are empty
    if(this->values_[j].rows() != other.values_[j].rows())
      return false;
  return true;
}

/* ************************************************************************* */
void VectorValuesUnordered::swap(VectorValuesUnordered& other) {
  this->values_.swap(other.values_);
}

/* ************************************************************************* */
double VectorValuesUnordered::dot(const VectorValuesUnordered& v) const {
  double result = 0.0;
  if(this->size() != v.size())
    throw invalid_argument("VectorValues::dot called with different vector sizes");
  for(Index j = 0; j < this->size(); ++j)
    // Directly accessing maps instead of using VV::dim in case some values are empty
    if(this->values_[j].size() == v.values_[j].size())
      result += this->values_[j].dot(v.values_[j]);
    else
      throw invalid_argument("VectorValues::dot called with different vector sizes");
  return result;
}

/* ************************************************************************* */
double VectorValuesUnordered::norm() const {
  return std::sqrt(this->squaredNorm());
}

/* ************************************************************************* */
double VectorValuesUnordered::squaredNorm() const {
  double sumSquares = 0.0;
  for(Index j = 0; j < this->size(); ++j)
    // Directly accessing maps instead of using VV::dim in case some values are empty
    sumSquares += this->values_[j].squaredNorm();
  return sumSquares;
}

/* ************************************************************************* */
VectorValuesUnordered VectorValuesUnordered::operator+(const VectorValuesUnordered& c) const {
  VectorValuesUnordered result = SameStructure(*this);
  if(this->size() != c.size())
    throw invalid_argument("VectorValues::operator+ called with different vector sizes");
  for(Index j = 0; j < this->size(); ++j)
    // Directly accessing maps instead of using VV::dim in case some values are empty
    if(this->values_[j].size() == c.values_[j].size())
      result.values_[j] = this->values_[j] + c.values_[j];
    else
      throw invalid_argument("VectorValues::operator- called with different vector sizes");
  return result;
}

/* ************************************************************************* */
VectorValuesUnordered VectorValuesUnordered::operator-(const VectorValuesUnordered& c) const {
  VectorValuesUnordered result = SameStructure(*this);
  if(this->size() != c.size())
    throw invalid_argument("VectorValues::operator- called with different vector sizes");
  for(Index j = 0; j < this->size(); ++j)
    // Directly accessing maps instead of using VV::dim in case some values are empty
    if(this->values_[j].size() == c.values_[j].size())
      result.values_[j] = this->values_[j] - c.values_[j];
    else
      throw invalid_argument("VectorValues::operator- called with different vector sizes");
  return result;
}

/* ************************************************************************* */
void VectorValuesUnordered::operator+=(const VectorValuesUnordered& c) {
  if(this->size() != c.size())
    throw invalid_argument("VectorValues::operator+= called with different vector sizes");
  for(Index j = 0; j < this->size(); ++j)
    // Directly accessing maps instead of using VV::dim in case some values are empty
    if(this->values_[j].size() == c.values_[j].size())
      this->values_[j] += c.values_[j];
    else
      throw invalid_argument("VectorValues::operator+= called with different vector sizes");
}

/* ************************************************************************* */

} // \namespace gtsam
