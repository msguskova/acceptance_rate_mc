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

class Potts1D {
public:

	int L;
	double kT = 1;
	const double J = 1;
	const int  K = 1, h = 0;
	int q;

	Potts1D(int q_, int len = 4) {
		q = q_;
		std::mt19937 gen(12345);
		std::uniform_int_distribution<int> dist_q(0, q - 1);
		L = len;
		fill(spins, gen, dist_q, L);
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
			H += (int) (spins[i] == spins[(i + 1) % L]);
			m += spins[i];
		}
		return -J * H - h * m;
	}

	inline double dE(int i, int newq) const {
		return J * ((int)(spins[i] == spins[(i + 1) % L]) + (int)(spins[i] == spins[(i - 1 + L) % L]) -
			(int)(newq == spins[(i + 1) % L]) - (int)(newq == spins[(i - 1 + L) % L]));

	}

	inline void change_spin(int i, int newq) {
		H += dE(i, newq);
		spins[i] = newq;
	}

	inline double get_H() const {
		return H;
	}

private:
	std::vector<int> spins;

	double H;

	int fill(std::vector<int>& spins, std::mt19937& gen,
		std::uniform_int_distribution<>& dist_q, int L) {
		for (int i = 0; i < L; i++) {
			spins.push_back(dist_q(gen) );
		}
		return 0;
	}
};

double mean(std::vector<double>& v) {
	return std::accumulate(v.begin(), v.end(), 0.) / v.size();
}


class MCMC {
public:
	MCMC(Potts1D& model, long n = 1e5, long sk = 1e5) : gen(12345), dist_int(0, model.L - 1), dist_q(0, model.q - 1), dist_float(0., 1.) {
		N = n; skip = sk;
	}
	void metropolis(Potts1D& model, double& mE, double& C, double& acc, double& disp, double& err, double& err_r) {
		int nn = 1000;
		std::vector<double> E(N / nn);
		std::vector<double> E2(N / nn);

		std::vector<double> A(N / nn);

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
	std::uniform_int_distribution<int> dist_int, dist_q;
	std::uniform_real_distribution<> dist_float;
	long N; int skip;

	int ms_metropolis(Potts1D& model, double& em, double& em2) {
		int a = 0;
		std::vector<double> e;
		for (int k = 0; k < model.L; k++) {
			int i = dist_int(gen), qnew = dist_q(gen);
			double dE = model.dE(i, qnew);
			if (dist_float(gen) < std::exp(-dE / model.kT)) {
				model.change_spin(i, qnew);
				a++;
			}
			e.push_back(model.get_H());
		}
		em = mean(e);
		em2 = std::inner_product(e.begin(), e.end(), e.begin(), 0.) / e.size();
		return a;
	}
	int ms_heatbath(Potts1D& model, double& em, double& em2) {
		int a = 0;
		std::vector<double> e;
		for (int k = 0; k < model.L; k++) {
			int i = dist_int(gen), qnew = dist_q(gen);
			double dE = model.dE(i, qnew);
			if (dist_float(gen) < std::exp(-dE / model.kT) / (std::exp(-dE / model.kT) + 1)) {
				model.change_spin(i, qnew);
				a++;
			}
			e.push_back(model.get_H());
		}
		em = mean(e);
		em2 = std::inner_product(e.begin(), e.end(), e.begin(), 0.) / e.size();
		return a;
	}
	int sweep_glauber(Potts1D& model) {
		int a = 0;
		for (int k = 0; k < model.L; k++) {
			int i = dist_int(gen), qnew = dist_q(gen);
			double dE = model.dE(i, qnew);
			if (dist_float(gen) < 0.5 * (1 - std::tanh(dE / model.kT / 2))) {
				model.change_spin(i, qnew);
				a++;
			}
		}
		return a;
	}
};



int main()
{
	int L = 128, q=4;

	double E, C, R, disp, err, err_r;
	std::map<double, std::vector<double>> results;
	std::vector<double> kTs = { 0.2,0.27760593475242323,0.3451509151643941,0.4073987135517785,0.46911310223044295,0.5350578535162532,0.609997269402615,0.6990823087514186,0.8085336057984078,0.9463147615630795,1.1255128695751415,1.3607576501236411,1.6620233687106831,2.0230723520054745,2.424853571173207,2.848734967396604,3.283876056003356,3.724909352079287,4.169136617572918,4.615213345356755,5.062365045398677,5.510207445964958,5.958495878761996,6.407087120198209,6.8558868804152375,7.304831875621032,7.75388251680069,8.203011730005297,8.65220031486957,9.10143472657639,9.550703806052573,10.0 };

	char fname[128];
	std::sprintf(fname, "metropolis q=%d T, E, C, R disp std L=%d sweeps term.csv", q, L);

	std::ofstream file(fname);

	{
#pragma omp parallel for  num_threads(8) private(E,C,R, disp, err, err_r)  
		for (int i = 0; i < kTs.size(); i++) {
			double kT = kTs[i];
			std::cout << kT << std::endl;
			Potts1D model(q, L);
			long sweeps = 1e5, term = 1e3;

			MCMC simulation(model, sweeps, term);

			model.kT = kT;
			simulation.metropolis(model, E, C, R, disp, err, err_r);

			std::cout << std::setw(10) << ' ' << kT << ' ' << E / L << ' ' << C << ' ' << R << ' ' << disp << ' ' << err << '\n';
			std::vector<double> data = { kT, E / L , C, R, disp, err, err_r};

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