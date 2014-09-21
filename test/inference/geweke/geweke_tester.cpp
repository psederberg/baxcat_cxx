
#include "geweke_tester.hpp"

using std::vector;
using std::string;
using std::map;

namespace baxcat {

GewekeTester::GewekeTester(size_t num_rows, size_t num_cols, unsigned int seed)
{
    _seeder.seed(seed);
    _num_cols = num_cols;

    vector<string> datatypes;
    for(size_t i = 0; i < num_cols; ++i)
        datatypes.push_back("continuous");

    SyntheticDataGenerator sdg(num_rows, datatypes, seed);
    _datatypes = datatypes;
    _seed_data = sdg.getData();
    _seed_args.resize(_num_cols);
    State temp_state(_seed_data, _datatypes, _seed_args, seed);

    _state =  State(_seed_data, _datatypes, _seed_args, seed);

    for(size_t i = 0; i < num_cols; ++i)
        _state.setHyperConfig(i, {0.0, .1, .25, .25});
    _state.__geweke_initHypers();
}

// GewekeTester::GewekeTester(size_t num_rows, svector<string> datatypes, size_t seed) : 
//     _seeder(PRNG(seed))
// {
//     SyntheticDataGenerator sdg(num_rows, datatypes, seed);
//     _seed_data = sdg.getData();   
// }

void GewekeTester::forwardSample(size_t num_times, bool do_init)
{
    std::uniform_int_distribution<unsigned int> urnd;
    if(do_init)
        __initStats( _state, _state_crp_alpha_forward, _all_stats_forward);

    for(size_t i = 0; i < num_times; ++i){
        if( !((i+1) % 100))
            printf("\rSample %zu of %zu        ", i+1, num_times); fflush(stdout);

        _state = State(_seed_data, _datatypes, _seed_args, urnd(_seeder));
        for(size_t i = 0; i < _datatypes.size(); ++i){
            auto type = _datatypes[i];
            if(type == "categorical"){
                _state.setHyperConfig(i, {1.0});
            }else if (type == "continuous"){
                _state.setHyperConfig(i, {0.0, .1, .25, .25});
            }else{
                std::cout << "invalid datatype" << std::endl;
                assert(false);
            }
        }
        _state.__geweke_initHypers();
        _state.__geweke_clear();
        _state.__geweke_resampleRows();
        __updateStats( _state, _state_crp_alpha_forward, _all_stats_forward);
    }
    printf("\n");
}

void GewekeTester::posteriorSample(size_t num_times, bool do_init, size_t lag)
{
    std::uniform_int_distribution<unsigned int> urnd;

    if(do_init)
        __initStats( _state, _state_crp_alpha_posterior, _all_stats_posterior);

    // forward sample
    _state = State(_seed_data, _datatypes, _seed_args, urnd(_seeder));
    for(size_t i = 0; i < _datatypes.size(); ++i){
        auto type = _datatypes[i];
        if(type == "categorical"){
            _state.setHyperConfig(i, {1.0});
        }else if (type == "continuous"){
            _state.setHyperConfig(i, {0.0, .1, .25, .25});
        }else{
            std::cout << "invalid datatype" << std::endl;
            assert(false);
        }
    }
    _state.__geweke_initHypers();
    _state.__geweke_clear();
    _state.__geweke_resampleRows();
    // do a bunch of posterior samples
    for(size_t i = 0; i < num_times; ++i){
        if( !((i+1) % 5))
            printf("\rSample %zu of %zu        ", i+1, num_times); fflush(stdout);

        for( size_t j = 0; j < lag; ++j ){
            _state.transition({},{},{},0,1);
            _state.__geweke_resampleRows();    
        }
        
        // auto x = _state.__geweke_pullDataColumn(0);
        // utils::print_vector(x);
        __updateStats( _state, _state_crp_alpha_posterior, _all_stats_posterior);
    }
    printf("\n");
}

// Helpers
//`````````````````````````````````````````````````````````````````````````````````````````````````
template <typename T>
vector<string> GewekeTester::__getMapKeys(map<string, T> map_in)
{
    vector<string> keys;
    for(auto imap : map_in)
        keys.push_back(imap.first);

    return keys;
}

template <typename T>
vector<double> GewekeTester::__getDataStats(const vector<T> &data, bool is_categorial)
{
    vector<double> stats;
    if(is_categorial){
        stats.push_back(test_utils::chi2Stat(data));
    }else{
        double mean = utils::vector_mean(data);
        double var = 0;
        for( auto &x : data )
            var += (mean-x)*(mean-x);
        stats.push_back(mean);
        stats.push_back(var);
    }
    return stats;
}

void GewekeTester::__updateStats( const State &state, vector<double> &state_crp_alpha,
    vector<map<string, vector<double>>> &all_stats)
{
    state_crp_alpha.push_back(state.getStateCRPAlpha());
    auto column_hypers = state.getColumnHypers();

    for(size_t i = 0; i < column_hypers.size(); ++i)
    {
        auto hyper_keys = GewekeTester::__getMapKeys( column_hypers[i] );
        string categorial_marker = "dirichlet_alpha";
        bool is_categorial = test_utils::hasElement(hyper_keys,categorial_marker) == 1;
        auto data = state.__geweke_pullDataColumn(i);

        auto data_stat = GewekeTester::__getDataStats( data, is_categorial);

        if(is_categorial){  
            all_stats[i]["chi-square"].push_back(data_stat[0]);
        }else{
            all_stats[i]["mean"].push_back(data_stat[0]);
            all_stats[i]["var"].push_back(data_stat[1]);
        }

        for(auto &hyper_key : hyper_keys)
            all_stats[i][hyper_key].push_back(column_hypers[i][hyper_key]);
    }
}

void GewekeTester::__initStats( const State &state, vector<double> &state_crp_alpha,
    vector<map<string, vector<double>>> &all_stats)
{
    state_crp_alpha = {};
    auto column_hypers = state.getColumnHypers();

    all_stats.resize(column_hypers.size());

    for(size_t i = 0; i < column_hypers.size(); ++i)
    {
        auto hyper_keys = GewekeTester::__getMapKeys( column_hypers[i] );
        string categorial_marker = "dirichlet_alpha";
        bool is_categorial = test_utils::hasElement(hyper_keys,categorial_marker) == 1;

        if(is_categorial){  
            all_stats[i]["chi-square"] = {};
        }else{
            all_stats[i]["mean"] = {};
            all_stats[i]["var"] = {};
        }

        for(auto &hyper_key : hyper_keys)
            all_stats[i][hyper_key] = {};
    }
}

void GewekeTester::run(size_t num_times, size_t num_posterior_chains, size_t lag=25)
{
    assert( lag >= 1 );

    size_t samples_per_chain = num_times/num_posterior_chains;
    std::cout << "Running forward samples" << std::endl;
    forwardSample(num_times, true);

    std::cout << "Running posterior samples (1 of " << num_posterior_chains << ")"  << std::endl;
    posteriorSample(samples_per_chain, true, lag);
    for( size_t i = 0; i < num_posterior_chains-1; ++i){
        std::cout << "Running posterior samples (" << i+2 << " of "; 
        std::cout << num_posterior_chains << ")"  << std::endl;
        posteriorSample(samples_per_chain, false, lag);
    }
    std::cout << "done." << std::endl;
}

// Test results output and plotting
//`````````````````````````````````````````````````````````````````````````````````````````````````
void GewekeTester::outputResults()
{
    size_t num_pass = 0;
    size_t num_fail = 0;
    bool all_pass = true;

    for( size_t i = 0; i < _num_cols; ++i){
        // get stat keys
        std::cout << "COLUMN " << i << std::endl;
        auto keys = GewekeTester::__getMapKeys(_all_stats_forward[i]);
        mglGraph gr;
        int plots_y = 3;
        int plots_x = keys.size();

        std::stringstream filename;
        filename << "results/column_" << i << ".png";
        gr.SetSize(500*plots_x,500*plots_y);

        int index = 0;
        for( auto key : keys ){
            int pp_plot_index = index;
            int forward_hist_index = index + plots_x;
            int posterior_hist_index = index + 2*plots_x;

            std::stringstream test_name;
            test_name << "column " << i << " " << key;

            std::stringstream ss;
            ss << "ks-test column " << i << " [" << key << "]";

            size_t n_forward = _all_stats_forward[i][key].size();
            size_t n_posterior = _all_stats_posterior[i][key].size();

            gr.SubPlot(plots_x, plots_y, pp_plot_index);
            auto ks_stat = test_utils::twoSampleKSTest(_all_stats_forward[i][key], 
                _all_stats_posterior[i][key], true, &gr, test_name.str());

            
            bool distributions_differ = test_utils::ksTestRejectNull(ks_stat, n_forward, n_posterior);
            test_utils::__output_ks_test_result(distributions_differ, ks_stat, ss.str());
            test_utils::__update_pass_counters(num_pass, num_fail, all_pass, !distributions_differ);

            gr.SubPlot(plots_x, plots_y, forward_hist_index);
            baxcat::plotting::hist(&gr, _all_stats_forward[i][key], 31, "forward");

            gr.SubPlot(plots_x, plots_y, posterior_hist_index);
            baxcat::plotting::hist(&gr, _all_stats_posterior[i][key], 31, "posterior");

            index++; 

        }
        gr.WriteFrame(filename.str().c_str());
    }

    std::stringstream ss;
    ss << "ks-test [state alpha]";
    size_t n_forward = _state_crp_alpha_forward.size();
    size_t n_posterior = _state_crp_alpha_posterior.size();
    auto ks_stat = test_utils::twoSampleKSTest(_state_crp_alpha_forward, _state_crp_alpha_posterior);
    bool distributions_differ = test_utils::ksTestRejectNull(ks_stat, n_forward, n_posterior);
    test_utils::__output_ks_test_result(distributions_differ, ks_stat, ss.str());
    test_utils::__update_pass_counters(num_pass, num_fail, all_pass, !distributions_differ);

    if(all_pass){
        std::cout << "**No failures detected." << std::endl;
    }else{
        std::cout << "**" << num_fail << " failures." << std::endl;
    }
}

} // end namespace