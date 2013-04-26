// See LICENSE_CELLO file for license and copyright information

/// @file     comm_CommBlock.cpp
/// @author   James Bordner (jobordner@ucsd.edu)
/// @date     Mon Feb 28 13:22:26 PST 2011
/// @brief    Implementation of the CommBlock object
/// @todo     Remove hierarchy dependency via Initial--only need domain bounds

#include "cello.hpp"

#include "mesh.hpp"
#include "main.hpp"
#include "charm_simulation.hpp"

//----------------------------------------------------------------------

CommBlock::CommBlock
(
 int ibx, int iby, int ibz,
 int nbx, int nby, int nbz,
 int nx, int ny, int nz,
 double xpm, double ypm, double zpm, // Domain begin
 double xb, double yb, double zb,    // CommBlock width
 int num_field_blocks,
 bool testing
) throw ()
  :
    cycle_(0),
    time_(0),
    dt_(0)
{ 
  TRACE("CommBlock::CommBlock");

  block_ = new Block  (nx, ny, nz, num_field_blocks,
		       xpm+ibx*xb, xpm+(ibx+1)*xb,
		       ypm+iby*yb, ypm+(iby+1)*yb,
		       zpm+ibz*zb, zpm+(ibz+1)*zb);

  initialize_(ibx,iby,ibz, nbx,nby,nbz, nx,ny,nz,
	      xpm,ypm,zpm, xb,yb,zb,    testing);

#ifdef CONFIG_USE_CHARM
  sync_refresh_.stop() = count_refresh_();
#endif /* CONFIG_USE_CHARM */

}

//----------------------------------------------------------------------

void CommBlock::initialize_
(
 int ibx, int iby, int ibz,
 int nbx, int nby, int nbz,
 int nx, int ny, int nz,
 double xpm, double ypm, double zpm, // Domain begin
 double xb, double yb, double zb,   // CommBlock width
 bool testing
 )
 {
   size_[0] = nbx;
   size_[1] = nby;
   size_[2] = nbz;

   index_[0] = ibx;
   index_[1] = iby;
   index_[2] = ibz;

#ifdef CONFIG_USE_CHARM

   if (! testing) {
     // Count CommBlocks on each processor
   
     SimulationCharm * simulation_charm  = 
       dynamic_cast<SimulationCharm *> (proxy_simulation.ckLocalBranch());

     TRACE1 ("simulation_charm = %p",simulation_charm);
     TRACE1 ("simulation = %p",proxy_simulation.ckLocalBranch());
     TRACE1 ("proxy_simulation = %p",&proxy_simulation);
     if (simulation_charm) simulation_charm->insert_block();
   }
#endif


 }

//----------------------------------------------------------------------

CommBlock::~CommBlock() throw ()
{ 
#ifdef CONFIG_USE_CHARM

  if (block_) delete block_;
  block_ = 0;

  SimulationCharm * simulation_charm  = proxy_simulation.ckLocalBranch();

  if (simulation_charm) simulation_charm->delete_block();
#endif

}

//----------------------------------------------------------------------

CommBlock::CommBlock(const CommBlock & block) throw ()
/// @param     block  Object being copied
{
  copy_(block);
}

//----------------------------------------------------------------------

CommBlock & CommBlock::operator = (const CommBlock & block) throw ()
/// @param     block  Source object of the assignment
/// @return    The target assigned object
{
  copy_(block);
  return *this;
}

//----------------------------------------------------------------------

void CommBlock::index_forest (int * ix, int * iy, int * iz) const throw ()
{
  if (ix) (*ix) = index_[0]; 
  if (iy) (*iy) = index_[1]; 
  if (iz) (*iz) = index_[2]; 
}

//----------------------------------------------------------------------

void CommBlock::size_forest (int * nx=0, int * ny=0, int * nz=0) const throw ()
{
  if (nx) (*nx)=size_[0]; 
  if (ny) (*ny)=size_[1]; 
  if (nz) (*nz)=size_[2]; 
}

//======================================================================
// MPI FUNCTIONS
//======================================================================

#ifndef CONFIG_USE_CHARM

void CommBlock::refresh_ghosts(const FieldDescr * field_descr,
			       const Hierarchy * hierarchy,
			       int fx, int fy, int fz,
			       int index_field_set) throw()
{
  int ibx,iby,ibz;

  index_forest(&ibx,&iby,&ibz);

  block_->field_block(index_field_set)
    -> refresh_ghosts (field_descr,
		       hierarchy->group_process(),
		       hierarchy->layout(),
		       ibx,iby,ibz, fx,fy,fz);
}

#endif

//======================================================================
// CHARM FUNCTIONS
//======================================================================

#ifdef CONFIG_USE_CHARM

void CommBlock::prepare()
{

  TRACE1("CommBlock::prepare() %p",&thisProxy);
  Simulation * simulation = proxy_simulation.ckLocalBranch();

  //--------------------------------------------------
  // Enforce boundary conditions
  //--------------------------------------------------

  update_boundary_();

  FieldDescr * field_descr = simulation->field_descr();

  //--------------------------------------------------
  // Compute local dt
  //--------------------------------------------------

  Problem * problem = simulation->problem();

  double dt_block;
  Timestep * timestep = problem->timestep();

  dt_block = timestep->evaluate(field_descr,this);

  // Reduce timestep to coincide with scheduled output if needed

  int index_output=0;
  while (Output * output = problem->output(index_output++)) {
    Schedule * schedule = output->schedule();
    dt_block = schedule->update_timestep(time_,dt_block);
  }


  // Reduce timestep to not overshoot final time from stopping criteria

  Stopping * stopping = problem->stopping();

  double time_stop = stopping->stop_time();
  double time_curr = time_;

  dt_block = MIN (dt_block, (time_stop - time_curr));

  //--------------------------------------------------
  // Evaluate local stopping criteria
  //--------------------------------------------------

  int stop_block = stopping->complete(cycle_,time_);

  //--------------------------------------------------
  // Reduce to find CommBlock array minimum dt and stopping criteria
  //--------------------------------------------------

  double min_reduce[2];

  min_reduce[0] = dt_block;
  min_reduce[1] = stop_block ? 1.0 : 0.0;

  CkCallback callback (CkIndex_CommBlock::p_output(NULL), thisProxy);
  TRACE1("Calling contribute %d",2*sizeof(double));
  contribute( 2*sizeof(double), min_reduce, CkReduction::min_double, callback);

}
#endif /* CONFIG_USE_CHARM */

//----------------------------------------------------------------------

#ifdef CONFIG_USE_CHARM

void CommBlock::p_output(CkReductionMsg * msg)
{

  TRACE("CommBlock::p_output()");
  double * min_reduce = (double * )msg->getData();

  double dt_forest   = min_reduce[0];
  bool   stop_forest = min_reduce[1] == 1.0 ? true : false;
  set_dt   (dt_forest);
  TRACE2("CommBlock::p_output(): dt=%f  stop=%d",dt_forest,stop_forest);

  delete msg;

  Simulation * simulation = proxy_simulation.ckLocalBranch();

  simulation->update_state(cycle_,time_,dt_forest,stop_forest);

  // Wait for all blocks to check in before calling Simulation::p_output()
  // for next output

  TRACE("CommBlock::p_output() calling SimulationCharm::p_output");
  SimulationCharm * simulation_charm = proxy_simulation.ckLocalBranch();
  simulation_charm->p_output();
}
#endif /* CONFIG_USE_CHARM */

//----------------------------------------------------------------------

#ifdef CONFIG_USE_CHARM

//----------------------------------------------------------------------
void CommBlock::p_compute (int cycle, double time, double dt)
{
  // set_cycle(cycle);
  // set_time(time);
  // set_dt(dt);

  TRACE3 ("CommBlock::p_compute() cycle %d time %f dt %f",cycle,time,dt);
  compute();
}
#endif /* CONFIG_USE_CHARM */

//----------------------------------------------------------------------

#ifdef CONFIG_USE_CHARM

void CommBlock::refresh ()
{
  TRACE ("CommBlock::refresh()");

  bool is_boundary[3][2];

  Simulation * simulation = proxy_simulation.ckLocalBranch();
  Boundary * boundary = simulation->problem()->boundary();
  FieldDescr * field_descr = simulation->field_descr();
  
  bool periodic = boundary->is_periodic();

  CProxy_CommBlock block_array = thisProxy;

  //--------------------------------------------------
  // Refresh
  //--------------------------------------------------

  int ibx,iby,ibz;

  thisIndex.array(&ibx,&iby,&ibz);

  int nbx = size_[0];
  int nby = size_[1];
  int nbz = size_[2];
  
  bool fx3[3],fy3[3],fz3[3];
  determine_boundary_
    (is_boundary,&fx3[0],&fx3[2],&fy3[0],&fy3[2],&fz3[0],&fz3[2]);
  fx3[1]=true;
  fy3[1]=true;
  fz3[1]=true;
  fx3[0] = fx3[0] && (periodic || ! is_boundary[axis_x][face_lower]);
  fx3[2] = fx3[2] && (periodic || ! is_boundary[axis_x][face_upper]);
  fy3[0] = fy3[0] && (periodic || ! is_boundary[axis_y][face_lower]);
  fy3[2] = fy3[2] && (periodic || ! is_boundary[axis_y][face_upper]);
  fz3[0] = fz3[0] && (periodic || ! is_boundary[axis_z][face_lower]);
  fz3[2] = fz3[2] && (periodic || ! is_boundary[axis_z][face_upper]);
  int ix3[3],iy3[3],iz3[3];
  ix3[0] = (ibx - 1 + nbx) % nbx;
  iy3[0] = (iby - 1 + nby) % nby;
  iz3[0] = (ibz - 1 + nbz) % nbz;
  ix3[1] = ibx;
  iy3[1] = iby;
  iz3[1] = ibz;
  ix3[2] = (ibx + 1) % nbx;
  iy3[2] = (iby + 1) % nby;
  iz3[2] = (ibz + 1) % nbz;
  
  // Refresh face ghost zones

  bool gx,gy,gz;
  gx = false;
  gy = false;
  gz = false;

  int fxl = 1;
  int fyl = (nby==1 && ! periodic) ? 0 : 1;
  int fzl = (nbz==1 && ! periodic) ? 0 : 1;

  for (int fx=-fxl; fx<=fxl; fx++) {
    for (int fy=-fyl; fy<=fyl; fy++) {
      for (int fz=-fzl; fz<=fzl; fz++) {
	int sum = abs(fx)+abs(fy)+abs(fz);
	if ((fx3[fx+1] && fy3[fy+1] && fz3[fz+1]) &&
	    ((sum==1 && field_descr->refresh_face(2)) ||
	     (sum==2 && field_descr->refresh_face(1)) ||
	     (sum==3 && field_descr->refresh_face(0)))) {

	  FieldFace field_face (block_->field_block(),field_descr);

	  field_face.set_face(fx,fy,fz);
	  field_face.set_ghost(gx,gy,gz);
	  
	  DEBUG9("index %d %d %d  %d %d %d  %d %d %d",
		 index_[0],index_[1],index_[2],
		 ix3[fx+1],iy3[fy+1],iz3[fz+1],
		 fx,fy,fz);

	  int n; 
	  char * array;
	  field_face.load(&n, &array);

	  Index index;

	  index.set_array(ix3[fx+1],iy3[fy+1],iz3[fz+1]);
	  index.set_level(0);
	  index.clean();

	  thisProxy[index].x_refresh (n,array,-fx,-fy,-fz);
	}
      }
    }
  }

  // NOTE: x_refresh() calls compute, but if no incoming faces
  // it will never get called.  So every block also calls
  // x_refresh() itself with a null array

  x_refresh (0,0,0, 0, 0);

}
#endif /* CONFIG_USE_CHARM */

//----------------------------------------------------------------------

#ifdef CONFIG_USE_CHARM

void CommBlock::determine_boundary_
(
 bool is_boundary[3][2],
 bool * fxm, bool * fxp,
 bool * fym, bool * fyp,
 bool * fzm, bool * fzp
 )
{
  
  // return is_boundary[] array of faces on domain boundary

  is_on_boundary (is_boundary);

  int nx,ny,nz;
  block_->field_block()->size (&nx,&ny,&nz);

  // Determine in which directions we need to communicate or update boundary

  if (fxm) *fxm = (nx > 1);
  if (fxp) *fxp = (nx > 1);
  if (fym) *fym = (ny > 1);
  if (fyp) *fyp = (ny > 1);
  if (fzm) *fzm = (nz > 1);
  if (fzp) *fzp = (nz > 1);
}

#endif

//----------------------------------------------------------------------


#ifdef CONFIG_USE_CHARM

void CommBlock::update_boundary_ ()
{

  bool is_boundary[3][2];
  bool fxm,fxp,fym,fyp,fzm,fzp;

  determine_boundary_(is_boundary,&fxm,&fxp,&fym,&fyp,&fzm,&fzp);

  Simulation * simulation = proxy_simulation.ckLocalBranch();

  Boundary * boundary = simulation->problem()->boundary();
  const FieldDescr * field_descr = simulation->field_descr();


  // Update boundaries

  if ( fxm && is_boundary[axis_x][face_lower] ) {
    boundary->enforce(field_descr,this,face_lower,axis_x);
  }
  if ( fxp && is_boundary[axis_x][face_upper] ) {
    boundary->enforce(field_descr,this,face_upper,axis_x);
  }
  if ( fym && is_boundary[axis_y][face_lower] ) {
    boundary->enforce(field_descr,this,face_lower,axis_y);
  }
  if ( fyp && is_boundary[axis_y][face_upper] ) {
    boundary->enforce(field_descr,this,face_upper,axis_y);
  }
  if ( fzm && is_boundary[axis_z][face_lower] ) {
    boundary->enforce(field_descr,this,face_lower,axis_z);
  }
  if ( fzp && is_boundary[axis_z][face_upper] ) {
    boundary->enforce(field_descr,this,face_upper,axis_z);
  }
}

#endif

//----------------------------------------------------------------------

#ifdef CONFIG_USE_CHARM

void CommBlock::x_refresh (int n, char * buffer, int fx, int fy, int fz)
{

  TRACE ("CommBlock::x_refresh()");
  Simulation * simulation = proxy_simulation.ckLocalBranch();

  FieldDescr * field_descr = simulation->field_descr();

  if ( n != 0) {

    // n == 0 is the call from self to ensure x_refresh()
    // always gets called at least once

    bool gx,gy,gz;
    gx = false;
    gy = false;
    gz = false;

    FieldFace field_face(block_->field_block(), field_descr);

    field_face.set_face(fx,fy,fz);
    field_face.set_ghost(gx,gy,gz);

    field_face.store (n, buffer);
  }

  //--------------------------------------------------
  // Compute
  //--------------------------------------------------

  if (sync_refresh_.done()) {
    TRACE ("CommBlock::x_refresh() calling prepare()");
    prepare();
  }
}
#endif /* CONFIG_USE_CHARM */

//----------------------------------------------------------------------

#ifdef CONFIG_USE_CHARM

int CommBlock::count_refresh_()
{
  //--------------------------------------------------
  // Count incoming faces
  // (SHOULD NOT RECOMPUTE EVERY CALL)
  //--------------------------------------------------

  int nx,ny,nz;
  block_->field_block()->size (&nx,&ny,&nz);

  // Determine axes that may be neighbors

  bool fxm = nx > 1;
  bool fxp = nx > 1;
  bool fym = ny > 1;
  bool fyp = ny > 1;
  bool fzm = nz > 1;
  bool fzp = nz > 1;

  // Adjust for boundary faces

  Simulation * simulation = proxy_simulation.ckLocalBranch();

  bool periodic = simulation->problem()->boundary()->is_periodic();

  bool is_boundary[3][2];
  is_on_boundary (is_boundary);

  fxm = fxm && (periodic || ! is_boundary[axis_x][face_lower]);
  fxp = fxp && (periodic || ! is_boundary[axis_x][face_upper]);
  fym = fym && (periodic || ! is_boundary[axis_y][face_lower]);
  fyp = fyp && (periodic || ! is_boundary[axis_y][face_upper]);
  fzm = fzm && (periodic || ! is_boundary[axis_z][face_lower]);
  fzp = fzp && (periodic || ! is_boundary[axis_z][face_upper]);

  // count self

  int count = 1;

  // count faces

  FieldDescr * field_descr = simulation->field_descr();

  if (field_descr->refresh_face(2)) {
    if ( fxm ) ++count;
    if ( fxp ) ++count;
    if ( fym ) ++count;
    if ( fyp ) ++count;
    if ( fzm ) ++count;
    if ( fzp ) ++count;
  }

  // count edges

  if (field_descr->refresh_face(1)) {
    if ( fxm && fym ) ++count;
    if ( fxm && fyp ) ++count;
    if ( fxp && fym ) ++count;
    if ( fxp && fyp ) ++count;
    if ( fym && fzm ) ++count;
    if ( fym && fzp ) ++count;
    if ( fyp && fzm ) ++count;
    if ( fyp && fzp ) ++count;
    if ( fzm && fxm ) ++count;
    if ( fzm && fxp ) ++count;
    if ( fzp && fxm ) ++count;
    if ( fzp && fxp ) ++count;
  }

  // count corners

  if (field_descr->refresh_face(0)) {
    if ( fxm && fym && fzm ) ++count;
    if ( fxm && fym && fzp ) ++count;
    if ( fxm && fyp && fzm ) ++count;
    if ( fxm && fyp && fzp ) ++count;
    if ( fxp && fym && fzm ) ++count;
    if ( fxp && fym && fzp ) ++count;
    if ( fxp && fyp && fzm ) ++count;
    if ( fxp && fyp && fzp ) ++count;
  }

  return count;
}

#endif /* CONFIG_USE_CHARM */

//----------------------------------------------------------------------

#ifdef CONFIG_USE_CHARM

void CommBlock::compute()
{
  TRACE ("CommBlock::compute()");

  Simulation * simulation = proxy_simulation.ckLocalBranch();

 #ifdef CONFIG_USE_PROJECTIONS
   double time_start = CmiWallTimer();
 #endif

  FieldDescr * field_descr = simulation->field_descr();

  int index_method = 0;
  while (Method * method = simulation->problem()->method(index_method++)) {
    method -> compute_block (field_descr,this);
  }

 #ifdef CONFIG_USE_PROJECTIONS
   traceUserBracketEvent(10,time_start, CmiWallTimer());
 #endif

  // Update CommBlock cycle and time to Simulation time and cycle

  set_cycle (cycle_ + 1);

  set_time  (time_  + dt_);
  
  // prepare for next cycle: Timestep, Stopping, Monitor, Output

  TRACE ("CommBlock::compute() calling refresh()");
  refresh();

}
#endif /* CONFIG_USE_CHARM */

//======================================================================

void CommBlock::copy_(const CommBlock & comm_block) throw()
{
  block_->copy_(*comm_block.block());
  for (int i=0; i<3; i++) {
    index_[i] = comm_block.index_[i];
    size_[i] = comm_block.size_[i];
  }

  cycle_ = comm_block.cycle_;
  time_ = comm_block.time_;
  dt_ = comm_block.dt_;

#ifdef CONFIG_USE_CHARM
  sync_refresh_ = comm_block.sync_refresh_;
#endif
  
}

//----------------------------------------------------------------------

void CommBlock::is_on_boundary (bool is_boundary[3][2]) throw()
{

  // // COMPARISON MAY BE INACCURATE FOR VERY SMALL BLOCKS NEAR BOUNDARY

  // is_boundary[axis_x][face_lower] = 
  //   (cello::err_abs(lower_[axis_x],lower[axis_x]) < 1e-6);
  // is_boundary[axis_y][face_lower] = 
  //   (cello::err_abs(lower_[axis_y],lower[axis_y]) < 1e-6);
  // is_boundary[axis_z][face_lower] = 
  //   (cello::err_abs(lower_[axis_z],lower[axis_z]) < 1e-6);
  // is_boundary[axis_x][face_upper] = 
  //   (cello::err_abs(upper_[axis_x],upper[axis_x]) < 1e-6);
  // is_boundary[axis_y][face_upper] = 
  //   (cello::err_abs(upper_[axis_y],upper[axis_y]) < 1e-6);
  // is_boundary[axis_z][face_upper] = 
  //   (cello::err_abs(upper_[axis_z],upper[axis_z]) < 1e-6);
  is_boundary[0][0] = (index_[0] == 0);
  is_boundary[1][0] = (index_[1] == 0);
  is_boundary[2][0] = (index_[2] == 0);
  is_boundary[0][1] = (index_[0] == size_[0] - 1);
  is_boundary[1][1] = (index_[1] == size_[1] - 1);
  is_boundary[2][1] = (index_[2] == size_[2] - 1);
}
//----------------------------------------------------------------------

void CommBlock::allocate (const FieldDescr * field_descr) throw()
{ 
  block_->allocate(field_descr);
    //    block_.field_block(i)->allocate_array(field_descr);
    //    block_.field_block(i)->allocate_ghosts(field_descr);
}

//----------------------------------------------------------------------

