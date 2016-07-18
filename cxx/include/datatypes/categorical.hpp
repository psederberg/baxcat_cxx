
// BaxCat: an extensible cross-catigorization engine.
// Copyright (C) 2014 Baxter Eaves
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, version 3.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License (LICENSE.txt) along with this
// program. If not, see <http://www.gnu.org/licenses/>.
//
// You may contact the mantainers of this software via github
// <https://github.com/BaxterEaves/baxcat_cxx>.

#ifndef baxcat_cxx_categorical_hpp
#define baxcat_cxx_categorical_hpp

#include <map>
#include <cmath>
#include <string>
#include <vector>
#include <sstream>

#include "utils.hpp"
#include "numerics.hpp"
#include "samplers/slice.hpp"
#include "distributions/gamma.hpp"

#include "component.hpp"
#include "models/msd.hpp"

namespace baxcat{
namespace datatypes{


// TODO: update to accept multiple integral types
class Categorical : public SubComponent<Categorical, size_t>{
public:
	Categorical(std::vector<double> &distargs) : _dirichlet_alpha(1)
	{
		_n = 0;
		_counts.resize(static_cast<size_t>(distargs[0]+.5), 0);
		_log_Z0 = 0;
	}

	Categorical(double n, std::vector<size_t> counts, double dirichlet_alpha)
	{
		_n = n;
		_counts = counts;
		_dirichlet_alpha = dirichlet_alpha;
		_log_Z0 = 0;
	}

	// cleanup
	virtual void insertElement(size_t x) override;
    virtual void removeElement(size_t x) override;
    virtual void clear(const std::vector<double> &distargs) override;

    // setters
    virtual void setHypers(std::vector<double> hypers) override;
    virtual void setHypersByMap(std::map<std::string, double> hypers_map) override;

    // getters
    virtual std::vector<double> getHypers() const override;
    virtual std::map<std::string, double> getHypersMap() const override;
    virtual std::map<std::string, double> getSuffstatsMap() const override;

    // probability
    virtual double logp() const override;
    virtual double elementLogp(size_t x) const override;
    virtual double singletonLogp(size_t x) const override;
	virtual double hyperpriorLogp(const std::vector<double> &hyperprior_config) const override;

    // draw
    virtual size_t draw(baxcat::PRNG *rng) const override;
    virtual size_t drawConstrained(std::vector<size_t> contsraints,
								   baxcat::PRNG *rng) const override;

    // hypers
    static std::vector<double> constructHyperpriorConfig(const std::vector<double> &X);
    static std::vector<double> initHypers(const std::vector<double> &hyperprior_config,
        baxcat::PRNG *rng);
    static std::vector<double> resampleHypers(std::vector<Categorical> &models,
        const std::vector<double> &hyperprior_config, baxcat::PRNG *rng, size_t burn=50);

    // construct hyper-parameter conditionals
    static std::function<double(double)> constructDirichletAlphaConditional(
        const std::vector<Categorical> &models, const std::vector<double> &hyperprior_config);

    // updates normalizing constants
    void updateConstants();

protected:
    // hyperparameter conditionals
    double hyperDirichletAlphaConditional_(double alpha) const;

private:
	baxcat::models::MultinomialDirichlet<size_t> _msd;

	// for indexing
	enum hyper_idx {HYPER_DIRICHLET_ALPHA=0};
	enum hyperprior_config {DIRICHLET_ALPHA_SCALE=0};

	double _log_Z0;

	// sufficient statistics
	std::vector<size_t> _counts;

	// hyperparameters
	double _dirichlet_alpha;
};


}} // end namespaces

#endif