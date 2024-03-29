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

class XY1D {
public:

	int L;
	double kT = 1;
	const double J = 1;
	const int  K = 1, h = 0;

	XY1D(int len = 4) {
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
			H += std::cos(spins[i] - spins[(i + 1) % L]);
			m += std::cos(spins[i]);
		}
		return -J * H - h * m;
	}

	inline double dE(int i, double news) const {
		return J * (std::cos(spins[i] - spins[(i + 1) % L]) + std::cos(spins[i] - spins[(i - 1 + L) % L])) -
			J * (std::cos(news - spins[(i + 1) % L]) + std::cos(news - spins[(i - 1 + L) % L]));
	}

	inline void change_spin(int i, double news) {
		H += dE(i, news);
		spins[i] = news;
	}

	inline double get_H() const {
		return H;
	}

private:
	std::vector<double> spins;

	double H;

	int fill(std::vector<double>& spins, std::mt19937& gen,
		std::uniform_real_distribution<>& dist, int L) {
		for (int i = 0; i < L; i++) {
			spins.push_back((dist(gen)*2 - 1)*M_PI);
		}
		return 0;
	}
};

double mean(std::vector<double>& v) {
	return std::accumulate(v.begin(), v.end(), 0.) / v.size();
}


class MCMC {
public:
	MCMC(XY1D& model, long n = 1e5, long sk = 1e5) : gen(12345), dist_int(0, model.L - 1), dist_float(0., 1.) {
		N = n; skip = sk;
	}
	void metropolis(XY1D& model, double& mE, double& C, double& acc, double& disp, double& err, double& err_r) {
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
			//ms_metropolis(model, em, em2);
			ms_heatbath(model, em, em2);
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

			//inc = ms_metropolis(model, em, em2);
			inc = ms_heatbath(model, em, em2);
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
	long N; int skip;

	int ms_metropolis(XY1D& model, double& em, double& em2) {
		int a = 0;
		std::vector<double> e;
		for (int k = 0; k < model.L; k++) {
			int i = dist_int(gen);
			double news = (dist_float(gen)-0.5)*2*M_PI;
			double dE = model.dE(i, news);
			if (dist_float(gen) < std::exp(-dE / model.kT)) {
				model.change_spin(i, news);
				a++;
			}
			e.push_back(model.get_H());
		}
		em = mean(e);
		em2 = std::inner_product(e.begin(), e.end(), e.begin(), 0.) / e.size();
		return a;
	}
	int ms_heatbath(XY1D& model, double& em, double& em2) {
		int a = 0;
		std::vector<double> e;
		for (int k = 0; k < model.L; k++) {
			int i = dist_int(gen);
			double news = (dist_float(gen) - 0.5) * 2 * M_PI;
			double dE = model.dE(i, news);
			if (dist_float(gen) < std::exp(-dE / model.kT) / (std::exp(-dE / model.kT) + 1)) {
				model.change_spin(i, news);
				a++;
			}
			e.push_back(model.get_H());
		}
		em = mean(e);
		em2 = std::inner_product(e.begin(), e.end(), e.begin(), 0.) / e.size();
		return a;
	}
	int sweep_glauber(XY1D& model) {
		int a = 0;
		for (int k = 0; k < model.L; k++) {
			int i = dist_int(gen), j = dist_int(gen);
			double news = (dist_float(gen) - 0.5) * 2 * M_PI;
			double dE = model.dE(i, news);
			if (dist_float(gen) < 0.5 * (1 - std::tanh(dE / model.kT / 2))) {
				model.change_spin(i, news);
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
	std::vector<double> kTs = { 0.2,0.27973892337906686,0.3542161062539198,0.4262541957862526,0.4986758391377597,0.574303683470135,0.655979484284857,0.746760120912593,0.8498579708150342,0.9690274593727599,1.1089298847628921,1.2769037845585671,1.4817270097634114,1.7323386172095512,2.0352387638082043,2.3916944832237914,2.7930317526326185,3.2270908688839155,3.682428422896675,4.150918259146376,4.627474642196468,5.108945360793799,5.593577159518168,6.080180156192258,6.5682242071629435,7.057180992718406,7.5467740867711255,8.036868429191399,8.527310559493127,9.018029527608274,9.508935571042185,10.0 };

	std::ofstream file("heatbath XY T, E, C, R disp std L=512 sweeps term.csv");
	{
#pragma omp parallel for  num_threads(8) private(E,C,R, disp, err, err_r)  
		for (int i = 0; i < kTs.size(); i++) {
			double kT = kTs[i];
			std::cout << kT << std::endl;
			XY1D model(L);
			long sweeps = 1e5, term = 1e4;

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