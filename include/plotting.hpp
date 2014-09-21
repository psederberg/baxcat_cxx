
#ifndef baxcat_cxx_plotting_utils
#define baxcat_cxx_plotting_utils

#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <mgl2/mgl.h> // MathGL plotting

#include "utils.hpp"

namespace baxcat { namespace plotting{

	static void __buildHistogram(std::vector<double> X, std::vector<double> &counts,
								 std::vector<double> &edges, size_t n_bins)
	{
		double x_min = baxcat::utils::vector_min(X);
		double x_max = baxcat::utils::vector_max(X);
		edges = baxcat::utils::linspace(x_min, x_max, n_bins+1);

		counts.resize(n_bins);
		std::fill( counts.begin(), counts.end(), 0);

		for(auto &x : X){
			for(size_t i = 0; i < n_bins; ++i){
				if( x <= edges[i+1] && x >= edges[i]){
					++counts[i];
					break;
				}
			}
		}
	}


	static void hist(mglGraph *gr, std::vector<double> X, size_t n_bins, std::string title=" ")
	{
		std::vector<double> counts;
		std::vector<double> edges;

		__buildHistogram(X, counts, edges, n_bins);

		// convert vectors to mglData
		mglData edges_plt;
		double* edges_ptr = &edges[0];
		edges_plt.Set(edges_ptr, n_bins);

		mglData counts_plt;
		double* counts_ptr = &counts[0];
		counts_plt.Set(counts_ptr, n_bins);

		gr->Title(title.c_str());
		gr->SetRanges(edges_plt, counts_plt);
		gr->Axis();
		gr->Bars(edges_plt, counts_plt);
	}


	template <typename lambda>
	static void functionPlot(mglGraph *gr, std::vector<double> x, lambda f, std::string title=" ",
							 std::string x_label="x", std::string y_label="f(x)")
	{
		auto n = x.size();

		// get f(x)
		std::vector<double> fx(n,0);
		for(size_t i = 0; i < n; ++i)
			fx[i] = f(x[i]);

		mglData x_plt;
		double* x_ptr = &x[0];
		x_plt.Set(x_ptr, n);

		mglData fx_plt;
		double* fx_ptr = &fx[0];
		fx_plt.Set(fx_ptr, n);

		gr->Title(title.c_str());
		gr->SetRanges(x_plt, fx_plt);
		gr->Axis();
		gr->Plot(x_plt, fx_plt);

		gr->Label('x',x_label.c_str(),0);
		gr->Label('y',y_label.c_str(),0);
	}


	static void compPlot(mglGraph *gr, std::vector<double> x, std::vector<double> fx_a,
						 std::vector<double> fx_b, std::string title=" ", std::string x_label="x",
						 std::string y_label="f(x)")
	{
		auto n = x.size();

		mglData x_plt;
		double* x_ptr = &x[0];
		x_plt.Set(x_ptr, n);

		mglData fx_plt(x.size(),2);
		for(size_t i = 0; i < n; ++i){
			fx_plt.a[i]=fx_a[i];
			fx_plt.a[n+i]=fx_b[i];
		}

		gr->Title(title.c_str());
		gr->SetRanges(x_plt, fx_plt);
		gr->Axis();
		gr->Plot(x_plt, fx_plt);

		gr->Label('x',x_label.c_str(),0);
		gr->Label('y',y_label.c_str(),0);
	}


	static void pPPlot(mglGraph *gr, std::vector<double> cdf_1, std::vector<double> cdf_2,
					   std::string title, std::string x_label, std::string y_label)
	{
		auto n = cdf_1.size();
		// convert vectors to plot data
		mglData cdf_1_plt;
		double* cdf_1_ptr = &cdf_1[0];
		cdf_1_plt.Set(cdf_1_ptr, n);

		mglData cdf_2_plt;
		double* cdf_2_ptr = &cdf_2[0];
		cdf_2_plt.Set(cdf_2_ptr, n);

		gr->Title(title.c_str());
		gr->SetRanges(cdf_1_plt, cdf_2_plt);
		gr->Axis();

		// plot baseline
		gr->Plot(cdf_1_plt, cdf_1_plt, "r");
		gr->Plot(cdf_1_plt, cdf_2_plt, "k");
		gr->Label('x',x_label.c_str());
		gr->Label('y',y_label.c_str());
	}

}}

#endif
