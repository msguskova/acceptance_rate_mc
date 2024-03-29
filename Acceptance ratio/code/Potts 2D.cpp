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

class Potts2D {
public:

	int L, q;
	double kT = 1;
	const int J = 1, K = 1, h = 0;

	Potts2D(int q_, int len = 4) {
		L = len;
		q = q_;
		std::mt19937 gen(12345);
		std::uniform_int_distribution<int> dist_q(0, q - 1);
		fill(spins, gen, dist_q, L);
		H = hamiltonian();
	}

	void print_state() const {
		for (const auto& x : spins) {
			for (const auto& y : x) {
				std::cout << std::setw(4) << y;
			}
			std::cout << std::endl;
		}
	}

	double hamiltonian() {
		double H = 0, m = 0;
		for (int i = 0; i < L; i++) {
			for (int j = 0; j < L; j++) {
				H += (int)(spins[i][j] == spins[(i + 1) % L][j]) +
					(int)(spins[i][j] == spins[(i - 1 + L) % L][j]) +
					(int)(spins[i][j] == spins[i][(j + 1) % L]) +
					(int)(spins[i][j] == spins[i][(j - 1 + L) % L]);
				m += spins[i][j];
			}
		}

		return -J * H / 2 - h * m;
	}

	inline double dE(int i, int j, int qnew) const {
		return (int)(spins[i][j] == spins[(i + 1) % L][j]) + (int)(spins[i][j] == spins[(i - 1 + L) % L][j]) + (int)(spins[i][j] == spins[i][(j + 1) % L]) + (int)(spins[i][j] == spins[i][(j - 1 + L) % L])
			- ((int)(qnew == spins[(i + 1) % L][j]) + (int)(qnew == spins[(i - 1 + L) % L][j]) + (int)(qnew == spins[i][(j + 1) % L]) + (int)(qnew == spins[i][(j - 1 + L) % L]));
	}

	inline void change_spin(int i, int j, int qnew) {
		H += dE(i, j, qnew);
		spins[i][j] = qnew;
	}

	inline double get_H() const {
		return H;
	}

private:
	std::vector<std::vector<int>> spins;
	double H;

	int fill(std::vector<std::vector<int>>& spins, std::mt19937& gen,
		std::uniform_int_distribution<>& dist, int L) {
		for (int i = 0; i < L; i++) {
			std::vector<int> x(L);
			for (int j = 0; j < L; j++) {
				x[j] = dist(gen);
			}
			spins.push_back(x);
		}
		return 0;
	}
};

double mean(std::vector<double>& v) {
	return std::accumulate(v.begin(), v.end(), 0.) / v.size();
}


class MCMC {
public:
	MCMC(Potts2D& model, long n = 1e5, long sk = 1e5) : gen(12345), dist_int(0, model.L - 1), dist_q(0, model.q - 1), dist_float(0., 1.) {
		N = n; skip = sk;
	}
	void metropolis(Potts2D& model, double& mE, double& C, double& acc, double& disp, double& err, double& err_r) {
		int nn = 1000;
		std::vector<double> E(N / nn);
		std::vector<double> E2(N / nn);

		std::vector<double> A(N / nn);
		int k = 0, j = 0;
		std::vector<double> ei(nn), ei2(nn), ai(nn);
		double  inc; double a0 = 0; double em, em2;
		acc = 0;
		for (long i = 0; i < skip; i++) {
			//ms_metropolis(model, em, em2);
			ms_heatbath(model, em, em2);
		}

		for (long i = 0; i < N; i++) {
			if (i % nn == 0 && i > 0) {
				A[k] = mean(ai);
				double mei = mean(ei);
				E[k] = mei;
				E2[k] = mean(ei2);
				k++;
				j = 0;

			}

			//inc = ms_metropolis(model, em, em2);
			inc = ms_heatbath(model, em, em2);
			ai[j] = inc;
			ei[j] = em;
			ei2[j] = em2;
			j++;

		}

		A[k] = mean(ai);
		E[k] = mean(ei);
		E2[k] = mean(ei2);

		mE = mean(E);
		C = (mean(E2) - mE * mE) / model.kT / model.kT;

		disp = (mean(E2) - mE * mE) / N / model.L / model.L;
		err = std::sqrt(disp);

		acc = mean(A);
		err_r = acc * (1 - acc) / N / model.L / model.L;
	}

private:
	std::mt19937 gen;
	std::uniform_int_distribution<int> dist_int, dist_q;
	std::uniform_real_distribution<> dist_float;
	long N; int skip;

	double ms_metropolis(Potts2D& model, double& mE, double& mE2) {
		double a = 0;
		std::vector<double> E;
		for (int k = 0; k < model.L* model.L; k++) {
			int i = dist_int(gen), j = dist_int(gen), qnew = dist_q(gen);
			double dE = model.dE(i, j, qnew);
			if (dist_float(gen) < std::exp(-dE / model.kT)) {
				model.change_spin(i, j, qnew);
				a++;
			}
			E.push_back(model.get_H());
		}
		mE = mean(E);
		mE2 = std::inner_product(E.begin(), E.end(), E.begin(), 0.) / E.size();
		return a / (model.L* model.L);
	}
	double ms_heatbath(Potts2D& model, double& mE, double& mE2) {
		double a = 0;
		std::vector<double> E;
		for (int k = 0; k < model.L* model.L; k++) {
			int i = dist_int(gen), j = dist_int(gen), qnew = dist_q(gen);
			double dE = model.dE(i, j, qnew);
			if (dist_float(gen) < std::exp(-dE / model.kT) / (std::exp(-dE / model.kT) + 1)) {
				model.change_spin(i, j, qnew);
				a++;
			}
			E.push_back(model.get_H());
		}
		mE = mean(E);
		mE2 = std::inner_product(E.begin(), E.end(), E.begin(), 0.) / E.size();
		return a/(model.L* model.L);
	}
};



int main()
{
	int L = 64, q = 4;

	double E, C, R, disp, err, err_r;
	std::map<double, std::vector<double>> results;
	std::vector<double> kTs = { 1 / std::log(1 + std::sqrt(q)), 0.2,0.718439667304252,0.8002772817581106,0.8143540869082877,0.8423745698168552,0.8796640335538309,0.9215477811892325,0.9633511157930776,1.0003993438064824,1.030020083813676,1.0572661305796758,1.0890530562972593,1.1322964331592018,1.1939118333582797,1.2811036436046184,1.4025555997823802,1.5702453284903384,1.8068803723085456,2.1390810947287595,2.5796230083528098,3.1086498808891214,3.6906199128275703,4.2992801142485,4.921052222713629,5.549587111973607,6.181810895022606,6.816152669119409,7.451778181550806,8.088189720657393,8.725138284149125,9.362445558235198,10.0 };
	std::vector<double> kTs = { 0.92375644,

	char fname[128];
	std::sprintf(fname, "potts %d heatbath T, E, C, R disp std L=%d sweeps term.csv", q, L);
	std::ofstream file(fname, std::ofstream::app);

	{
#pragma omp parallel for  num_threads(8) private(E,C,R, disp, err, err_r)  
		for (int i = 0; i < kTs.size(); i++) {
			double kT = kTs[i];
			std::cout << kT << std::endl;
			Potts2D model(q, L);
			long sweeps = 1e6, term = 1e5;

			MCMC simulation(model, sweeps, term);

			model.kT = kT;
			simulation.metropolis(model, E, C, R, disp, err, err_r);

			std::vector<double> data = { kT, E / L / L, C, R, disp, err, err_r, (double)sweeps, (double)term };

			results[kT] = data;
			#pragma omp critical
			{
				for (auto& y : data) {
					std::cout << ' ' << y;
					file << ' ' << std::setprecision(16) <<  y;
				}
				std::cout << std::endl;
				file << std::endl;
			}

		}
	}

	file.close();

	return 0;
}