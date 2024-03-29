#include <iostream>
#include <omp.h>
//#include <iterator>
#include <algorithm>
#include <vector>
#include <random>
#include <iomanip>
#include <cmath>
#include <functional>
#include <numeric>
#include <fstream>
#include <map>

class Ising1D {
public:

	int L;
	double kT = 1;
	const double J = 1;
	const int  K = 1, h = 0;

	Ising1D(int len = 4) {
		L = len;
		std::mt19937 gen(12345);
		std::uniform_real_distribution<> dist(0., 1.);
		fill(spins, gen, dist, L);
		H = hamiltonian();
	}

	void print_state() const {
		for (const auto& x : spins) {
			std::cout << std::setw(4) << x;
		}
		std::cout << std::endl;
	}

	double hamiltonian() {
		double H = 0, m = 0;
		for (int i = 0; i < L; i++) {
			H += spins[i] * spins[(i + 1) % L];
			m += spins[i];
		}
		return -J * H  - h * m;
	}

	inline double dE(int i) const {
		return 2 * J * spins[i] * (spins[(i + 1) % L] + spins[(i - 1 + L) % L]);
	}

	inline void change_spin(int i) {
		H += dE(i);
		spins[i] *= -1;
	}

	inline double get_H() const {
		return H;
	}

private:
	std::vector<int> spins;

	double H;

	int fill(std::vector<int>& spins, std::mt19937& gen,
		std::uniform_real_distribution<>& dist, int L) {
		//std::cout << spins.size() << std::endl;
		for (int i = 0; i < L; i++) {
				spins.push_back( (dist(gen) - 0.5) > 0 ? 1 : -1);
		}
		return 0;
	}
};

double mean(std::vector<double>& v) {
	return std::accumulate(v.begin(), v.end(), 0.) / v.size();
}


class MCMC {
public:
	MCMC(Ising1D& model, unsigned n = 1e5, unsigned sk = 1e5) : gen(12345), dist_int(0, model.L - 1), dist_float(0., 1.) {
		N = n; skip = sk;
	}
	void metropolis(Ising1D& model, double& mE, double& C, double& acc, double& disp, double& err, double& err_r) {
		int nn = 1000;
		std::vector<double> E(N / nn);
		std::vector<double> E2(N / nn);

		std::vector<double> A(N / nn);
		std::vector<double> A2(N / nn);

		unsigned k = 0, j = 0;
		std::vector<double> ei(nn), ei2(nn), ai(nn), ai2(nn);
		unsigned  inc; double a0 = 0;
		double em, em2, inc_d;
		acc = 0;
		for (long i = 0; i < skip; i++) {
			ms_metropolis(model, em, em2);
			//ms_heatbath(model, em, em2);
		}
		for (long i = 0; i < N; i++) {

			if (i % nn == 0 && i > 0) {
				A[k] = a0 / nn / model.L;
				a0 = 0;
				double mei = mean(ei);
				E[k] = mei;
				E2[k] = mean(ei2); 
				k++;
				j = 0;

			}

			inc = ms_metropolis(model, em, em2);
			//inc = ms_heatbath(model, em, em2);
			ei[j] = em;
			ei2[j] = em2;
			j++;
			a0 += inc;
		}

		A[k] = a0 / nn / model.L; 
		a0 = 0;
		double mei = mean(ei);
		E[k] = mei;
		E2[k] = mean(ei2);

		mE = mean(E);
		disp = (mean(E2) - mE * mE) / N / model.L;
		err = std::sqrt(disp);
		C = (mean(E2) - mE * mE) / model.kT / model.kT;

		acc = mean(A);
		err_r = std::sqrt(std::inner_product(A.begin(), A.end(), A.begin(), 0.) / A.size() - acc * acc);
		
	}

private:
	std::mt19937 gen;
	std::uniform_int_distribution<int> dist_int;
	std::uniform_real_distribution<> dist_float;
	unsigned N; unsigned skip;

	int ms_metropolis(Ising1D& model, double& em, double& em2 ) {
		int a = 0;
		std::vector<double> e;
		for (int k = 0; k < model.L; k++) {
			int i = dist_int(gen);
			double dE = model.dE(i);
			if (dist_float(gen) < std::exp(-dE / model.kT)) {
				model.change_spin(i);
				a++;
			}
			e.push_back(model.get_H());
		}
		em = mean(e);
		em2 = std::inner_product(e.begin(), e.end(), e.begin(), 0.) / e.size();
		return a;
	}
	int ms_heatbath(Ising1D& model, double& em, double& em2) {
		int a = 0;
		std::vector<double> e;
		for (int k = 0; k < model.L; k++) {
			int i = dist_int(gen);
			double dE = model.dE(i);
			if (dist_float(gen) < std::exp(-dE / model.kT) / (std::exp(-dE / model.kT) + 1)) {
				model.change_spin(i);
				a++;
			}
			e.push_back(model.get_H());
		}
		em = mean(e);
		em2 = std::inner_product(e.begin(), e.end(), e.begin(), 0.) / e.size();
		return a;
	}
	int sweep_glauber(Ising1D& model) {
		int a = 0;
		for (int k = 0; k < model.L; k++) {
			int i = dist_int(gen), j = dist_int(gen);
			double dE = model.dE(i);
			if (dist_float(gen) < 0.5 * (1 - std::tanh(dE / model.kT / 2))) {
				model.change_spin(i);
				a++;
			}
		}
		return a;
	}
};



int main()
{
	int L = 512;

	double E, C, R, disp, err, err_r;
	std::map<double, std::vector<double>> results;
	std::vector<double> kTs = { 0.2,0.5144818447123739,0.6279049274598625,0.7422823706597046,0.8560973993783693,0.9634233811244165,1.075123326581977,1.1958220178100092,1.3263376730881773,1.4705920691666368,1.6323247209453848,1.8146600210602584,2.021887690975566,2.2584957186386174,2.529000600114926,2.836342257357229,3.178757803411446,3.553214955237606,3.9553759735912477,4.378724998884133,4.81789746361766,5.268770000491321,5.7280730262863075,6.193219986657983,6.662610051042365,7.134975038950121,7.609295572788033,8.085558424305056,8.562945809868532,9.041312855219694,9.52029333863015,10.0 };

	std::ofstream file("metropolis T, E, C, R disp std L=512 sweeps term.csv");
	{
#pragma omp parallel for  num_threads(8) private(E,C,R, disp, err, err_r)  
		for (int i = 0; i < kTs.size(); i++) {
			double kT = kTs[i];
			std::cout << kT << std::endl;
			Ising1D model(L);
			unsigned sweeps = 1e6, term = 1e5;

			MCMC simulation(model, sweeps, term);

			model.kT = kT;
			simulation.metropolis(model, E, C, R, disp, err, err_r);

			std::cout << std::setw(10) <<  std::setprecision(16) << ' ' << kT << ' ' << E / L  << ' ' << C << ' ' << R << ' ' << disp << ' ' << err  <<'\n';
			std::vector<double> data = { kT, E / L , C, R, disp, err, err_r };

			results[kT] = data;

		}
	}
	for (auto& x : results) {
		std::cout << x.first << ' ';
		for (auto& y : x.second) {
			std::cout << ' ' << y;
			file << ' ' << std::setprecision(16) << y;
		}
		file << std::endl;
		std::cout << std::endl;
	}

	file.close();

	return 0;
}