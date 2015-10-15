// See LICENSE_CELLO file for license and copyright information

/// @file     data_ParticleDescr.hpp
/// @author   James Bordner (jobordner@ucsd.edu)
/// @date     2015-10-12

/// @brief    [\ref Data] Declaration of the ParticleDescr class

#ifndef DATA_PARTICLE_DESCR_HPP
#define DATA_PARTICLE_DESCR_HPP

class ParticleDescr {

  /// @class    ParticleDescr
  /// @ingroup  Data
  /// @brief    [\ref Data] 

  //----------------------------------------------------------------------

public: // interface

  /// Constructor
  ParticleDescr() throw();

  /// CHARM++ Pack / Unpack function
  void pup (PUP::er &p);

  //--------------------------------------------------
  // TYPES
  //--------------------------------------------------

  /// Create a new type and return its id

  int new_type(std::string type);

  /// Return the number of types of particles

  int num_types() const;

  /// Return the index for the given particle type

  int type_index (std::string type) const;

  /// Return the name of the given particle type given its index

  std::string type_name (int index) const;

  //--------------------------------------------------
  // ATTRIBUTES
  //--------------------------------------------------

  /// Create a new attribute for the given type and return its id

  int new_attribute(int it, std::string attribute, int attribute_type);

  /// Return the number of attributes of the given type.

  int num_attributes(int it) const;

  /// Return the index for the given attribute

  int attribute_index (int it, std::string attribute) const;

  /// Return the index for the given attribute

  std::string attribute_name (int it, int ia) const;

  //--------------------------------------------------
  // BYTES
  //--------------------------------------------------

  /// Return the number of bytes per particle allocated for all attributes

  int attribute_bytes (int it) const;

  /// Return the number of bytes allocated for the given attribute.

  int attribute_bytes(int it,int ia) const;

  //--------------------------------------------------
  // INTERLEAVING
  //--------------------------------------------------

  /// Set whether attributes are interleaved for the given type.

  void set_interleaved (int it, bool interleaved);

  /// Return whether attributes are interleaved or not

  bool interleaved (int it) const;

  /// Return the stride of the given attribute if interleaved, otherwise 1.
  /// Computed as attribute\_bytes(it) / attribute\_bytes(it,ia).
  /// Must be evenly divisible.

  int stride(int it, int ia) const;

  //--------------------------------------------------
  // BATCHES
  //--------------------------------------------------

  /// Set the size of batches.  Must be set at most once.  May be
  /// defined when ParticleDescr is created.

  void set_batch_size (int mb);

  /// Return the current batch size.

  int batch_size() const;

  /// Return the batch and particle indices given a global particle
  /// index i.  This is useful e.g. for iterating over a range of
  /// particles, e.g. initializing new particles after insert().
  /// Basically just div / mod.

  void index (int i, int * ib, int * ip) const;

  //--------------------------------------------------
  // GROUPING
  //--------------------------------------------------

  /// Return the Grouping object for the particle types

  Grouping * groups () { return & groups_; }

private: // functions

  void check_it_(std::string type_name, std::string file, int line);

  void check_ia_(int it, std::string attribute_name, std::string file, int line);

  //----------------------------------------------------------------------

private: // attributes

  //--------------------------------------------------
  // TYPES
  //--------------------------------------------------

  /// List of particle types
  std::vector<std::string> type_name_;

  /// Index of each particle type (inverse of type_)
  std::map<std::string,int> type_index_;

  //--------------------------------------------------
  // ATTRIBUTES
  //--------------------------------------------------

  /// List of particle attributes
  std::vector < std::vector<std::string> > attribute_name_;

  /// Index of each particle attribute (inverse of attribute_)
  std::vector < std::map<std::string,int> > attribute_index_;

  //--------------------------------------------------
  // BYTES
  //--------------------------------------------------

  /// Bytes used for each particle attribute, max 127
  std::vector < std::vector<char> > attribute_bytes_;

  //--------------------------------------------------
  // INTERLEAVING
  //--------------------------------------------------

  /// Whether attributes are interleaved
  std::vector < bool > attribute_interleaved_;

  //--------------------------------------------------
  // GROUPING
  //--------------------------------------------------

  Grouping groups_;

  //--------------------------------------------------
  // BATCHES
  //--------------------------------------------------

  /// Number of particles per "batch".  Particles are allocated,
  /// deallocated, and operated on a batch at a time

  int batch_size_;
};

#endif /* DATA_PARTICLE_DESCR_HPP */

