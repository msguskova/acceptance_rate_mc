#include <iostream>
#include <omp.h>
#include <iterator>
#include <algorithm>
#include <vector>
#include <random>
#include <iomanip>
#include <cmath>
#include <functional>
#include <numeric>
#include <fstream>
#include <map>


class Ising2D {
public:

    int L;
    double kT = 1;
    const int J = 1, K = 1, h = 0;

    Ising2D(int len=4) {
        L = len;
        std::mt19937 gen(12345);
        std::uniform_real_distribution<> dist(0., 1.);
        fill(spins, gen, dist, L);
        H = hamiltonian();
    }

	Ising2D(int len, int state) {
		L = len;
		std::mt19937 gen(state);
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
                H += spins[i][j]*(spins[(i + 1) % L][j] + spins[(i - 1 + L) % L][j] +
                    spins[i][(j + 1) % L] +  spins[i][(j - 1 + L) % L]);
                m += spins[i][j];
            }
        }

        return - J * H / 2 - h * m;
    }

    inline double dE(int i, int j) const {
        return 2 * J * spins[i][j] *(spins[(i + 1) % L][j] + spins[(i - 1 + L) % L][j]
            + spins[i][(j + 1) % L] + spins[i][(j - 1 + L) % L] ) ;
    }

    inline void change_spin(int i, int j) {
        H += dE(i, j);
        spins[i][j] *= -1;
    }

    inline double get_H() const {
        return H;
    }

private:
    std::vector<std::vector<int>> spins;
    double H;

    int fill(std::vector<std::vector<int>>& spins, std::mt19937& gen, 
            std::uniform_real_distribution<>& dist, int L) {
        for (int i = 0; i < L; i++) {
            std::vector<int> x(L);
            for (int j = 0; j < L; j++) {
                x[j] = (dist(gen) - 0.5) > 0 ? 1 : -1;
            }
            spins.push_back(x);
        }
        return 0;
    }
};

double mean(const std::vector<double>& v) {
    return std::accumulate(v.begin(), v.end(), 0.)/ v.size();
}


class MCMC {
public:
    MCMC(Ising2D& model, long n = 1e5, long sk=1e5) : gen(12345), dist_int(0, model.L-1), dist_float(0., 1.) {
        N = n; skip = sk;
    }
    void metropolis(Ising2D& model, double& mE, double& C, double& acc, double& disp, double& err, double& err_r) {
        int nn = 1000;
		//nn = 1;
        std::vector<double> E(N / nn);
        std::vector<double> E2(N / nn);

        std::vector<double> A(N / nn);
        int k = 0, j=0;
        std::vector<double> ei(nn), ei2(nn), ai(nn);
        double  inc; double a0 = 0; double em, em2;
        acc = 0;
        for (long i = 0; i < skip; i++) {
            ms_metropolis(model, em, em2);
            //ms_heatbath(model, em, em2);
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

            inc = ms_metropolis(model, em, em2);
            //inc = ms_heatbath(model, em, em2);
			ai[j] = inc;
            ei[j] = em;
			ei2[j] = em2;
            j++;
			
        }

        
		A[k] = mean(ai);
        double mei = mean(ei);
        E[k] = mei;
		E2[k] = mean(ei2);

        mE = mean(E);
		C = (mean(E2) - mE * mE) / model.kT / model.kT;
		disp = ( mean(E2) - mE * mE ) / N / model.L / model.L;
		err = std::sqrt(disp);
		
        acc = mean(A);
		err_r = acc * (1 - acc) / N / model.L / model.L;

    }

private:
    std::mt19937 gen;
    std::uniform_int_distribution<int> dist_int;
    std::uniform_real_distribution<> dist_float;
    long N; int skip;

	double ms_metropolis(Ising2D& model, double& mE, double& mE2) {
		double a = 0.;

		std::vector<double> E;
		for (int k = 0; k < model.L* model.L; k++) {
			int i = dist_int(gen), j = dist_int(gen);
			double dE = model.dE(i, j);
			
			if (dist_float(gen) < std::exp(-dE / model.kT)) {
				model.change_spin(i, j);
				a++;

			}
			E.push_back(model.get_H());
		}
		mE = mean(E);
		mE2 = std::inner_product(E.begin(), E.end(), E.begin(), 0.) / E.size();
		return a/model.L/model.L;
	}

    double ms_heatbath(Ising2D& model, double& mE, double& mE2 ) {
        double a = 0.;
		
        std::vector<double> E;
        for (int k = 0; k < model.L* model.L; k++) {
            int i = dist_int(gen), j = dist_int(gen);
            double dE = model.dE(i, j);
            if (dist_float(gen) < std::exp(-dE / model.kT) / (std::exp(-dE / model.kT) + 1)) {
                model.change_spin(i, j);
                a++;
				
            }
            E.push_back(model.get_H());
        }
        mE = mean(E);
		mE2 = std::inner_product(E.begin(), E.end(), E.begin(), 0.) / E.size();
        return a / model.L / model.L;
    }
};



int main()
{
    int L = 64;

    double E, C, R, disp, err, err_r; 
    std::map<double, std::vector<double>> results;
	std::vector<double> kTs = { 2.26918, 0.5,1.1415798704654652,1.6184384162702947,1.8583614591856508,1.959267693821665,2.033267245650485,
		2.1187962586312965,2.18275652897005,2.23091839870944,2.2712792193602525,2.3097941260703485,2.3524893599192915,2.4055935914410735,
		2.4737609476195157,2.5573926374049534,2.6621335444039147,2.7880241696267607,2.938982931297895,3.120710055233258,3.339351032875764,
		3.60858541852433,3.9349465962186843,4.328633470936952,4.790359645539459,5.3196231355538925,5.907115609575584,6.537460113675752,
		7.198168465786738,7.882420637417465,8.578098430412634,9.28636833422815,10.0,
		2.23092   , 2.24069106, 2.25005205, 2.25834808, 2.26492427,
		2.26912574, 2.27291859, 2.28286481, 2.29641979, 2.30979376,
		2.32002391, 2.32745209, 2.33324555, 2.33857152, 2.34459726,
		2.35249 };

	char fname[256];


	std::sprintf(fname, "metropolis T, E, C, R disp std L = %d sweeps term.csv", L);
	std::ofstream file(fname, std::ofstream::app);

	#pragma omp parallel for  num_threads(8) private(E,C,R, disp, err)  
	for (int i = 0; i < kTs.size(); i++) {
		double kT = kTs[i];
		std::cout << kT << std::endl;
		Ising2D model(L);
		long sweeps = 2e6, term = 1e5;
		MCMC simulation(model, sweeps, term);

		model.kT = kT;
		simulation.metropolis(model, E, C, R, disp, err, err_r);

		std::vector<double> data = { kT, E / L / L, C, R, disp, err, err_r, (double)sweeps, (double)term };

		results[kT] = data;
		#pragma omp critical
		{
			std::cout << std::setw(10) << ' ' << kT << ' ' << E / L / L << ' ' << C << ' ' << R << ' ' << disp << ' ' << err << '\n';
			for (auto& y : data) {
				file << ' ' << std::setprecision(16)<< y;
			}
			file << std::endl;
		}
	}

		file.close();
	

    return 0;
}