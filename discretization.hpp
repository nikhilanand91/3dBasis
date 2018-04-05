#ifndef DISCRETIZATION_HPP
#define DISCRETIZATION_HPP

#include <vector>
#include <cmath>
#include <unordered_map>

#include <boost/functional/hash.hpp>
#include <gsl/gsl_sf_hyperg.h>
#include <gsl/gsl_errno.h>

#include "constants.hpp"
#include "mono.hpp"
#include "basis.hpp"

DMatrix DiscretizePolys(const DMatrix& polysOnMinBasis, 
        const std::size_t partitions);

// direct matrices ------------------------------------------------------------

DMatrix MuPart(const std::size_t partitions, const coeff_class partWidth, 
        const MATRIX_TYPE type);
DMatrix MuPart_Kinetic(const std::size_t partitions, 
        const coeff_class partWidth);

// same-n interactions --------------------------------------------------------

const DMatrix& MuPart(const std::array<char,3>& r, 
        const std::size_t partitions, const coeff_class partWidth);
coeff_class InteractionWindow(const std::array<char,3>& r,
        const std::array<builtin_class,2>& mu1_ab,
        const std::array<builtin_class,2>& mu2_ab);
coeff_class InteractionWindow_Less(const builtin_class a, const builtin_class b,
        const builtin_class c, const std::array<builtin_class,2>& mu1_ab,
        const std::array<builtin_class,2>& mu2_ab);
coeff_class InteractionWindow_Equal(const builtin_class a, const builtin_class b,
        const builtin_class c, const std::array<builtin_class,2>& mu_ab);
coeff_class InteractionWindow_Greater(const builtin_class a, const builtin_class b,
        const builtin_class c, const std::array<builtin_class,2>& mu1_ab,
        const std::array<builtin_class,2>& mu2_ab);
coeff_class Hypergeometric3F2(const std::array<builtin_class,3>& a, 
        const std::array<builtin_class,2>& b, const builtin_class x);
coeff_class Hypergeometric3F2_Reg(const std::array<builtin_class,3>& a, 
        const std::array<builtin_class,2>& b, const builtin_class x);

// n+2 interactions -----------------------------------------------------------

const DMatrix& MuPart(const char r, const std::size_t partitions,
        const coeff_class partWidth);

#endif
