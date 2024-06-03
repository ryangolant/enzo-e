// See LICENSE_CELLO file for license and copyright information

/// @file     EnzoEwald.cpp
/// @author   Ryan Golant (ryan.golant@columbia.edu)
/// @date     September 28, 2023
/// @brief    Compute Ewald sums for periodic boundary conditions

// I probably don't need to include all of these right? Probably just EnzoMethodEwald.hpp and EnzoMethodMultipole.hpp
#include "cello.hpp"

#include "enzo.hpp"

#include "EnzoMethodEwald.hpp"

#include "enzo_EnzoMethodMultipole.hpp"

#include "charm_simulation.hpp"
#include "charm_mesh.hpp"
#include "main.hpp"


EnzoMethodEwald::EnzoMethodEwald (int interp_xpoints, int interp_ypoints, int interp_zpoints)
  : d0_array_(interp_xpoints * interp_ypoints * interp_zpoints), // Nx x Ny x Nz x 1 (on down-sampled grid of dimension Nx x Ny x Nz)
    d1_array_(interp_xpoints * interp_ypoints * interp_zpoints), // Nx x Ny x Nz x 3
    d2_array_(interp_xpoints * interp_ypoints * interp_zpoints), // Nx x Ny x Nz x 6
    d3_array_(interp_xpoints * interp_ypoints * interp_zpoints), // Nx x Ny x Nz x 10
    d4_array_(interp_xpoints * interp_ypoints * interp_zpoints), // Nx x Ny x Nz x 15
    d5_array_(interp_xpoints * interp_ypoints * interp_zpoints), // Nx x Ny x Nz x 21
    d6_array_(interp_xpoints * interp_ypoints * interp_zpoints), // Nx x Ny x Nz x 28
    interp_xpoints_(interp_xpoints),  // number of interpolation points in the x-direction
    interp_ypoints_(interp_ypoints),  // number of interpolation points in the y-direction
    interp_zpoints_(interp_zpoints)   // number of interpolation points in the z-direction
{ 

  // EnzoMethodEwald constructor is called in *compute* of EnzoMethodMultipole
  

  //CkPrintf("before init interpolate\n");

  init_interpolate_();

  CkPrintf("after init interpolate\n");

}

// set up interpolation arrays on the primary domain
void EnzoMethodEwald::init_interpolate_() throw()
{
  Hierarchy * hierarchy = enzo::simulation()->hierarchy();

  // int max_level_ = hierarchy->max_level();
  // CkPrintf("max_level\n");

  double lox, loy, loz; 
  double hix, hiy, hiz;
  
  hierarchy->lower(&lox, &loy, &loz);
  hierarchy->upper(&hix, &hiy, &hiz);
  //loz = 0.5; 
  //hiz = 0.5;
  // CkPrintf("in init interpolate: %f, %f", loz, hiz);

  // cello::hierarchy()->lower(&lox, &loy);
  // cello::hierarchy()->upper(&hix, &hiy);

  // double lox = -0.5; double loy = -0.5; double loz = 0.5;
  // double hix = 0.5; double hiy = 0.5; double hiz = 0.5;

  // what to do if only 1 interp point in a given direction?
  double dx = (hix - lox) / (interp_xpoints_ - 1);
  double dy = (hiy - loy) / (interp_ypoints_ - 1);
  double dz = (hiz - loz) / (interp_zpoints_ - 1);

  // do we need to do this full loop? could we just do a single octant? 
  for (int iz = 0; iz < interp_zpoints_; iz++) {
    for (int iy = 0; iy < interp_ypoints_; iy++) {
      for (int ix = 0; ix < interp_xpoints_; ix++) {
        
        int i = ix + interp_xpoints_ * (iy + iz * interp_ypoints_);
        double x = lox + ix*dx;  
        double y = loy + iy*dy;
        double z = loz + iz*dz;

        if (i == 2015) CkPrintf("x, y, z: %f, %f, %f\n", x, y, z);

        d0_array_[i] = d0(x, y, z);   // d0 is not necessary

        d1_array_[i] = d1(x, y, z);

        d2_array_[i] = d2(x, y, z);

        d3_array_[i] = d3(x, y, z);

        d4_array_[i] = d4(x, y, z);

        d5_array_[i] = d5(x, y, z);

        d6_array_[i] = d6(x, y, z);
      }
    }
  }

}


/********************************************************************************************/
/*  Evaluate the Taylor series used to interpolate the Ewald derivatives */


// compute the Taylor series to interpolate derivative tensors from interpolation points to (x,y,z).
// Note: this function is not necessary, since our Taylor series start at d1
double EnzoMethodEwald::interp_d0(double x, double y, double z) throw()
{
  int i;
  double interp_x, interp_y, interp_z;
  find_nearest_interp_point(x, y, z, &interp_x, &interp_y, &interp_z, &i);

    
  // std::array<double, 3> delta_r = {x - interp_x, y - interp_y, z - interp_z};
  // std::array<double, 6> delta_r2 = outer_11_(delta_r, delta_r);
  // std::array<double, 10> delta_r3 = outer_12_(delta_r, delta_r2);

  std::vector<double> delta_r = {x - interp_x, y - interp_y, z - interp_z};
  std::vector<double> delta_r2 = EnzoMethodMultipole::outer_11_(delta_r, delta_r);
  std::vector<double> delta_r3 = EnzoMethodMultipole::outer_12_(delta_r, delta_r2);

  double zeroth_term = d0_array_[i];
  double first_term = EnzoMethodMultipole::dot_11_(delta_r, d1_array_[i]);
  double second_term = 0.5 * EnzoMethodMultipole::dot_22_(delta_r2, d2_array_[i]);
  double third_term = 1.0/6.0 * EnzoMethodMultipole::dot_33_(delta_r3, d3_array_[i]);

  return zeroth_term + first_term + second_term + third_term;
  
}

std::vector<double> EnzoMethodEwald::interp_d1(double x, double y, double z) throw()
{

  int i;
  double interp_x, interp_y, interp_z;
  find_nearest_interp_point(x, y, z, &interp_x, &interp_y, &interp_z, &i);
    
  // std::array<double, 3> delta_r = {x - interp_x, y - interp_y, z - interp_z};
  // std::array<double, 6> delta_r2 = outer_11_(delta_r, delta_r);
  // std::array<double, 10> delta_r3 = outer_12_(delta_r, delta_r2);

  std::vector<double> delta_r = {x - interp_x, y - interp_y, z - interp_z};
  // std::vector<double> delta_r = {x - interp_x, y - interp_y, 0};
  std::vector<double> delta_r2 = EnzoMethodMultipole::outer_11_(delta_r, delta_r);
  std::vector<double> delta_r3 = EnzoMethodMultipole::outer_12_(delta_r, delta_r2);

  // std::array<double, 3> zeroth_term = d1_array[i];
  // std::array<double, 3> first_term = dot_12_(delta_r, d2_array[i]);
  // std::array<double, 3> second_term = 0.5 * dot_23_(delta_r2, d3_array[i]);
  // std::array<double, 3> third_term = 1.0/6.0 * dot_34_(delta_r3, d4_array[i]);

  std::vector<double> zeroth_term = d1_array_[i];
  std::vector<double> first_term = EnzoMethodMultipole::dot_12_(delta_r, d2_array_[i]);
  std::vector<double> second_term = EnzoMethodMultipole::dot_scalar_(0.5, EnzoMethodMultipole::dot_23_(delta_r2, d3_array_[i]), 3);
  std::vector<double> third_term = EnzoMethodMultipole::dot_scalar_(1.0/6.0, EnzoMethodMultipole::dot_34_(delta_r3, d4_array_[i]), 3);

  std::vector<double> zero_plus_one = EnzoMethodMultipole::add_(zeroth_term, first_term, 3);
  std::vector<double> two_plus_three = EnzoMethodMultipole::add_(second_term, third_term, 3);

  return EnzoMethodMultipole::add_(zero_plus_one, two_plus_three, 3);
  
}

std::vector<double> EnzoMethodEwald::interp_d2(double x, double y, double z) throw()
{

  int i;
  double interp_x, interp_y, interp_z;
  find_nearest_interp_point(x, y, z, &interp_x, &interp_y, &interp_z, &i);
  CkPrintf("i, interp_x, interp_y: %d, %f, %f\n", i, interp_x, interp_y);
  // std::array<double, 3> delta_r = {x - interp_x, y - interp_y, z - interp_z};
  // std::array<double, 6> delta_r2 = outer_11_(delta_r, delta_r);
  // std::array<double, 10> delta_r3 = outer_12_(delta_r, delta_r2);

  std::vector<double> delta_r = {x - interp_x, y - interp_y, z - interp_z};
  // std::vector<double> delta_r = {x - interp_x, y - interp_y, 0};
  std::vector<double> delta_r2 = EnzoMethodMultipole::outer_11_(delta_r, delta_r);
  std::vector<double> delta_r3 = EnzoMethodMultipole::outer_12_(delta_r, delta_r2);

  // std::array<double, 6> zeroth_term = d2_array[i];
  // std::array<double, 6> first_term = dot_13_(delta_r, d3_array[i]);
  // std::array<double, 6> second_term = 0.5 * dot_24_(delta_r2, d4_array[i]);
  // std::array<double, 6> third_term = 1.0/6.0 * dot_35_(delta_r3, d5_array[i]);

  std::vector<double> zeroth_term = d2_array_[i];
  std::vector<double> first_term = EnzoMethodMultipole::dot_13_(delta_r, d3_array_[i]);
  std::vector<double> second_term = EnzoMethodMultipole::dot_scalar_(0.5, EnzoMethodMultipole::dot_24_(delta_r2, d4_array_[i]), 6);
  std::vector<double> third_term = EnzoMethodMultipole::dot_scalar_(1.0/6.0, EnzoMethodMultipole::dot_35_(delta_r3, d5_array_[i]), 6);

  CkPrintf("0th term: %f\n", zeroth_term[0]);
  CkPrintf("1st term: %f\n", first_term[0]);
  CkPrintf("2nd term: %f\n", second_term[0]);
  CkPrintf("3rd term: %f\n", third_term[0]);

  std::vector<double> zero_plus_one = EnzoMethodMultipole::add_(zeroth_term, first_term, 6);
  std::vector<double> two_plus_three = EnzoMethodMultipole::add_(second_term, third_term, 6);

  return EnzoMethodMultipole::add_(zero_plus_one, two_plus_three, 6);
  
}

std::vector<double> EnzoMethodEwald::interp_d3(double x, double y, double z) throw()
{

  int i;
  double interp_x, interp_y, interp_z;
  find_nearest_interp_point(x, y, z, &interp_x, &interp_y, &interp_z, &i);
    
  // std::array<double, 3> delta_r = {x - interp_x, y - interp_y, z - interp_z};
  // std::array<double, 6> delta_r2 = outer_11_(delta_r, delta_r);
  // std::array<double, 10> delta_r3 = outer_12_(delta_r, delta_r2);

  std::vector<double> delta_r = {x - interp_x, y - interp_y, z - interp_z};
  // std::vector<double> delta_r = {x - interp_x, y - interp_y, 0};
  std::vector<double> delta_r2 = EnzoMethodMultipole::outer_11_(delta_r, delta_r);
  std::vector<double> delta_r3 = EnzoMethodMultipole::outer_12_(delta_r, delta_r2);

  // std::array<double, 10> zeroth_term = d3_array[i];
  // std::array<double, 10> first_term = dot_14_(delta_r, d4_array[i]);
  // std::array<double, 10> second_term = 0.5 * dot_25_(delta_r2, d5_array[i]);
  // std::array<double, 10> third_term = 1.0/6.0 * dot_36_(delta_r3, d6_array[i]);

  std::vector<double> zeroth_term = d3_array_[i];
  std::vector<double> first_term = EnzoMethodMultipole::dot_14_(delta_r, d4_array_[i]);
  std::vector<double> second_term = EnzoMethodMultipole::dot_scalar_(0.5, EnzoMethodMultipole::dot_25_(delta_r2, d5_array_[i]), 10);
  std::vector<double> third_term = EnzoMethodMultipole::dot_scalar_(1.0/6.0, EnzoMethodMultipole::dot_36_(delta_r3, d6_array_[i]), 10);

  std::vector<double> zero_plus_one = EnzoMethodMultipole::add_(zeroth_term, first_term, 10);
  std::vector<double> two_plus_three = EnzoMethodMultipole::add_(second_term, third_term, 10);

  return EnzoMethodMultipole::add_(zero_plus_one, two_plus_three, 10);
  
}


/********************************************************************************************/
/*  Compute the terms of the Taylor series required for interpolating Ewald derivatives */


// compute the d0 term of the Ewald sum at coordinates (x, y, z)
double EnzoMethodEwald::d0(double x, double y, double z) throw()
{
  /* d0 = g0 */

  // CkPrintf("we're in d0\n");

  double d0_counter = 0;

  Hierarchy * hierarchy = enzo::simulation()->hierarchy();
  double lox, loy, loz; 
  double hix, hiy, hiz;
  hierarchy->lower(&lox, &loy, &loz);
  hierarchy->upper(&hix, &hiy, &hiz);

  double Lx = hix - lox;
  double Ly = hiy - loy;
  double Lz = hiz - loz;
  double box_vol = Lx * Ly * Lz;
  //cello::hierarchy()->root_size(&Lx, &Ly, &Lz);

  
  // gadget code has alpha = 2.0 / pow(box_vol, 1./3.), but gadget paper suggests 1/(2L)
  // double alpha = 1.0 / (2.0 * pow(box_vol, 1./3)); 
  double alpha = 2.0 / pow(box_vol, 1./3.); 
  
  
  double alpha2 = alpha * alpha;

  // sum in real space -- any way to remove these loops?
  for (int nz = -5; nz <= 5; nz++) {
    for (int ny = -5; ny <= 5; ny++) {
      for (int nx = -5; nx <= 5; nx++) {

        double rx = x + nx * Lx;
        double ry = y + ny * Ly;
        double rz = z + nz * Lz;
        double r = sqrt(rx*rx + ry*ry + rz*rz);

        double g0;

        if (nx != 0 || ny != 0 || nz !=0) {
          g0 = erfc(alpha * r) / r;
        }
        else {
          
          // Taylor expand if r is close to 0
          if ((alpha * r) < 0.5) {
            g0 = -2.0 * alpha / sqrt(M_PI) * (1.0 - pow(alpha * r, 2) / 3.0 + pow(alpha * r, 4) / 10.0 - pow(alpha * r, 6) / 42.0 
                                            + pow(alpha * r, 8) / 216.0 - pow(alpha * r, 10) / 1320.0);
          }
          else { // incorporate Newtonian 1/r term
            g0 = -erf(alpha*r)/r; // 1/r - erfc(alpha*r) / r;
          }
        }

        d0_counter -= g0;
      }
    }
  }

  // sum in Fourier space
  for (int nz = -5; nz <= 5; nz++) {
    for (int ny = -5; ny <= 5; ny++) {
      for (int nx = -5; nx <= 5; nx++) {

        if (nx != 0 || ny != 0 || nz != 0) {

          double kx = 2.0 * M_PI * nx / Lx;
          double ky = 2.0 * M_PI * ny / Ly;
          double kz = 2.0 * M_PI * nz / Lz;
          double k2 = kx*kx + ky*ky + kz*kz;
          double kdotx = kx*x + ky*y + kz*z;

          double k_exp = exp(-k2 / (4.0*alpha2)) / k2;

          d0_counter -= 4.0*M_PI/box_vol * k_exp * cos(kdotx);
        } 
      }
    }
  }

  // additional pi/(alpha^2 V) term, only for d0
  d0_counter += M_PI/(alpha2 * box_vol);

  // CkPrintf("made it to end of d0\n");
  return d0_counter;

  
}

// compute the d1 term of the Ewald sum at coordinates (x, y, z)
std::vector<double> EnzoMethodEwald::d1(double x, double y, double z) throw()
{
  /* (d1)_i = g1 r_i */

  std::vector<double> d1_counter (3, 0);

  Hierarchy * hierarchy = enzo::simulation()->hierarchy();
  double lox, loy, loz; 
  double hix, hiy, hiz;
  hierarchy->lower(&lox, &loy, &loz);
  hierarchy->upper(&hix, &hiy, &hiz);

  double Lx = hix - lox;
  double Ly = hiy - loy;
  double Lz = hiz - loz;
  double box_vol = Lx * Ly * Lz;


  // double alpha = 1.0 / (2.0 * pow(box_vol, 1./3));
  double alpha = 2.0 / pow(box_vol, 1./3.);
  double alpha2 = alpha * alpha;

  // sum in real space
  for (int nz = -5; nz <= 5; nz++) {
    for (int ny = -5; ny <= 5; ny++) {
      for (int nx = -5; nx <= 5; nx++) {

        double rx = x + nx * Lx;
        double ry = y + ny * Ly;
        double rz = z + nz * Lz;
        double r = sqrt(rx*rx + ry*ry + rz*rz);
        if (r==0) CkPrintf("r: %f", r);
        double r2 = r*r;
        double r3 = r*r2;

        double g1;

        if (nx != 0 || ny != 0 || nz != 0) {
          g1 = (-2.0 * alpha * r - sqrt(M_PI) * exp(alpha2*r2) * erfc(alpha*r)) * exp(-1.0 * alpha2 * r2) / (sqrt(M_PI) * r3); 
        }
        else {
          
          // Taylor expand if r is close to 0
          if ((alpha * r) < 0.5) {
            // check sign
            g1 = -4.0 * pow(alpha, 3) / sqrt(M_PI) *
                       (-1.0 / 3.0 + pow(alpha * r, 2) / 5.0 - pow(alpha * r, 4) / 14.0 + pow(alpha * r, 6) / 54.0 -
                        pow(alpha * r, 8) / 264.0 + pow(alpha * r, 10) / 1560.0);

            if (abs(x + 0.007937) <= 1e-5 && abs(y + 0.007937) <= 1e-5 && abs(z + 0) < 1e-2) {
              CkPrintf("D1 Taylor: g1, rx, ry, rz: %f, %f, %f, %f\n", g1, rx, ry, rz);
            }
          }
          else { // incorporate Newtonian 1/r term
            g1 = 1.0/r3 + ((-2.0 * alpha * r - sqrt(M_PI) * exp(alpha2*r2) * erfc(alpha*r)) 
                  * exp(-1.0 * alpha2 * r2) / (sqrt(M_PI) * r3));
          }
        }

        d1_counter[0] -= g1 * rx;
        d1_counter[1] -= g1 * ry;
        d1_counter[2] -= g1 * rz;

      }
    }
  }
  

  // sum in Fourier space
  for (int nz = -5; nz <= 5; nz++) {
    for (int ny = -5; ny <= 5; ny++) {
      for (int nx = -5; nx <= 5; nx++) {

        if (nx != 0 || ny != 0 || nz != 0) {

          double kx = 2.0 * M_PI * nx / Lx;
          double ky = 2.0 * M_PI * ny / Ly;
          double kz = 2.0 * M_PI * nz / Lz;
          double k2 = kx*kx + ky*ky + kz*kz;
          double kdotx = kx*x + ky*y + kz*z;

          double k_exp = 4.0*M_PI/box_vol * (exp(-k2 / (4.0*alpha2)) / k2) * sin(kdotx); 

          d1_counter[0] += k_exp * kx;
          d1_counter[1] += k_exp * ky;
          d1_counter[2] += k_exp * kz;
        } 
      }
    }
  }


  return d1_counter;

}

// compute the d2 term of the Ewald sum at coordinates (x, y, z)
std::vector<double> EnzoMethodEwald::d2(double x, double y, double z) throw()
{
  /* (d2)_ij = g1 delta_ij + g2 r_i r_j */

  std::vector<double> d2_counter (6, 0);

  Hierarchy * hierarchy = enzo::simulation()->hierarchy();
  double lox, loy, loz; 
  double hix, hiy, hiz;
  hierarchy->lower(&lox, &loy, &loz);
  hierarchy->upper(&hix, &hiy, &hiz);

  double Lx = hix - lox;
  double Ly = hiy - loy;
  double Lz = hiz - loz;
  double box_vol = Lx * Ly * Lz;

  
  // double alpha = 1.0 / (2.0 * pow(box_vol, 1./3));
  double alpha = 2.0 / pow(box_vol, 1./3.);
  double alpha2 = alpha * alpha;
  double alpha3 = alpha2 * alpha;

  // sum in real space
  for (int nz = -3; nz <= 3; nz++) {
    for (int ny = -3; ny <= 3; ny++) {
      for (int nx = -3; nx <= 3; nx++) {

        double rx = x + nx * Lx;
        double ry = y + ny * Ly;
        double rz = z + nz * Lz;
        double r = sqrt(rx*rx + ry*ry + rz*rz);
        double r2 = r*r;
        double r3 = r*r2;
        double r5 = r2*r3;

        double g1, g2;

        if (nx != 0 || ny != 0 || nz !=0) {
          g1 = (-2.0 * alpha * r - sqrt(M_PI) * exp(alpha2*r2) * erfc(alpha*r)) * exp(-1.0 * alpha2 * r2) / (sqrt(M_PI) * r3);

          g2 = (4.0 * sqrt(M_PI) * alpha3 * r3 + 6.0 * sqrt(M_PI) * alpha * r + 3.0 * M_PI * exp(alpha2 * r2) * erfc(alpha*r))
                * exp(-1.0 * alpha2 * r2) / (M_PI * r5);
          
          // if (abs(x + 0.007937) <= 1e-5 && abs(y + 0.007937) <= 1e-5 && abs(z + 0) < 1e-2) {
          //     CkPrintf("Away: g1, g2, rx, ry, rz: %f, %f, %f, %f, %f\n", g1, g2, rx, ry, rz);
          //   }
        }
        else {
          
          // Taylor expand if r is close to 0
          if ((alpha * r) < 0.5) {
            g1 = -4.0 * pow(alpha, 3) / sqrt(M_PI) *
                       (-1.0 / 3.0 + pow(alpha * r, 2) / 5.0 - pow(alpha * r, 4) / 14.0 + pow(alpha * r, 6) / 54.0 -
                        pow(alpha * r, 8) / 264.0 + pow(alpha * r, 10) / 1560.0);

            g2 = -8.0 * pow(alpha, 5) / sqrt(M_PI) *
                       (1.0 / 5.0 - pow(alpha * r, 2) / 7.0 + pow(alpha * r, 4) / 18.0 - pow(alpha * r, 6) / 66.0 +
                        pow(alpha * r, 8) / 312.0 - pow(alpha * r, 10) / 1800.0);

            if (abs(x + 0.007937) <= 1e-5 && abs(y + 0.007937) <= 1e-5 && abs(z + 0) < 1e-2) {
              CkPrintf("D2 Taylor: g1, g2, rx, ry, rz: %f, %f, %f, %f, %f\n", g1, g2, rx, ry, rz);
            }

          }
          else { // incorporate Newtonian 1/r term
            g1 = 1.0/r3 + ((-2.0 * alpha * r - sqrt(M_PI) * exp(alpha2*r2) * erfc(alpha*r)) 
                  * exp(-1.0 * alpha2 * r2) / (sqrt(M_PI) * r3));

            g2 = -3.0/r5 + (4.0 * sqrt(M_PI) * alpha3 * r3 + 6.0 * sqrt(M_PI) * alpha * r + 3.0 * M_PI * exp(alpha2 * r2) * erfc(alpha*r))
                * exp(-1.0 * alpha2 * r2) / (M_PI * r5);
            
            if (abs(x + 0.007937) <= 1e-5 && abs(y + 0.007937) <= 1e-5 && abs(z + 0) < 1e-2) {
              // CkPrintf("Primary domain: g1, g2, rx, ry, rz: %f, %f, %f, %f, %f\n", g1, g2, rx, ry, rz);
            }
          }
        }

        // for (int j = 0; j < 3; j++) {
        //     for (int i = 0; i < 3; i++) {
        //       int index = 3*i + j;

        //       d2_counter[index] -= g2 * rvec[i] * rvec[j];
        //       if (i == j) {
        //         d2_counter[index] -= g1;
        //       }
        //     }
        //   }

        d2_counter[0] -= g2*rx*rx + g1;
        d2_counter[1] -= g2*rx*ry;
        d2_counter[2] -= g2*rx*rz; 
        d2_counter[3] -= g2*ry*ry + g1;
        d2_counter[4] -= g2*ry*rz; 
        d2_counter[5] -= g2*rz*rz + g1;

      }
    }
  }

  if (abs(x + 0.007937) <= 1e-5 && abs(y + 0.007937) <= 1e-5 && abs(z + 0) < 1e-2) {
    CkPrintf("D2_counter: %f, %f, %f, %f, %f, %f\n", d2_counter[0], d2_counter[1], d2_counter[2], d2_counter[3], d2_counter[4], d2_counter[5]);
  }
  

  // sum in Fourier space
  for (int nz = -5; nz <= 5; nz++) {
    for (int ny = -5; ny <= 5; ny++) {
      for (int nx = -5; nx <= 5; nx++) {

        if (nx != 0 || ny != 0 || nz != 0) {

          double kx = 2.0 * M_PI * nx / Lx;
          double ky = 2.0 * M_PI * ny / Ly;
          double kz = 2.0 * M_PI * nz / Lz;
          double k2 = kx*kx + ky*ky + kz*kz;
          double kdotx = kx*x + ky*y + kz*z;

          double k_exp = 4.0*M_PI/box_vol * (exp(-1.0*k2 / (4.0*alpha2)) / k2) * cos(kdotx); 

          // for (int j = 0; j < 3; j++) {
          //   for (int i = 0; i < 3; i++) {
          //     int index = 3*i + j;

          //     d2_counter[index] += k_exp * kvec[i] * kvec[j];
          //   }
          // }

          d2_counter[0] += k_exp * kx*kx;
          d2_counter[1] += k_exp * kx*ky;
          d2_counter[2] += k_exp * kx*kz;
          d2_counter[3] += k_exp * ky*ky;
          d2_counter[4] += k_exp * ky*kz;
          d2_counter[5] += k_exp * kz*kz;
        } 
      }
    }
  }

  return d2_counter;
  
}

// compute the d3 term of the Ewald sum at coordinates (x, y, z)
std::vector<double> EnzoMethodEwald::d3(double x, double y, double z) throw()
{
  /* (d3)_ijk = g2 * (delta_ij * r_k + delta_jk * r_i + delta_ik * r_j) + g_3 * r_i * r_j * r_k */

  std::vector<double> d3_counter (10, 0);

  Hierarchy * hierarchy = enzo::simulation()->hierarchy();
  double lox, loy, loz; 
  double hix, hiy, hiz;
  hierarchy->lower(&lox, &loy, &loz);
  hierarchy->upper(&hix, &hiy, &hiz);

  double Lx = hix - lox;
  double Ly = hiy - loy;
  double Lz = hiz - loz;
  double box_vol = Lx * Ly * Lz;

  
  // double alpha = 1.0 / (2.0 * pow(box_vol, 1./3));
  double alpha = 2.0 / pow(box_vol, 1./3.);
  double alpha2 = alpha * alpha;
  double alpha3 = alpha2 * alpha;
  double alpha5 = alpha2 * alpha3;

  // sum in real space
  for (int nz = -5; nz <= 5; nz++) {
    for (int ny = -5; ny <= 5; ny++) {
      for (int nx = -5; nx <= 5; nx++) {

        double rx = x + nx * Lx;
        double ry = y + ny * Ly;
        double rz = z + nz * Lz;
        double r = sqrt(rx*rx + ry*ry + rz*rz);
        double r2 = r*r;
        double r3 = r2*r;
        double r5 = r2*r3;
        double r7 = r2*r5;

        double g2, g3;

        if (nx != 0 || ny != 0 || nz !=0) {
          g2 = (4.0 * sqrt(M_PI) * alpha3 * r3 + 6.0 * sqrt(M_PI) * alpha * r + 3.0 * M_PI * exp(alpha2 * r2) * erfc(alpha*r))
                * exp(-1.0 * alpha2 * r2) / (M_PI * r5);

          g3 = (-8.0 * sqrt(M_PI) * alpha5 * r5 - 20.0 * sqrt(M_PI) * alpha3 * r3 - 30.0 * sqrt(M_PI) * alpha * r
                - 15.0 * M_PI * exp(alpha2 * r2) * erfc(alpha*r)) * exp(-1.0 * alpha2 * r2) / (M_PI * r7);
        }
        else {
          
          // Taylor expand if r is close to 0
          if ((alpha * r) < 0.5) {
            // check signs
            g2 = -8.0 * pow(alpha, 5) / sqrt(M_PI) *
                       (1.0 / 5.0 - pow(alpha * r, 2) / 7.0 + pow(alpha * r, 4) / 18.0 - pow(alpha * r, 6) / 66.0 +
                        pow(alpha * r, 8) / 312.0 - pow(alpha * r, 10) / 1800.0);

            g3 = -16.0 * pow(alpha, 7) / sqrt(M_PI) *
                       (-1.0 / 7.0 + pow(alpha * r, 2) / 9.0 - pow(alpha * r, 4) / 22.0 + pow(alpha * r, 6) / 78.0 -
                        pow(alpha * r, 8) / 360.0 + pow(alpha * r, 10) / 2040.0);

            if (abs(x + 0.007937) <= 1e-5 && abs(y + 0.007937) <= 1e-5 && abs(z + 0) < 1e-2) {
              CkPrintf("D3 Taylor: g2, g3, rx, ry, rz: %f, %f, %f, %f, %f\n", g2, g3, rx, ry, rz);
            }

          }
          else { // incorporate Newtonian 1/r term
            g2 = -3.0/r5 + (4.0 * sqrt(M_PI) * alpha3 * r3 + 6.0 * sqrt(M_PI) * alpha * r + 3.0 * M_PI * exp(alpha2 * r2) * erfc(alpha*r))
                * exp(-1.0 * alpha2 * r2) / (M_PI * r5);

            g3 = 15.0/r7 + (-8.0 * sqrt(M_PI) * alpha5 * r5 - 20.0 * sqrt(M_PI) * alpha3 * r3 - 30.0 * sqrt(M_PI) * alpha * r
                - 15.0 * M_PI * exp(alpha2 * r2) * erfc(alpha*r)) * exp(-1.0 * alpha2 * r2) / (M_PI * r7);
          }
        }

        // for (int k = 0; k < 3; k++) {
        //   for (int j = 0; j < 3; j++) {
        //     for (int i = 0; i < 3; i++) {

        //       int index = j + 3 * (i + 3*k);

        //       d3_counter[index] -= g3 * rvec[i] * rvec[j] * rvec[k];

        //       if (i == j) {
        //         d3_counter[index] -= g2 * rvec[k];
        //       }

        //       if (j == k) {
        //         d3_counter[index] -= g2 * rvec[i];
        //       }

        //       if (i == k) {
        //         d3_counter[index] -= g2 * rvec[j];
        //       }
              
        //     }
        //   }
        // }

        d3_counter[0] -= g3*rx*rx*rx + 3*g2*rx;
        d3_counter[1] -= g3*rx*rx*ry + g2*ry;
        d3_counter[2] -= g3*rx*rx*rz + g2*rz;
        d3_counter[3] -= g3*rx*ry*ry + g2*rx;
        d3_counter[4] -= g3*rx*ry*rz;
        d3_counter[5] -= g3*rx*rz*rz + g2*rx;
        d3_counter[6] -= g3*ry*ry*ry + 3*g2*ry;
        d3_counter[7] -= g3*ry*ry*rz + g2*rz;
        d3_counter[8] -= g3*ry*rz*rz + g2*ry;
        d3_counter[9] -= g3*rz*rz*rz + 3*g2*rz;
      }
    }
  }
  

  // sum in Fourier space
  for (int nz = -5; nz <= 5; nz++) {
    for (int ny = -5; ny <= 5; ny++) {
      for (int nx = -5; nx <= 5; nx++) {

        if (nx != 0 || ny != 0 || nz != 0) {

          double kx = 2.0 * M_PI * nx / Lx;
          double ky = 2.0 * M_PI * ny / Ly;
          double kz = 2.0 * M_PI * nz / Lz;
          double k2 = kx*kx + ky*ky + kz*kz;
          double kdotx = kx*x + ky*y + kz*z;

          double k_exp = 4.0*M_PI/box_vol * (exp(-1.0*k2 / (4.0*alpha2)) / k2) * sin(kdotx); 

          // for (int k = 0; k < 3; k++) {
          //   for (int j = 0; j < 3; j++) {
          //     for (int i = 0; i < 3; i++) {
          //       int index = j + 3 * (i + k * 3);

          //       d3_counter[index] -= k_exp * kvec[i] * kvec[j] * kvec[k];

          //     }
          //   }
          // }

          d3_counter[0] -= k_exp * kx*kx*kx;
          d3_counter[1] -= k_exp * kx*kx*ky;
          d3_counter[2] -= k_exp * kx*kx*kz;
          d3_counter[3] -= k_exp * kx*ky*ky;
          d3_counter[4] -= k_exp * kx*ky*kz;
          d3_counter[5] -= k_exp * kx*kz*kz;
          d3_counter[6] -= k_exp * ky*ky*ky;
          d3_counter[7] -= k_exp * ky*ky*kz;
          d3_counter[8] -= k_exp * ky*kz*kz;
          d3_counter[9] -= k_exp * kz*kz*kz;
        } 
      }
    }
  }

  return d3_counter;
  
}

// compute the d4 term of the Ewald sum at coordinates (x, y, z)
std::vector<double> EnzoMethodEwald::d4(double x, double y, double z) throw()
{
  /* (d4)_ijkl = g2 * (delta_ij * delta_kl + delta_jk * delta_il + delta_ik * delta_jl) 
               + g3 * (delta_ij r_k r_l + delta_jk r_i r_l + delta_ik r_j r_l + delta_il r_j r_k + delta_jl r_i r_k + delta_kl r_i r_j)
               + g4 * r_i * r_j * r_k * r_l      */

  std::vector<double> d4_counter (15, 0);

  Hierarchy * hierarchy = enzo::simulation()->hierarchy();
  double lox, loy, loz; 
  double hix, hiy, hiz;
  hierarchy->lower(&lox, &loy, &loz);
  hierarchy->upper(&hix, &hiy, &hiz);

  double Lx = hix - lox;
  double Ly = hiy - loy;
  double Lz = hiz - loz;
  double box_vol = Lx * Ly * Lz;


  // double alpha = 1.0 / (2.0 * pow(box_vol, 1./3));
  double alpha = 2.0 / pow(box_vol, 1./3.);
  double alpha2 = alpha * alpha;
  double alpha3 = alpha2 * alpha;
  double alpha5 = alpha2 * alpha3;
  double alpha7 = alpha2 * alpha5;

  // sum in real space
  for (int nz = -5; nz <= 5; nz++) {
    for (int ny = -5; ny <= 5; ny++) {
      for (int nx = -5; nx <= 5; nx++) {

        double rx = x + nx * Lx;
        double ry = y + ny * Ly;
        double rz = z + nz * Lz;
        double r = sqrt(rx*rx + ry*ry + rz*rz);
        double r2 = r*r;
        double r3 = r2*r;
        double r5 = r2*r3;
        double r7 = r2*r5;
        double r9 = r2*r7;

        double g2, g3, g4;

        if (nx != 0 || ny != 0 || nz !=0) {
          g2 = (4.0 * sqrt(M_PI) * alpha3 * r3 + 6.0 * sqrt(M_PI) * alpha * r + 3.0 * M_PI * exp(alpha2 * r2) * erfc(alpha*r))
                * exp(-1.0 * alpha2 * r2) / (M_PI * r5);

          g3 = (-8.0 * sqrt(M_PI) * alpha5 * r5 - 20.0 * sqrt(M_PI) * alpha3 * r3 - 30.0 * sqrt(M_PI) * alpha * r
                - 15.0 * M_PI * exp(alpha2 * r2) * erfc(alpha*r)) * exp(-1.0 * alpha2 * r2) / (M_PI * r7);

          g4 = (16.0 * sqrt(M_PI) * alpha7 * r7  + 56.0 * sqrt(M_PI) * alpha5 * r5 + 140.0 * sqrt(M_PI) * alpha3 * r3
                + 210.0 * sqrt(M_PI) * alpha * r + 105.0 * M_PI * exp(alpha2 * r2) * erfc(alpha * r)) 
                * exp(-1.0 * alpha2 * r2) / (M_PI * r9);
        }
        else {
          
          // Taylor expand if r is close to 0
          if ((alpha * r) < 0.5) {
            // check signs
            g2 = -8.0 * pow(alpha, 5) / sqrt(M_PI) *
                       (1.0 / 5.0 - pow(alpha * r, 2) / 7.0 + pow(alpha * r, 4) / 18.0 - pow(alpha * r, 6) / 66.0 +
                        pow(alpha * r, 8) / 312.0 - pow(alpha * r, 10) / 1800.0);

            g3 = -16.0 * pow(alpha, 7) / sqrt(M_PI) *
                       (-1.0 / 7.0 + pow(alpha * r, 2) / 9.0 - pow(alpha * r, 4) / 22.0 + pow(alpha * r, 6) / 78.0 -
                        pow(alpha * r, 8) / 360.0 + pow(alpha * r, 10) / 2040.0);

            g4 = -32.0 * pow(alpha, 9) / sqrt(M_PI) *
                       (1.0 / 9.0 - pow(alpha * r, 2) / 11.0 + pow(alpha * r, 4) / 26.0 - pow(alpha * r, 6) / 90.0 +
                        pow(alpha * r, 8) / 408.0 - pow(alpha * r, 10) / 2280.0);

            if (abs(x + 0.007937) <= 1e-5 && abs(y + 0.007937) <= 1e-5 && abs(z + 0) < 1e-2) {
              CkPrintf("D4 Taylor: g2, g3, g4, rx, ry, rz: %f, %f, %f, %f, %f, %f\n", g2, g3, g4, rx, ry, rz);
            }

          }
          else { // incorporate Newtonian 1/r term
            g2 = -3.0/r5 + (4.0 * sqrt(M_PI) * alpha3 * r3 + 6.0 * sqrt(M_PI) * alpha * r + 3.0 * M_PI * exp(alpha2 * r2) * erfc(alpha*r))
                * exp(-1.0 * alpha2 * r2) / (M_PI * r5);

            g3 = 15.0/r7 + (-8.0 * sqrt(M_PI) * alpha5 * r5 - 20.0 * sqrt(M_PI) * alpha3 * r3 - 30.0 * sqrt(M_PI) * alpha * r
                - 15.0 * M_PI * exp(alpha2 * r2) * erfc(alpha*r)) * exp(-1.0 * alpha2 * r2) / (M_PI * r7);
            
            g4 = -105.0/r9 + (16.0 * sqrt(M_PI) * alpha7 * r7  + 56.0 * sqrt(M_PI) * alpha5 * r5 + 140.0 * sqrt(M_PI) * alpha3 * r3
                + 210.0 * sqrt(M_PI) * alpha * r + 105.0 * M_PI * exp(alpha2 * r2) * erfc(alpha * r)) 
                * exp(-1.0 * alpha2 * r2) / (M_PI * r9);
          }
        }

        // for (int l = 0; l < 3; l++) {
        //   for (int k = 0; k < 3; k++) {
        //     for (int j = 0; j < 3; j++) {
        //       for (int i = 0; i < 3; i++) {

        //         int index = j + 3 * (i + 3*(k + 3*l));

        //         d4_counter[index] -= g4 * rvec[i] * rvec[j] * rvec[k] * rvec[l];

        //         if (i == j) {
        //           d4_counter[index] -= g3 * rvec[k] * rvec[l];

        //           if (k == l) {
        //             d4_counter[index] -= g2;
        //           }
        //         }

        //         if (j == k) {
        //           d4_counter[index] -= g3 * rvec[i] * rvec[l];

        //           if (i == l) {
        //             d4_counter[index] -= g2;
        //           }
        //         }

        //         if (i == k) {
        //           d4_counter[index] -= g3 * rvec[j] * rvec[l];

        //           if (j == l) {
        //             d4_counter[index] -= g2;
        //           }
        //         }

        //         if (i == l) {
        //           d4_counter[index] -= g3 * rvec[j] * rvec[k];
        //         }

        //         if (j == l) {
        //           d4_counter[index] -= g3 * rvec[i] * rvec[k];
        //         }

        //         if (k == l) {
        //           d4_counter[index] -= g3 * rvec[i] * rvec[j];
        //         }
        //       }
        //     }
        //   }
        // }

        d4_counter[0] -= g4*rx*rx*rx*rx + 6*g3*rx*rx + 3*g2;
        d4_counter[1] -= g4*rx*rx*rx*ry + 3*g3*rx*ry;
        d4_counter[2] -= g4*rx*rx*rx*rz + 3*g3*rx*rz;
        d4_counter[3] -= g4*rx*rx*ry*ry + g3*(rx*rx + ry*ry) + g2;
        d4_counter[4] -= g4*rx*rx*ry*rz + g3*ry*rz;
        d4_counter[5] -= g4*rx*rx*rz*rz + g3*(rx*rx + rz*rz) + g2;
        d4_counter[6] -= g4*rx*ry*ry*ry + 3*g3*rx*ry;
        d4_counter[7] -= g4*rx*ry*ry*rz + g3*rx*rz;
        d4_counter[8] -= g4*rx*ry*rz*rz + g3*rx*ry;
        d4_counter[9] -= g4*rx*rz*rz*rz + 3*g3*rx*rz;
        d4_counter[10] -= g4*ry*ry*ry*ry + 6*g3*ry*ry + 3*g2;
        d4_counter[11] -= g4*ry*ry*ry*rz + 3*g3*ry*rz;
        d4_counter[12] -= g4*ry*ry*rz*rz + g3*(ry*ry + rz*rz) + g2;
        d4_counter[13] -= g4*ry*rz*rz*rz + 3*g3*ry*rz;
        d4_counter[14] -= g4*rz*rz*rz*rz + 6*g3*rz*rz + 3*g2;

      }
    }
  }
  
  // sum in Fourier space
  for (int nz = -5; nz <= 5; nz++) {
    for (int ny = -5; ny <= 5; ny++) {
      for (int nx = -5; nx <= 5; nx++) {

        if (nx != 0 || ny != 0 || nz != 0) {

          double kx = 2.0 * M_PI * nx / Lx;
          double ky = 2.0 * M_PI * ny / Ly;
          double kz = 2.0 * M_PI * nz / Lz;
          double k2 = kx*kx + ky*ky + kz*kz;
          double kdotx = kx*x + ky*y + kz*z;

          double k_exp = 4.0*M_PI/box_vol * (exp(-1.0*k2 / (4.0*alpha2)) / k2) * cos(kdotx); 

          // for (int l = 0; l < 3; l++) {
          //   for (int k = 0; k < 3; k++) {
          //     for (int j = 0; j < 3; j++) {
          //       for (int i = 0; i < 3; i++) {

          //         int index = j + 3 * (i + 3 * (k + 3 * l));

          //         d4_counter[index] -= k_exp * kvec[i] * kvec[j] * kvec[k] * kvec[l];
          //       }
          //     }
          //   }
          // }

          d4_counter[0] -= k_exp * kx*kx*kx*kx;
          d4_counter[1] -= k_exp * kx*kx*kx*ky;
          d4_counter[2] -= k_exp * kx*kx*kx*kz;
          d4_counter[3] -= k_exp * kx*kx*ky*ky;
          d4_counter[4] -= k_exp * kx*kx*ky*kz;
          d4_counter[5] -= k_exp * kx*kx*kz*kz;
          d4_counter[6] -= k_exp * kx*ky*ky*ky;
          d4_counter[7] -= k_exp * kx*ky*ky*kz;
          d4_counter[8] -= k_exp * kx*ky*kz*kz;
          d4_counter[9] -= k_exp * kx*kz*kz*kz;
          d4_counter[10] -= k_exp * ky*ky*ky*ky;
          d4_counter[11] -= k_exp * ky*ky*ky*kz;
          d4_counter[12] -= k_exp * ky*ky*kz*kz;
          d4_counter[13] -= k_exp * ky*kz*kz*kz;
          d4_counter[14] -= k_exp * kz*kz*kz*kz;

        } 
      }
    }
  }

  return d4_counter;
  
}

// compute the d5 term of the Ewald sum at coordinates (x, y, z)
std::vector<double> EnzoMethodEwald::d5(double x, double y, double z) throw()
{

  /* (d5)_ijklm = g3 [r_m (delta_ij delta_kl + delta_jk delta_il + delta_ik delta_jl) 
                      + r_l (delta_ij delta_km + delta_jk delta_im + delta_ik delta_jm) 
                      + r_k (delta_ij delta_lm + delta_jm delta_il + delta_im delta_jl)
                      + r_j (delta_im delta_kl + delta_km delta_il + delta_ik delta_lm)
                      + r_i (delta_jm delta_kl + delta_jk delta_lm + delta_km delta_jl)]
                  + g4 (delta_ij r_k r_l r_m + delta_jk r_i r_l r_m + delta_ik r_j r_l r_m
                      + delta_il r_j r_k r_m + delta_jl r_i r_k r_m + delta_kl r_i r_j r_m
                      + delta_im r_j r_k r_l + delta_jm r_i r_k r_l + delta_km r_i r_j r_l
                      + delta_lm r_i r_j r_k)
                  + g5 r_i r_j r_k r_l r_m       */

  std::vector<double> d5_counter (21, 0);

  Hierarchy * hierarchy = enzo::simulation()->hierarchy();
  double lox, loy, loz; 
  double hix, hiy, hiz;
  hierarchy->lower(&lox, &loy, &loz);
  hierarchy->upper(&hix, &hiy, &hiz);

  double Lx = hix - lox;
  double Ly = hiy - loy;
  double Lz = hiz - loz;
  double box_vol = Lx * Ly * Lz;

  
  // double alpha = 1.0 / (2.0 * pow(box_vol, 1./3));
  double alpha = 2.0 / pow(box_vol, 1./3.);
  double alpha2 = alpha * alpha;
  double alpha3 = alpha2 * alpha;
  double alpha5 = alpha2 * alpha3;
  double alpha7 = alpha2 * alpha5;
  double alpha9 = alpha2 * alpha7;

  // sum in real space
  for (int nz = -5; nz <= 5; nz++) {
    for (int ny = -5; ny <= 5; ny++) {
      for (int nx = -5; nx <= 5; nx++) {

        double rx = x + nx * Lx;
        double ry = y + ny * Ly;
        double rz = z + nz * Lz;
        double r = sqrt(rx*rx + ry*ry + rz*rz);
        double r2 = r*r;
        double r3 = r2*r;
        double r5 = r2*r3;
        double r7 = r2*r5;
        double r9 = r2*r7;
        double r11 = r2*r9;

        double g3, g4, g5;

        if (nx != 0 || ny != 0 || nz !=0) {
          g3 = (-8.0 * sqrt(M_PI) * alpha5 * r5 - 20.0 * sqrt(M_PI) * alpha3 * r3 - 30.0 * sqrt(M_PI) * alpha * r
                - 15.0 * M_PI * exp(alpha2 * r2) * erfc(alpha*r)) * exp(-1.0 * alpha2 * r2) / (M_PI * r7);

          g4 = (16.0 * sqrt(M_PI) * alpha7 * r7  + 56.0 * sqrt(M_PI) * alpha5 * r5 + 140.0 * sqrt(M_PI) * alpha3 * r3
                + 210.0 * sqrt(M_PI) * alpha * r + 105.0 * M_PI * exp(alpha2 * r2) * erfc(alpha * r)) 
                * exp(-1.0 * alpha2 * r2) / (M_PI * r9);

          g5 = (-32.0 * sqrt(M_PI) * alpha9 * r9 - 144.0 * sqrt(M_PI) * alpha7 * r7  - 504.0 * sqrt(M_PI) * alpha5 * r5 
                - 1260.0 * sqrt(M_PI) * alpha3 * r3 - 1890.0 * sqrt(M_PI) * alpha * r - 945.0 * M_PI * exp(alpha2 * r2) 
                * erfc(alpha * r)) * exp(-1.0 * alpha2 * r2) / (M_PI * r11);
        }
        else {
          
          // Taylor expand if r is close to 0
          if ((alpha * r) < 0.5) {
            // check signs
            g3 = -16.0 * pow(alpha, 7) / sqrt(M_PI) *
                       (-1.0 / 7.0 + pow(alpha * r, 2) / 9.0 - pow(alpha * r, 4) / 22.0 + pow(alpha * r, 6) / 78.0 -
                        pow(alpha * r, 8) / 360.0 + pow(alpha * r, 10) / 2040.0);

            g4 = -32.0 * pow(alpha, 9) / sqrt(M_PI) *
                       (1.0 / 9.0 - pow(alpha * r, 2) / 11.0 + pow(alpha * r, 4) / 26.0 - pow(alpha * r, 6) / 90.0 +
                        pow(alpha * r, 8) / 408.0 - pow(alpha * r, 10) / 2280.0);

            g5 = -64.0 * pow(alpha, 11) / sqrt(M_PI) *
                       (-1.0 / 11.0 + pow(alpha * r, 2) / 13.0 - pow(alpha * r, 4) / 30.0 + pow(alpha * r, 6) / 102.0 -
                        pow(alpha * r, 8) / 456.0 + pow(alpha * r, 10) / 2520.0);

            if (abs(x + 0.007937) <= 1e-5 && abs(y + 0.007937) <= 1e-5 && abs(z + 0) < 1e-2) {
              CkPrintf("D5 Taylor: g3, g4, g5, rx, ry, rz: %f, %f, %f, %f, %f, %f\n", g3, g4, g5, rx, ry, rz);
            }
          }

          else { // incorporate Newtonian 1/r term
            g3 = 15.0/r7 + (-8.0 * sqrt(M_PI) * alpha5 * r5 - 20.0 * sqrt(M_PI) * alpha3 * r3 - 30.0 * sqrt(M_PI) * alpha * r
                - 15.0 * M_PI * exp(alpha2 * r2) * erfc(alpha*r)) * exp(-1.0 * alpha2 * r2) / (M_PI * r7);
            
            g4 = -105.0/r9 + (16.0 * sqrt(M_PI) * alpha7 * r7  + 56.0 * sqrt(M_PI) * alpha5 * r5 + 140.0 * sqrt(M_PI) * alpha3 * r3
                + 210.0 * sqrt(M_PI) * alpha * r + 105.0 * M_PI * exp(alpha2 * r2) * erfc(alpha * r)) 
                * exp(-1.0 * alpha2 * r2) / (M_PI * r9);

            g5 = 945.0/r11 + (-32.0 * sqrt(M_PI) * alpha9 * r9 - 144.0 * sqrt(M_PI) * alpha7 * r7  - 504.0 * sqrt(M_PI) * alpha5 * r5 
                - 1260.0 * sqrt(M_PI) * alpha3 * r3 - 1890.0 * sqrt(M_PI) * alpha * r - 945.0 * M_PI * exp(alpha2 * r2) 
                * erfc(alpha * r)) * exp(-1.0 * alpha2 * r2) / (M_PI * r11);
          }
        }

        // for (int m = 0; m < 3; m++) {
        //   for (int l = 0; l < 3; l++) {
        //     for (int k = 0; k < 3; k++) {
        //       for (int j = 0; j < 3; j++) {
        //         for (int i = 0; i < 3; i++) {

        //           int index = j + 3 * (i + 3*(k + 3*(l + 3*m)));

        //           d5_counter[index] -= g5 * rvec[i] * rvec[j] * rvec[k] * rvec[l] * rvec[m];

        //           if (i == j) {
        //             d5_counter[index] -= g4 * rvec[k] * rvec[l] * rvec[m];

        //             if (k == l) {
        //               d5_counter[index] -= g3 * rvec[m];
        //             }

        //             if (k == m) {
        //               d5_counter[index] -= g3 * rvec[l];
        //             }

        //             if (l == m) {
        //               d5_counter[index] -= g3 * rvec[k];
        //             }
        //           }

        //           if (j == k) {
        //             d5_counter[index] -= g4 * rvec[i] * rvec[l] * rvec[m];

        //             if (i == l) {
        //               d5_counter[index] -= g3 * rvec[m];
        //             }

        //             if (i == m) {
        //               d5_counter[index] -= g3 * rvec[l];
        //             }

        //             if (l == m) {
        //               d5_counter[index] -= g3 * rvec[i];
        //             }
        //           }

        //           if (i == k) {
        //             d5_counter[index] -= g4 * rvec[j] * rvec[l] * rvec[m];

        //             if (j == l) {
        //               d5_counter[index] -= g3 * rvec[m];
        //             }

        //             if (j == m) {
        //               d5_counter[index] -= g3 * rvec[l];
        //             }

        //             if (l == m) {
        //               d5_counter[index] -= g3 * rvec[j];
        //             }
        //           }

        //           if (i == l) {
        //             d5_counter[index] -= g4 * rvec[j] * rvec[k] * rvec[m];
        //           }

        //           if (j == l) {
        //             d5_counter[index] -= g4 * rvec[i] * rvec[k] * rvec[m];
        //           }

        //           if (k == l) {
        //             d5_counter[index] -= g4 * rvec[i] * rvec[j] * rvec[m];
        //           }

        //           if (i == m) {
        //             d5_counter[index] -= g4 * rvec[j] * rvec[k] * rvec[l];

        //             if (j == l) {
        //               d5_counter[index] -= g3 * rvec[k];
        //             }

        //             if (k == l) {
        //               d5_counter[index] -= g3 * rvec[j];
        //             }
        //           }

        //           if (j == m) {
        //             d5_counter[index] -= g4 * rvec[i] * rvec[k] * rvec[l];

        //             if (i == l) {
        //               d5_counter[index] -= g3 * rvec[k];
        //             }

        //             if (k == l) {
        //               d5_counter[index] -= g3 * rvec[i];
        //             }
        //           }

        //           if (k == m) {
        //             d5_counter[index] -= g4 * rvec[i] * rvec[j] * rvec[l];

        //             if (i == l) {
        //               d5_counter[index] -= g3 * rvec[j];
        //             }

        //             if (j == l) {
        //               d5_counter[index] -= g3 * rvec[i];
        //             }
        //           }

        //           if (l == m) {
        //             d5_counter[index] -= g4 * rvec[i] * rvec[j] * rvec[k];
        //           }
        //         }
        //       }
        //     }
        //   }
        // }


        d5_counter[0] -= g5*rx*rx*rx*rx*rx + 10*g4*rx*rx*rx + 15*g3*rx;
        d5_counter[1] -= g5*rx*rx*rx*rx*ry + 6*g4*rx*rx*ry + 3*g3*ry;
        d5_counter[2] -= g5*rx*rx*rx*rx*rz + 6*g4*rx*rx*rz + 3*g3*rz;
        d5_counter[3] -= g5*rx*rx*rx*ry*ry + g4*(3*rx*ry*ry + rx*rx*rx) + 3*g3*rx;
        d5_counter[4] -= g5*rx*rx*rx*ry*rz + 3*g4*rx*ry*rz;
        d5_counter[5] -= g5*rx*rx*rx*rz*rz + g4*(3*rx*rz*rz + rx*rx*rx) + 3*g3*rx;
        d5_counter[6] -= g5*rx*rx*ry*ry*ry + g4*(3*rx*rx*ry + ry*ry*ry) + 3*g3*ry;
        d5_counter[7] -= g5*rx*rx*ry*ry*rz + g4*(ry*ry*rz + rx*rx*rz) + g3*rz;
        d5_counter[8] -= g5*rx*rx*ry*rz*rz + g4*(ry*rz*rz + rx*rx*ry) + g3*ry;
        d5_counter[9] -= g5*rx*rx*rz*rz*rz + g4*(3*rx*rx*rz + rz*rz*rz) + 3*g3*rz;
        d5_counter[10] -= g5*rx*ry*ry*ry*ry + 6*g4*rx*ry*ry + 3*g3*rx;
        d5_counter[11] -= g5*rx*ry*ry*ry*rz + 3*g4*rx*ry*rz;
        d5_counter[12] -= g5*rx*ry*ry*rz*rz + g4*(rx*ry*ry + rx*rz*rz) + g3*rx;
        d5_counter[13] -= g5*rx*ry*rz*rz*rz + 3*g4*rx*ry*rz;
        d5_counter[14] -= g5*rx*rz*rz*rz*rz + 6*g4*rx*rz*rz + 3*g3*rx;
        d5_counter[15] -= g5*ry*ry*ry*ry*ry + 10*g4*ry*ry*ry + 15*g3*ry;
        d5_counter[16] -= g5*ry*ry*ry*ry*rz + 6*g4*ry*ry*rz + 3*g3*rz;
        d5_counter[17] -= g5*ry*ry*ry*rz*rz + g4*(3*ry*rz*rz + ry*ry*ry) + 3*g3*ry;
        d5_counter[18] -= g5*ry*ry*rz*rz*rz + g4*(3*ry*ry*rz + rz*rz*rz) + 3*g3*rz;
        d5_counter[19] -= g5*ry*rz*rz*rz*rz + 6*g4*ry*rz*rz + 3*g3*ry;
        d5_counter[20] -= g5*rz*rz*rz*rz*rz + 10*g4*rz*rz*rz + 15*g3*rz;
      }
    }
  }
  
  // sum in Fourier space
  for (int nz = -5; nz <= 5; nz++) {
    for (int ny = -5; ny <= 5; ny++) {
      for (int nx = -5; nx <= 5; nx++) {

        if (nx != 0 || ny != 0 || nz != 0) {

          double kx = 2.0 * M_PI * nx / Lx;
          double ky = 2.0 * M_PI * ny / Ly;
          double kz = 2.0 * M_PI * nz / Lz;
          double k2 = kx*kx + ky*ky + kz*kz;
          double kdotx = kx*x + ky*y + kz*z;

          double k_exp = 4.0*M_PI/box_vol * (exp(-1.0*k2 / (4.0*alpha2)) / k2) * sin(kdotx); 

          // for (int m = 0; m < 3; m++) {
          //   for (int l = 0; l < 3; l++) {
          //     for (int k = 0; k < 3; k++) {
          //       for (int j = 0; j < 3; j++) {
          //         for (int i = 0; i < 3; i++) {

          //           int index = j + 3 * (i + 3 * (k + 3 * l));

          //           d5_counter[index] += k_exp * kvec[i] * kvec[j] * kvec[k] * kvec[l] * kvec[m];
          //         }
          //       }
          //     }
          //   }
          // }

          d5_counter[0] += k_exp * kx*kx*kx*kx*kx;
          d5_counter[1] += k_exp * kx*kx*kx*kx*ky;
          d5_counter[2] += k_exp * kx*kx*kx*kx*kz;
          d5_counter[3] += k_exp * kx*kx*kx*ky*ky;
          d5_counter[4] += k_exp * kx*kx*kx*ky*kz;
          d5_counter[5] += k_exp * kx*kx*kx*kz*kz;
          d5_counter[6] += k_exp * kx*kx*ky*ky*ky;
          d5_counter[7] += k_exp * kx*kx*ky*ky*kz;
          d5_counter[8] += k_exp * kx*kx*ky*kz*kz;
          d5_counter[9] += k_exp * kx*kx*kz*kz*kz;
          d5_counter[10] += k_exp * kx*ky*ky*ky*ky;
          d5_counter[11] += k_exp * kx*ky*ky*ky*kz;
          d5_counter[12] += k_exp * kx*ky*ky*kz*kz;
          d5_counter[13] += k_exp * kx*ky*kz*kz*kz;
          d5_counter[14] += k_exp * kx*kz*kz*kz*kz;
          d5_counter[15] += k_exp * ky*ky*ky*ky*ky;
          d5_counter[16] += k_exp * ky*ky*ky*ky*kz;
          d5_counter[17] += k_exp * ky*ky*ky*kz*kz;
          d5_counter[18] += k_exp * ky*ky*kz*kz*kz;
          d5_counter[19] += k_exp * ky*kz*kz*kz*kz;
          d5_counter[20] += k_exp * kz*kz*kz*kz*kz;

        } 
      }
    }
  }

  return d5_counter;

  
}

// compute the d6 term of the Ewald sum at coordinates (x, y, z)
std::vector<double> EnzoMethodEwald::d6(double x, double y, double z) throw()
{
  /* (d6)_ijklmn = g3 (delta_mn delta_ij delta_kl + delta_mn delta_jk delta_il + delta_mn delta_ik delta_jl
                        + delta_ln delta_ij delta_km + delta_ln delta_jk delta_im + delta_ln delta_ik delta_jm
                        + delta_kn delta_ij delta_lm + delta_kn delta_jl delta_im + delta_kn delta_il delta_jm
                        + delta_jn delta_ik delta_lm + delta_jn delta_lk delta_im + delta_jn delta_il delta_km
                        + delta_in delta_jk delta_lm + delta_in delta_lk delta_jm + delta_in delta_jl delta_km)
                +  g4 [r_i r_j (delta_kl delta_mn + delta_lm delta_kn + delta_km delta_ln)
                     + r_i r_k (delta_jl delta_mn + delta_lm delta_jn + delta_jm delta_ln)
                     + r_i r_l (delta_jk delta_mn + delta_km delta_jn + delta_jm delta_kn)
                     + r_i r_m (delta_jk delta_ln + delta_kl delta_jn + delta_jl delta_kn)
                     + r_i r_n (delta_jk delta_lm + delta_kl delta_jm + delta_jl delta_km)
                     + r_j r_k (delta_il delta_mn + delta_lm delta_in + delta_im delta_ln)
                     + r_j r_l (delta_ik delta_mn + delta_km delta_in + delta_im delta_kn)
                     + r_j r_m (delta_ik delta_ln + delta_kl delta_in + delta_il delta_kn)
                     + r_j r_n (delta_ik delta_lm + delta_kl delta_im + delta_il delta_km)
                     + r_k r_l (delta_ij delta_mn + delta_jm delta_in + delta_im delta_jn)
                     + r_k r_m (delta_ij delta_ln + delta_jl delta_in + delta_il delta_jn)
                     + r_k r_n (delta_ij delta_lm + delta_jl delta_im + delta_il delta_jm)
                     + r_l r_m (delta_ij delta_kn + delta_jk delta_in + delta_ik delta_jn)
                     + r_l r_n (delta_ij delta_km + delta_jk delta_im + delta_ik delta_jm)
                     + r_m r_n (delta_ij delta_kl + delta_jk delta_il + delta_ik delta_jl)]
                +  g5 (delta_ij r_k r_l r_m r_n + delta_ik r_j r_l r_m r_n + delta_il r_j r_k r_m r_n
                      + delta_im r_j r_k r_l r_n + delta_in r_j r_k r_l r_m + delta_jk r_i r_l r_m r_n
                      + delta_jl r_i r_k r_m r_n + delta_jm r_i r_k r_l r_n + delta_jn r_i r_k r_l r_m
                      + delta_kl r_i r_j r_m r_n + delta_km r_i r_j r_l r_n + delta_kn r_i r_j r_l r_m
                      + delta_lm r_i r_j r_k r_n + delta_ln r_i r_j r_k r_m + delta_mn r_i r_j r_k r_l)
                +  g6 r_i r_j r_k r_l r_m r_n       
                      */

  std::vector<double> d6_counter (28, 0);

  Hierarchy * hierarchy = enzo::simulation()->hierarchy();
  double lox, loy, loz; 
  double hix, hiy, hiz;
  hierarchy->lower(&lox, &loy, &loz);
  hierarchy->upper(&hix, &hiy, &hiz);

  double Lx = hix - lox;
  double Ly = hiy - loy;
  double Lz = hiz - loz;
  double box_vol = Lx * Ly * Lz;

  // double alpha = 1.0 / (2.0 * pow(box_vol, 1./3));
  double alpha = 2.0 / pow(box_vol, 1./3.);
  double alpha2 = alpha * alpha;
  double alpha3 = alpha2 * alpha;
  double alpha5 = alpha2 * alpha3;
  double alpha7 = alpha2 * alpha5;
  double alpha9 = alpha2 * alpha7;
  double alpha11 = alpha2 * alpha9;

  // sum in real space
  for (int nz = -5; nz <= 5; nz++) {
    for (int ny = -5; ny <= 5; ny++) {
      for (int nx = -5; nx <= 5; nx++) {

        double rx = x + nx * Lx;
        double ry = y + ny * Ly;
        double rz = z + nz * Lz;
        double r = sqrt(rx*rx + ry*ry + rz*rz);
        double r2 = r*r;
        double r3 = r2*r;
        double r5 = r2*r3;
        double r7 = r2*r5;
        double r9 = r2*r7;
        double r11 = r2*r9;
        double r13 = r2*r11;

        double g3, g4, g5, g6;

        if (nx != 0 || ny != 0 || nz !=0) {
          g3 = (-8.0 * sqrt(M_PI) * alpha5 * r5 - 20.0 * sqrt(M_PI) * alpha3 * r3 - 30.0 * sqrt(M_PI) * alpha * r
                - 15.0 * M_PI * exp(alpha2 * r2) * erfc(alpha*r)) * exp(-1.0 * alpha2 * r2) / (M_PI * r7);

          g4 = (16.0 * sqrt(M_PI) * alpha7 * r7  + 56.0 * sqrt(M_PI) * alpha5 * r5 + 140.0 * sqrt(M_PI) * alpha3 * r3
                + 210.0 * sqrt(M_PI) * alpha * r + 105.0 * M_PI * exp(alpha2 * r2) * erfc(alpha * r)) 
                * exp(-1.0 * alpha2 * r2) / (M_PI * r9);

          g5 = (-32.0 * sqrt(M_PI) * alpha9 * r9 - 144.0 * sqrt(M_PI) * alpha7 * r7  - 504.0 * sqrt(M_PI) * alpha5 * r5 
                - 1260.0 * sqrt(M_PI) * alpha3 * r3 - 1890.0 * sqrt(M_PI) * alpha * r - 945.0 * M_PI * exp(alpha2 * r2) 
                * erfc(alpha * r)) * exp(-1.0 * alpha2 * r2) / (M_PI * r11);

          g6 = (64.0 * sqrt(M_PI) * alpha11 * r11 + 352.0 * sqrt(M_PI) * alpha9 * r9 + 1584.0 * sqrt(M_PI) * alpha7 * r7  
                + 5544.0 * sqrt(M_PI) * alpha5 * r5 + 13860.0 * sqrt(M_PI) * alpha3 * r3 + 20790.0 * sqrt(M_PI) * alpha * r 
                + 10395.0 * M_PI * exp(alpha2 * r2) * erfc(alpha * r)) * exp(-1.0 * alpha2 * r2) / (M_PI * r13);
        }
        else {
          
          // Taylor expand if r is close to 0
          if ((alpha * r) < 0.5) {
            // check signs
            g3 = -16.0 * pow(alpha, 7) / sqrt(M_PI) *
                       (-1.0 / 7.0 + pow(alpha * r, 2) / 9.0 - pow(alpha * r, 4) / 22.0 + pow(alpha * r, 6) / 78.0 -
                        pow(alpha * r, 8) / 360.0 + pow(alpha * r, 10) / 2040.0);

            g4 = -32.0 * pow(alpha, 9) / sqrt(M_PI) *
                       (1.0 / 9.0 - pow(alpha * r, 2) / 11.0 + pow(alpha * r, 4) / 26.0 - pow(alpha * r, 6) / 90.0 +
                        pow(alpha * r, 8) / 408.0 - pow(alpha * r, 10) / 2280.0);

            g5 = -64.0 * pow(alpha, 11) / sqrt(M_PI) *
                       (-1.0 / 11.0 + pow(alpha * r, 2) / 13.0 - pow(alpha * r, 4) / 30.0 + pow(alpha * r, 6) / 102.0 -
                        pow(alpha * r, 8) / 456.0 + pow(alpha * r, 10) / 2520.0);

            g6 = -128.0 * pow(alpha, 13) / sqrt(M_PI) *
                       (1.0 / 13.0 - pow(alpha * r, 2) / 15.0 + pow(alpha * r, 4) / 34.0 - pow(alpha * r, 6) / 114.0 +
                        pow(alpha * r, 8) / 504.0 - pow(alpha * r, 10) / 2760.0);


            if (abs(x + 0.007937) <= 1e-5 && abs(y + 0.007937) <= 1e-5 && abs(z + 0) < 1e-2) {
              CkPrintf("D6 Taylor: g3, g4, g5, g6, rx, ry, rz: %f, %f, %f, %f, %f, %f, %f\n", g3, g4, g5, g6, rx, ry, rz);
            }

          }

          else { // incorporate Newtonian 1/r term
            g3 = 15.0/r7 + (-8.0 * sqrt(M_PI) * alpha5 * r5 - 20.0 * sqrt(M_PI) * alpha3 * r3 - 30.0 * sqrt(M_PI) * alpha * r
                - 15.0 * M_PI * exp(alpha2 * r2) * erfc(alpha*r)) * exp(-1.0 * alpha2 * r2) / (M_PI * r7);

            g4 = -105.0/r9 + (16.0 * sqrt(M_PI) * alpha7 * r7  + 56.0 * sqrt(M_PI) * alpha5 * r5 + 140.0 * sqrt(M_PI) * alpha3 * r3
                + 210.0 * sqrt(M_PI) * alpha * r + 105.0 * M_PI * exp(alpha2 * r2) * erfc(alpha * r)) 
                * exp(-1.0 * alpha2 * r2) / (M_PI * r9);

            g5 = 945.0/r11 + (-32.0 * sqrt(M_PI) * alpha9 * r9 - 144.0 * sqrt(M_PI) * alpha7 * r7  - 504.0 * sqrt(M_PI) * alpha5 * r5 
                - 1260.0 * sqrt(M_PI) * alpha3 * r3 - 1890.0 * sqrt(M_PI) * alpha * r - 945.0 * M_PI * exp(alpha2 * r2) 
                * erfc(alpha * r)) * exp(-1.0 * alpha2 * r2) / (M_PI * r11);

            g6 = -10395.0/r13 + (64.0 * sqrt(M_PI) * alpha11 * r11 + 352.0 * sqrt(M_PI) * alpha9 * r9 + 1584.0 * sqrt(M_PI) * alpha7 * r7  
                + 5544.0 * sqrt(M_PI) * alpha5 * r5 + 13860.0 * sqrt(M_PI) * alpha3 * r3 + 20790.0 * sqrt(M_PI) * alpha * r 
                + 10395.0 * M_PI * exp(alpha2 * r2) * erfc(alpha * r)) * exp(-1.0 * alpha2 * r2) / (M_PI * r13);
          }
        }

        // for (int n = 0; n < 3; n++) {
        //   for (int m = 0; m < 3; m++) {
        //     for (int l = 0; l < 3; l++) {
        //       for (int k = 0; k < 3; k++) {
        //         for (int j = 0; j < 3; j++) {
        //           for (int i = 0; i < 3; i++) {

        //             int index = j + 3 * (i + 3*(k + 3*(l + 3*(m + 3*n))));

        //             d6_counter[index] -= g6 * rvec[i] * rvec[j] * rvec[k] * rvec[l] * rvec[m] * rvec[n];

        //             if (i == j) {
        //               d6_counter[index] -= g5 * rvec[k] * rvec[l] * rvec[m] * rvec[n];

        //               if (m == n) {
        //                 d6_counter[index] -= g4 * rvec[k] * rvec[l];
        //               }

        //               if (l == n) {
        //                 d6_counter[index] -= g4 * rvec[k] * rvec[m];
        //               }

        //               if (l == m) {
        //                 d6_counter[index] -= g4 * rvec[k] * rvec[n];
        //               }

        //               if (k == n) {
        //                 d6_counter[index] -= g4 * rvec[l] * rvec[m];

        //                 if (l == m) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }

        //               if (k == m) {
        //                 d6_counter[index] -= g4 * rvec[l] * rvec[n];

        //                 if (l == n) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }

        //               if (k == l) {
        //                 d6_counter[index] -= g4 * rvec[m] * rvec[n];

        //                 if (m == n) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }
        //             }

        //             if (j == k) {
        //               d6_counter[index] -= g5 * rvec[i] * rvec[l] * rvec[m] * rvec[n];

        //               if (m == n) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[l];
        //               }

        //               if (l == n) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[m];
        //               }

        //               if (l == m) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[n];
        //               }

        //               if (i == n) {
        //                 d6_counter[index] -= g4 * rvec[l] * rvec[m];

        //                 if (l == m) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }

        //               if (i == m) {
        //                 d6_counter[index] -= g4 * rvec[l] * rvec[n];

        //                 if (l == n) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }

        //               if (i == l) {
        //                 d6_counter[index] -= g4 * rvec[m] * rvec[n];

        //                 if (m == n) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }
                      
        //             }

        //             if (i == k) {
        //               d6_counter[index] -= g5 * rvec[j] * rvec[l] * rvec[m] * rvec[n];

        //               if (m == n) {
        //                 d6_counter[index] -= g4 * rvec[j] * rvec[l];
        //               }

        //               if (l == n) {
        //                 d6_counter[index] -= g4 * rvec[j] * rvec[m];
        //               }

        //               if (l == m) {
        //                 d6_counter[index] -= g4 * rvec[j] * rvec[n];
        //               }

        //               if (j == n) {
        //                 d6_counter[index] -= g4 * rvec[l] * rvec[m];

        //                 if (l == m) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }

        //               if (j == m) {
        //                 d6_counter[index] -= g4 * rvec[l] * rvec[n];

        //                 if (l == n) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }

        //               if (j == l) {
        //                 d6_counter[index] -= g4 * rvec[m] * rvec[n];

        //                 if (m == n) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }
        //             }

        //             if (i == l) {
        //               d6_counter[index] -= g5 * rvec[j] * rvec[k] * rvec[m] * rvec[n];

        //               if (m == n) {
        //                 d6_counter[index] -= g4 * rvec[j] * rvec[k];
        //               }

        //               if (k == n) {
        //                 d6_counter[index] -= g4 * rvec[j] * rvec[m];
        //               }

        //               if (k == m) {
        //                 d6_counter[index] -= g4 * rvec[j] * rvec[n];
        //               }

        //               if (j == n) {
        //                 d6_counter[index] -= g4 * rvec[k] * rvec[m];

        //                 if (k == m) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }

        //               if (j == m) {
        //                 d6_counter[index] -= g4 * rvec[k] * rvec[n];

        //                 if (k == n) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }
        //             }

        //             if (j == l) {
        //               d6_counter[index] -= g5 * rvec[i] * rvec[k] * rvec[m] * rvec[n];

        //               if (m == n) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[k];
        //               }

        //               if (k == n) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[m];
        //               }

        //               if (k == m) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[n];
        //               }

        //               if (i == n) {
        //                 d6_counter[index] -= g4 * rvec[k] * rvec[m];

        //                 if (k == m) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }

        //               if (i == m) {
        //                 d6_counter[index] -= g4 * rvec[k] * rvec[n];

        //                 if (k == n) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }
        //             }

        //             if (k == l) {
        //               d6_counter[index] -= g5 * rvec[i] * rvec[j] * rvec[m] * rvec[n];

        //               if (m == n) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[j];
        //               }

        //               if (j == n) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[m];

        //                 if (i == m) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }

        //               if (j == m) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[n];
        //               }

        //               if (i == n) {
        //                 d6_counter[index] -= g4 * rvec[j] * rvec[m];

        //                 if (j == m) {
        //                   d6_counter[index] -= g3;
        //                 }
        //               }

        //               if (i == m) {
        //                 d6_counter[index] -= g4 * rvec[j] * rvec[n];
        //               }
        //             }

        //             if (i == m) {
        //               d6_counter[index] -= g5 * rvec[j] * rvec[k] * rvec[l] * rvec[n];

        //               if (l == n) {
        //                 d6_counter[index] -= g4 * rvec[j] * rvec[k];
        //               }

        //               if (k == n) {
        //                 d6_counter[index] -= g4 * rvec[j] * rvec[l];
        //               }

        //               if (j == n) {
        //                 d6_counter[index] -= g4 * rvec[k] * rvec[l];
        //               }
        //             }

        //             if (j == m) {
        //               d6_counter[index] -= g5 * rvec[i] * rvec[k] * rvec[l] * rvec[n];

        //               if (l == n) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[k];
        //               }

        //               if (k == n) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[l];
        //               }

        //               if (i == n) {
        //                 d6_counter[index] -= g4 * rvec[k] * rvec[l];
        //               }
        //             }

        //             if (k == m) {
        //               d6_counter[index] -= g5 * rvec[i] * rvec[j] * rvec[l] * rvec[n];

        //               if (l == n) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[j];
        //               }

        //               if (j == n) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[l];
        //               }

        //               if (i == n) {
        //                 d6_counter[index] -= g4 * rvec[j] * rvec[l];
        //               }
        //             }

        //             if (l == m) {
        //               d6_counter[index] -= g5 * rvec[i] * rvec[j] * rvec[k] * rvec[n];

        //               if (k == n) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[j];
        //               }

        //               if (j == n) {
        //                 d6_counter[index] -= g4 * rvec[i] * rvec[k];
        //               }

        //               if (i == n) {
        //                 d6_counter[index] -= g4 * rvec[j] * rvec[k];
        //               }
        //             }

        //             if (i == n) {
        //               d6_counter[index] -= g5 * rvec[j] * rvec[k] * rvec[l] * rvec[m];
        //             }

        //             if (j == n) {
        //               d6_counter[index] -= g5 * rvec[i] * rvec[k] * rvec[l] * rvec[m];
        //             }

        //             if (k == n) {
        //               d6_counter[index] -= g5 * rvec[i] * rvec[j] * rvec[l] * rvec[m];
        //             }

        //             if (l == n) {
        //               d6_counter[index] -= g5 * rvec[i] * rvec[j] * rvec[k] * rvec[m];
        //             }

        //             if (m == n) {
        //               d6_counter[index] -= g5 * rvec[i] * rvec[j] * rvec[k] * rvec[l];
        //             }
        //           }
        //         }
        //       }
        //     }
        //   }
        // }

        d6_counter[0] -= g6*rx*rx*rx*rx*rx*rx + 15*g5*rx*rx*rx*rx + 45*g4*rx*rx + 15*g3;
        d6_counter[1] -= g6*rx*rx*rx*rx*rx*ry + 10*g5*rx*rx*rx*ry + 15*g4*rx*ry; 
        d6_counter[2] -= g6*rx*rx*rx*rx*rx*rz + 10*g5*rx*rx*rx*rz + 15*g4*rx*rz;
        d6_counter[3] -= g6*rx*rx*rx*rx*ry*ry + g5*(6*rx*rx*ry*ry + rx*rx*rx*rx) + g4*(6*rx*rx + 3*ry*ry) + 3*g3; 
        d6_counter[4] -= g6*rx*rx*rx*rx*ry*rz + 6*g5*rx*rx*ry*rz + 3*g4*ry*rz;
        d6_counter[5] -= g6*rx*rx*rx*rx*rz*rz + g5*(6*rx*rx*rz*rz + rx*rx*rx*rx) + g4*(6*rx*rx + 3*rz*rz) + 3*g3;
        d6_counter[6] -= g6*rx*rx*rx*ry*ry*ry + g5*(3*rx*ry*ry*ry + 3*rx*rx*rx*ry) + 9*g4*rx*ry;
        d6_counter[7] -= g6*rx*rx*rx*ry*ry*rz + g5*(3*rx*ry*ry*rz + rx*rx*rx*rz) + 3*g4*rx*rz;
        d6_counter[8] -= g6*rx*rx*rx*ry*rz*rz + g5*(3*rx*ry*rz*rz + rx*rx*rx*ry) + 3*g4*rx*ry;
        d6_counter[9] -= g6*rx*rx*rx*rz*rz*rz + g5*(3*rx*rz*rz*rz + 3*rx*rx*rx*rz) + 9*g4*rx*rz;
        d6_counter[10] -= g6*rx*rx*ry*ry*ry*ry + g5*(6*rx*rx*ry*ry + ry*ry*ry*ry) + g4*(6*ry*ry + 3*rx*rx) + 3*g3;
        d6_counter[11] -= g6*rx*rx*ry*ry*ry*rz + g5*(3*rx*rx*ry*rz + ry*ry*ry*rz) + 3*g4*ry*rz;
        d6_counter[12] -= g6*rx*rx*ry*ry*rz*rz + g5*(ry*ry*rz*rz + rx*rx*rz*rz + rx*rx*ry*ry) + g4*(rx*rx + ry*ry + rz*rz) + g3;
        d6_counter[13] -= g6*rx*rx*ry*rz*rz*rz + g5*(3*rx*rx*ry*rz + ry*rz*rz*rz) + 3*g4*ry*rz;
        d6_counter[14] -= g6*rx*rx*rz*rz*rz*rz + g5*(6*rx*rx*rz*rz + rz*rz*rz*rz) + g4*(6*rz*rz + 3*rx*rx) + 3*g3;
        d6_counter[15] -= g6*rx*ry*ry*ry*ry*ry + 10*g5*rx*ry*ry*ry + 15*g4*rx*ry;
        d6_counter[16] -= g6*rx*ry*ry*ry*ry*rz + 6*g5*rx*ry*ry*rz + 3*g4*rx*rz;
        d6_counter[17] -= g6*rx*ry*ry*ry*rz*rz + g5*(3*rx*ry*rz*rz + rx*ry*ry*ry) + 3*g4*rx*ry;
        d6_counter[18] -= g6*rx*ry*ry*rz*rz*rz + g5*(3*rx*ry*ry*rz + rx*rz*rz*rz) + 3*g4*rx*rz;
        d6_counter[19] -= g6*rx*ry*rz*rz*rz*rz + 6*g5*rx*ry*rz*rz + 3*g4*rx*ry;
        d6_counter[20] -= g6*rx*rz*rz*rz*rz*rz + 10*g5*rx*rz*rz*rz + 15*g4*rx*rz;
        d6_counter[21] -= g6*ry*ry*ry*ry*ry*ry + 15*g5*ry*ry*ry*ry + 45*g4*ry*ry + 15*g3;
        d6_counter[22] -= g6*ry*ry*ry*ry*ry*rz + 10*g5*ry*ry*ry*rz + 15*g4*ry*rz;
        d6_counter[23] -= g6*ry*ry*ry*ry*rz*rz + g5*(6*ry*ry*rz*rz + ry*ry*ry*ry) + g4*(6*ry*ry + 3*rz*rz) + 3*g3;
        d6_counter[24] -= g6*ry*ry*ry*rz*rz*rz + g5*(3*ry*rz*rz*rz + 3*ry*ry*ry*rz) + 9*g4*ry*rz;
        d6_counter[25] -= g6*ry*ry*rz*rz*rz*rz + g5*(6*ry*ry*rz*rz + rz*rz*rz*rz) + g4*(6*rz*rz + 3*ry*ry) + 3*g3;
        d6_counter[26] -= g6*ry*rz*rz*rz*rz*rz + 10*g5*ry*rz*rz*rz + 15*g4*ry*rz;
        d6_counter[27] -= g6*rz*rz*rz*rz*rz*rz + 15*g5*rz*rz*rz*rz + 45*g4*rz*rz + 15*g3;
        
      }
    }
  }
  
  // sum in Fourier space
  for (int nz = -5; nz <= 5; nz++) {
    for (int ny = -5; ny <= 5; ny++) {
      for (int nx = -5; nx <= 5; nx++) {

        if (nx != 0 || ny != 0 || nz != 0) {

          double kx = 2.0 * M_PI * nx / Lx;
          double ky = 2.0 * M_PI * ny / Ly;
          double kz = 2.0 * M_PI * nz / Lz;
          double k2 = kx*kx + ky*ky + kz*kz;
          double kdotx = kx*x + ky*y + kz*z;

          double k_exp = 4.0*M_PI/box_vol * (exp(-1.0*k2 / (4.0*alpha2)) / k2) * cos(kdotx); 

          // for (int n = 0; n < 3; n++) {
          //   for (int m = 0; m < 3; m++) {
          //     for (int l = 0; l < 3; l++) {
          //       for (int k = 0; k < 3; k++) {
          //         for (int j = 0; j < 3; j++) {
          //           for (int i = 0; i < 3; i++) {

          //             int index = j + 3 * (i + 3 * (k + 3 * l));

          //             d6_counter[index] += k_exp * kvec[i] * kvec[j] * kvec[k] * kvec[l] * kvec[m] * kvec[n];
          //           }
          //         }
          //       }
          //     }
          //   }
          // }

          d6_counter[0] += k_exp * kx*kx*kx*kx*kx*kx;
          d6_counter[1] += k_exp * kx*kx*kx*kx*kx*ky;
          d6_counter[2] += k_exp * kx*kx*kx*kx*kx*kz;
          d6_counter[3] += k_exp * kx*kx*kx*kx*ky*ky;
          d6_counter[4] += k_exp * kx*kx*kx*kx*ky*kz;
          d6_counter[5] += k_exp * kx*kx*kx*kx*kz*kz;
          d6_counter[6] += k_exp * kx*kx*kx*ky*ky*ky;
          d6_counter[7] += k_exp * kx*kx*kx*ky*ky*kz;
          d6_counter[8] += k_exp * kx*kx*kx*ky*kz*kz;
          d6_counter[9] += k_exp * kx*kx*kx*kz*kz*kz;
          d6_counter[10] += k_exp * kx*kx*ky*ky*ky*ky;
          d6_counter[11] += k_exp * kx*kx*ky*ky*ky*kz;
          d6_counter[12] += k_exp * kx*kx*ky*ky*kz*kz;
          d6_counter[13] += k_exp * kx*kx*ky*kz*kz*kz;
          d6_counter[14] += k_exp * kx*kx*kz*kz*kz*kz;
          d6_counter[15] += k_exp * kx*ky*ky*ky*ky*ky;
          d6_counter[16] += k_exp * kx*ky*ky*ky*ky*kz;
          d6_counter[17] += k_exp * kx*ky*ky*ky*kz*kz;
          d6_counter[18] += k_exp * kx*ky*ky*kz*kz*kz;
          d6_counter[19] += k_exp * kx*ky*kz*kz*kz*kz;
          d6_counter[20] += k_exp * kx*kz*kz*kz*kz*kz;
          d6_counter[21] += k_exp * ky*ky*ky*ky*ky*ky;
          d6_counter[22] += k_exp * ky*ky*ky*ky*ky*kz;
          d6_counter[23] += k_exp * ky*ky*ky*ky*kz*kz;
          d6_counter[24] += k_exp * ky*ky*ky*kz*kz*kz;
          d6_counter[25] += k_exp * ky*ky*kz*kz*kz*kz;
          d6_counter[26] += k_exp * ky*kz*kz*kz*kz*kz;
          d6_counter[27] += k_exp * kz*kz*kz*kz*kz*kz;
        } 
      }
    }
  }

  return d6_counter;
  
}
