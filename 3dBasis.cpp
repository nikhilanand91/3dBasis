#include "3dBasis.hpp"

int main(int argc, char* argv[]) {
	arguments args = ParseArguments(argc, argv);
	if (args.options & OPT_VERSION){
		std::cout << "This is 3dBasis version " << VERSION << ", released "
			<< RELEASE_DATE << ". The latest updates can always be found at "
			<< "https://github.com/chussong/3dBasis." << std::endl;
		return EXIT_SUCCESS;
	}

	if (args.degree == 0 || args.numP == 0){
		std::cerr << "Error: you must enter a number of particles, a degree, "
			<< "and a value for delta." << std::endl;
		return EXIT_FAILURE;
	}

	if (args.options & OPT_MULTINOMTEST) {
		Multinomial::Initialize(args.numP, args.degree);
		for (char n = 0; n <= args.degree; ++n) {
			std::cout << "n = " << std::to_string(n) << ": ";
			for (auto& mVector : Multinomial::GetMVectors(args.numP, n)) {
				//std::cout << std::endl << Multinomial::MVectorOut(mVector) << ": ";
				std::cout << Multinomial::Lookup(args.numP, mVector) << ", ";
			}
			std::cout << std::endl;
		}
		return EXIT_SUCCESS;
	}

	//ExactBinomial_FillTo(args.degree);
	// Multinomial::Initialize(2, args.degree); // binomial coefficients
	for (int n = 1; n <= args.numP; ++n) {
		Multinomial::Initialize(n, 2*args.degree);
	}

	//if(args.options & OPT_IPTEST){
		return InnerProductTest(args);
	//}
}

int InnerProductTest(const arguments& args) {
	int numP = args.numP;
	int degree = args.degree + args.numP; // add required Dirichlet derivatives
	//coeff_class delta = args.delta;
	int options = args.options;

	//options = options | OPT_DEBUG;

	std::cout << "Beginning inner product test with N=" << numP << ", L="
		<< degree << " (including Dirichlet derivatives)." << std::endl;
	
	//std::cout << "Testing gamma cache construction." << std::endl;
	// GammaCache cache(numP, 2*degree, 2*(degree-numP));
	// KVectorCache kCache(numP, 2*(degree-numP)); // maxPt might be half this?
	//std::cout << "A coefficient from it: " << cache.Middle(degree-2, 2, 1)
		//<< std::endl;

	std::vector<Basis<Mono>> allEvenBases;
	std::vector<Basis<Mono>> allOddBases;
	for(int deg = numP; deg <= degree; ++deg){
		splitBasis<Mono> degBasis(numP, deg, options);
		allEvenBases.push_back(degBasis.EvenBasis());
		allOddBases.push_back(degBasis.OddBasis());
		//std::cout << allEvenBases.back() << std::endl;
		//std::cout << allOddBases.back() << std::endl;
	}

	/*for(auto& basis : allEvenBases){
		for(auto& basisMono : basis){
			basisMono /= std::sqrt(Mono::InnerProduct(basisMono, basisMono,
						cache, kCache) );
		}
	}
	for(auto& basis : allOddBases){
		for(auto& basisMono : basis){
			basisMono /= std::sqrt(Mono::InnerProduct(basisMono, basisMono,
						cache, kCache) );
		}
	}*/

	std::cout << "EVEN STATE ORTHOGONALIZATION" << std::endl;
	Orthogonalize(allEvenBases);
	/*std::cout << "REORDERED" << std::endl;
	std::swap(allEvenBases.front(), allEvenBases.back());
	Orthogonalize(allEvenBases, cache, kCache);*/

	std::cout << "ODD STATE ORTHOGONALIZATION" << std::endl;
	Orthogonalize(allOddBases);
	/*std::cout << "REORDERED" << std::endl;
	std::swap(allOddBases.front(), allOddBases.back());
	Orthogonalize(allOddBases, cache, kCache);*/

	return EXIT_SUCCESS;
}

arguments ParseArguments(int argc, char* argv[]) {
	std::vector<std::string> options;
	std::string arg;
	arguments ret;
	ret.delta = 0;
	int j = 0;
	for(int i = 1; i < argc; ++i){
		arg = argv[i];
		if(arg.size() > 0){
			if(arg[0] == '-'){
				options.push_back(arg);
			} else {
				switch(j){
					case 0:
						ret.numP = ReadArg<int>(arg);
						break;
					case 1:
						ret.degree = ReadArg<int>(arg);
						break;
					case 2:
						ret.delta = ReadArg<coeff_class>(arg);
						break;
					default:
						std::cerr << "Error: at most three non-option arguments"
							<< " may be given." << std::endl;
						return ret;
				}
				++j;
			}
		}
	}
	if(j < 2) ret.numP = 0; // invalidate the input since it was insufficient
	ret.options = ParseOptions(options);
	if (argc < 3 || std::abs(ret.delta) < EPSILON) ret.delta = 0.5;
	return ret;
}

// -b solves using the non-split method
int ParseOptions(std::vector<std::string> options) {
	int ret = 0;
	for(auto& opt : options){
		if(opt.compare(0, 2, "-v") == 0){
			ret = ret | OPT_VERSION;
			continue;
		}
		if(opt.compare(0, 2, "-d") == 0){
			ret = ret | OPT_DEBUG;
			ret = ret | OPT_OUTPUT;
			continue;
		}
		if(opt.compare(0, 2, "-o") == 0){
			ret = ret | OPT_OUTPUT;
			continue;
		}
		if(opt.compare(0, 2, "-i") == 0){
			ret = ret | OPT_IPTEST;
			continue;
		}
		if(opt.compare(0, 2, "-m") == 0){
			ret = ret | OPT_MULTINOMTEST;
			continue;
		}
		if(opt.compare(0, 2, "-M") == 0){
			ret = ret | OPT_ALLMINUS;
			continue;
		}
		if(opt.compare(0, 1, "-") == 0){
			std::cerr << "Warning: unrecognized option " << opt << " will be "
				<< "ignored." << std::endl;
			continue;
		}
	}
	return ret;
}

bool particle::operator==(const particle& other) const {
	return (pm == other.pm) && (pt == other.pt);
}

// this version is a 'loose' EoM compliance that only removes Pp which are on
// the same particle as a Pm
bool EoMAllowed() {
	std::cout << "Warning: EoMAllowed is deprecated." << std::endl;
	return true;
}

/*
std::list<Triplet> ConvertToRows(const std::vector<Poly>& polyForms, 
		const Basis<Mono>& targetBasis, const Eigen::Index rowOffset) {
	if(polyForms.size() == 0) return std::list<Triplet>();
	std::list<Triplet> ret = targetBasis.ExpressPoly(polyForms[0], 0,
			rowOffset);
	for(auto i = 1u; i < polyForms.size(); ++i){
		ret.splice(ret.end(), targetBasis.ExpressPoly(polyForms[i], i,
				rowOffset));
	}
	return ret;
}
*/

/*Poly ColumnToPoly(const SMatrix& kernelMatrix, const Eigen::Index col, 
		const Basis<Mono>& startBasis) {
	Poly ret;
	if(static_cast<size_t>(kernelMatrix.rows()) != startBasis.size()){
		std::cerr << "Error: the given Q matrix has " << kernelMatrix.rows()
			<< " rows, " << "but the given basis has " << startBasis.size() 
			<< " monomials. These must be the same." << std::endl;
		return ret;
	}
	for(Eigen::Index row = 0; row < kernelMatrix.rows(); ++row){
		if(kernelMatrix.coeff(row, col) == 0) continue;
		ret += kernelMatrix.coeff(row, col)*startBasis[row];
	}

	if(ret.size() == 0) return ret;
	coeff_class smallestCoeff = std::abs(ret[0].Coeff());
	for(auto& term : ret) smallestCoeff = std::min(std::abs(term.Coeff()), smallestCoeff);
	for(auto& term : ret) term /= smallestCoeff;
	return ret;
}*/

Poly ColumnToPoly(const DMatrix& kernelMatrix, const Eigen::Index col, 
		const Basis<Mono>& startBasis) {
	Poly ret;
	if(static_cast<size_t>(kernelMatrix.rows()) != startBasis.size()){
		std::cerr << "Error: the given Q matrix has " << kernelMatrix.rows()
			<< " rows, " << "but the given basis has " << startBasis.size() 
			<< " monomials. These must be the same." << std::endl;
		return ret;
	}
	for(Eigen::Index row = 0; row < kernelMatrix.rows(); ++row){
		if(kernelMatrix.coeff(row, col) == 0) continue;
		ret += kernelMatrix.coeff(row, col)*startBasis[row];
	}

	if(ret.size() == 0) return ret;
	coeff_class smallestCoeff = std::abs(ret[0].Coeff());
	for(auto& term : ret) smallestCoeff = std::min(std::abs(term.Coeff()), smallestCoeff);
	for(auto& term : ret) term /= smallestCoeff;
	return ret;
}

void ClearZeros(DMatrix* toClear) {
	if(!toClear){
		std::cerr << "Error: asked to clear the zeros from a nullptr instead of"
			<< " a matrix." << std::endl;
		return;
	}
	coeff_class threshold = EPSILON*toClear->cwiseAbs().maxCoeff();
	for(Eigen::Index row = 0; row < toClear->rows(); ++row){
		for(Eigen::Index col = 0; col < toClear->cols(); ++col){
			if(std::abs((*toClear)(row, col)) < threshold){
				(*toClear)(row, col) = 0;
			}
		}
	}
}
