#include "matrix.hpp"

// Fock space part (ONLY) of the inner product between two monomials
coeff_class InnerFock(const Mono& A, const Mono& B) {
    //B.ChangePm(0, B.Pm(0)+1); // this will be reverted in MatrixTerm below
    return MatrixInternal::MatrixTerm(A, B, MAT_INNER);
    //return MatrixInternal::MatrixTerm(A, B, MatrixInter::MAT_INNER)/A.NParticles();
}

// inner product between two partitions of monomials
coeff_class InnerProduct(const Mono& A, const Mono& B) {
    // DVector muIntegrals = MuIntegral(A, B, partitions, MAT_INNER);
    return MatrixInternal::MatrixTerm(A, B, MAT_INNER);
        // *muIntegrals(part);
}

// creates a gram matrix for the given basis using the Fock space inner product
//
// this returns the rank 2 matrix containing only the Fock part of the product
DMatrix GramFock(const Basis<Mono>& basis) {
    return MatrixInternal::Matrix(basis, 0, MAT_INNER);
}

// creates a gram matrix for the given basis using the Fock space inner product
// 
// this returns the rank 4 tensor relating states with different partitions
DMatrix GramMatrix(const Basis<Mono>& basis, const std::size_t partitions) {
    return MatrixInternal::Matrix(basis, partitions, MAT_INNER);
}

// creates a mass matrix M for the given monomials. To get the mass matrix of a 
// basis of primary operators, one must express the primaries as a matrix of 
// vectors, A, and multiply A^T M A.
DMatrix MassMatrix(const Basis<Mono>& basis, const std::size_t partitions) {
    return MatrixInternal::Matrix(basis, partitions, MAT_MASS);
}

DMatrix KineticMatrix(const Basis<Mono>& basis, const std::size_t partitions) {
    return MatrixInternal::Matrix(basis, partitions, MAT_KINETIC);
}

// creates a matrix of n->n interactions between the given basis's monomials
DMatrix InteractionMatrix(const Basis<Mono>& basis, const std::size_t partitions) {
    return MatrixInternal::Matrix(basis, partitions, MAT_INTER_SAME_N);
}

DMatrix NPlus2Matrix(const Basis<Mono>& basisA, const Basis<Mono>& basisB,
                     const std::size_t partitions) {
    DMatrix output(basisA.size()*partitions, basisB.size()*partitions);
    for (std::size_t i = 0; i < basisA.size(); ++i) {
        for (std::size_t j = 0; j < basisB.size(); ++j) {
            output.block(i*partitions, j*partitions, partitions, partitions)
                = MatrixInternal::MatrixBlock(basisA[i], basisB[j], 
                                              MAT_INTER_N_PLUS_2, partitions);
        }
    }
    return output;
}

namespace MatrixInternal {

// static hash tables for memoizing slow steps
namespace {
    // all matrices: map from {x,y}->{u,yTilde}
    std::unordered_map<std::string, std::vector<MatrixTerm_Intermediate>>
        intermediateCache;
    // direct matrices: map from {x,y}->{u,theta}
    std::unordered_map<std::string, std::vector<MatrixTerm_Final>> directCache;
    // interaction matrices: map from {x,y}->{u,r,theta}
    std::unordered_map<std::string, std::vector<InteractionTerm_Step2>>
        interactionCache;
    // interaction n+2 matrices: map from {x,y}->{u,theta}
    std::unordered_map<std::string, std::vector<MatrixTerm_Final>> nPlus2Cache;

    // integrals: map from (a,b)->#
    std::unordered_map<std::array<builtin_class,2>, builtin_class,
            boost::hash<std::array<builtin_class,2>> > uCache; // FIXME: dep'd
    std::unordered_map<std::array<builtin_class,2>, builtin_class,
            boost::hash<std::array<builtin_class,2>> > uPlusCache;
    std::unordered_map<std::array<builtin_class,2>, builtin_class,
            boost::hash<std::array<builtin_class,2>> > thetaCache;
} // anonymous namespace

YTerm::YTerm(const coeff_class coeff, const std::string& y, 
		const std::string& nAndm): coeff(coeff), y(y.begin(), y.end()-1) {
	for (std::size_t i = 0; i < size(); ++i) {
		this->y[i] += nAndm[i+1];
	}
}

OStream& operator<<(OStream& os, const YTerm& out) {
	return os << out.coeff << " * " << out.y;
}

MatrixTerm_Intermediate::MatrixTerm_Intermediate(const size_t n): coeff(1),
	uPlus(n), uMinus(n), yTilde(n) {
}

void MatrixTerm_Intermediate::Resize(const size_t n) {
    uPlus.resize(n);
    uMinus.resize(n);
    yTilde.resize(n);
}

MatrixTerm_Intermediate operator*(
		const MatrixTerm_Intermediate& A, MatrixTerm_Intermediate B) {
	B.coeff *= A.coeff;
	B.uPlus  = AddVectors(A.uPlus , B.uPlus );
	B.uMinus = AddVectors(A.uMinus, B.uMinus);
	B.yTilde = AddVectors(A.yTilde, B.yTilde);
	return B;
}

OStream& operator<<(OStream& os, const MatrixTerm_Intermediate& out) {
	return os << out.coeff << " * {" << out.uPlus << ", "
		<< out.uMinus << ", " << out.yTilde << "}";
}

// this n would be called (n-1) in Zuhair's equations
MatrixTerm_Final::MatrixTerm_Final(const size_t n): coeff(1), uPlus(n), 
	uMinus(n), sinTheta(n-1), cosTheta(n-1) {
}

// maybe should be rvalue references instead? hopefully it's the same
MatrixTerm_Final::MatrixTerm_Final(const coeff_class coeff, 
		const std::vector<char>& uPlus, const std::vector<char>& uMinus, 
		const std::vector<char>& sinTheta, const std::vector<char>& cosTheta): 
	coeff(coeff), uPlus(uPlus), uMinus(uMinus), sinTheta(sinTheta), 
	cosTheta(cosTheta) {
}

// notice that there are fewer Thetas than Us, so the vectors aren't all the
// same size; also, the n here would be called (n-1) in Zuhair's equations
void MatrixTerm_Final::Resize(const size_t n) {
    uPlus.resize(n);
    uMinus.resize(n);
    sinTheta.resize(n-1);
    cosTheta.resize(n-1);
}

OStream& operator<<(OStream& os, const InteractionTerm_Step2& out) {
    return os << out.coeff << " * {" << out.u << ", "
        << out.theta << ", " << out.r << "}";
}

// generically return direct or interaction matrix of the specified type
DMatrix Matrix(const Basis<Mono>& basis, const std::size_t kMax, 
        const MATRIX_TYPE type) {
    // kMax == 0 means that the Fock part has been requested by itself
    if (kMax == 0) {
        DMatrix fockPart(basis.size(), basis.size());
        for (std::size_t i = 0; i < basis.size(); ++i) {
            fockPart(i, i) = MatrixTerm(basis[i], basis[i], type);
            for (std::size_t j = i+1; j < basis.size(); ++j) {
                fockPart(i, j) = MatrixTerm(basis[i], basis[j], type);
                fockPart(j, i) = fockPart(i, j);
            }
        }

        return fockPart;
    } else {
        DMatrix output(basis.size()*kMax, basis.size()*kMax);
        for (std::size_t i = 0; i < basis.size(); ++i) {
            output.block(i*kMax, i*kMax, kMax, kMax)
                = MatrixBlock(basis[i], basis[i], type, kMax);
            for (std::size_t j = i+1; j < basis.size(); ++j) {
                output.block(i*kMax, j*kMax, kMax, kMax)
                    = MatrixBlock(basis[i], basis[j], type, kMax);
                output.block(j*kMax, i*kMax, kMax, kMax)
                    = output.block(i*kMax, j*kMax, kMax, kMax).transpose();
                // FIXME: make sure above assignment is correct
            }
        }
        return output;
    }
}

coeff_class MatrixTerm(const Mono& A, const Mono& B, const MATRIX_TYPE type) {
    if (type == MAT_INNER || type == MAT_MASS) {
        return MatrixTerm_Direct(A, B, type);
    } else if (type == MAT_KINETIC) {
        return MatrixTerm_Direct(A, B, MAT_INNER);
    } else if (type == MAT_INTER_N_PLUS_2) {
        throw std::logic_error("MatrixTerm: n-n+2 interaction can't be scalar");
    } else if (type == MAT_INTER_SAME_N) {
        throw std::logic_error("MatrixTerm: n-n interaction can't give scalar");
    } else {
        throw std::logic_error("MatrixTerm: unrecognized matrix type");
    }
}

DMatrix MatrixBlock(const Mono& A, const Mono& B, const MATRIX_TYPE type,
        const std::size_t partitions) {
    if (type == MAT_INTER_SAME_N) {
        NtoN_Final terms = MatrixTerm_NtoN(A, B);
        DMatrix output = DMatrix::Zero(partitions, partitions);
        std::cout << "NtoN terms for " << A << " x " << B << ":\n";
        for (auto& term : terms) {
            // if (!std::isfinite(static_cast<builtin_class>(newTerm.second))) {
                // std::cerr << "Error: term (" << newTerm.first << ", " 
                    // << newTerm.second << ") is not finite." << std::endl;
            // }
            std::cout << "(" << term.first << ", " << term.second << ")" 
                << std::endl;
            output += term.second*MuPart_NtoN(A.NParticles(), term.first, 
                                              partitions);
        }
        return output;
    } else if (type == MAT_INTER_N_PLUS_2) {
        const char n = A.NParticles();
        auto terms = MatrixTerm_NPlus2(A, B);
        // algebraically add terms by r exponent before doing the discretization
        std::unordered_map<char, coeff_class> addedTerms;
        for (const auto& term : terms) {
            if (addedTerms.count(term.r) == 0) {
                addedTerms.emplace(term.r, term.coeff);
            } else {
                addedTerms[term.r] += term.coeff;
            }
        }

        DMatrix output = DMatrix::Zero(partitions, partitions);
        std::cout << "N+2 terms for " << A << " x " << B << ":\n";
        for (const auto& term : addedTerms) {
            std::cout << term.second << " * (" << (int)n << ", " << 
                (int)term.first << ")" << std::endl;
            output += term.second
                    * MuPart_NPlus2(std::array<char,2>{{n, term.first}}, 
                                    partitions);
        }
        return output;
    } else {
        return MatrixTerm(A, B, type)*MuPart(partitions, type);
    }
}

coeff_class MatrixTerm_Direct(const Mono& A, const Mono& B, const MATRIX_TYPE type) {
    //std::cout << "TERM: " << A.HumanReadable() << " x " << B.HumanReadable() 
            //<< std::endl;

    // degeneracy factors result from turning the ordered monomials into 
    // symmetric polynomials
    coeff_class degeneracy = 1;
    degeneracy *= Factorial(A.NParticles());
    // for (auto& count : A.CountIdentical()) degeneracy *= Factorial(count);
    for (auto& count : B.CountIdentical()) degeneracy *= Factorial(count);

    coeff_class prefactor = degeneracy*A.Coeff()*B.Coeff()*Prefactor(A, B, type);

    std::string xAndy_A = ExtractXY(A);
    std::string xAndy_B = ExtractXY(B);
    std::vector<MatrixTerm_Final> fFromA, fFromB, combinedFs;

    coeff_class total = 0;
    // do {
    fFromA = DirectTermsFromXY(xAndy_A);
    do {
        fFromB = DirectTermsFromXY(xAndy_B);
        combinedFs = CombineTwoFs(fFromA, fFromB);
        total += FinalResult(combinedFs, type);
    } while (PermuteXY(xAndy_B));
    // } while (PermuteXY(xAndy_A));

    return prefactor*total;
}

NtoN_Final MatrixTerm_NtoN(const Mono& A, const Mono& B) {
    //std::cout << "INTERACTION: " << A.HumanReadable() << " x " 
    //<< B.HumanReadable() << std::endl;

    // degeneracy factors result from turning the ordered monomials into 
    // symmetric polynomials
    coeff_class degeneracy = 1;
    // degeneracy *= Factorial(A.NParticles());
    // degeneracy *= Factorial(B.NParticles());
    for (auto& count : A.CountIdentical()) degeneracy *= Factorial(count);
    for (auto& count : B.CountIdentical()) degeneracy *= Factorial(count);

    coeff_class prefactor = degeneracy*A.Coeff()*B.Coeff()
                          * Prefactor(A, B, MAT_INTER_SAME_N);

    std::string xAndy_A = ExtractXY(A);
    std::string xAndy_B = ExtractXY(B);
    std::vector<MatrixTerm_Intermediate> fFromA, fFromB;
    std::vector<InteractionTerm_Step2> combinedFs;
    NtoN_Final output;
    do {
        fFromA = InteractionTermsFromXY(xAndy_A);
        do {
            fFromB = InteractionTermsFromXY(xAndy_B);
            combinedFs = CombineInteractionFs(fFromA, fFromB);

            auto newTerms = InteractionOutput(combinedFs, prefactor);
            for (const auto& newTerm : newTerms) {
                // if (!std::isfinite(static_cast<builtin_class>(newTerm.second))) {
                    // std::cerr << "Error: term (" << newTerm.first << ", " 
                        // << newTerm.second << ") is not finite." << std::endl;
                // }
                if (output.count(newTerm.first) == 0) {
                    output.insert(newTerm);
                    // output.emplace(newTerm.first, newTerm.second);
                } else {
                    output[newTerm.first] += newTerm.second;
                }
            }
            // output.insert(output.end(), newTerms.begin(), newTerms.end());
        } while (PermuteXY(xAndy_B));
    } while (PermuteXY(xAndy_A));
    
    return output;
}

std::vector<NPlus2Term_Output> MatrixTerm_NPlus2(const Mono& A, const Mono& B) {
    //std::cout << "N+2 TERM: " << A << " x " << B << std::endl;

    if (B.NParticles() != A.NParticles() + 2) {
        throw std::logic_error("MatrixTerm_NPlus2: n_B != n_A + 2");
    }

    // degeneracy factors result from turning the ordered monomials into 
    // symmetric polynomials
    coeff_class degeneracy = 1;
    // degeneracy *= Factorial(A.NParticles());
    for (auto& count : A.CountIdentical()) degeneracy *= Factorial(count);
    for (auto& count : B.CountIdentical()) degeneracy *= Factorial(count);

    coeff_class prefactor = degeneracy*A.Coeff()*B.Coeff()
        * Prefactor(A, B, MAT_INTER_N_PLUS_2);

    std::string xAndy_A = ExtractXY(A);
    std::string xAndy_B = ExtractXY(B);
    std::vector<MatrixTerm_Intermediate> fFromA, fFromB;
    std::vector<NPlus2Term_Step2> combinedFs;
    std::vector<NPlus2Term_Output> output;
    do {
        fFromA = InteractionTermsFromXY(xAndy_A);
        do {
            fFromB = InteractionTermsFromXY(xAndy_B);
            combinedFs = CombineNPlus2Fs(fFromA, fFromB);
            auto newTerms = NPlus2Output(combinedFs, prefactor);
            output.insert(output.end(), newTerms.begin(), newTerms.end());
        } while (PermuteXY(xAndy_B));
    } while (PermuteXY(xAndy_A));
    
    return output;
}

// custom std::next_permutation for xAndy using particle precedence
bool PermuteXY(std::string& xAndy) {
    if (xAndy.size() % 2 != 0) {
        throw std::logic_error("Odd-length vector passed to PermuteXY");
    }
    if (xAndy.size() <= 2) return false;

    auto half = xAndy.size()/2;
    auto i = half - 1;

    while (i > 0) {
        auto i1 = i;
        i -= 1;
        if ( (xAndy[i] > xAndy[i1])
            || (xAndy[i] == xAndy[i1] && xAndy[half + i] > xAndy[half + i1]) ) {
            auto i2 = half;
            do {
                i2 -= 1;
            } while ( (xAndy[i] < xAndy[i2]) || 
                    (xAndy[i] == xAndy[i2] && xAndy[half+i] <= xAndy[half+i2]));
            std::swap(xAndy[i], xAndy[i2]);
            std::swap(xAndy[i + half], xAndy[i2 + half]);
            std::reverse(xAndy.begin() + i1, xAndy.begin() + half);
            std::reverse(xAndy.begin() + half + i1, xAndy.end());
            return true;
        }
    }
    std::reverse(xAndy.begin(), xAndy.begin() + half);
    std::reverse(xAndy.begin() + half, xAndy.end());
    return false;
}

const std::vector<MatrixTerm_Final>& DirectTermsFromXY(const std::string& xAndy)
{
    if (directCache.count(xAndy) == 0) {
        // copy so we can break it in the next function
        std::vector<MatrixTerm_Intermediate> intermediate 
            = InteractionTermsFromXY(xAndy);
        directCache.emplace(xAndy, ThetaFromYTilde(intermediate));
    }

    return directCache[xAndy];
}

const std::vector<MatrixTerm_Intermediate>& InteractionTermsFromXY(
        const std::string& xAndy) {
    if (intermediateCache.count(xAndy) == 0) {
        std::string x(xAndy.begin(), xAndy.begin() + xAndy.size()/2);
        std::string y(xAndy.begin() + xAndy.size()/2, xAndy.end());
        std::vector<char> uFromX(UFromX(x));
        std::vector<MatrixTerm_Intermediate> terms(YTildeFromY(y));
        for (auto& term : terms) {
            if (term.uPlus.size() < uFromX.size()/2) {
                term.uPlus.resize(uFromX.size()/2, 0);
                term.uMinus.resize(uFromX.size()/2, 0);
                term.yTilde.resize(uFromX.size()/2, 0);
            }
            for (std::size_t i = 0; i < term.uPlus.size(); ++i) {
                term.uPlus[i] += uFromX[i];
                term.uMinus[i] += uFromX[term.uPlus.size() + i];
                // term.coeff *= std::pow(std::sqrt(2), term.uPlus[i] + term.uMinus[i]);
            }
        }
        intermediateCache.emplace(xAndy, std::move(terms));
    }

    return intermediateCache[xAndy];
}

// exponent transformations ---------------------------------------------------
//
// rather than transforming the variables themselves, these take in a list of 
// exponents in one coordinate system and give out lists of exponents in the new 
// coordinate system.

// just gets the exponents of each x and y, so we don't have to worry about
// things like dividing by P or \mu
//
// note that I'm storing the Dirichlet-mandated P_- on each particle but Zuhair
// isn't, so we have to subtract that off; that is, this computes Fbar, not F
std::string ExtractXY(const Mono& extractFromThis) {
    auto n = extractFromThis.NParticles();
    std::string xAndy(2*n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        xAndy[i] = extractFromThis.Pm(i) - 1;
        xAndy[n + i] = extractFromThis.Pt(i);
    }
    return xAndy;
}

// goes from x to u using Zuhair's (4.21); returned vector has a list of all u+ 
// in order followed by a list of all u- in order
std::vector<char> UFromX(const std::string& x) {
    if (x.size() < 2) {
        std::cerr << "Error: asked to do exponent transform from X to U but "
            << "there were only " << x.size() << " entries in X." << std::endl;
        return {};
    }

    std::vector<char> u(2*x.size() - 2);
    // the terms from x_1 through x_{n-1} are regular
    for (auto i = 0u; i < x.size()-1; ++i) {
        u[i] = 2*x[i];
        for (auto j = 0u; j < i; ++j) {
            u[x.size()-1 + j] += 2*x[i];
        }
    }
    // the last term, from x_n, is different
    for (auto j = 0u; j < x.size()-1; ++j) {
        u[x.size()-1 + j] += 2*x.back();
    }

    //std::cout << "Converted " << x << " into " << u << std::endl;
    return u;
}

// convert from y to y-tilde following (4.26).
//
// this is by far the most intensive of the coordinate transformations: it has
// u biproducts in addition to the yTilde that you want, but worst of all it has
// two terms, so you end up with a sum of return terms, each with some 
// binomial-derived coefficient.
std::vector<MatrixTerm_Intermediate> YTildeFromY(const std::string& y) {
    //std::cout << "Transforming this y: " << y << std::endl;
    std::vector<MatrixTerm_Intermediate> ret;
    // i here is the i'th particle (pair); each one only sees those whose
    // numbers are lower, so we can treat them using lower particle numbers
    //
    // this loop goes to y.size()-1 because the last term, y_n, is special. The
    // first term is not special mathematically, but it's easier to code if we
    // do it separately as well

    // we begin with y_n because it's different from the others; it's restricted
    // to be the negative sum of all other y_i, so we can replace it directly at
    // the beginning; sadly this produces a bunch of terms
    std::vector<YTerm> yTerms = EliminateYn(y);
    //std::cout << "yTerms: " << std::endl;
    //for (const auto& yTerm : yTerms) std::cout << yTerm << std::endl;
    
    for (const YTerm& yTerm : yTerms) {
        std::vector<MatrixTerm_Intermediate> termsFromThisYTerm;
        // y_1 handled separately because it doesn't need multinomials. We 
        // always do this step even if yTerm[0] == 0 because it puts the
        // coefficient in and provides somewhere for the other terms to combine
        termsFromThisYTerm.emplace_back(1);
        termsFromThisYTerm.back().coeff = yTerm.coeff;
        termsFromThisYTerm.back().uPlus[0] = yTerm[0];
        termsFromThisYTerm.back().uMinus[0] = yTerm[0];
        termsFromThisYTerm.back().yTilde[0] = yTerm[0];

        // terms between y_2 and y_{n-1}, inclusive (beware 1- vs 0-indexing)
        for (auto i = 1u; i < yTerm.size(); ++i) {
            if (yTerm[i] == 0) continue;
            std::vector<MatrixTerm_Intermediate> termsFromThisY;
            for (char l = 0; l <= yTerm[i]; ++l) {
                for (const auto& nAndm : Multinomial::GetMVectors(i, yTerm[i]-l)) {
                    std::vector<MatrixTerm_Intermediate> newTerms(
                                    YTildeTerms(i, yTerm[i], l, nAndm) );
                    termsFromThisY.insert(termsFromThisY.end(),
                                    newTerms.begin(), newTerms.end());
                    //ret.insert(ret.end(), newTerms.begin(), newTerms.end());
                }
            }
            termsFromThisYTerm = MultiplyIntermediateTerms(
                            termsFromThisYTerm, termsFromThisY );
        }
        //std::cout << "Transformed " << yTerm << " into these:" << std::endl;
        //for (auto& term : termsFromThisYTerm) std::cout << term << std::endl;
        ret.insert(ret.end(), termsFromThisYTerm.begin(),
                        termsFromThisYTerm.end() );
    }

    //std::cout << "Got these:" << std::endl;
    //for (auto& term : ret) std::cout << term << std::endl;

    return ret;
}

std::vector<YTerm> EliminateYn(const std::string& y) {
    std::vector<YTerm> output;
    for (auto nAndm : Multinomial::GetMVectors(y.size()-1, y.back())) {
        coeff_class coeff = Multinomial::Lookup(y.size()-1, nAndm);
        if (y.back() % 2 == 1) coeff = -coeff;
        do {
                output.emplace_back(coeff, y, nAndm);
        } while (std::prev_permutation(nAndm.begin()+1, nAndm.end()));
    }
    return output;
}

// i is the y from y_i; a is the exponent of y_i; l is a binomial index; nAndm
// is a multinomial vector with total order a-l
std::vector<MatrixTerm_Intermediate> YTildeTerms(
		const unsigned int i, const char a, const char l, std::string nAndm) {
    std::vector<MatrixTerm_Intermediate> ret;

    coeff_class coeff = YTildeCoefficient(a, l, nAndm);
    do {
        ret.emplace_back(i+1);
        for (auto j = 0u; j <= i-1; ++j) {
            ret.back().uPlus[j] = nAndm[j+1];
            ret.back().yTilde[j] = nAndm[j+1];
            ret.back().uMinus[j] = a;
            // for (auto k = j+1; k < i; ++k) {
            for (auto k = 0u; k < j; ++k) {
                ret.back().uMinus[j] += nAndm[k+1];
                // ret.back().uMinus[k] += nAndm[j+1];
            }
        }
        ret.back().uPlus[i] = 2*a - l;
        ret.back().uMinus[i] = l;
        ret.back().yTilde[i] = l;

        ret.back().coeff = coeff;
    } while (std::prev_permutation(nAndm.begin()+1, nAndm.end()));
    return ret;
}

std::vector<MatrixTerm_Intermediate> MultiplyIntermediateTerms(
		const std::vector<MatrixTerm_Intermediate>& termsA, 
		const std::vector<MatrixTerm_Intermediate>& termsB) {
    if (termsA.size() == 0 || termsB.size() == 0) {
        std::cerr << "Warning: asked to multiply two term lists but one was "
            << "empty. A: " << termsA.size() << "; B: " << termsB.size() << "." 
            << std::endl;
        return termsA.size() == 0 ? termsB : termsA;
    }
    //if (termsA.size() == 0) return termsB;
    //if (termsB.size() == 0) return termsA;
    std::vector<MatrixTerm_Intermediate> output;
    for (const auto& termA : termsA) {
        for (const auto& termB : termsB) {
            output.push_back(termA * termB);
        }
    }
    return output;
}

// the coefficient of a YTildeTerm, i.e. everything that's not a u or yTilde
coeff_class YTildeCoefficient(const char a, const char l, 
		const std::string& nAndm) {
    //coeff_class ret = ExactBinomial(a, l);
    coeff_class ret = Multinomial::Choose(2, a, {static_cast<char>(a-l),l});
    ret *= Multinomial::Lookup(nAndm.size()-1, nAndm);
    if ((a-l) % 2 == 1) ret = -ret;
    return ret;
}

// convert from y-tilde to sines and cosines of theta following (4.32).
//
// returned vector has sines of all components in order followed by all cosines
std::vector<MatrixTerm_Final> ThetaFromYTilde(
        std::vector<MatrixTerm_Intermediate>& intermediateTerms) {
    std::vector<MatrixTerm_Final> ret;

    for (auto& term : intermediateTerms) {
        // sine[i] appears in all yTilde[j] with j > i (strictly greater)
        std::vector<char> sines(term.yTilde.size()-1, 0);
        for (auto i = 0u; i < sines.size(); ++i) {
            for (auto j = i+1; j < term.yTilde.size(); ++j) {
                sines[i] += term.yTilde[j];
            }
        }
        term.yTilde.resize(term.yTilde.size()-1); // so it can become "cosines"
        ret.emplace_back(term.coeff,
                std::move(term.uPlus), 
                std::move(term.uMinus),
                std::move(sines),
                std::move(term.yTilde) );
    }

    return ret;
}

// combines two u-and-theta coordinate wavefunctions (called F in Zuhair's
// notes), each corresponding to one monomial. Each is a sum over many terms, so
// combining them involves multiplying out two sums.
std::vector<MatrixTerm_Final> CombineTwoFs(const std::vector<MatrixTerm_Final>& F1,
		const std::vector<MatrixTerm_Final>& F2) {
    /*std::cout << "Combining two Fs with sizes " << F1.size() << " and " 
            << F2.size() << std::endl;*/
    std::vector<MatrixTerm_Final> ret;
    for (auto& term1 : F1) {
        for (auto& term2 : F2) {
            ret.emplace_back(term1.coeff * term2.coeff,
                    AddVectors(term1.uPlus, term2.uPlus),
                    AddVectors(term1.uMinus, term2.uMinus),
                    AddVectors(term1.sinTheta, term2.sinTheta),
                    AddVectors(term1.cosTheta, term2.cosTheta));
        }
    }
    //std::cout << "Combined Fs has " << ret.size() << " terms." << std::endl;
    return ret;
}

std::vector<InteractionTerm_Step2> CombineInteractionFs(
        const std::vector<MatrixTerm_Intermediate>& F1, 
        const std::vector<MatrixTerm_Intermediate>& F2) {
    std::vector<InteractionTerm_Step2> output;
    for (const auto& f1 : F1) {
        for (const auto& f2 : F2) {
            output.push_back(CombineInteractionFs_OneTerm(f1, f2));
        }
    }
    // terms with odd powers of r will eventually integrate to 0 so ditch them
    // output.erase(std::remove_if(output.begin(), output.end(),
                // [](const InteractionTerm_Step2& term){return term.r[0]%2 == 1;}),
            // output.end());
    // TODO: verify that this variant targeting all odd powers is legitimate,
    //        and make sure it's not safe to delete odd r[0] as well
    output.erase(std::remove_if(output.begin(), output.end(),
                [](const InteractionTerm_Step2& term)
                {return term.r[1]%2 == 1 || term.r[2]%2 == 1;}),
            output.end());
    return output;
}

InteractionTerm_Step2 CombineInteractionFs_OneTerm(
        const MatrixTerm_Intermediate& f1, const MatrixTerm_Intermediate& f2) {
    InteractionTerm_Step2 output;
    output.coeff = f1.coeff * f2.coeff;

    output.u.resize(f1.uPlus.size() + f1.uMinus.size() + 2);
    for (std::size_t i = 0; i < f1.uPlus.size()-1; ++i) {
        output.u[2*i] = f1.uPlus[i] + f2.uPlus[i];
        output.u[2*i + 1] = f1.uMinus[i] + f2.uMinus[i];
    }
    output.u[output.u.size() - 4] = f1.uPlus.back();
    output.u[output.u.size() - 3] = f1.uMinus.back();
    output.u[output.u.size() - 2] = f2.uPlus.back();
    output.u[output.u.size() - 1] = f2.uMinus.back();

    // if n >= 3, do this; for n == 2, theta is empty and r doesn't matter
    if (f1.yTilde.size() >= 2) {
        output.theta.resize(f1.yTilde.size()-2 + f2.yTilde.size()-2, 0);
        // sine[i] appears in all yTilde[j] with j > i (strictly greater)
        for (auto i = 0u; i < f1.yTilde.size()-2; ++i) {
            for (auto j = i+1; j < f1.yTilde.size()-1; ++j) {
                output.theta[2*i] += f1.yTilde[j] + f2.yTilde[j];
            }
            output.theta[2*i + 1] = f1.yTilde[i] + f2.yTilde[i];
        }

        output.alpha = 0;
        output.r[0] = 0;
        for (std::size_t i = 0; i < f1.yTilde.size()-1; ++i) {
            output.alpha += f2.yTilde[i];
            output.r[0] += f1.yTilde[i] + f2.yTilde[i];
        }
        output.r[1] = f1.yTilde.back();
        output.r[2] = f2.yTilde.back();
    } else {
        // set output r so that it doesn't get erased after it's returned
        output.alpha = 0;
        output.r = {{0, 0, 0}};
    }
    return output;
}

std::vector<NPlus2Term_Step2> CombineNPlus2Fs(
        const std::vector<MatrixTerm_Intermediate>& F1, 
        const std::vector<MatrixTerm_Intermediate>& F2) {
    std::vector<NPlus2Term_Step2> output;
    for (const auto& f1 : F1) {
        for (const auto& f2 : F2) {
            output.push_back(CombineNPlus2Fs_OneTerm(f1, f2));
        }
    }
    // terms with odd powers of r will eventually integrate to 0 so ditch them
    output.erase(std::remove_if(output.begin(), output.end(),
                [](const NPlus2Term_Step2& term){return term.r%2 == 1;}),
            output.end());
    return output;
}

NPlus2Term_Step2 CombineNPlus2Fs_OneTerm(
        const MatrixTerm_Intermediate& f1, const MatrixTerm_Intermediate& f2) {
    // std::cout << f1 << " |U| " << f2 << std::endl;

    NPlus2Term_Step2 output;
    output.coeff = f1.coeff * f2.coeff;

    output.u.resize(f1.uPlus.size() + f1.uMinus.size() + 4);
    for (std::size_t i = 0; i < f1.uPlus.size(); ++i) {
        output.u[2*i] = f1.uPlus[i] + f2.uPlus[i];
        output.u[2*i + 1] = f1.uMinus[i] + f2.uMinus[i];
    }
    output.u[output.u.size() - 4] = f2.uPlus [f2.uPlus .size()-2];
    output.u[output.u.size() - 3] = f2.uMinus[f2.uMinus.size()-2];
    output.u[output.u.size() - 2] = f2.uPlus [f2.uPlus .size()-1];
    output.u[output.u.size() - 1] = f2.uMinus[f2.uMinus.size()-1];

    output.theta.resize(f1.yTilde.size() + f2.yTilde.size() - 2, 0);
    // sine[i] appears in all yTilde[j] with j > i (strictly greater)
    for (std::size_t i = 0; i+2 < f1.yTilde.size(); ++i) {
        for (auto j = i+1; j+1 < f1.yTilde.size(); ++j) {
            output.theta[2*i] += f1.yTilde[j] + f2.yTilde[j];
        }
        output.theta[2*i + 1] = f1.yTilde[i] + f2.yTilde[i];
    }
    output.theta[output.theta.size()-2] = f2.yTilde[f2.yTilde.size()-1];
    output.theta[output.theta.size()-1] = f2.yTilde[f2.yTilde.size()-2];

    output.r = f2.yTilde[f2.yTilde.size()-2] + f2.yTilde[f2.yTilde.size()-1];

    return output;
}

// WARNING: if type == MAT_MASS this changes the MatrixTerm_Final vector by
// by changing each term's uMinus entries by -2
coeff_class FinalResult(std::vector<MatrixTerm_Final>& exponents,
		const MATRIX_TYPE type) {
    if (exponents.size() == 0) {
        std::cerr << "No exponents detected; returning 1." << std::endl;
        return 1;
    }
    // auto n = exponents.front().uPlus.size() + 1;
    coeff_class totalFromIntegrals = 0;
    for (auto& term : exponents) {
        if (type == MAT_INNER) {
            // just do the integrals
            totalFromIntegrals += DoAllIntegrals(term);
        } else if (type == MAT_MASS) {
            // sum over integral results for every possible 1/x
            term.uPlus[0] -= 2;
            totalFromIntegrals += DoAllIntegrals(term);
            for (std::size_t i = 1; i < term.uPlus.size(); ++i) {
                term.uPlus[i-1] += 2;
                term.uMinus[i-1] -= 2;
                term.uPlus[i] -= 2;
                totalFromIntegrals += DoAllIntegrals(term);
            }
            term.uPlus.back() += 2;
            term.uMinus.back() -= 2;
            totalFromIntegrals += DoAllIntegrals(term);
        }
    }
    //std::cout << "Returning FinalResult = " << totalFromIntegrals << std::endl;
    return totalFromIntegrals;
}

// do all of the integrals which are possible before mu discretization, and
// return an object mapping {alpha and r exponents} -> value
//
// WARNING: this changes combinedFs so that it can't be reused
NtoN_Final InteractionOutput(std::vector<InteractionTerm_Step2>& combinedFs, 
                             const coeff_class prefactor) {
    NtoN_Final output;
    for (auto& combinedF : combinedFs) {
        coeff_class integralPart = prefactor*DoAllIntegrals(combinedF);

        // if (!std::isfinite(static_cast<builtin_class>(integralPart))) {
            // std::cerr << "Error: integralPart(" << combinedF.u << ", " 
                // << combinedF.theta << ", " << prefactor << ") is not finite." 
                // << std::endl;
        // }

        const NtoN_Final& expansion = Expand(combinedF.r, combinedF.alpha);
        for (const auto& pair : expansion) {
            // if (!std::isfinite(static_cast<builtin_class>(pair.second))) {
                // std::cerr << "Error: Expand(" << combinedF.r 
                    // << ") returns a non-finite coefficient of " << pair.first
                    // << "." << std::endl;
            // }

            if (output.count(pair.first) == 0) {
                output.emplace(pair.first, pair.second*integralPart);
            } else {
                output[pair.first] += pair.second*integralPart;
            }
        }
    }
    return output;
}

// do the double multinomial expansion to turn a list of exponents of 
// {r, sqrt(1-r^2), sqrt(1-alpha^2 r^2)} into a map from exponents of 
// {alpha^2, r} to their coefficients (both represent a single monomial which is
// the product of its constituent powers)
const NtoN_Final& Expand(const std::array<char,3>& r, const char alpha) {
    static std::unordered_map<std::array<char,3>, NtoN_Final,
                              boost::hash<std::array<char,3>> > expansionCache;
    if (expansionCache.count(r) == 0) {
        NtoN_Final expansion;

        for (char mb = 0; mb <= r[1]/2; ++mb) {
            for (char mc = 0; mc <= r[2]/2; ++mc) {
                coeff_class value = Multinomial::Choose(r[1]/2, mb)
                                  * Multinomial::Choose(r[2]/2, mc);
                if ((mb + mc)%2 == 1) value = -value;

                // if (std::isnan(static_cast<builtin_class>(value))) {
                    // std::cerr << "Error: Expand(" << r << ", "
                        // << std::array<char,2>{{mb, mc}} << ") has a NaN value." 
                        // << std::endl;
                // }


                std::array<char,2> key{{static_cast<char>(alpha + 2*mc), 
                                        static_cast<char>(r[0] + 2*mb + 2*mc)}};
                expansion.emplace(key, value);
            }
        }

        // std::cout << "Expand(" << r << ") =\n";
        // for (const auto& entry : expansion) {
            // std::cout << "(" << entry.first << ", " << entry.second << ")"
                // << std::endl;
        // }

        expansionCache.emplace(r, std::move(expansion));
    }

    return expansionCache[r];
}

// do all of the integrals which are possible before mu discretization, and
// return a list of {value, {r exponents}} objects
//
// WARNING: this changes combinedFs so that it can't be reused
std::vector<NPlus2Term_Output> NPlus2Output(
        std::vector<NPlus2Term_Step2>& combinedFs, const coeff_class prefactor){
    std::vector<NPlus2Term_Output> output;
    for (auto& combinedF : combinedFs) {
        output.emplace_back(prefactor*DoAllIntegrals_NPlus2(combinedF), 
                combinedF.r );
    }
    return output;
}

// prefactors -----------------------------------------------------------------

coeff_class Prefactor(const Mono& A, const Mono&, const MATRIX_TYPE type) {
    if (type == MAT_INNER) {
        return InnerProductPrefactor(A.NParticles());
    } else if (type == MAT_MASS) {
        return MassMatrixPrefactor(A.NParticles());
    } else if (type == MAT_INTER_SAME_N) {
        return InteractionMatrixPrefactor(A.NParticles());
    } else if (type == MAT_INTER_N_PLUS_2) {
        return NPlus2MatrixPrefactor(A.NParticles());
    }
    std::cerr << "Error: prefactor type not recognized." << std::endl;
    return 0;
}

// this follows (2.2) in Matrix Formulas.pdf
coeff_class InnerProductPrefactor(const char n) {
    static std::unordered_map<char, coeff_class> ipPrefactorCache;
    if (ipPrefactorCache.count(n) == 0) {
        coeff_class denominator = std::tgamma(n+1); // tgamma = "true" gamma fcn
        denominator *= std::pow(8, n-1);
        denominator *= std::pow(M_PI, 2*n-3);
        //std::cout << "PREFACTOR: " << 1/denominator << std::endl;
        ipPrefactorCache.emplace(n, 1/denominator);
    }

    return ipPrefactorCache[n];
}

// this follows (2.3) in Matrix Formulas.pdf
coeff_class MassMatrixPrefactor(const char n) {
    // return n*InnerProductPrefactor(n); // if we're permuting M^2, remove n
    return InnerProductPrefactor(n);
}

coeff_class InteractionMatrixPrefactor(const char n) {
    static std::unordered_map<char, coeff_class> sameNPrefactorCache;
    if (sameNPrefactorCache.count(n) == 0) {
        coeff_class denominator = std::tgamma(n-1);
        denominator *= std::pow(M_PI*M_PI, n-1);
        denominator *= 4*std::pow(8, n);
        sameNPrefactorCache.emplace(n, 1/denominator);
    }

    return sameNPrefactorCache[n];
}

coeff_class NPlus2MatrixPrefactor(const char n) {
    static std::unordered_map<char, coeff_class> nPlus2PrefactorCache;
    if (nPlus2PrefactorCache.count(n) == 0) {
        coeff_class denominator = std::tgamma(n);
        denominator *= 6;
        denominator *= std::pow(M_PI, 2*n);
        denominator *= std::pow(8, n+1);
        nPlus2PrefactorCache.emplace(n, 1/denominator);
    }

    return nPlus2PrefactorCache[n];
}

// integrals ------------------------------------------------------------------

// do all the integrals for a direct matrix computation
coeff_class DoAllIntegrals(const MatrixTerm_Final& term) {
    std::size_t n = term.uPlus.size() + 1;
    coeff_class output = term.coeff;

    // do the u integrals first
    for (auto i = 0u; i < n-1; ++i) {
        output *= UPlusIntegral(term.uPlus[i] + 3, 
                                term.uMinus[i] + 5*(n - (i+1)) - 2);
    }

    // now the theta integrals; sineTheta.size() = cosTheta.size() = n-2.
    // All but the last one are short, while the last one is long
    //
    // these have constant terms which differ from Nikhil's because his i
    // starts at 1 instead of 0, so I use (i+1) instead
    if (n >= 3) {
        for (auto i = 0u; i < n-3; ++i) {
            output *= ThetaIntegral_Short(n - (i+1) - 2 + term.sinTheta[i],
                            term.cosTheta[i] );
        }
        output *= ThetaIntegral_Long(term.sinTheta[n-3], term.cosTheta[n-3]);
    } else {
        output *= 2;
    }
    // std::cout << term.coeff << " * {" << term.uPlus << ", " << term.uMinus
        // << ", " << term.sinTheta << ", " << term.cosTheta << "} -> " << output 
        // << std::endl;
    return output;
}

// do all the integrals for an interaction matrix computation
//
// WARNING: this changes the exponents in term, rendering it non-reusable
coeff_class DoAllIntegrals(InteractionTerm_Step2& term) {
    // std::cout << "DoAllIntegrals(" << term.u << ", " << term.theta << ")"
        // << std::endl;
    auto n = term.u.size()/2;

    // adjust exponents before doing integrals
    for (std::size_t i = 0; i < n-2; ++i) {
        term.u[2*i] += 3;
        term.u[2*i + 1] += 5*(n-i) - 8;
    }
    term.u[term.u.size()-1] += 1;
    term.u[term.u.size()-2] += 1;
    term.u[term.u.size()-3] += 1;
    term.u[term.u.size()-4] += 1;

    for (std::size_t k = 0; k+4 < n; ++k) {
        term.theta[2*k] += n-k-3;
    }

    // this part actually does the integrals
    coeff_class product = 1;
    for (std::size_t i = 0; i < n; ++i) {
        product *= UPlusIntegral(term.u[2*i], term.u[2*i + 1]);
    }
    for (std::size_t k = 0; k+3 < n; ++k) {
        product *= ThetaIntegral_Short(term.theta[2*k], term.theta[2*k + 1]);
    }
    return product;
}

// do all the integrals for an n+2 interaction computation
coeff_class DoAllIntegrals_NPlus2(const NPlus2Term_Step2& term) {
    std::size_t n = term.u.size()/2 - 1;
    coeff_class output = term.coeff;

    // do the non-primed u integrals first
    for (std::size_t i = 0; i < n-1; ++i) {
        output *= UPlusIntegral(term.u[2*i] + 3, 5*(n - i) - 3 + term.u[2*i + 1]);
    }

    // next the two primed u integrals (TODO: check additions!!)
    output *= UPlusIntegral(term.u[2*(n-1)] + 1, term.u[2*(n-1) + 1] + 1);
    output *= UPlusIntegral(term.u[2*n] + 1, term.u[2*n + 1] + 4);

    // now the theta integrals; there are n-2 "normal" theta integrals, followed
    // by one primed one. The primed and the last normal are long (I think?)
    //
    // these have constant terms which differ from Zuhair's because his i
    // starts at 1 instead of 0, so I use (i+1) instead
    if (n >= 3) {
        for (auto i = 0u; i < n-3; ++i) {
            output *= ThetaIntegral_Short(n - (i+1) - 2 + term.theta[2*i],
                            term.theta[2*i + 1] );
        }
        output *= ThetaIntegral_Long(term.theta[2*(n-3)],
                term.theta[2*(n-3) + 1]);
    }
    // I think this one (the primed one) is guaranteed to be there
    output *= ThetaIntegral_Long(term.theta[2*(n-2)], term.theta[2*(n-2) + 1]);

    // std::cout << "DOALLINTEGRALS_OUTPUT:" << output << '\n';
    return output;
}

// this is the integral of uplus^a uminus^b d(uplus) instead of d(u)
builtin_class UPlusIntegral(const builtin_class a, const builtin_class b) {
    std::array<builtin_class,2> abArray{{a,b}};
    if (b < a) std::swap(abArray[0], abArray[1]);
    if (uPlusCache.count(abArray) == 0) {
        uPlusCache.emplace(abArray, gsl_sf_beta(a/2.0 + 1.0, b/2.0 + 1.0));
    }

    return uPlusCache[abArray];
}

// this is the integral over the "theta" veriables from 0 to pi; it implements 
// Zuhair's 5.35, where a is the exponent of sin(theta) and b is the exponent of 
// cos(theta).
//
// results are cached by (a,b); since a and b are symmetric, we only store the
// results with a <= b, swapping the two parameters if they're the other order
builtin_class ThetaIntegral_Short(const builtin_class a, const builtin_class b) {
    if (static_cast<int>(b) % 2 == 1) return 0;
    std::array<builtin_class,2> abArray{{a,b}};
    if (b < a) std::swap(abArray[0], abArray[1]);
    if (thetaCache.count(abArray) == 1) return thetaCache.at(abArray);

    // builtin_class ret = std::exp(std::lgamma((1+a)/2) + std::lgamma((1+b)/2) 
                    // - std::lgamma((2 + a + b)/2) );
    builtin_class ret = gsl_sf_beta((a+1.0)/2.0, (b+1.0)/2.0);
    thetaCache.emplace(abArray, ret);
    return ret;
}

// this is the integral over the "theta" veriables from 0 to 2pi; it implements 
// Zuhair's 5.36, where a is the exponent of sin(theta) and b is the exponent of
// cos(theta).
builtin_class ThetaIntegral_Long(const builtin_class a, const builtin_class b) {
    if (static_cast<int>(a+b) % 2 == 1) return 0;
    return 2*ThetaIntegral_Short(a, b);
}

} // namespace MatrixInternal
