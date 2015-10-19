// See LICENSE_CELLO file for license and copyright information

/// @file     data_ParticleDescr.cpp
/// @author   James Bordner (jobordner@ucsd.edu)
/// @date     Fri Aug 15 11:48:15 PDT 2014
/// @brief    Implementation of the ParticleDescr class
///
/// ParticleDescr is used to describe the content of ParticleData's
/// data Separate classes are used to avoid redundant storage between
/// ParticleBlocks, which are designed to be memory-efficient.
///
/// Particles have different types (e.g. tracer, dark matter, etc.),
/// and different types have different attributes (e.g. position,
/// velocity, mass, etc.)  ParticleDescr objects store which particle
/// types each ParticleBlock contains, and provides operations to
/// assist in "decoding" the data stored in ParticleBlocks.

#include "data.hpp"

//----------------------------------------------------------------------

ParticleDescr::ParticleDescr() throw()
  : type_name_(),
    type_index_(),
    attribute_name_(),
    attribute_index_(),
    attribute_bytes_(),
    attribute_offset_(),
    batch_size_(1)
{
}

//----------------------------------------------------------------------

void ParticleDescr::pup (PUP::er &p)
{
  p | type_name_;
  p | type_index_;
  p | attribute_name_;
  p | attribute_index_;
  p | attribute_bytes_;
  p | attribute_offset_;
}

//----------------------------------------------------------------------
// Types
//----------------------------------------------------------------------

int ParticleDescr::new_type(std::string type_name)
{
  const int nt = num_types();

#ifdef CELLO_CHECK
  for (int i=0; i<nt; i++) {
    ASSERT1("ParticleDescr::new_type()",
	    "Particle type %s already exists",
	    type_name.c_str(),
	    type_name_.at(i) != type_name);
  }
#endif

  type_name_.push_back(type_name);
  attribute_interleaved_.push_back(false);

  type_index_[type_name] = nt;

  attribute_name_. resize(nt + 1);
  attribute_index_.resize(nt + 1);
  attribute_bytes_.resize(nt + 1);
  attribute_offset_.resize(nt + 1);

  return nt;
}

//----------------------------------------------------------------------

int ParticleDescr::num_types() const
{
  return type_name_.size();
}

//----------------------------------------------------------------------

int ParticleDescr::type_index (std::string type_name) const
{
  std::map<const std::string,int>::const_iterator it;
  it=type_index_.find(type_name);
  if (it != type_index_.end()) {
    return it->second;
  } else {
    WARNING1("ParticleDescr::type_index()",
	     "Trying to access unknown Particle type \"%s\"",
	     type_name.c_str());
    return -1;
  }
}

//----------------------------------------------------------------------

std::string ParticleDescr::type_name (int it) const
{
  ASSERT1("ParticleDescr::type_name",
	  "Trying to access unknown particle index %d",
	  it,
	  (0 <= it && it < int(type_name_.size()) ));

  return type_name_[it];
}

//----------------------------------------------------------------------
// Attributes
//----------------------------------------------------------------------

int ParticleDescr::new_attribute
(int         it,
 std::string attribute_name,
 int         attribute_bytes)
{
#ifdef CELLO_CHECK
  check_ia_(it,attribute_name,__FILE__,__LINE__);
  int b = attribute_bytes;
  ASSERT1("ParticleDescr::new_attribute",
	 "attribute_bytes %d must be a power of 2",
	 attribute_bytes,
	 (b&&!(b&(b-1))));
	 
#endif

  const int ia = num_attributes(it);

  attribute_name_[it].push_back(attribute_name);
  attribute_index_[it][attribute_name] = ia;
  attribute_bytes_[it].push_back(attribute_bytes);

  // Calculate offset, ensuring type of size N bytes is aligned
  // accordingly in memory on at least N-byte boundary.

  int offset;
  if (attribute_interleaved_[it]) {
    offset = (ia > 0) ?
      attribute_offset_[it][ia-1] + attribute_bytes_[it][ia-1] : 0;
    int align = offset % attribute_bytes;
    if (align != 0) {
      offset += (attribute_bytes - align);
    }
  } else {
    ASSERT ("ParticleDescr::new_attribute",
	    "Non-interleaved particles not implemented",
	    false);
  }
  attribute_offset_[it].push_back(offset);

  return ia;
}

//----------------------------------------------------------------------

int ParticleDescr::num_attributes(int it) const
{
  return attribute_name_[it].size();
}

//----------------------------------------------------------------------

int ParticleDescr::attribute_index (int it, std::string attribute_name) const
{
  std::map<const std::string,int>::const_iterator iter;
  iter=attribute_index_[it].find(attribute_name);
  if (iter != attribute_index_[it].end()) {
    return iter->second;
  } else {
    WARNING2("ParticleDescr::attribute_index()",
	     "Trying to access unknown attribute %s in particle type \"%s\"",
	     attribute_name.c_str(),type_name(it).c_str());
    return -1;
  }
}

//----------------------------------------------------------------------

std::string ParticleDescr::attribute_name (int it, int ia) const
{
  ASSERT1("ParticleDescr::type_name",
	  "Trying to access unknown particle index %d",
	  it,
	  (0 <= it && it < int(type_name_.size()) ));
  ASSERT1("ParticleDescr::type_name",
	  "Trying to access unknown particle attribute %d",
	  ia,
	  (0 <= ia && ia < int(attribute_name_[it].size()) ));

  return attribute_name_[it][ia];
}

//----------------------------------------------------------------------

int ParticleDescr::attribute_bytes (int it) const
{
  int sum = 0;
  int max = -1;
  for (int ia=0; ia < attribute_bytes_[it].size(); ia++) {
    sum += attribute_bytes_[it][ia];
    max = std::max(max,int(attribute_bytes_[it][ia]));
  }
  //  return (sum/max)*max == sum ? sum : (sum/max+1)*max;
  return ((sum-1)/max+1)*max;
}

//----------------------------------------------------------------------

int ParticleDescr::attribute_bytes(int it,int ia) const
{
  return attribute_bytes_[it][ia];
}

//----------------------------------------------------------------------

int ParticleDescr::stride(int it, int ia) const
{
  return attribute_interleaved_[it] ? 
    attribute_bytes(it) / attribute_bytes(it,ia) : 1;
}

//----------------------------------------------------------------------

void ParticleDescr::set_interleaved (int it, bool interleaved)
{
  attribute_interleaved_[it] = interleaved;
}

//----------------------------------------------------------------------

bool ParticleDescr::interleaved (int it) const
{
  return attribute_interleaved_.at(it);
}

//----------------------------------------------------------------------

void ParticleDescr::set_batch_size (int mb)
{
  batch_size_ = mb;
}

//----------------------------------------------------------------------

int ParticleDescr::batch_size () const
{
  return batch_size_;
}

//----------------------------------------------------------------------

void ParticleDescr::index (int i, int * ib, int * ip) const
{
  *ib = i / batch_size_;
  *ip = i % batch_size_;
}

//----------------------------------------------------------------------

int ParticleDescr::attribute_offset (int it, int ia) const
{
  return attribute_offset_[it][ia]; 
}

//======================================================================

void ParticleDescr::check_it_(std::string type_name,
			      std::string file, int line)
{
  const int nt = num_types();

  for (int i=0; i<nt; i++) {
    ASSERT3("ParticleDescr::new_attribute()",
    "Particle type %s already exists [ file %s line %d ]",
    type_name.c_str(), file.c_str(),line,
	   type_name_.at(i) != type_name);
  }
}

//----------------------------------------------------------------------

void ParticleDescr::check_ia_(int it, std::string attribute_name, 
			      std::string file, int line)
{
  const int nt = num_types();
  const int na = num_attributes(nt);

  for (int i=0; i<na; i++) {
    ASSERT3("ParticleDescr::new_attribute()",
	   "Particle type %s already exists [ file %s line %d ]",
    attribute_name.c_str(), file.c_str(),line,
    attribute_name_[it].at(i) != attribute_name);
  }

}
