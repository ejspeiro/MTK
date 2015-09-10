/*!
\file mtk_lap_1d.cc

\brief Includes the implementation of the class Lap1D.

This class implements a 1D Laplacian operator, constructed using the
Castillo-Blomgren-Sanchez (CBS) Algorithm (CBSA).

\author: Eduardo J. Sanchez (ejspeiro) - esanchez at mail dot sdsu dot edu
*/
/*
Copyright (C) 2015, Computational Science Research Center, San Diego State
University. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Modifications to source code should be reported to: esanchez@mail.sdsu.edu
and a copy of the modified files should be reported once modifications are
completed. Documentation related to said modifications should be included.

2. Redistributions of source code must be done through direct
downloads from the project's GitHub page: http://www.csrc.sdsu.edu/mtk

3. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

4. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

5. Usage of the binary form on proprietary applications shall require explicit
prior written permission from the the copyright holders.

6. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

The copyright holders provide no reassurances that the source code provided does
not infringe any patent, copyright, or any other intellectual property rights of
third parties. The copyright holders disclaim any liability to any recipient for
claims brought against recipient by any third party for infringement of that
parties intellectual property rights.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <iostream>
#include <iomanip>

#include "mtk_roots.h"
#include "mtk_tools.h"
#include "mtk_blas_adapter.h"
#include "mtk_grad_1d.h"
#include "mtk_div_1d.h"
#include "mtk_lap_1d.h"

namespace mtk {

std::ostream& operator <<(std::ostream &stream, mtk::Lap1D &in) {

  /// 1. Print order of accuracy.

  stream << "laplacian_[0] = " << in.laplacian_[0] << std::endl << std::endl;

  /// 2. Print approximating coefficients for the interior.

  stream << "laplacian_[1:" << 2*in.order_accuracy_ - 1 << "] = " <<
    std::endl << std::endl;
  for (auto ii = 1; ii <= (2*in.order_accuracy_ - 1); ++ii) {
    stream << std::setw(13) << in.laplacian_[ii] << " ";
  }
  stream << std::endl << std::endl;

  /// 3. No weights, thus print the mimetic boundary coefficients.

  auto offset = 1 + (2*in.order_accuracy_ - 1);

  stream << "laplacian_[" << offset << ":" << offset +
    (in.order_accuracy_ - 1)*(2*in.order_accuracy_) - 1 << "] = " <<
    std::endl << std::endl;

  for (auto ii = 0; ii < in.order_accuracy_ - 1; ++ii) {
    for (auto jj = 0; jj < 2*in.order_accuracy_; ++jj) {
      stream << std::setw(13) <<
        in.laplacian_[offset + ii*(2*in.order_accuracy_) + jj];
    }
    stream << std::endl;
  }

  return stream;
}
}

mtk::Lap1D::Lap1D():
  order_accuracy_(mtk::kDefaultOrderAccuracy),
  laplacian_length_(),
  mimetic_threshold_(mtk::kDefaultMimeticThreshold) {}

mtk::Lap1D::~Lap1D() {

  delete [] laplacian_;
  laplacian_ = nullptr;
}

bool mtk::Lap1D::ConstructLap1D(int order_accuracy,
                                mtk::Real mimetic_threshold) {
  
  #if MTK_DEBUG_LEVEL > 0
  mtk::Tools::Prevent(order_accuracy < 2, __FILE__, __LINE__, __func__);
  mtk::Tools::Prevent((order_accuracy%2) != 0, __FILE__, __LINE__, __func__);
  mtk::Tools::Prevent(mimetic_threshold <= mtk::kZero,
                      __FILE__, __LINE__, __func__);

  if (order_accuracy >= mtk::kCriticalOrderAccuracyDiv) {
    std::cout << "WARNING: Numerical accuracy is high." << std::endl;
  }

  std::cout << "order_accuracy_ = " << order_accuracy << std::endl;
  std::cout << "mimetic_threshold_ = " << mimetic_threshold << std::endl;
  #endif

  order_accuracy_ = order_accuracy;
  mimetic_threshold_ = mimetic_threshold;

  /// 1. Create gradient operator using specific values for the Laplacian.

  mtk::Grad1D grad; // Mimetic gradient.

  bool info = grad.ConstructGrad1D(order_accuracy_, mimetic_threshold_);

  if (!info) {
    std::cerr << "Mimetic grad could not be built." << std::endl;
    return false;
  }

  /// 2. Create gradient operator using specific values for the Laplacian.

  mtk::Div1D div; // Mimetic divergence.

  info = div.ConstructDiv1D(order_accuracy_, mimetic_threshold_);

  if (!info) {
    std::cerr << "Mimetic div could not be built." << std::endl;
    return false;
  }

  /// 3. Create both operators as matrices.

  // Since these are mimetic operator, we must multiply the matrices arising
  // from both the divergence and the Laplacian, in order to get the
  // approximating coefficients for the Laplacian operator.

  // However, we must choose a grid that implied a step size of 1, so to get
  // the approximating coefficients, without being affected from the
  // normalization with respect to the grid.

  // Also, the grid must be of the minimum size to support the requested order
  // of accuracy. We must please the divergence.

  mtk::UniStgGrid1D aux(mtk::kZero,
                        (mtk::Real) 3*order_accuracy_ - 1,
                        3*order_accuracy_ - 1);

  #if MTK_DEBUG_LEVEL > 0
  std::cout << "aux =" << std::endl;
  std::cout << aux << std::endl;
  std::cout <<"aux.delta_x() = " << aux.delta_x() << std::endl;
  std::cout << std::endl;
  #endif

  mtk::DenseMatrix grad_m(grad.ReturnAsDenseMatrix(aux));

  #if MTK_DEBUG_LEVEL > 0
  std::cout << "grad_m =" << std::endl;
  std::cout << grad_m << std::endl;
  #endif

  mtk::DenseMatrix div_m(div.ReturnAsDenseMatrix(aux));

  #if MTK_DEBUG_LEVEL > 0
  std::cout << "div_m =" << std::endl;
  std::cout << div_m << std::endl;
  #endif

  /// 4. Multiply both operators:
  /// \f$ \breve{\mathbf{L}}^k_x =
  /// \breve{\mathbf{D}}^k_x\breve{\mathbf{G}}^k_x \f$

  mtk::DenseMatrix lap; // Laplacian matrix to hold to computed coefficients.

  lap = mtk::BLASAdapter::RealDenseMM(div_m, grad_m);

  #if MTK_DEBUG_LEVEL > 0
  std::cout << "lap =" << std::endl;
  std::cout << lap << std::endl;
  #endif

  /// 5. Extract the coefficients from the matrix and store them in the array.

  /// \warning We do not compute weights for this operator.

  // The output array will have this form:
  // 1. The first entry of the array will contain the used order kk.
  // 2. The second entry of the array will contain the collection of
  // approximating coefficients for the interior of the grid.
  // 3. The next entries will contain the collections of approximating
  // coefficients for the west boundary of the grid.

  laplacian_length_ = 1 + (2*order_accuracy_ - 1) +
    (order_accuracy_ - 1)*(2*order_accuracy_);

  #if MTK_DEBUG_LEVEL > 0
  std::cout << "laplacian_length_ = " << laplacian_length_ << std::endl;
  std::cout << std::endl;
  #endif

  try {
    laplacian_ = new mtk::Real[laplacian_length_];
  } catch (std::bad_alloc &memory_allocation_exception) {
    std::cerr << "Memory allocation exception on line " << __LINE__ - 3 <<
      std::endl;
    std::cerr << memory_allocation_exception.what() << std::endl;
  }
  memset(laplacian_, mtk::kZero, sizeof(laplacian_[0])*laplacian_length_);

  /// 1. The first entry of the array will contain the order of accuracy.

  laplacian_[0] = order_accuracy_;

  /// 2. The second entry of the array will contain the collection of
  /// approximating coefficients for the interior of the grid.

  for (auto ii = 0; ii < 2*order_accuracy_ - 1; ++ii) {
    laplacian_[ii + 1] = lap.GetValue(1 + (order_accuracy_ - 1), ii + 1);
  }

  /// 3. We DO NOT have weights in this operator. Copy mimetic bndy coeffs.

  auto offset = 1 + (2*order_accuracy_ - 1);

  for (auto ii = 0; ii < order_accuracy_ - 1; ++ii) {
    for (auto jj = 0; jj < 2*order_accuracy_; ++jj) {
      laplacian_[offset + ii*(2*order_accuracy_) + jj] =
        lap.GetValue(1 + ii,jj);
    }
  }

  return true;
}

mtk::DenseMatrix mtk::Lap1D::ReturnAsDenseMatrix(const UniStgGrid1D &grid) {
  
  int nn{grid.num_cells_x()};  // Number of cells on the grid.

  #if MTK_DEBUG_LEVEL > 0
  mtk::Tools::Prevent(nn <= 0, __FILE__, __LINE__, __func__);
  mtk::Tools::Prevent(nn < 3*order_accuracy_ - 1, __FILE__, __LINE__, __func__);
  #endif

  mtk::DenseMatrix lap(nn + 2, nn + 2); // Laplacian matrix to be returned.

  mtk::Real idx{mtk::kOne/(grid.delta_x()*grid.delta_x())}; // Inverse of dx^2.

  /// 1. Extract mimetic coefficients from the west boundary.

  auto offset = (1 + 2*order_accuracy_ - 1);

  for (auto ii = 0; ii < order_accuracy_ - 1; ++ii) {
    for (auto jj = 0; jj < 2*order_accuracy_; ++jj) {
      lap.SetValue(1 + ii,
                   jj,
                   idx*laplacian_[offset + ii*2*order_accuracy_ + jj]);
    }
  }

  /// 2. Extract interior coefficients.

  offset = 1 + (order_accuracy_ - 1);

  int kk{1};
  for (auto ii = order_accuracy_; ii <= nn - (order_accuracy_ - 1); ++ii) {
    int mm{1};
    for (auto jj = 0; jj < 2*order_accuracy_ - 1; ++jj) {
      lap.SetValue(ii, jj + kk, idx*laplacian_[mm]);
      mm = mm + 1;
    }
    kk = kk + 1;
  }

  /// 3. Extract mimetic coefficients from the west boundary to go east.

  offset = (1 + 2*order_accuracy_ - 1);

  auto aux = order_accuracy_ + (nn - 2*(order_accuracy_ - 1));

  auto ll = 1;
  auto rr = 1;
  for (auto ii = nn; ii > aux - 1; --ii) {
    auto cc = 0;
    for (auto jj = nn + 2 - 1; jj >= (nn + 2) - 2*order_accuracy_; --jj) {
      lap.SetValue(ii, jj, lap.GetValue(rr,cc));
      ++ll;
      ++cc;
    }
    rr++;
  }

  /// \note We could create two matrices of the requested size and multiply
  /// them, but that would be inefficient, since we already have the computed
  /// coefficients stored. We just have to set them in place, in a matrix of an
  /// adequate size, and multiply them times the inverse of the square of the
  /// step size, in order for the matrix to actually represent a differential
  /// operator.

  return lap;
}

mtk::Real* mtk::Lap1D::Data(const UniStgGrid1D &grid) {

  mtk::DenseMatrix tmp;

  tmp = ReturnAsDenseMatrix(grid);

  return tmp.data();
}