#include "calculation.hpp"

int Calculate(const Arguments& args) {
    // OStream& console = *args.console;
    gsl_set_error_handler(&GSLErrorHandler);

    if (args.options & OPT_TEST) {
        return Test::RunAllTests(args);
    }

    // initialize all multinomials which might come up
    //
    // this is obviously something of a blunt instrument and could easily be
    // made more efficient

    // for (int n = 1; n <= args.numP; ++n) {
        // console << "Initialize(" << n << ", " << 2*args.degree << ")" 
            // << endl;
        // Multinomial::Initialize(n, 2*args.degree);
    // }

    if (args.options & OPT_STATESONLY) {
        ComputeBasisStates(args);
        // if (args.outStream->rdbuf() != std::cout.rdbuf()) {
            // delete args.outStream;
        // }
        return EXIT_SUCCESS;
    }

    DMatrix hamiltonian = ComputeHamiltonian(args);
    // if (args.outStream->rdbuf() != std::cout.rdbuf()) {
        // delete args.outStream;
    // }
    return EXIT_SUCCESS;
}

// return basis polynomials. They are NOT normalized w.r.t. partitions
std::vector<Poly> ComputeBasisStates(const Arguments& args) {
    int numP = args.numP;
    int degree = args.degree + args.numP; // add required Dirichlet derivatives
    // int options = args.options;

    *args.outStream << "(*Orthogonal basis states with N=" << numP << ", L="
        << degree << " (including Dirichlet derivatives).*)" << endl;
    
    std::vector<Basis<Mono>> allEvenBases;
    std::vector<Basis<Mono>> allOddBases;
    for (int deg = numP; deg <= degree; ++deg) {
        splitBasis<Mono> degBasis(numP, deg, args);
        allEvenBases.push_back(degBasis.EvenBasis());
        allOddBases.push_back(degBasis.OddBasis());
    }

    *args.outStream << "(*EVEN STATE ORTHOGONALIZATION*)" << endl;
    std::vector<Poly> basisEven = ComputeBasisStates_SameParity(allEvenBases, 
                                                                args, false);

    *args.outStream << "(*ODD STATE ORTHOGONALIZATION*)" << endl;
    std::vector<Poly> basisOdd = ComputeBasisStates_SameParity(allOddBases, 
                                                               args, true);

    *args.outStream << endl;

    basisEven.insert(basisEven.end(), basisOdd.begin(), basisOdd.end());
    return basisEven;
}

// return basis polynomials. They are NOT normalized w.r.t. partitions
std::vector<Poly> ComputeBasisStates_SameParity(
        const std::vector<Basis<Mono>>& inputBases, const Arguments& args,
        const bool odd) {
    OStream& console = *args.console;
    std::vector<Poly> orthogonalized = Orthogonalize(inputBases, console, odd);

    // Basis<Mono> minimalBasis(MinimalBasis(orthogonalized));
    // if (outStream.rdbuf() == std::cout.rdbuf()) {
        // outStream << "Minimal basis: " << minimalBasis << endl;
    // } else {
        // outStream << "minimalBasis[" << suffix <<"] = " << MathematicaOutput(minimalBasis) 
            // << endl;
    // }

    return orthogonalized;
}

// output a matrix where each column is one of the basis vectors expressed in
// terms of the monomials on the minimal basis
DMatrix PolysOnMinBasis(const Basis<Mono>& minimalBasis,
                        const std::vector<Poly> orthogonalized, OStream&) {
    DMatrix polysOnMinBasis(minimalBasis.size(), orthogonalized.size());
    for (std::size_t i = 0; i < orthogonalized.size(); ++i) {
        polysOnMinBasis.col(i) = minimalBasis.DenseExpressPoly(
                        orthogonalized[i] );
    }

    // outStream << "(*Polynomials on this basis (as rows, not columns!):*)\n"
        // "polysOnMinBasis[" << suffix <<"] = " << MathematicaOutput(polysOnMinBasis.transpose()) 
        // << endl;

    return polysOnMinBasis;
}

DMatrix ComputeHamiltonian(const Arguments& args) {
    if (args.delta == 0.0) {
        *args.outStream << "(*Hamiltonian test at (n,l)=(" << args.numP 
            << "," << args.degree << "), ";
    } else {
        *args.outStream << "(*Hamiltonian test with delta=" << args.delta 
            << ", ";
    }
    *args.outStream << "kMax=" << args.partitions 
        << ". (m^2, \\lambda, \\Lambda) = (" << args.msq << ',' << args.lambda
        << ',' << args.cutoff << ")*)" << endl;

    Timer overallTimer;
    
    *args.outStream << "(*EVEN STATES*)" << endl;
    Hamiltonian evenHam = FullHamiltonian(args, false);
    // AnalyzeHamiltonian(evenHam, args);

    *args.outStream << "(*ODD STATES*)" << endl;
    Hamiltonian oddHam  = FullHamiltonian(args, true);
    // AnalyzeHamiltonian(oddHam, args);

    *args.console << "\nEntire computation took " 
        << overallTimer.TimeElapsedInWords() << "." << endl;

    return DMatrix();
}

// compute the hamiltonian for all states with delta up to args.delta; if 
// args.delta == 0, only compute one n-level (the DiagonalBlock at n=args.numP)
Hamiltonian FullHamiltonian(Arguments args, const bool odd) {
    int minN, maxN;
    if (args.delta != 0.0) {
        minN = 2;
        maxN = std::ceil(args.delta / 1.5);
    } else {
        minN = args.numP;
        maxN = args.numP;
    }
    Hamiltonian output;
    output.maxN = maxN;
    const std::string parity = odd ? ", odd" : ", even";
    const bool mathematica = (args.options & OPT_MATHEMATICA) != 0;
    OStream& outStream = *args.outStream;

    std::vector<Basis<Mono>> minBases;
    std::vector<SMatrix> discPolys;
    for (int n = minN; n <= maxN; ++n) {
        // FIXME: remove adjustment so degree's consistently "L above dirichlet"
        if (args.delta != 0.0) {
            args.numP = n;
            args.degree = std::ceil(args.delta - 0.5*n);
        } else {
            args.degree = args.degree + n;
        }

        // FIXME: directly generate only the monomials with the correct parity
        std::vector<Basis<Mono>> allEvenBases;
        std::vector<Basis<Mono>> allOddBases;
        for(int deg = n; deg <= args.degree; ++deg){
            splitBasis<Mono> degBasis(n, deg, args);
            allEvenBases.push_back(degBasis.EvenBasis());
            allOddBases.push_back(degBasis.OddBasis());
        }
        const std::vector<Basis<Mono>>& inputBases = 
                                            (odd ? allOddBases : allEvenBases);

        const std::string suffix = std::to_string(n) + parity;
        std::vector<Poly> orthogonalized = 
                        ComputeBasisStates_SameParity(inputBases, args, odd);
        minBases.push_back(MinimalBasis(orthogonalized));
        DMatrix polysOnMinBasis = PolysOnMinBasis(minBases[n-minN],
                                               orthogonalized, outStream);
        discPolys.push_back(DiscretizePolys(polysOnMinBasis, args.partitions));
        if (mathematica) {
            outStream << "minimalBasis[" << suffix << "] = "
                << MathematicaOutput(minBases[n-minN]) << endl;
            outStream << "(*Polynomials on this basis (as rows, not columns!):*)\n"
                << "polysOnMinBasis[" << suffix << "] = " 
                << MathematicaOutput(polysOnMinBasis.transpose()) << endl;
            outStream << "(*And discretized:*)\ndiscretePolys[" << suffix 
                << "] = " << MathematicaOutput(discPolys[n-minN].transpose()) 
                << endl;
        } else {
            outStream << "Minimal basis (" << n << "):" << minBases[n-minN] << endl;
        }

        if (minBases[n-minN].size() == 0) {
            continue;
        }

        output.diagonal.push_back(DiagonalBlock(minBases[n-minN], 
                                                discPolys[n-minN], 
                                                args, odd));
        if ((args.options & OPT_INTERACTING) != 0 && n-2 >= minN) {
            output.nPlus2.push_back(NPlus2Block(minBases[n-2-minN], 
                                                discPolys[n-2-minN],
                                                minBases[n-minN],
                                                discPolys[n-minN], 
                                                args, odd));
        }
    }

    return output;
}

DMatrix DiagonalBlock(const Basis<Mono>& minimalBasis, 
                      const SMatrix& discPolys, 
                      const Arguments& args, const bool odd) {
    *args.console << "DiagonalBlock(" << args.numP << ", " << args.degree << ")" 
        << endl;
    Timer timer;
    const bool interacting = (args.options & OPT_INTERACTING) != 0;
    std::string suffix = std::to_string(args.numP) + (odd ? ", odd" : ", even");

    timer.Start();
    DMatrix monoMassMatrix(MassMatrix(minimalBasis, args.partitions));
    DMatrix polyMassMatrix = discPolys.transpose()*monoMassMatrix*discPolys;
    OutputMatrix(monoMassMatrix, polyMassMatrix, "mass matrix", suffix, timer,
                 args);

    timer.Start();
    DMatrix monoKineticMatrix(KineticMatrix(minimalBasis, args.partitions));
    DMatrix polyKineticMatrix = discPolys.transpose()*monoKineticMatrix*discPolys;
    OutputMatrix(monoKineticMatrix, polyKineticMatrix, "kinetic matrix", suffix,
                 timer, args);

    DMatrix hamiltonian = args.msq*polyMassMatrix 
                        + (args.cutoff*args.cutoff)*polyKineticMatrix;
    if (interacting) {
        timer.Start();
        DMatrix monoNtoN(InteractionMatrix(minimalBasis, args.partitions));
        DMatrix polyNtoN = discPolys.transpose()*monoNtoN*discPolys;
        OutputMatrix(monoNtoN, polyNtoN, "NtoN matrix", suffix, timer, 
                     args);
        hamiltonian += (args.lambda*args.cutoff)*polyNtoN;
    }

    /*
    if (mathematica) {
        outStream << "hamiltonian[" << suffix <<"] = "
                << MathematicaOutput(hamiltonian) << endl;
    } else {
        EigenSolver solver(hamiltonian.cast<builtin_class>());
        outStream << "Here are the Hamiltonian eigenvalues:\n" 
            << solver.eigenvalues() << endl;
    }
    */

    return hamiltonian;
}

// basisA is the minBasis of degree n, while basisB is the one for degree n+2
DMatrix NPlus2Block(const Basis<Mono>& basisA, const SMatrix& discPolysA,
                    const Basis<Mono>& basisB, const SMatrix& discPolysB,
                    const Arguments& args, const bool odd) {
    *args.console << "NPlus2Block(" << args.numP-2 << " -> " << args.numP << ")" 
        << endl;
    Timer timer;
    std::string suffix = std::to_string(args.numP-2) 
                       + (odd ? ", odd" : ", even");

    timer.Start();
    DMatrix monoNPlus2(NPlus2Matrix(basisA, basisB, args.partitions));
    DMatrix polyNPlus2 = discPolysA.transpose()*monoNPlus2*discPolysB;
    OutputMatrix(monoNPlus2, polyNPlus2, "NPlus2 matrix", suffix, timer, args);

    return (args.lambda*args.cutoff) * polyNPlus2;
}

void AnalyzeHamiltonian(const Hamiltonian& hamiltonian, const Arguments& args) {
    Eigen::Index totalSize = 0;
    for (const auto& block : hamiltonian.diagonal) totalSize += block.rows();
    if (totalSize <= MAX_DENSE_SIZE) {
        AnalyzeHamiltonian_Dense(hamiltonian, args);
    } else {
        AnalyzeHamiltonian_Sparse(hamiltonian, args);
    }
}

void AnalyzeHamiltonian_Dense(const Hamiltonian& hamiltonian, 
                               const Arguments& args) {
    Eigen::Index totalSize = 0;
    for (const auto& block : hamiltonian.diagonal) totalSize += block.rows();
    DMatrix matrixForm(totalSize, totalSize);

    Eigen::Index offset = 0;
    Eigen::Index trailingOffset = 0;
    for (std::size_t n = 2; n < hamiltonian.diagonal.size()+2; ++n) {
        const auto& block = hamiltonian.diagonal[n-2];
        matrixForm.block(offset, offset, block.rows(), block.cols()) = block;

        if (n >= 4) {
            const auto& nPlus2Block = hamiltonian.nPlus2[n-4];
            matrixForm.block(trailingOffset, offset, nPlus2Block.rows(),
                             nPlus2Block.cols()) = nPlus2Block;
            matrixForm.block(offset, trailingOffset, nPlus2Block.cols(),
                             nPlus2Block.rows()) = nPlus2Block.transpose();
            trailingOffset += nPlus2Block.rows();
        }
        offset += block.rows();
    }

    EigenSolver solver(matrixForm.cast<builtin_class>());
    *args.console << "Hamiltonian eigenvalues:\n" 
        << solver.eigenvalues() << endl;
}

void AnalyzeHamiltonian_Sparse(const Hamiltonian& hamiltonian, 
                               const Arguments&) {
    Eigen::Index offset = 0;
    Eigen::Index trailingOffset = 0;
    std::vector<Triplet> triplets;
    for (std::size_t n = 2; n < hamiltonian.diagonal.size()+2; ++n) {
        // translate the diagonal block into sparse triplets
        const auto& block = hamiltonian.diagonal[n-2];
        for (Eigen::Index i = 0; i < block.rows(); ++i) {
            for (Eigen::Index j = 0; j < block.cols(); ++j) {
                triplets.emplace_back(offset+i, offset+j, block(i,j));
            }
        }

        // if there's an nPlus2 block ending on this n, tripletize it too
        if (n >= 4) {
            const auto& nPlus2Block = hamiltonian.nPlus2[n-4];
            for (Eigen::Index i = 0; i < nPlus2Block.rows(); ++i) {
                for (Eigen::Index j = 0; j < nPlus2Block.cols(); ++j) {
                    triplets.emplace_back(trailingOffset+i, offset+j, 
                                          nPlus2Block(i,j));
                    triplets.emplace_back(offset+i, trailingOffset+j, 
                                          nPlus2Block(i,j));
                }
            }
            trailingOffset += nPlus2Block.rows();
        }
        offset += block.rows();
    }

    SMatrix matrixForm(offset, offset);
    matrixForm.setFromTriplets(triplets.begin(), triplets.end());

    // TODO: get eigenvalues of matrixForm
}

void OutputMatrix(const DMatrix& monoMatrix, const DMatrix& polyMatrix,
                  std::string name, const std::string& suffix, Timer& timer, 
                  const Arguments& args) {
    OStream& outStream = *args.outStream;
    OStream& console = *args.console;
    const bool mathematica = (args.options & OPT_MATHEMATICA) != 0;

    if (mathematica) {
        std::string mathematicaName = MathematicaName(name);
        name[0] = std::toupper(name[0]);
        outStream << "minBasis" << mathematicaName << "[" << suffix <<"] = "
            << MathematicaOutput(monoMatrix) << endl;
        outStream << "basisState" << mathematicaName << "[" << suffix <<"] = "
            << MathematicaOutput(polyMatrix) << endl;
        console << name << " computed in " << timer.TimeElapsedInWords()
            << "." << endl;
    } else if (polyMatrix.rows() <= 10 && polyMatrix.cols() <= 10) {
        outStream << "Computed a " << name << " for the basis in " 
            << timer.TimeElapsedInWords() << "; mono:\n" << monoMatrix 
            << "\npoly:\n" << polyMatrix << endl;
    } else if (polyMatrix.rows() == polyMatrix.cols()) {
        EigenSolver solver(polyMatrix.cast<builtin_class>());
        outStream << "Computed a " << name << " for the basis in " 
            << timer.TimeElapsedInWords() << "; its eigenvalues are:\n"
            << solver.eigenvalues() << endl;
    } else {
        outStream << "Computed a " << name << " for the basis in " 
            << timer.TimeElapsedInWords() << ", but it's not square and is "
            << "too large to show." << endl;
    }
}

// capitalize each word, then delete all non-alphanumeric chars (inc. spaces)
std::string MathematicaName(std::string name) {
    name[0] = std::toupper(name[0]);
    bool capNext = false;
    for (std::size_t i = 0; i < name.size(); ++i) {
        if (capNext) {
            capNext = false;
            name[i] = std::toupper(name[i]);
        } else if (name[i] == ' ') {
            capNext = true;
        }
    }
    name.erase(std::remove_if(name.begin(), name.end(), 
                              [](char c){ return !std::isalnum(c); }),
               name.end());
    return name;
}

void GSLErrorHandler(const char* reason, const char* file, int line, int err) {
    std::cerr << "GSL Error in " << file << ":" << line << " --- "
        << gsl_strerror(err) << ", " << reason << std::endl;
}
