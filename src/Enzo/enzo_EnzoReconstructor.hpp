
#ifndef ENZO_ENZO_RECONSTRUCTOR_HPP
#define ENZO_ENZO_RECONSTRUCTOR_HPP
class EnzoReconstructor : public PUP::able
{
  /// @class    EnzoReconstructor
  /// @ingroup  Enzo
  /// @brief    [\ref Enzo] Encapsulates reconstruction of primitives at
  //            interface

public: // interface

  /// Create a new EnzoReconstructor
  EnzoReconstructor() throw()
  {}

  /// CHARM++ PUP::able declaration
  //PUPable_abstract(EnzoReconstructor);

  /// CHARM++ migration constructor for PUP::able
  //EnzoReconstructor (CkMigrateMessage *m)
  //  : PUP::able(m)
  //{  }

  /// Virtual destructor
  virtual ~EnzoReconstructor()
  {  }

  /// CHARM++ Pack / Unpack function
  //void pup (PUP::er &p)
  //{
  //  PUP::able::pup(p);
  //}

  // Reconstructs the interface values
  // priml and primr are formally defined as corner-centered. However all
  // operations assume that they are face-centered along only 1 dimension.
  // This amounts to having some extra space at the end of the array
  virtual void reconstruct_interface (Block *block, std::vector<int> &prim_ids,
				      std::vector<int> &priml_ids,
				      std::vector<int> &primr_ids,
				      int dim)=0;
};

#endif /* ENZO_ENZO_RECONSTRUCTOR_HPP */
