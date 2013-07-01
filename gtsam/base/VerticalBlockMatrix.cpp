/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation, 
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file    VerticalBlockMatrix.cpp
 * @brief   A matrix with column blocks of pre-defined sizes.  Used in JacobianFactor and
 *        GaussianConditional.
 * @author  Richard Roberts
 * @date    Sep 18, 2010 */

#include <gtsam/base/VerticalBlockMatrix.h>

namespace gtsam {

  /* ************************************************************************* */
  VerticalBlockMatrix VerticalBlockMatrix::LikeActiveViewOf(const VerticalBlockMatrix& rhs) {
    VerticalBlockMatrix result;
    result.variableColOffsets_.resize(rhs.nBlocks() + 1);
    for(Index i = 0; i < rhs.nBlocks(); ++i)
      result.variableColOffsets_[i] = rhs.variableColOffsets_[i + rhs.blockStart_];
    result.matrix_.resize(rhs.rows(), result.variableColOffsets_.back());
    result.rowEnd_ = rhs.rows();
    result.assertInvariants();
    return result;
  }

}
