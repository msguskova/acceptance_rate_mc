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

#define M_PI           3.14159265358979323846

class XY2D {
public:

	int L;
	double kT = 1;
	const int J = 1, K = 1, h = 0;

	XY2D(int len = 4) {
		L = len;
		std::mt19937 gen(12345);
		std::uniform_real_distribution<> dist(0., 1.);
		fill(spins, gen, dist, L);
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
				H += std::cos(spins[i][j] - spins[(i+1)%L][j]) + 
					std::cos(spins[i][j] - spins[i][(j+1)%L]) ;
				m += std::cos(spins[i][j]);
			}
		}

		return -J * H  - h * m;
	}

	inline double dE(int i, int j, double news) const {
		return J * (std::cos(spins[i][j] - spins[(i + 1) % L][j])
			+ std::cos(spins[i][j] - spins[i][(j + 1) % L]) 
			+ std::cos(spins[i][j] - spins[(i - 1 + L) % L][j]) 
			+ std::cos(spins[i][j] - spins[i][(j - 1 + L) % L])
			- std::cos(news - spins[(i + 1) % L][j])
			- std::cos(news - spins[i][(j + 1) % L])
			- std::cos(news - spins[(i - 1 + L) % L][j])
			- std::cos(news - spins[i][(j - 1 + L) % L]));
	}

	inline void change_spin(int i, int j, double news) {
		H += dE(i, j, news);
		spins[i][j] = news;
	}

	inline double get_H() const {
		return H;
	}

private:
	std::vector<std::vector<double>> spins;
	double H;

	int fill(std::vector<std::vector<double>>& spins, std::mt19937& gen,
		std::uniform_real_distribution<>& dist, int L) {
		for (int i = 0; i < L; i++) {
			std::vector<double> x(L);
			for (int j = 0; j < L; j++) {
				x[j] = (dist(gen)*2 - 1) * M_PI;
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
	MCMC(XY2D& model, long n = 1e5, long sk = 1e5) : gen(12345), dist_int(0, model.L - 1), dist_float(0., 1.) {
		N = n; skip = sk;
	}
	void metropolis(XY2D& model, double& mE, double& C, double& acc, double& disp, double& err, double& err_r) {
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

				E[k] = mean(ei);
				E2[k] = mean(ei2);
				k++;
				j = 0;
			}

			//inc = ms_metropolis(model, em, em2);
			inc = ms_heatbath(model, em, em2);
			ei[j] = em;
			ei2[j] = em2;
			ai[j] = inc;
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
	std::uniform_int_distribution<int> dist_int;
	std::uniform_real_distribution<> dist_float;
	long N; int skip;

	double ms_metropolis(XY2D& model, double& mE, double& mE2) {
		int a = 0;
		std::vector<double> E;
		for (int k = 0; k < model.L* model.L; k++) {
			int i = dist_int(gen), j = dist_int(gen);
			double news = (dist_float(gen) - 0.5) * 2 * M_PI;
			double dE = model.dE(i, j, news);
			if (dist_float(gen) < std::exp(-dE / model.kT)) {
				model.change_spin(i, j, news);
				a++;
			}
			E.push_back(model.get_H());
		}
		mE = mean(E);
		mE2 = std::inner_product(E.begin(), E.end(), E.begin(), 0.) / E.size();
		return (double)a / model.L / model.L;
	}
	double ms_heatbath(XY2D& model, double& mE, double& mE2) {
		int a = 0;
		std::vector<double> E;
		for (int k = 0; k < model.L* model.L; k++) {
			int i = dist_int(gen), j = dist_int(gen);
			double news = (dist_float(gen) - 0.5) * 2 * M_PI;
			double dE = model.dE(i, j, news);
			if (dist_float(gen) < std::exp(-dE / model.kT) / (std::exp(-dE / model.kT) + 1)) {
				model.change_spin(i, j, news);
				a++;
			}
			E.push_back(model.get_H());
		}
		mE = mean(E);
		mE2 = std::inner_product(E.begin(), E.end(), E.begin(), 0.) / E.size();
		return (double)a / model.L / model.L;
	}
	int sweep_glauber(XY2D& model) {
		int a = 0;
		for (int k = 0; k < model.L* model.L; k++) {
			int i = dist_int(gen), j = dist_int(gen);
			double news = (dist_float(gen) - 0.5) * 2 * M_PI;
			double dE = model.dE(i, j, news);
			if (dist_float(gen) < 0.5 * (1 - std::tanh(dE / model.kT / 2))) {
				model.change_spin(i, j, news);
				a++;
			}
		}
		return a;
	}
};



int main()
{
	int L = 64;

	double E, C, R, disp, err, err_r;
	std::map<double, std::vector<double>> results;
	std::vector<double> kTs = { 0.887, 0.2,0.33636984077555465,0.46809109478523914,0.5921095633589053,0.7053275099179347,0.8044346714124541,0.8874235427256947,0.9570173193630291,1.0169650458524235,1.0713079890165684,1.1245880910397852,1.181396469036179,1.246186015110997,1.3219676958625213,1.4108961124675847,1.5161690435004485,1.6440097614769658,1.8024735643841485,2.0026387588153187,2.2601621043169917,2.5945155403816096,3.0226371936669256,3.5501689303268344,4.16213740329426,4.832745669924188,5.538592163091343,6.2646451280981585,7.002268075032056,7.746781292483651,8.49551492550639,9.246873022238578,10.0 };

	char fname[128];
	std::sprintf(fname, "heatbath XY 2D T, E, C, R disp std L=%d sweeps term.csv", L);
	std::ofstream file(fname);

	{
#pragma omp parallel for  num_threads(9) private(E,C,R, disp, err, err_r)  
		for (int i = 0; i < kTs.size(); i++) {
			double kT = kTs[i];
			std::cout << kT << std::endl;
			XY2D model(L);
			long sweeps = 1e6, term = 1e5;

			MCMC simulation(model, sweeps, term);

			model.kT = kT;
			simulation.metropolis(model, E, C, R, disp, err, err_r);

			std::cout << std::setw(10) << ' ' << kT << ' ' << E / L / L << ' ' << C << ' ' << R << ' ' << disp << ' ' << err << '\n';
			std::vector<double> data = { kT, E / L / L, C, R, disp, err, err_r, (double)sweeps, (double)term };

			results[kT] = data;
			#pragma omp critical
			{
				for (auto& y : data) {
					file << ' ' << std::setprecision(16) << y;
				}
				file << std::endl;
			}

		}
	}

	file.close();

	return 0;
}