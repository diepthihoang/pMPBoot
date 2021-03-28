/***************************************************************************
 *   Copyright (C) 2009-2015 by                                            *
 *   BUI Quang Minh <minh.bui@univie.ac.at>                                *
 *   Lam-Tung Nguyen <nltung@gmail.com>                                    *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/



#include "tools.h"
#include "starttree.h" //for START_TREE_RECOGNIZED macro.
#include "stringfunctions.h"
#include "timeutil.h"
#include "MPIHelper.h"
#if !defined(CLANG_UNDER_VS) && !defined(_MSC_VER)
    #include <dirent.h>
#else
     //James B. Workaround for Windows builds where these macros might not be defined
    #ifndef S_ISDIR
    #define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
    #endif
    #ifndef S_ISREG
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
    #endif
#endif
#include <thread>


#if defined(Backtrace_FOUND)
#include <execinfo.h>
#include <cxxabi.h>
#endif

#include "tools.h"
#include "timeutil.h"
#include "progress.h"
#include "gzstream.h"
#include "MPIHelper.h"
#include "alignment/alignment.h"

VerboseMode verbose_mode;
extern void printCopyright(ostream &out);

#if defined(WIN32) ||defined(WIN64)
#include <sstream>
#endif

/********************************************************
        Miscellaneous
 ********************************************************/

/**
        Output an error to screen, then exit program
        @param error error message
 */
void outError(const char *error, bool quit) {
	if (error == ERR_NO_MEMORY) {
        print_stacktrace(cerr);
	}
	cerr << error << endl;
    if (quit)
    	exit(2);
}

/**
        Output an error to screen, then exit program
        @param error error message
 */
void outError(string error, bool quit) {
    outError(error.c_str(), quit);
}

void outError(const char *error, const char *msg, bool quit) {
    string str = error;
    str += msg;
    outError(str, quit);
}

void outError(const char *error, string msg, bool quit) {
    string str = error;
    str += msg;
    outError(str, quit);
}

/**
        Output a warning message to screen
        @param error warning message
 */
void outWarning(const char *warn) {
    cout << "WARNING: " << warn << endl;
}

void outWarning(string warn) {
    outWarning(warn.c_str());
}

double randomLen(Params &params) {
    double ran = static_cast<double> (random_int(999) + 1) / 1000;
    double len = -params.mean_len * log(ran);

    if (len < params.min_len) {
        int fac = random_int(1000);
        double delta = static_cast<double> (fac) / 1000.0; //delta < 1.0
        len = params.min_len + delta / 1000.0;
    }

    if (len > params.max_len) {
        int fac = random_int(1000);
        double delta = static_cast<double> (fac) / 1000.0; //delta < 1.0
        len = params.max_len - delta / 1000.0;
    }
    return len;
}



//From Tung

bool copyFile(const char SRC[], const char DEST[]) {
    std::ifstream src; // the source file
    std::ofstream dest; // the destination file

    src.open(SRC, std::ios::binary); // open in binary to prevent jargon at the end of the buffer
    dest.open(DEST, std::ios::binary); // same again, binary
    if (!src.is_open() || !dest.is_open())
        return false; // could not be copied

    dest << src.rdbuf(); // copy the content
    dest.close(); // close destination file
    src.close(); // close source file

    return true; // file copied successfully
}

bool fileExists(string strFilename) {
    struct stat stFileInfo;
    bool blnReturn;
    int intStat;

    // Attempt to get the file attributes
    intStat = stat(strFilename.c_str(), &stFileInfo);
    if (intStat == 0) {
        // We were able to get the file attributes
        // so the file obviously exists.
        blnReturn = true;
    } else {
        // We were not able to get the file attributes.
        // This may mean that we don't have permission to
        // access the folder which contains this file. If you
        // need to do that level of checking, lookup the
        // return values of stat which will give you
        // more details on why stat failed.
        blnReturn = false;
    }
    return (blnReturn);
}

int isDirectory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISDIR(statbuf.st_mode);
}

int isFile(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISREG(statbuf.st_mode);
}

size_t getFilesInDir(const char *path, StrVector &filenames)
{
    if (!isDirectory(path)) {
        return 0;
    }
    string path_name = path;
    if (path_name.back() != '/') {
        path_name.append("/");
    }
    size_t oldCount = filenames.size();
#if !defined(CLANG_UNDER_VS) && !defined(_MSC_VER)
    DIR* dp = opendir (path);
    if (dp == nullptr) {
        return 0;
    }
    struct dirent* ep;
    while ((ep = readdir (dp)) != NULL) {
        if (isFile((path_name + ep->d_name).c_str()))
            filenames.push_back(ep->d_name);
    }
    (void) closedir (dp);
    
#else
    path_name += "*";
    WIN32_FIND_DATA find_data;
    HANDLE search_handle = FindFirstFile(path_name.c_str(), &find_data);
    if (search_handle == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            filenames.emplace_back(find_data.cFileName);
        }
    } while (FindNextFile(search_handle, &find_data));
    FindClose(search_handle);
#endif
    return filenames.size() - oldCount;
}

bool renameString(std::string &name) {
    bool renamed = false;
    for (auto i = name.begin(); i != name.end(); ++i) {
        if ((*i) == '/') {
            // PLL does not accept '/' in names, turn it off
            if (Params::getInstance().start_tree == STT_PLL_PARSIMONY)
                Params::getInstance().start_tree = STT_PARSIMONY;
        }
        if (!isalnum(*i) && (*i) != '_' && (*i) != '-' && (*i) != '.' && (*i) != '|' && (*i) != '/') {
            (*i) = '_';
            renamed = true;
        }
    }
    return renamed;
}


void readWeightFile(Params &params, int ntaxa, double &scale, StrVector &tax_name, DoubleVector &tax_weight) {
    cout << "Reading scale factor and taxa weights file " << params.param_file << " ..." << endl;
    try {
        ifstream in;
        in.exceptions(ios::failbit | ios::badbit);
        in.open(params.param_file);
        string name, tmp;

        in >> tmp;
        scale = convert_double(tmp.c_str());

        for (; !in.eof() && ntaxa > 0; ntaxa--) {
            // remove the failbit
            in.exceptions(ios::badbit);
            if (!(in >> name)) break;
            // set the failbit again
            in.exceptions(ios::failbit | ios::badbit);

            tax_name.push_back(name);
            // read the sequence weight
            in >> tmp;
            tax_weight.push_back(convert_double(tmp.c_str()));
        }
        in.clear();
        // set the failbit again
        in.exceptions(ios::failbit | ios::badbit);
        in.close();
    } catch (ios::failure) {
        outError(ERR_READ_INPUT);
    } catch (string str) {
        outError(str);
    }
}

void readStringFile(const char* filename, int max_num, StrVector &strv) {
    try {
        ifstream in;
        // set the failbit and badbit
        in.exceptions(ios::failbit | ios::badbit);
        in.open(filename);
        string name;

        // remove the failbit
        in.exceptions(ios::badbit);
        for (; !in.eof() && max_num > 0; max_num--) {
            if (!(in >> name)) break;
            strv.push_back(name);
        }
        in.clear();
        // set the failbit again
        in.exceptions(ios::failbit | ios::badbit);
        in.close();
    } catch (ios::failure) {
        outError(ERR_READ_INPUT);
    }
}

void readInitTaxaFile(Params &params, int ntaxa, StrVector &tax_name) {
    cout << "Reading initial taxa set file " << params.initial_file << " ..." << endl;
    readStringFile(params.initial_file, ntaxa, tax_name);
}

void printString2File(string myString, string filename) {
    ofstream myfile(filename.c_str());
    if (myfile.is_open()) {
        myfile << myString;
        myfile.close();
    } else {
        cout << "Unable to open file " << filename << endl;
    }
}

void readInitAreaFile(Params &params, int nareas, StrVector &area_name) {
    cout << "Reading initial area file " << params.initial_area_file << " ..." << endl;
    readStringFile(params.initial_area_file, nareas, area_name);
}

void readAreasBoundary(char *file_name, MSetsBlock *areas, double *areas_boundary) {

    try {
        ifstream in;
        in.exceptions(ios::failbit | ios::badbit);
        in.open(file_name);

        int nset;
        in >> nset;
        if (nset != areas->getNSets())
            throw "File has different number of areas";
        int pos = 0, seq1, seq2;
        for (seq1 = 0; seq1 < nset; ++seq1) {
            string seq_name;
            in >> seq_name;
            if (seq_name != areas->getSet(seq1)->name)
                throw "Area name " + seq_name + " is different from " + areas->getSet(seq1)->name;
            for (seq2 = 0; seq2 < nset; ++seq2) {
                in >> areas_boundary[pos++];
            }
        }
        // check for symmetric matrix
        for (seq1 = 0; seq1 < nset - 1; ++seq1) {
            if (areas_boundary[seq1 * nset + seq1] <= 1e-6)
                throw "Diagonal elements of distance matrix should represent the boundary of single areas";
            for (seq2 = seq1 + 1; seq2 < nset; ++seq2)
                if (areas_boundary[seq1 * nset + seq2] != areas_boundary[seq2 * nset + seq1])
                    throw "Shared boundary between " + areas->getSet(seq1)->name + " and " + areas->getSet(seq2)->name + " is not symmetric";
        }


        in.close();
        cout << "Areas relation matrix was read from " << file_name << endl;
    } catch (const char *str) {
        outError(str);
    } catch (string str) {
        outError(str);
    } catch (ios::failure) {
        outError(ERR_READ_INPUT, file_name);
    }
}

void readTaxaSets(char *filename, MSetsBlock *sets) {
    TaxaSetNameVector *allsets = sets->getSets();
    try {
        int count = 0;
        ifstream in;
        // set the failbit and badbit
        in.exceptions(ios::failbit | ios::badbit);
        in.open(filename);
        string name;

        // remove the failbit
        in.exceptions(ios::badbit);
        while (!in.eof()) {
            int ntaxa = 0;
            string str_taxa;
            if (!(in >> str_taxa)) break;
            ntaxa = convert_int(str_taxa.c_str());
            if (ntaxa <= 0) throw "Number of taxa must be > 0";
            ++count;
            //allsets->resize(allsets->size()+1);
            TaxaSetName *myset = new TaxaSetName;
            allsets->push_back(myset);
            myset->name = "";
            myset->name += count;
            for (; ntaxa > 0; ntaxa--) {
                string str;
                if (!(in >> str)) throw "Cannot read in taxon name";
                if ((ntaxa > 1) && in.eof()) throw "Unexpected end of file while reading taxon names";
                myset->taxlist.push_back(str);
            }
        }
        in.clear();
        // set the failbit again
        in.exceptions(ios::failbit | ios::badbit);
        in.close();
        if (count == 0) throw "No set found, you must specify at least 1 set";
    } catch (ios::failure) {
        outError(ERR_READ_INPUT);
    } catch (const char *str) {
        outError(str);
    } catch (string str) {
        outError(str);
    }
}

void get2RandNumb(const int size, int &first, int &second) {
    // pick a random element
    first = random_int(size);
    // pick a random element from what's left (there is one fewer to choose from)...
    second = random_int(size - 1);
    // ...and adjust second choice to take into account the first choice
    if (second >= first) {
        ++second;
    }
}

void quickStartGuide();

namespace {

    template <class V, class S> void throw_if_not_in_set
    ( const char* name, const V& value, S set, size_t setCount ) {
        for ( size_t i=0; i<setCount; ++i ) {
            if (value == set[i]) {
                return;
            }
        }
        std::string complaint = std::string(name) + " was " + value + " but must be one of ";
        for ( size_t i=0; i<setCount; ++i ) {
            complaint += (0<i) ? " , " : "";
            complaint += set[i];
        }
        throw complaint;
    }

    int strip_number_suffix(std::string &stripMe, int defaultValue) {
        size_t c = stripMe.length();
        while (0<c && '0'<=stripMe[c-1] && stripMe[c-1]<='9') {
            --c;
        }
        if (c==stripMe.length()) {
            return defaultValue;
        }
        int rv = convert_int( stripMe.c_str() + c );
        stripMe = stripMe.substr(0, c);
        return rv;
    }

    bool parseTreeName(const char* name, Params& params) {
        std::string tree_name = name;
        if (tree_name == "PARS")
            params.start_tree = STT_PARSIMONY;
        else if (tree_name == "PJ")
            params.start_tree = STT_PARSIMONY_JOINING;
        else if (tree_name == "PLLPARS")
            params.start_tree = STT_PLL_PARSIMONY;
        else if (tree_name == "RAND" || tree_name=="RANDOM" ||
                 tree_name == "RBT"  || tree_name == "QDT") {
            params.start_tree_subtype_name = tree_name;
            params.start_tree = STT_RANDOM_TREE;
        }
        else if (START_TREE_RECOGNIZED(tree_name)) {
            params.start_tree_subtype_name = tree_name;
            params.start_tree = STT_BIONJ;
        } else {
            return false;
        }
        return true;
    }
};

void parseArg(int argc, char *argv[], Params &params) {
    int cnt;
    #if USE_PROGRESS_DISPLAY
    progress_display::setProgressDisplay(false);
    #endif
    verbose_mode = VB_MIN;
    params.tree_gen = NONE;
    params.constraint_tree_file = NULL;
    params.opt_gammai = true;
    params.opt_gammai_fast = false;
    params.opt_gammai_keep_bran = false;
    params.testAlphaEpsAdaptive = false;
    params.randomAlpha = false;
    params.testAlphaEps = 0.1;
    params.exh_ai = false;
    params.alpha_invar_file = NULL;
    params.out_file = NULL;
    params.sub_size = 0;
    params.pd_proportion = 0.0;
    params.min_proportion = 0.0;
    params.step_proportion = 0.01;
    params.min_size = 0;
    params.step_size = 1;
    params.find_all = false;
    params.run_mode = DETECTED;
    params.detected_mode = DETECTED;
    params.param_file = NULL;
    params.initial_file = NULL;
    params.initial_area_file = NULL;
    params.pdtaxa_file = NULL;
    params.areas_boundary_file = NULL;
    params.boundary_modifier = 1.0;
    params.dist_file = nullptr;
    params.dist_format = "square";
    params.incremental = false;
    params.dist_compression_level = 1;
    params.compute_obs_dist = false;
    params.count_unknown_as_different = false;
    params.compute_jc_dist = true;
    params.use_alignment_summary_for_distance_calculation = true;
    params.use_custom_matrix_diagonal_math = true;
    params.compute_likelihood = true;
    params.compute_ml_dist = true;
    params.compute_ml_tree = true;
    params.compute_ml_tree_only = false;
    params.budget_file = NULL;
    params.overlap = 0;
    params.is_rooted = false;
    params.root_move_dist = 2;
    params.root_find = false;
    params.root_test = false;
    params.sample_size = -1;
    params.repeated_time = 1;
    //params.nr_output = 10000;
    params.nr_output = 0;
    //params.smode = EXHAUSTIVE;
    params.intype = IN_OTHER;
    params.budget = -1;
    params.min_budget = -1;
    params.step_budget = 1;
    params.root = NULL;
    params.num_splits = 0;
    params.min_len = 0.001;
    params.mean_len = 0.1;
    params.max_len = 0.999;
    params.num_zero_len = 0;
    params.pd_limit = 100;
    params.calc_pdgain = false;
    params.multi_tree = false;
    params.second_tree = NULL;
    params.support_tag = NULL;
    params.site_concordance = 0;
    params.site_concordance_partition = false;
    params.print_cf_quartets = false;
    params.print_df1_trees = false;
    params.internode_certainty = 0;
    params.tree_weight_file = NULL;
    params.consensus_type = CT_NONE;
    params.find_pd_min = false;
    params.branch_cluster = 0;
    params.taxa_order_file = NULL;
    params.endemic_pd = false;
    params.exclusive_pd = false;
    params.complement_area = NULL;
    params.scaling_factor = -1;
    params.numeric_precision = -1;
    params.binary_programming = false;
    params.quad_programming = false;
    params.test_input = TEST_NONE;
    params.tree_burnin = 0;
    params.tree_max_count = 1000000;
    params.split_threshold = 0.0;
    params.split_threshold_str = NULL;
    params.split_weight_threshold = -1000;
    params.collapse_zero_branch = false;
    params.split_weight_summary = SW_SUM;
    params.gurobi_format = true;
    params.gurobi_threads = 1;
    params.num_bootstrap_samples = 0;
    params.bootstrap_spec = NULL;
    params.transfer_bootstrap = 0;
    params.mpboot2 = false;

    params.aln_file = NULL;
    params.phylip_sequential_format = false;
    params.symtest = SYMTEST_NONE;
    params.symtest_only = false;
    params.symtest_remove = 0;
    params.symtest_keep_zero = false;
    params.symtest_type = 0;
    params.symtest_pcutoff = 0.05;
    params.symtest_stat = false;
    params.symtest_shuffle = 1;
    //params.treeset_file = NULL;
    params.topotest_replicates = 0;
    params.topotest_optimize_model = false;
    params.do_weighted_test = false;
    params.do_au_test = false;
    params.siteLL_file = NULL; //added by MA
    params.partition_file = NULL;
    params.partition_type = BRLEN_OPTIMIZE;
    params.partfinder_rcluster = 100;
    params.partfinder_rcluster_max = 0;
    params.partition_merge = MERGE_NONE;
    params.merge_models = "1";
    params.merge_rates = "1";
    params.partfinder_log_rate = true;
    params.remove_empty_seq = true;
    params.terrace_aware = true;
#ifdef IQTREE_TERRAPHAST
    params.terrace_analysis = false;
#else
    params.terrace_analysis = false;
#endif
    params.sequence_type = NULL;
    params.aln_output = NULL;
    params.aln_site_list = NULL;
    params.aln_output_format = IN_PHYLIP;
    params.output_format = FORMAT_NORMAL;
    params.newick_extended_format = false;
    params.gap_masked_aln = NULL;
    params.concatenate_aln = NULL;
    params.aln_nogaps = false;
    params.aln_no_const_sites = false;
    params.print_aln_info = false;
//    params.parsimony = false;
//    params.parsimony_tree = false;
    params.tree_spr = false;
    params.max_spr_iterations = 0;
    params.nexus_output = false;
    params.k_representative = 4;
    params.loglh_epsilon = 0.001;
    params.numSmoothTree = 1;
    params.use_compute_parsimony_tree_new = false;
    params.use_batch_parsimony_addition   = false;
    params.distance_uses_max_threads      = false;
    params.parsimony_uses_max_threads     = false;
    params.parsimony_nni_iterations       = 0;
    params.use_lazy_parsimony_spr         = false;
    params.parsimony_spr_iterations       = 0;
    params.parsimony_pll_spr              = false;
    params.parsimony_tbr_iterations       = 0;
    params.use_lazy_parsimony_tbr         = false;
    params.parsimony_hybrid_iterations    = 0;
    params.optimize_ml_tree_with_parsimony = false;
    params.nni5 = true;
    params.nni5_num_eval = 1;
    params.brlen_num_traversal = 1;
    params.leastSquareBranch = false;
    params.pars_branch_length = false;
    params.bayes_branch_length = false;
    params.manuel_analytic_approx = false;
    params.leastSquareNNI = false;
    params.ls_var_type = OLS;
    params.maxCandidates = 20;
    params.popSize = 5;
    params.p_delete = -1;
    params.min_iterations = -1;
    params.max_iterations = 1000;
    params.num_param_iterations = 100;
    params.stop_condition = SC_UNSUCCESS_ITERATION;
    params.stop_confidence = 0.95;
    params.num_runs = 1;
    params.model_name = "";
    params.model_opt_steps = 10;
    params.model_set = "ALL";
    params.model_extra_set = NULL;
    params.model_subset = NULL;
    params.state_freq_set = NULL;
    params.ratehet_set = "AUTO";
    params.score_diff_thres = 10.0;
    params.modelomatic = false;
    params.model_test_again = false;
    params.model_test_and_tree = 0;
    params.model_test_separate_rate = false;
    params.optimize_mixmodel_weight = false;
    params.optimize_rate_matrix = false;
    params.store_trans_matrix = false;
    //params.freq_type = FREQ_EMPIRICAL;
    params.freq_type = FREQ_UNKNOWN;
    params.keep_zero_freq = true;
    params.min_state_freq = MIN_FREQUENCY;
    params.min_rate_cats = 2;
    params.num_rate_cats = 4;
    params.max_rate_cats = 10;
    params.gamma_shape = -1.0;
    params.min_gamma_shape = MIN_GAMMA_SHAPE;
    params.gamma_median = false;
    params.p_invar_sites = -1.0;
    params.optimize_model_rate_joint = false;
    params.optimize_by_newton = true;
    params.optimize_alg_freerate = "2-BFGS,EM";
    params.optimize_alg_mixlen = "EM";
    params.optimize_alg_gammai = "EM";
    params.optimize_from_given_params = false;
    params.fixed_branch_length = BRLEN_OPTIMIZE;
    params.min_branch_length = 0.0; // this is now adjusted later based on alignment length
    // TODO DS: This seems inappropriate for PoMo.  It is handled in
    // phyloanalysis::2908.
    params.max_branch_length = 10.0; // Nov 22 2016: reduce from 100 to 10!
    params.iqp_assess_quartet = IQP_DISTANCE;
    params.iqp = false;
    params.write_intermediate_trees = 0;
//    params.avoid_duplicated_trees = false;
    params.writeDistImdTrees = false;
    params.rf_dist_mode = 0;
    params.rf_same_pair = false;
    params.normalize_tree_dist = false;
    params.mvh_site_rate = false;
    params.rate_mh_type = true;
    params.discard_saturated_site = false;
    params.mean_rate = 1.0;
    params.aLRT_threshold = 101;
    params.aLRT_replicates = 0;
    params.aLRT_test = false;
    params.aBayes_test = false;
    params.localbp_replicates = 0;
#ifdef __AVX512KNL
    params.SSE = LK_AVX512;
#else
    params.SSE = LK_AVX_FMA;
#endif
    params.lk_safe_scaling = false;
    params.numseq_safe_scaling = 2000;
    params.ignore_any_errors = false;
    params.kernel_nonrev = false;
    params.print_site_lh = WSL_NONE;
    params.print_partition_lh = false;
    params.print_site_prob = WSL_NONE;
    params.print_site_state_freq = WSF_NONE;
    params.print_site_rate = 0;
    params.print_trees_site_posterior = 0;
    params.print_ancestral_sequence = AST_NONE;
    params.min_ancestral_prob = 0.0;
    params.print_tree_lh = false;
    params.lambda = 1;
    params.speed_conf = 1.0;
    params.whtest_simulations = 1000;
    params.mcat_type = MCAT_LOG + MCAT_PATTERN;
    params.rate_file = NULL;
    params.ngs_file = NULL;
    params.ngs_mapped_reads = NULL;
    params.ngs_ignore_gaps = true;
    params.do_pars_multistate = false;
    params.gene_pvalue_file = NULL;
    params.gene_scale_factor = -1;
    params.gene_pvalue_loga = false;
    params.second_align = NULL;
    params.ncbi_taxid = 0;
    params.ncbi_taxon_level = NULL;
    params.ncbi_names_file = NULL;
    params.ncbi_ignore_level = NULL;

	params.eco_dag_file  = NULL;
	params.eco_type = NULL;
	params.eco_detail_file = NULL;
	params.k_percent = 0;
	params.diet_min = 0;
	params.diet_max = 0;
	params.diet_step = 0;
	params.eco_weighted = false;
	params.eco_run = 0;

	params.upper_bound = false;
	params.upper_bound_NNI = false;
	params.upper_bound_frac = 0.0;

    params.gbo_replicates = 0;
	params.ufboot_epsilon = 0.5;
    params.check_gbo_sample_size = 0;
    params.use_rell_method = true;
    params.use_elw_method = false;
    params.use_weighted_bootstrap = false;
    params.use_max_tree_per_bootstrap = true;
    params.max_candidate_trees = 0;
    params.distinct_trees = false;
    params.online_bootstrap = true;
    params.min_correlation = 0.99;
    params.step_iterations = 100;
//    params.store_candidate_trees = false;
	params.print_ufboot_trees = 0;
    params.jackknife_prop = 0.0;
    params.robust_phy_keep = 1.0;
    params.robust_median = false;
    //const double INF_NNI_CUTOFF = -1000000.0;
    params.nni_cutoff = -1000000.0;
    params.estimate_nni_cutoff = false;
    params.nni_sort = false;
    //params.nni_opt_5branches = false;
    params.testNNI = false;
    params.approximate_nni = false;
    params.do_compression = false;

    params.new_heuristic = true;
    params.iteration_multiple = 1;
    params.initPS = 0.5;
#ifdef USING_PLL
    params.pll = true;
#else
    params.pll = false;
#endif
    params.modelEps = 0.01;
    params.modelfinder_eps = 0.1;
    params.parbran = false;
    params.binary_aln_file = NULL;
    params.maxtime = 1000000;
    params.reinsert_par = false;
    params.bestStart = true;
    params.snni = true; // turn on sNNI default now
//    params.autostop = true; // turn on auto stopping rule by default now
    params.unsuccess_iteration = 100;
    params.speednni = true; // turn on reduced hill-climbing NNI by default now
    params.numInitTrees = 100;
    params.fixStableSplits = false;
    params.stableSplitThreshold = 0.9;
    params.five_plus_five = false;
    params.memCheck = false;
    params.tabu = false;
    params.adaptPertubation = false;
    params.numSupportTrees = 20;
    params.spr_radius = 20;
    params.tbr_radius = 10;
    params.sankoff_cost_file = NULL;
    params.numNNITrees = 20;
    params.avh_test = 0;
    params.bootlh_test = 0;
    params.bootlh_partitions = NULL;
    params.site_freq_file = NULL;
    params.tree_freq_file = NULL;
    params.num_threads = 1;
    params.num_threads_max = 10000;
    params.openmp_by_model = false;
    params.model_test_criterion = MTC_BIC;
//    params.model_test_stop_rule = MTC_ALL;
    params.model_test_sample_size = 0;
    params.root_state = NULL;
    params.print_bootaln = false;
    params.print_boot_site_freq = false;
	params.print_subaln = false;
	params.print_partition_info = false;
	params.print_conaln = false;
	params.count_trees = false;
    params.pomo = false;
    params.pomo_random_sampling = false;
	// params.pomo_counts_file_flag = false;
	params.pomo_pop_size = 9;
	params.print_branch_lengths = false;
    params.max_mem_is_in_bytes = false;
	params.lh_mem_save = LM_PER_NODE; // auto detect
    params.buffer_mem_save = false;
	params.start_tree = STT_PLL_PARSIMONY;
    params.start_tree_subtype_name = StartTree::Factory::getNameOfDefaultTreeBuilder();

    params.modelfinder_ml_tree = true;
    params.final_model_opt = true;
	params.print_splits_file = false;
    params.print_splits_nex_file = true;
    params.ignore_identical_seqs = true;
    params.write_init_tree = false;
    params.write_candidate_trees = false;
    params.write_branches = false;
    params.freq_const_patterns = NULL;
    params.no_rescale_gamma_invar = false;
    params.compute_seq_identity_along_tree = false;
    params.compute_seq_composition = true;
    params.lmap_num_quartets = -1;
    params.lmap_cluster_file = NULL;
    params.print_lmap_quartet_lh = false;
    params.num_mixlen = 1;
    params.link_alpha = false;
    params.link_model = false;
    params.model_joint = NULL;
    params.ignore_checkpoint = false;
    params.checkpoint_dump_interval = 60;
    params.force_unfinished = false;
    params.suppress_output_flags = 0;
    params.ufboot2corr = false;
    params.u2c_nni5 = false;
    params.date_with_outgroup = true;
    params.date_debug = false;
    params.date_replicates = 0;
    params.clock_stddev = -1.0;
    params.date_outlier = -1.0;
    
    params.matrix_exp_technique = MET_EIGEN3LIB_DECOMPOSITION;

	if (params.nni5) {
	    params.nni_type = NNI5;
	} else {
	    params.nni_type = NNI1;
	}

    struct timeval tv;
    struct timezone tz;
    // initialize random seed based on current time
    gettimeofday(&tv, &tz);
    //params.ran_seed = (unsigned) (tv.tv_sec+tv.tv_usec);
    params.ran_seed = (tv.tv_usec);
    params.subsampling_seed = params.ran_seed;
    params.subsampling = 0;
    
    params.suppress_list_of_sequences = false;
    params.suppress_zero_distance_warnings = false;
    params.suppress_duplicate_sequence_warnings = false;

    for (cnt = 1; cnt < argc; ++cnt) {
        try {
            std::string arg = argv[cnt];
            if (arg=="-h" || arg=="--help") {
#ifdef IQ_TREE
                usage_iqtree(argv, arg=="--help");
#else
                usage(argv, false);
#endif
                continue;
            }
            if (arg=="-V" || arg=="-version" || arg=="--version") {
                printCopyright(cout);
                exit(EXIT_SUCCESS);
            }
            if (arg=="-ho" || arg=="-?") {
                usage_iqtree(argv, false);
                continue;
            }
            if (arg=="-hh" || arg=="-hhh") {
#ifdef IQ_TREE
                usage_iqtree(argv, true);
#else
                usage(argv);
#endif
                continue;
            }
            if (arg=="-v0") {
                verbose_mode = VB_QUIET;
                continue;
            }
            if (arg=="-v" || arg=="--verbose") {
                verbose_mode = VB_MED;
                continue;
            }
            if (arg=="-vv" || arg=="-v2") {
                verbose_mode = VB_MAX;
                continue;
            }
            if (arg=="-vvv" || arg=="-v3") {
                verbose_mode = VB_DEBUG;
                continue;
            }
            if (arg=="-k") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -k <num_taxa>";
                }
                convert_range(argv[cnt], params.min_size, params.sub_size,
                              params.step_size);
                params.k_representative = params.min_size;
                continue;
            }
            if (arg=="-pre" || arg=="--prefix") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -pre <output_prefix>";
                }
                params.out_prefix = argv[cnt];
                continue;
            }
            if (arg=="-pp") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -pp <pd_proportion>";
                }
                convert_range(argv[cnt], params.min_proportion,
                              params.pd_proportion, params.step_proportion);
                if (params.pd_proportion < 0 || params.pd_proportion > 1) {
                    throw "PD proportion must be between 0 and 1";
                }
                continue;
            }
            if (arg=="-mk") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -mk <min_taxa>";
                }
                params.min_size = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-bud") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -bud <budget>";
                }
                convert_range(argv[cnt], params.min_budget, params.budget,
                              params.step_budget);
                continue;
            }
            if (arg=="-mb") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -mb <min_budget>";
                }
                params.min_budget = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-o") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -o <taxon>";
                }
                params.root = argv[cnt];
                continue;
            }
            if (arg=="-optalg") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -opt_alg <1-BFGS|2-BFGS|EM>";
                }
                params.optimize_alg_freerate = argv[cnt];
                continue;
            }
            if (arg=="-optlen") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -optlen <BFGS|EM>";
                }
                params.optimize_alg_mixlen = argv[cnt];
                continue;
            }
            if (arg=="-optalg_gammai") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -optalg_gammai <Brent|BFGS|EM>";
                }
                params.optimize_alg_gammai = argv[cnt];
                continue;
            }
            if (arg=="-root" || arg=="-rooted") {
                params.is_rooted = true;
                continue;
            }
            if (arg=="--root-dist") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --root-dist <maximum-root-move-distance>";
                }
                params.root_move_dist = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="--root-find") {
                params.root_find = true;
                continue;
            }
            if (arg=="--root-test") {
                if (!params.compute_likelihood) {
                    throw "Cannot combine --root-test and -no-ml parameters";
                }
                params.root_test = true;
                continue;
            }
            if (arg=="-all") {
                params.find_all = true;
                continue;
            }
            if (arg=="--greedy") {
                params.run_mode = GREEDY;
                continue;
            }
            if (arg=="-pr" || arg=="--pruning") {
                params.run_mode = PRUNING;
                continue;
            }
            if (arg=="-e") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -e <file>";
                }
                params.param_file = argv[cnt];
                continue;
            }
            if (arg=="-if") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -if <file>";
                }
                params.initial_file = argv[cnt];
                continue;
            }
            if (arg=="-nni_nr_step") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -nni_nr_step <newton_raphson_steps>";
                }
                NNI_MAX_NR_STEP = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-ia") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -ia <file>";
                }
                params.initial_area_file = argv[cnt];
                continue;
            }
            if (arg=="-u") {
                // file containing budget information
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -u <file>";
                }
                params.budget_file = argv[cnt];
                continue;
            }
            if (arg=="-dd") {
                // compute distribution of PD score on random sets
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -dd <sample_size>";
                }
                params.run_mode = PD_DISTRIBUTION;
                params.sample_size = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-ts") {
                // calculate PD score a taxa set listed in the file
                ++cnt;
                //params.run_mode = PD_USER_SET;
                if (cnt >= argc) {
                    throw "Use -ts <taxa_file>";
                }
                params.pdtaxa_file = argv[cnt];
                continue;
            }
            if (arg=="-bound") {
                // boundary length of areas
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -bound <file>";
                }
                params.areas_boundary_file = argv[cnt];
                continue;
            }
            if (arg=="-blm") {
                // boundary length modifier
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -blm <boundary_modifier>";
                }
                params.boundary_modifier = convert_double(argv[cnt]);
                continue;
            }
            if (arg=="-dist-format") {
                std::string format = next_argument(argc, argv,
                                                   "distance_file_format", cnt);
                params.dist_format = string_to_lower(format.c_str());
                params.dist_compression_level
                    = strip_number_suffix(params.dist_format,
                                          params.dist_compression_level);
                if (params.dist_compression_level<0) {
                    params.dist_compression_level=0;
                } else if (9<params.dist_compression_level) {
                    params.dist_compression_level=9;
                }
                const char* allowed[] = {
                    "square", "lower", "upper"
                    , "square.gz", "lower.gz", "upper.gz"
                };
                throw_if_not_in_set ( "dist-format", params.dist_format
                                    , allowed, sizeof(allowed)/sizeof(allowed[0]));
                continue;
            }
            if (arg=="-update") {
                params.incremental = true;
                std::string method = next_argument(argc, argv,
                                                   "incremental_method", cnt);
                params.incremental_method = string_to_upper(method.c_str());
                continue;
            }
            if (arg=="-merge") {
                std::string alignment_file = next_argument(argc, argv,
                                                           "alignment_file_to_merge", cnt);
                params.additional_alignment_files.emplace_back(alignment_file);            
                continue;
            }
            if (arg=="-dist" || arg=="-d") {
                // calculate distance matrix from the tree
                params.run_mode = CALC_DIST;
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -dist <distance_file>";
                }
                params.dist_file = argv[cnt];
                continue;
            }
            if (arg=="-djc" || arg=="-no-ml-dist") {
                params.compute_ml_dist = false;
                continue;
            }
            if (arg=="-no-ml") {
                if (params.compute_ml_tree_only) {
                    throw "-no-ml and -mlnj-only cannot be used together";
                }
                if (params.iqp_assess_quartet == IQP_BOOTSTRAP) {
                    throw "Cannot combine the -nb and -no-ml options";
                }
                if (!params.treeset_file.empty()) {
                    throw "Cannot combine the -z (or --trees) and -no-ml options";
                }
                if (params.bayes_branch_length) {
                    throw "cannot combine -bayesbran and -no-ml options";
                }
                if (params.root_test) {
                    throw "Cannot combine --root-test and -no-ml parameters";
                }
                params.compute_likelihood = false;
                params.compute_ml_dist    = false;
                params.compute_ml_tree    = false;
                params.model_name         = "JC";
                params.min_iterations     = 0;
                continue;
            }
            if (arg=="-mlnj-only" || arg=="--mlnj-only") {
                if (!params.compute_likelihood) {
                    throw "-no-ml and -mlnj-only cannot be used together";
                }
                params.compute_ml_tree_only = true;
                continue;
            }
            if (arg=="-dobs") {
                params.compute_obs_dist = true;
                continue;
            }
            if (arg=="-cud") {
                params.count_unknown_as_different = true;
                continue;
            }
            if (arg=="-pll-spr") {
                params.parsimony_pll_spr = true;
                continue;
            }
            if (arg=="-parsimony-batch") {
                params.use_batch_parsimony_addition = true;
                continue;
            }
            if (arg=="-experimental" || arg=="--experimental") {
                params.use_compute_parsimony_tree_new = true;
                continue;
            }
            if (arg=="-old-stepwise-parsimony") {
                params.use_compute_parsimony_tree_new = false;
                continue;
            }
            if (arg=="-old-matrix-math") {
                params.use_custom_matrix_diagonal_math = false;
                continue;
            }
            if (arg=="-old-distance-calculation") {
                params.use_alignment_summary_for_distance_calculation = false;
                continue;
            }
            if (arg=="--no-experimental") {
                continue;
            }
            if (arg=="-r") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -r <num_taxa>";
                }
                params.sub_size = convert_int(argv[cnt]);
                params.tree_gen = YULE_HARDING;
                continue;
            }
            if (arg=="-rs") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -rs <alignment_file>";
                }
                params.tree_gen = YULE_HARDING;
                params.aln_file = argv[cnt];
                continue;
            }
            if (arg=="-rstar") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -rstar <num_taxa>";
                }
                params.sub_size = convert_int(argv[cnt]);
                params.tree_gen = STAR_TREE;
                continue;
            }
            if (arg=="-ru") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -ru <num_taxa>";
                }
                params.sub_size = convert_int(argv[cnt]);
                params.tree_gen = UNIFORM;
                continue;
            }
            if (arg=="-rcat") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -rcat <num_taxa>";
                }
                params.sub_size = convert_int(argv[cnt]);
                params.tree_gen = CATERPILLAR;
                continue;
            }
            if (arg=="-rbal") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -rbal <num_taxa>";
                }
                params.sub_size = convert_int(argv[cnt]);
                params.tree_gen = BALANCED;
                continue;
            }
            if (arg=="--rand") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -rand UNI | CAT | BAL | NET";
                }
                arg = argv[cnt];
                if (arg=="UNI") {
                    params.tree_gen = UNIFORM;
                }
                else if (arg=="CAT") {
                    params.tree_gen = CATERPILLAR;
                }
                else if (arg=="BAL") {
                    params.tree_gen = BALANCED;
                }
                else if (arg=="NET") {
                    params.tree_gen = CIRCULAR_SPLIT_GRAPH;
                }
                else {
                    throw "wrong --rand option";
                }
                continue;
            }            
            if (arg=="--keep-ident" || arg=="-keep-ident") {
                params.ignore_identical_seqs = false;
                continue;
            }
            if (arg=="-rcsg") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -rcsg <num_taxa>";
                }
                params.sub_size = convert_int(argv[cnt]);
                params.tree_gen = CIRCULAR_SPLIT_GRAPH;
                continue;
            }
            if (arg=="-rpam") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -rpam <num_splits>";
                }
                params.num_splits = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-rlen" || arg=="--rlen") {
                ++cnt;
                if (cnt >= argc - 2) {
                    throw "Use -rlen <min_len> <mean_len> <max_len>";
                }
                params.min_len  = convert_double(argv[cnt]);
                params.mean_len = convert_double(argv[cnt + 1]);
                params.max_len  = convert_double(argv[cnt + 2]);
                cnt += 2;
                continue;
            }
            if (arg=="-rzero") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -rzero <num_zero_branch>";
                }
                params.num_zero_len = convert_int(argv[cnt]);
                if (params.num_zero_len < 0) {
                    throw "num_zero_len must not be negative";
                }
                continue;
            }
            if (arg=="-rset") {
                ++cnt;
                if (cnt >= argc - 1) {
                    throw "Use -rset <overlap> <outfile>";
                }
                params.overlap = convert_int(argv[cnt]);
                ++cnt;
                params.pdtaxa_file = argv[cnt];
                params.tree_gen = TAXA_SET;
                continue;
            }
            if (arg=="-rep") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -rep <repeated_times>";
                }
                params.repeated_time = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-lim") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -lim <pd_limit>";
                }
                params.pd_limit = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-noout") {
                params.nr_output = 0;
                continue;
            }
            if (arg=="-1out") {
                params.nr_output = 1;
                continue;
            }
            if (arg=="-oldout") {
                params.nr_output = 100;
                continue;
            }
            if (arg=="-nexout") {
                params.nexus_output = true;
                continue;
            }
            if (arg=="-exhaust") {
                params.run_mode = EXHAUSTIVE;
                continue;
            }
            if (arg=="-seed" || arg=="--seed") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -seed <random_seed>";
                }
                params.ran_seed = abs(convert_int(argv[cnt]));
                continue;
            }
            if (arg=="-pdgain") {
                params.calc_pdgain = true;
                continue;
            }
            if (arg=="-sup" || arg=="--support") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -sup <target_tree_file>";
                }
                params.second_tree = argv[cnt];
                params.consensus_type = CT_ASSIGN_SUPPORT;
                continue;
            }
            if (arg=="-suptag" || arg=="--suptag") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -suptag <tagname or ALL>";
                }
                params.support_tag = argv[cnt];
                continue;
            }
            if (arg=="-sup2") {
                outError("Deprecated -sup2 option, please use --gcf --tree FILE");
            }
            if (arg=="--gcf") {
                params.consensus_type = CT_ASSIGN_SUPPORT_EXTENDED;
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --gcf <user_trees_file>";
                }
                params.treeset_file = argv[cnt];
                continue;
            }
            if (arg=="--scf") {
                params.consensus_type = CT_ASSIGN_SUPPORT_EXTENDED;
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --scf NUM_QUARTETS";
                }
                params.site_concordance = convert_int(argv[cnt]);
                if (params.site_concordance < 1) {
                    throw "Positive --scf please";
                }
                continue;
            }
            if (arg=="--scf-part" || arg=="--cf-verbose") {
                params.site_concordance_partition = true;
                continue;
            }
            if (arg=="--cf-quartet") {
                params.print_cf_quartets = true;
                continue;
            }
            if (arg=="--df-tree") {
                params.print_df1_trees = true;
                continue;
            }
            if (arg=="--qic") {
                params.internode_certainty = 1;
                continue;
            }
            if (arg=="-treew") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -treew <tree_weight_file>";
                }
                params.tree_weight_file = argv[cnt];
                continue;
            }
            if (arg=="-con" || arg=="--contree") {
                params.consensus_type = CT_CONSENSUS_TREE;
                continue;
            }
            if (arg=="-net" || arg=="--connet") {
                params.consensus_type = CT_CONSENSUS_NETWORK;
                continue;
            }
            /**MINH ANH: to serve some statistics on tree*/
            if (arg=="-comp") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -comp <treefile>";
                }
                params.consensus_type = COMPARE;
                params.second_tree = argv[cnt];
                continue;
            }
            if (arg == "-mpboot2") {
                params.mpboot2 = true;
                continue;
            }
            if (arg=="-stats") {
                params.run_mode = STATS;
                continue;
            }
            if (arg=="-gbo") { //guided bootstrap
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -gbo <site likelihod file>";
                }
                params.siteLL_file = argv[cnt];
                //params.run_mode = GBO;
                continue;
            } // MA
            if (arg=="-mprob") { //compute multinomial distribution probability
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -mprob <ref_alignment>";
                }
                params.second_align = argv[cnt];
                //params.run_mode = MPRO;
                continue;
            } // MA
            if (arg=="-min") {
                params.find_pd_min = true;
                continue;
            }
            if (arg=="-excl") {
                params.exclusive_pd = true;
                continue;
            }
            if (arg=="-endem") {
                params.endemic_pd = true;
                continue;
            }
            if (arg=="-compl") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -compl <area_name>";
                }
                params.complement_area = argv[cnt];
                continue;
            }
            if (arg=="-cluster") {
                params.branch_cluster = 4;
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -cluster <taxa_order_file>";
                }
                params.taxa_order_file = argv[cnt];
                continue;
            }
            if (arg=="-taxa") {
                params.run_mode = PRINT_TAXA;
                continue;
            }
            if (arg=="-area") {
                params.run_mode = PRINT_AREA;
                continue;
            }
            if (arg=="-scale") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -scale <scaling_factor>";
                }
                params.scaling_factor = convert_double(argv[cnt]);
                continue;
            }
            if (arg=="-scaleg") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -scaleg <gene_scale_factor>";
                }
                params.gene_scale_factor = convert_double(argv[cnt]);
                continue;
            }
            if (arg=="-scalebranch") {
                params.run_mode = SCALE_BRANCH_LEN;
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -scalebranch <scaling_factor>";
                }
                params.scaling_factor = convert_double(argv[cnt]);
                continue;
            }
            if (arg=="-scalenode") {
                params.run_mode = SCALE_NODE_NAME;
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -scalenode <scaling_factor>";
                }
                params.scaling_factor = convert_double(argv[cnt]);
                continue;
            }
            if (arg=="-prec") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -prec <numeric_precision>";
                }
                params.numeric_precision = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-lp") {
                params.run_mode = LINEAR_PROGRAMMING;
                continue;
            }
            if (arg=="-lpbin") {
                params.run_mode = LINEAR_PROGRAMMING;
                params.binary_programming = true;
                continue;
            }
            if (arg=="-qp") {
                params.gurobi_format = true;
                params.quad_programming = true;
                continue;
            }
            if (arg=="-quiet" || arg=="--quiet") {
                verbose_mode = VB_QUIET;
                continue;
            }
            if (arg=="-mult") {
                params.multi_tree = true;
                continue;
            }
            if (arg=="-bi" || arg=="--burnin") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -bi <burnin_value>";
                }
                params.tree_burnin = convert_int(argv[cnt]);
                if (params.tree_burnin < 0) {
                    throw "Burnin value must not be negative";
                }
                continue;
            }
            if (arg=="-tm") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -tm <tree_max_count>";
                }
                params.tree_max_count = convert_int(argv[cnt]);
                if (params.tree_max_count < 0) {
                    throw "tree_max_count must not be negative";
                }
                continue;
            }
            if (arg=="-minsup" || arg=="--sup-min") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -minsup <split_threshold>";
                }
                params.split_threshold = convert_double(argv[cnt]);
                if (params.split_threshold < 0 || params.split_threshold > 1) {
                    throw "Split threshold must be between 0 and 1";
                }
                continue;
            }
            if (arg=="-minsupnew") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -minsupnew <split_threshold_1/.../split_threshold_k>";
                }
                params.split_threshold_str = argv[cnt];
                continue;
            }
            if (arg=="-tw") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -tw <split_weight_threshold>";
                }
                params.split_weight_threshold = convert_double(argv[cnt]);
                if (params.split_weight_threshold < 0) {
                    throw "Split weight threshold is negative";
                }
                continue;
            }
            if (arg=="-czb" || arg=="--polytomy") {
                params.collapse_zero_branch = true;
                continue;
            }
            if (arg=="-swc") {
                params.split_weight_summary = SW_COUNT;
                continue;
            }
            if (arg=="-swa") {
                params.split_weight_summary = SW_AVG_ALL;
                continue;
            }
            if (arg=="-swp") {
                params.split_weight_summary = SW_AVG_PRESENT;
                continue;
            }
            if (arg=="-iwc") {
                params.test_input = TEST_WEAKLY_COMPATIBLE;
                continue;
            }
            if (arg=="--aln" || arg=="--msa" || arg=="-s") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --aln, -s <alignment_file>";
                }
                params.aln_file = argv[cnt];
                continue;
            }
            if (arg=="--sequential") {
                params.phylip_sequential_format = true;
                continue;
            }
            if (arg=="--symtest") {
                params.symtest = SYMTEST_MAXDIV;
                continue;
            }
            if (arg=="--bisymtest") {
                params.symtest = SYMTEST_BINOM;
                continue;
            }
            if (arg=="--symtest-only") {
                params.symtest_only = true;
                if (params.symtest == SYMTEST_NONE) {
                    params.symtest = SYMTEST_MAXDIV;
                }
                continue;
            }
            if (arg=="--symtest-remove-bad") {
                params.symtest_remove = 1;
                if (params.symtest == SYMTEST_NONE) {
                    params.symtest = SYMTEST_MAXDIV;
                }
                continue;
            }
            if (arg=="--symtest-remove-good") {
                params.symtest_remove = 2;
                if (params.symtest == SYMTEST_NONE) {
                    params.symtest = SYMTEST_MAXDIV;
                }
                continue;
            }
            if (arg=="--symtest-keep-zero") {
                params.symtest_keep_zero = true;
                continue;
            }
            if (arg=="--symtest-type") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --symtest-type SYM|MAR|INT";
                }
                arg = argv[cnt];
                if (arg=="SYM") {
                    params.symtest_type = 0;
                }
                else if (arg=="MAR") {
                    params.symtest_type = 1;
                }
                else if (arg=="INT") {
                    params.symtest_type = 2;
                }
                else {
                    throw "Use --symtest-type SYM|MAR|INT";
                }
                if (params.symtest == SYMTEST_NONE) {
                    params.symtest = SYMTEST_MAXDIV;
                }
                continue;
            }

            if (arg=="--symtest-pval") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --symtest-pval PVALUE_CUTOFF";
                }
                params.symtest_pcutoff = convert_double(argv[cnt]);
                if (params.symtest_pcutoff <= 0 || params.symtest_pcutoff >= 1) {
                    throw "--symtest-pval must be between 0 and 1";
                }
                if (params.symtest == SYMTEST_NONE) {
                    params.symtest = SYMTEST_MAXDIV;
                }
                continue;
            }
            if (arg=="--symstat") {
                params.symtest_stat = true;
                if (params.symtest == SYMTEST_NONE) {
                    params.symtest = SYMTEST_MAXDIV;
                }
                continue;
            }
            if (arg=="--symtest-perm") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --symtest-perm INT";
                }
                params.symtest_shuffle = convert_int(argv[cnt]);
                if (params.symtest_shuffle <= 0) {
                    throw "--symtest-perm must be positive";
                }
                if (params.symtest == SYMTEST_NONE) {
                    params.symtest = SYMTEST_MAXDIV;
                }
                continue;
            }
            if (arg=="-z" || arg=="--trees") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -z <user_trees_file>";
                }
                if (!params.compute_likelihood) {
                    throw "-z or --trees cannot be used with -no-ml";
                }
                params.treeset_file = argv[cnt];
                continue;
            }
            if (arg=="-zb" || arg=="--test") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -zb <#replicates>";
                }
                params.topotest_replicates = convert_int(argv[cnt]);
                if (params.topotest_replicates < 1000) {
                    throw "Please specify at least 1000 replicates";
                }
                continue;
            }
            if (arg=="--estimate-model") {
                params.topotest_optimize_model = true;
                continue;
            }
            if (arg=="-zw" || arg=="--test-weight") {
                params.do_weighted_test = true;
                continue;
            }
            if (arg=="-au" || arg=="--test-au") {
                params.do_au_test = true;
                continue;
            }
            if (arg=="-sp" || arg=="-Q") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -sp <partition_file>";
                }
                params.partition_file = argv[cnt];
                continue;
            }
            if (arg=="-spp" || arg=="-p" || arg=="--partition") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -p <partition_file>";
                }
                params.partition_file = argv[cnt];
                params.partition_type = BRLEN_SCALE;
                params.opt_gammai = false;
                continue;
            }
            if (arg=="-spj" || arg=="-q") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -q <partition_file>";
                }
                params.partition_file = argv[cnt];
                params.partition_type = BRLEN_FIX;
                params.optimize_alg_gammai = "Brent";
                params.opt_gammai = false;
                continue;
            }
            if (arg=="-M") {
                params.partition_type = BRLEN_OPTIMIZE;
                continue;
            }
            if (arg=="-spu" || arg=="-S") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -spu <partition_file>";
                }
                params.partition_file = argv[cnt];
                params.partition_type = TOPO_UNLINKED;
                params.ignore_identical_seqs = false;
                params.buffer_mem_save = true;
                params.print_splits_nex_file = false;
                continue;
            }
            if (arg=="--edge") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --edge equal|scale|unlink";
                }
                arg = argv[cnt];
                if (arg=="equal") {
                    params.partition_type = BRLEN_FIX;
                }
                else if (arg=="scale") {
                    params.partition_type = BRLEN_SCALE;
                }
                else if (arg=="unlink") {
                    params.partition_type = BRLEN_OPTIMIZE;
                }
                else {
                    throw "Use --edge equal|scale|unlink";
                }
            }
            
            if (arg=="-rcluster" || arg=="--rcluster") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -rcluster <percent>";
                }
                params.partfinder_rcluster = convert_double(argv[cnt]);
                if (params.partfinder_rcluster < 0 ||
                    params.partfinder_rcluster > 100) {
                    throw "rcluster percentage must be between 0 and 100";
                }
                params.partition_merge = MERGE_RCLUSTER;
                continue;
            }
            if (arg=="-rclusterf" || arg=="--rclusterf") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -rclusterf <percent>";
                }
                params.partfinder_rcluster = convert_double(argv[cnt]);
                if (params.partfinder_rcluster < 0 ||
                    params.partfinder_rcluster > 100) {
                    throw "rcluster percentage must be between 0 and 100";
                }
                params.partition_merge = MERGE_RCLUSTERF;
                continue;
            }
            if (arg=="-rcluster-max" || arg=="--rcluster-max") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -rcluster-max <num>";
                }
                params.partfinder_rcluster_max = convert_int(argv[cnt]);
                if (params.partfinder_rcluster_max <= 0) {
                    throw "rcluster-max must be between > 0";
                }
                if (params.partfinder_rcluster == 100) {
                    params.partfinder_rcluster = 99.9999;
                }
                if (params.partition_merge != MERGE_RCLUSTER &&
                    params.partition_merge != MERGE_RCLUSTERF) {
                    params.partition_merge = MERGE_RCLUSTERF;
                }
                continue;
            }

            if (arg=="--merge") {
                if (cnt >= argc-1 || argv[cnt+1][0] == '-') {
                    if (params.partfinder_rcluster == 100) {
                        params.partfinder_rcluster = 99.9999;
                    }
                    params.partition_merge = MERGE_RCLUSTERF;
                    continue;
                }
                ++cnt;
                arg = argv[cnt];
                if (cnt >= argc) {
                    throw "Use --merge [none|greedy|rcluster|rclusterf|kmeans]";
                }
                if (arg=="none") {
                    params.partition_merge = MERGE_NONE;
                }
                else if (arg=="greedy")
                    params.partition_merge = MERGE_GREEDY;
                else if (arg=="rcluster") {
                    if (params.partfinder_rcluster == 100) {
                        params.partfinder_rcluster = 99.9999;
                    }
                    params.partition_merge = MERGE_RCLUSTER;
                }
                else if (arg=="rclusterf") {
                    if (params.partfinder_rcluster == 100) {
                        params.partfinder_rcluster = 99.9999;
                    }
                    params.partition_merge = MERGE_RCLUSTERF;
                }
                else if (arg=="rcluster") {
                    params.partition_merge = MERGE_KMEANS;
                }
                else {
                    throw "Use --merge [none|greedy|rcluster|rclusterf|kmeans]";
                }
                continue;
            }
            if (arg=="--merge-model") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --merge-model 1|4|ALL|model1,...,modelK";
                }
                params.merge_models = argv[cnt];
                if (params.partition_merge == MERGE_NONE) {
                    if (params.partfinder_rcluster == 100) {
                        params.partfinder_rcluster = 99.9999;
                    }
                    params.partition_merge = MERGE_RCLUSTERF;
                    continue;
                }
                continue;
            }
            if (arg=="--merge-rate") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --merge-rate rate1,...,rateK";
                }
                params.merge_rates = argv[cnt];
                if (params.partition_merge == MERGE_NONE) {
                    if (params.partfinder_rcluster == 100) {
                        params.partfinder_rcluster = 99.9999;
                    }
                    params.partition_merge = MERGE_RCLUSTERF;
                    continue;
                }
                continue;
            }
            if (arg=="--merge-log-rate") {
                params.partfinder_log_rate = true;
                continue;
            }
            if (arg=="--merge-normal-rate") {
                params.partfinder_log_rate = false;
                continue;
            }
            if (arg=="-keep_empty_seq") {
                params.remove_empty_seq = false;
                continue;
            }
            if (arg=="-no_terrace") {
                params.terrace_aware    = false;
                params.terrace_analysis = false;
                continue;
            }
            if (arg=="--terrace") {
#ifdef IQTREE_TERRAPHAST
                params.terrace_analysis = true;
#else
                throw "Unsupported command: --terrace.\n"
                    "Please build IQ-TREE with the USE_TERRAPHAST flag.";
#endif
                continue;
            }
            if (arg=="--no-terrace") {
                params.terrace_analysis = false;
                continue;
            }
            if (arg=="-sf") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -sf <ngs_file>";
                }
                params.ngs_file = argv[cnt];
                continue;
            }
            if (arg=="-sm") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -sm <ngs_mapped_read_file>";
                }
                params.ngs_mapped_reads = argv[cnt];
                continue;
            }
            if (arg=="-ngs_gap") {
                params.ngs_ignore_gaps = false;
                continue;
            }
            if (arg=="-st" || arg=="--seqtype") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -st BIN or -st DNA or -st AA or -st CODON or -st MORPH or -st CRXX or -st CFxx.";
                }
                params.sequence_type = argv[cnt];
                // if (arg.substr(0,2) == "CR") params.pomo_random_sampling = true;
                // if (arg.substr(0,2) == "CF" || arg.substr(0,2) == "CR") {
                //     outWarning("Setting the sampling method and population size with this flag is deprecated.");
                //     outWarning("Please use the model string instead (see `iqtree --help`).");
                //     if (arg.length() > 2) {
                //         int ps = convert_int(arg.substr(2).c_str());
                //         params.pomo_pop_size = ps;
                //         if (((ps != 10) && (ps != 2) && (ps % 2 == 0)) || (ps < 2) || (ps > 19)) {
                //             std::cout << "Please give a correct PoMo sequence type parameter; e.g., `-st CF09`." << std::endl;
                //             outError("Custom virtual population size of PoMo not 2, 10 or any other odd number between 3 and 19.");   
                //         }
                //     }
                // }
                continue;
            }
            if (arg=="-starttree") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -starttree BIONJ|PARS|PLLPARS|PJ";
                }
                else if (!parseTreeName(argv[cnt], params)) {
                    throw "Invalid option, please use -starttree with BIONJ or PARS or PLLPARS";
                }
                continue;
            }
            if (arg=="-ao" || arg=="--out-alignment" || arg=="--out-aln") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -ao <alignment_file>";
                }
                params.aln_output = argv[cnt];
                continue;
            }
            if (arg=="-as") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -as <aln_site_list>";
                }
                params.aln_site_list = argv[cnt];
                continue;
            }
            if (arg=="-an") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -an <ref_seq_name>";
                }
                params.ref_seq_name = argv[cnt];
                continue;
            }
            if (arg=="-af" || arg=="--out-format") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -af phy|fasta";
                }
                if (arg=="phy") {
                    params.aln_output_format = IN_PHYLIP;
                }
                else if (arg=="fasta") {
                    params.aln_output_format = IN_FASTA;
                }
                else if (arg=="nexus") {
                    params.aln_output_format = IN_NEXUS;
                }
                else {
                    throw "Unknown output format";
                }
                continue;
            }
            if (arg=="--out-csv") {
                params.output_format = FORMAT_CSV;
                continue;
            }
            if (arg=="--out-tsv") {
                params.output_format = FORMAT_TSV;
                continue;
            }            
            if (arg=="--figtree") {
                params.newick_extended_format = true;
                continue;
            }
            if (arg=="-am") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -am <gap_masked_aln>";
                }
                params.gap_masked_aln = argv[cnt];
                continue;
            }
            if (arg=="-ac") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -ac <concatenate_aln>";
                }
                params.concatenate_aln = argv[cnt];
                continue;
            }
            if (arg=="-nogap") {
                params.aln_nogaps = true;
                continue;
            }
            if (arg=="-noconst") {
                params.aln_no_const_sites = true;
                continue;
            }
            if (arg=="-alninfo") {
                params.print_aln_info = true;
                continue;
            }
            if (arg=="-spr") {
                // subtree pruning and regrafting
                params.tree_spr = true;
                continue;
            }
            if (arg=="-max-spr") {
                // subtree pruning and regrafting 
                std::string next_arg = next_argument(argc, argv,
                                                     "max_spr_iterations", cnt);
                params.tree_spr      = true; //but this turns on ML spr too.
                params.max_spr_iterations = atoi(next_arg.c_str());
                continue;
            }
            if (arg=="-parsimony-spr") {
                std::string next_arg = next_argument(argc, argv,
                                                     "max_parsimony_spr_iterations", cnt);
                params.parsimony_spr_iterations = atoi(next_arg.c_str());
                continue;
            }
            if (arg=="-lazy-spr") {
                params.use_lazy_parsimony_spr = true;
                continue;
            }
            if (arg=="-optimize-ml-tree-with-parsimony") {
                params.optimize_ml_tree_with_parsimony = true;
                continue;
            }
            if (arg=="-lazy-tbr") {
                params.use_lazy_parsimony_tbr = true;
                continue;
            }
            if (arg=="-parsimony-hybrid") {
                std::string next_arg = next_argument(argc, argv,
                                                     "max_parsimony_iterations", cnt);
                params.parsimony_hybrid_iterations = atoi(next_arg.c_str());
                continue;

            }
            
            
            if (arg=="-distance-uses-max-threads") {
                params.distance_uses_max_threads = true;
                continue;
            }
            if (arg=="-parsimony-uses-max-threads") {
                params.parsimony_uses_max_threads = true;
                continue;
            }
            if (arg=="-parsimony-nni") {
                std::string next_arg = next_argument(argc, argv,
                                                     "max_parsimony_nni_iterations", cnt);
                params.parsimony_nni_iterations = atoi(next_arg.c_str());
                continue;
            }
            if (arg=="-tbr-radius") {
                string next_arg = next_argument(argc, argv, "tbr_radius", cnt);
                params.tbr_radius = convert_int(next_arg.c_str());
                continue;
            }
            if (arg=="-parsimony-tbr") {
                std::string next_arg = next_argument(argc, argv,
                                                     "max_parsimony_tbr_iterations", cnt);
                params.parsimony_tbr_iterations = atoi(next_arg.c_str());
                continue;
            }
            if (arg=="-krep") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -krep <num_k>";
                }
                params.k_representative = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-pdel") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -pdel <probability>";
                }
                params.p_delete = convert_double(argv[cnt]);
                if (params.p_delete < 0.0 || params.p_delete > 1.0) {
                    throw "Probability of deleting a leaf must be between 0 and 1";
                }
                continue;
            }
            if (arg=="-pers" || arg=="--perturb") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -pers <perturbation_strength>";
                }
                params.initPS = convert_double(argv[cnt]);
                continue;
            }
            if (arg=="-n") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -n <#iterations>";
                }
                if (params.gbo_replicates != 0) {
                    throw("Ultrafast bootstrap does not work with -n option");
                }
                params.min_iterations = convert_int(argv[cnt]);
                params.stop_condition = SC_FIXED_ITERATION;
                //params.autostop     = false;
                continue;
            }
            if (arg=="-nparam") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -nparam <#iterations>";
                }
                params.num_param_iterations = convert_int(argv[cnt]);
                if (params.num_param_iterations < 0) {
                    throw "Number of parameter optimization iterations"
                    " (-nparam) must be non negative";
                }
                continue;
            }
            if (arg=="-nb") {
                if (!params.compute_likelihood) {
                    throw "Cannot combine the -nb and -no-ml options";
                }
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -nb <#bootstrap_replicates>";
                }
                params.min_iterations = convert_int(argv[cnt]);
                params.iqp_assess_quartet = IQP_BOOTSTRAP;
                //params.avoid_duplicated_trees = true;
                continue;
            }
            if (arg=="--model" || arg=="-m") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --model <model_name>";
                }
                params.model_name = argv[cnt];
                continue;
            }
            if (arg=="--model-file" || arg=="-mf") {
                params.yaml_model_file = next_argument(argc, argv,
                                                       "model_filepath", cnt);
                continue;
            }
            if (arg=="--init-model") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --init-model FILE";
                }
                params.model_name_init = argv[cnt];
                continue;
            }
            if (arg=="--loop-model") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --loop-model NUM";
                }
                params.model_opt_steps = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-mset" || arg=="--mset" || arg=="--models") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -mset <model_set>";
                }
                params.model_set = argv[cnt];
                continue;
            }
            if (arg=="-madd" || arg=="--madd") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -madd <extra_model_set>";
                }
                params.model_extra_set = argv[cnt];
                continue;
            }
            if (arg=="-msub" || arg=="--msub" || arg=="--model-sub") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -msub <model_subset>";
                }
                params.model_subset = argv[cnt];
                continue;
            }
            if (arg=="-mfreq" || arg=="--freqs") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -mfreq <state_freq_set>";
                }
                params.state_freq_set = argv[cnt];
                continue;
            }
            if (arg=="-mrate" || arg=="--mrate" || arg=="--rates") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -mrate <rate_set>";
                }
                params.ratehet_set = argv[cnt];
                continue;
            }
            if (arg=="--score-diff") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --score-diff <score>";
                }
                if (iEquals(argv[cnt], "all")) {
                    params.score_diff_thres = -1.0;
                }
                else {
                    params.score_diff_thres = convert_double(argv[cnt]);
                }
                continue;
            }
            if (arg=="-mdef" || arg=="--mdef") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -mdef <model_definition_file>";
                }
                params.model_def_file = argv[cnt];
                continue;
            }
            if (arg=="--modelomatic") {
                params.modelomatic = true;
                continue;
            }
            if (arg=="-mredo" || arg=="--mredo" || arg=="--model-redo") {
                params.model_test_again = true;
                continue;
            }
            if (arg=="-mtree" || arg=="--mtree") {
                params.model_test_and_tree = 1;
                continue;
            }
            if (arg=="-mretree") {
                params.model_test_and_tree = 2;
                continue;
            }
            if (arg=="-msep") {
                params.model_test_separate_rate = true;
                continue;
            }
            if (arg=="-mwopt" || arg=="--mix-opt") {
                params.optimize_mixmodel_weight = true;
                continue;
            }
            if (arg=="--opt-rate-mat") {
                params.optimize_rate_matrix = true;
                continue;
            }
//			if (arg=="-mh") {
//				params.mvh_site_rate = true;
//				params.discard_saturated_site = false;
//				params.SSE = LK_NORMAL;
//				continue;
//			}
//			if (arg=="-mhs") {
//				params.mvh_site_rate = true;
//				params.discard_saturated_site = true;
//				params.SSE = LK_NORMAL;
//				continue;
//			}
            if (arg=="-rl") {
                params.rate_mh_type = false;
                continue;
            }
            if (arg=="-nr") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -nr <mean_rate>";
                }
                params.mean_rate = convert_double(argv[cnt]);
                if (params.mean_rate < 0) {
                    throw "Wrong mean rate for MH model";
                }
                continue;
            }
            if (arg=="-mstore") {
                params.store_trans_matrix = true;
                continue;
            }
            if (arg=="-nni_lh") {
                params.nni_lh = true;
                continue;
            }
            if (arg=="-lmd") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -lmd <lambda>";
                }
                params.lambda = convert_double(argv[cnt]);
                if (params.lambda > 1.0) {
                    throw "Lambda must be in (0,1]";
                }
                continue;
            }
            if (arg=="-lk") {
                ++cnt;
                if (cnt >= argc) {
                    throw "-lk x86|SSE|AVX|FMA|AVX512";
                }
                arg=argv[cnt];
                if (arg=="x86") {
                    params.SSE = LK_386;
                }
                else if (arg=="SSE") {
                    params.SSE = LK_SSE2;
                }
                else if (arg=="AVX") {
                    params.SSE = LK_AVX;
                }
                else if (arg=="FMA") {
                    params.SSE = LK_AVX_FMA;
                }
                else if (arg=="AVX512") {
                    params.SSE = LK_AVX512;
                }
                else {
                    throw "Incorrect -lk likelihood kernel option";
                }
                continue;
            }
            if (arg=="-safe" || arg=="--safe") {
                params.lk_safe_scaling = true;
                continue;
            }
            if (arg=="-safe-seq") {
                ++cnt;
                if (cnt >= argc) {
                    throw "-safe-seq <number of sequences>";
                }
                params.numseq_safe_scaling = convert_int(argv[cnt]);
                if (params.numseq_safe_scaling < 10) {
                    throw "Too small -safe-seq";
                }
                continue;
            }
            if (arg=="--ignore-errors") {
                params.ignore_any_errors = true;
                continue;
            }
            if (arg=="--kernel-nonrev") {
                params.kernel_nonrev = true;
                continue;
            }
            if (arg=="-f") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -f <c | o | u | q | ry | ws | mk | <digits>>";
                }
                if (!parseStateFrequencyTypeName(argv[cnt], params.freq_type))
                {
                    //throws error message if can't parse
                    params.freq_type = parseStateFreqDigits(argv[cnt]);
                }
                continue;
            }
            if (arg=="--keep-zero-freq") {
                params.keep_zero_freq = true;
                continue;
            }
            if (arg=="--min-freq") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --min-freq NUM";
                }
                params.min_state_freq = convert_double(argv[cnt]);
                if (params.min_state_freq <= 0) {
                    throw "--min-freq must be positive";
                }
                if (params.min_state_freq >= 1.0) {
                    throw "--min-freq must be < 1.0";
                }
                continue;
            }
            if (arg=="--inc-zero-freq") {
                params.keep_zero_freq = false;
                continue;
            }
            if (arg=="-fs" || arg=="--site-freq") {
                if (params.tree_freq_file) {
                    throw "Specifying both -fs and -ft not allowed";
                }
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -fs <site_freq_file>";
                }
                params.site_freq_file = argv[cnt];
                //params.SSE = LK_EIGEN;
                continue;
            }
            if (arg=="-ft" || arg=="--tree-freq") {
                if (params.site_freq_file) {
                    throw "Specifying both -fs and -ft not allowed";
                }
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -ft <treefile_to_infer_site_frequency_model>";
                }
                params.tree_freq_file = argv[cnt];
                if (params.print_site_state_freq == WSF_NONE)
                    params.print_site_state_freq = WSF_POSTERIOR_MEAN;
                continue;
            }
            if (arg=="-fconst") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -fconst <const_pattern_frequencies>";
                }
                params.freq_const_patterns = argv[cnt];
                continue;
            }
            if (arg=="--nrate") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -c <#rate_category>";
                }
                params.num_rate_cats = convert_int(argv[cnt]);
                if (params.num_rate_cats < 1) {
                    throw "Wrong number of rate categories";
                }
                continue;
            }
            if (arg=="-cmin" || arg=="--cmin") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -cmin <#min_rate_category>";
                }
                params.min_rate_cats = convert_int(argv[cnt]);
                if (params.min_rate_cats < 2) {
                    throw "Wrong number of rate categories for -cmin";
                }
                continue;
            }
            if (arg=="-cmax" || arg=="--cmax") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -cmax <#max_rate_category>";
                }
                params.max_rate_cats = convert_int(argv[cnt]);
                if (params.max_rate_cats < 2) {
                    throw "Wrong number of rate categories for -cmax";
                }
                continue;
            }
            if (arg=="-a") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -a <gamma_shape>";
                }
                params.gamma_shape = convert_double(argv[cnt]);
                if (params.gamma_shape <= 0) {
                    throw "Wrong gamma shape parameter (alpha)";
                }
                continue;
            }
            if (arg=="-amin" || arg=="--alpha-min") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -amin <min_gamma_shape>";
                }
                params.min_gamma_shape = convert_double(argv[cnt]);
                if (params.min_gamma_shape <= 0) {
                    throw "Wrong minimum gamma shape parameter (alpha)";
                }
                continue;
            }
            if (arg=="-gmean" || arg=="--gamma-mean") {
                params.gamma_median = false;
                continue;
            }
            if (arg=="-gmedian" || arg=="--gamma-median") {
                params.gamma_median = true;
                continue;
            }
            if (arg=="-i") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -i <p_invar_sites>";
                }
                params.p_invar_sites = convert_double(argv[cnt]);
                if (params.p_invar_sites < 0) {
                    throw "Wrong number of proportion of invariable sites";
                }
                continue;
            }
            if (arg=="-optfromgiven") {
                params.optimize_from_given_params = true;
                continue;
            }
            if (arg=="-brent") {
                params.optimize_by_newton = false;
                continue;
            }
            if (arg=="-jointopt") {
                params.optimize_model_rate_joint = true;
                continue;
            }
            if (arg=="-brent_ginvar") {
                params.optimize_model_rate_joint = false;
                continue;
            }
            if (arg=="-fixbr" || arg=="-blfix") {
                params.fixed_branch_length = BRLEN_FIX;
                params.optimize_alg_gammai = "Brent";
                params.opt_gammai          = false;
                params.min_iterations      = 0;
                params.stop_condition      = SC_FIXED_ITERATION;
                continue;
            }
            if (arg=="-blscale") {
                params.fixed_branch_length = BRLEN_SCALE;
                params.optimize_alg_gammai = "Brent";
                params.opt_gammai          = false;
                params.min_iterations      = 0;
                params.stop_condition      = SC_FIXED_ITERATION;
                continue;
            }
            if (arg=="-blmin") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -blmin <min_branch_length>";
                }
                params.min_branch_length = convert_double(argv[cnt]);
                if (params.min_branch_length < 0.0) {
                    throw("Negative -blmin not allowed!");
                }
                if (params.min_branch_length == 0.0) {
                    throw("Zero -blmin is not allowed due to numerical problems");
                }
                if (params.min_branch_length > 0.1) {
                    throw("-blmin must be < 0.1");
                }
                continue;
            }
            if (arg=="-blmax") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -blmax <max_branch_length>";
                }
                params.max_branch_length = convert_double(argv[cnt]);
                if (params.max_branch_length < 0.5) {
                    throw("-blmax smaller than 0.5 is not allowed");
                }
                continue;
            }
            if (arg=="--show-lh") {
                params.ignore_identical_seqs = false;
                params.fixed_branch_length   = BRLEN_FIX;
                params.optimize_alg_gammai   = "Brent";
                params.opt_gammai            = false;
                params.min_iterations        = 0;
                params.stop_condition        = SC_FIXED_ITERATION;
                verbose_mode                 = VB_DEBUG;
                params.ignore_checkpoint     = true;
                continue;
            }
            if (arg=="-sr") {
                params.stop_condition = SC_WEIBULL;
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -sr <#max_iteration>";
                }
                params.max_iterations = convert_int(argv[cnt]);
                if (params.max_iterations <= params.min_iterations) {
                    throw "Specified max iteration"
                          " must be greater than min iteration";
                }
                continue;
            }
            if (arg=="-nm" || arg=="--nmax") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -nm <#max_iteration>";
                }
                params.max_iterations = convert_int(argv[cnt]);
                if (params.max_iterations <= params.min_iterations) {
                    throw "Specified max iteration"
                          " must be greater than min iteration";
                }
                continue;
            }
            if (arg=="-sc") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -sc <stop_confidence_value>";
                }
                params.stop_confidence = convert_double(argv[cnt]);
                if (params.stop_confidence <= 0.5
                    || params.stop_confidence >= 1) {
                    throw "Stop confidence value must be in range (0.5,1)";
                }
                continue;
            }
            if (arg=="--runs") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --runs <number_of_runs>";
                }
                params.num_runs = convert_int(argv[cnt]);
                if (params.num_runs < 1) {
                    throw "Positive --runs please";
                }
                continue;
            }
            if (arg=="-gurobi") {
                params.gurobi_format = true;
                continue;
            }
            if (arg=="-gthreads") {
                params.gurobi_format = true;
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -gthreads <gurobi_threads>";
                }
                params.gurobi_threads = convert_int(argv[cnt]);
                if (params.gurobi_threads < 1) {
                    throw "Wrong number of threads";
                }
                continue;
            }
            if (arg=="-b" || arg=="--boot" ||
                arg=="-j" || arg=="--jack" ||
                arg=="-bo" || arg=="--bonly") {
                params.multi_tree = true;
                if (arg=="-bo" || arg=="--bonly") {
                    params.compute_ml_tree = false;
                }
                else {
                    params.consensus_type = CT_CONSENSUS_TREE;
                }
                if ((arg=="-j" || arg=="--jack") &&
                    params.jackknife_prop == 0.0) {
                    params.jackknife_prop = 0.5;
                }
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -b <num_bootstrap_samples>";
                }
                params.num_bootstrap_samples = convert_int(argv[cnt]);
                if (params.num_bootstrap_samples < 1) {
                    throw "Wrong number of bootstrap samples";
                }
                if (params.num_bootstrap_samples == 1) {
                    params.compute_ml_tree = false;
                }
                if (params.num_bootstrap_samples == 1) {
                    params.consensus_type = CT_NONE;
                }
                continue;
            }
            if (arg=="--bsam" || arg=="-bsam" || arg=="--sampling") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -bsam <bootstrap_specification>";
                }
                params.bootstrap_spec = argv[cnt];
                params.remove_empty_seq = false;
                continue;
            }
            if (arg=="--subsample") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --subsample NUM";
                }
                params.subsampling = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="--subsample-seed") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --subsample-seed <random_seed>";
                }
                params.subsampling_seed = convert_int(argv[cnt]);
                continue;
            }
#ifdef USE_BOOSTER
            if (arg=="--tbe") {
                params.transfer_bootstrap = 1;
                continue;
            }
            if (arg=="--tbe-raw") {
                params.transfer_bootstrap = 2;
                continue;
            }
#endif
            if (arg=="-bc" || arg=="--bcon") {
                params.multi_tree = true;
                params.compute_ml_tree = false;
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -bc <num_bootstrap_samples>";
                }
                params.num_bootstrap_samples = convert_int(argv[cnt]);
                if (params.num_bootstrap_samples < 1) {
                    throw "Wrong number of bootstrap samples";
                }
                if (params.num_bootstrap_samples > 1) {
                    params.consensus_type = CT_CONSENSUS_TREE;
                }
                continue;
            }
            if (arg=="-iqppars") {
                params.iqp_assess_quartet = IQP_PARSIMONY;
                continue;
            }
            if (arg=="-iqp") {
                params.iqp = true;
                continue;
            }
            if (arg=="-wct") {
                params.write_candidate_trees = true;
                continue;
            }
            if (arg=="-wt" || arg=="--treels") {
                params.write_intermediate_trees = 1;
                continue;
            }
            if (arg=="-wdt") {
                params.writeDistImdTrees = true;
                continue;
            }
            if (arg=="-wtc") {
                params.write_intermediate_trees = 1;
                params.print_tree_lh = true;
                continue;
            }
            if (arg=="-wt2") {
                params.write_intermediate_trees = 2;
                //params.avoid_duplicated_trees = true;
                params.print_tree_lh = true;
                continue;
            }
            if (arg=="-wt3") {
                params.write_intermediate_trees = 3;
                //params.avoid_duplicated_trees = true;
                params.print_tree_lh = true;
                continue;
            }
            if (arg=="-wbl") {
                params.print_branch_lengths = true;
                continue;
            }
            if (arg=="-wit") {
                params.write_init_tree = true;
                continue;
            }
            if (arg=="--write-branches") {
                params.write_branches = true;
                continue;
            }
            if (arg=="-rf_all" || arg=="--tree-dist-all") {
                params.rf_dist_mode = RF_ALL_PAIR;
                continue;
            }
            if (arg=="-rf_adj") {
                params.rf_dist_mode = RF_ADJACENT_PAIR;
                continue;
            }
            if (arg=="-rf" || arg=="--tree-dist") {
                params.rf_dist_mode = RF_TWO_TREE_SETS;
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -rf <second_tree>";
                }
                params.second_tree = argv[cnt];
                continue;
            }
            if (arg=="-rf1" || arg=="--tree-dist1") {
                params.rf_dist_mode = RF_TWO_TREE_SETS;
                params.rf_same_pair = true;
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --tree-dist1 <second_tree>";
                }
                params.second_tree = argv[cnt];
                continue;
            }
            if (arg=="-rf2" || arg=="--tree-dist2") {
                params.rf_dist_mode = RF_TWO_TREE_SETS_EXTENDED;
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -rf2 <second_tree>";
                }
                params.second_tree = argv[cnt];
                continue;
            }
            if (arg=="--normalize-dist") {
                params.normalize_tree_dist = true;
                continue;
            }
            if (arg=="-aLRT") {
                ++cnt;
                if (cnt + 1 >= argc) {
                    throw "Use -aLRT <threshold%> <#replicates>";
                }
                params.aLRT_threshold = convert_int(argv[cnt]);
                if (params.aLRT_threshold < 85 || params.aLRT_threshold > 101) {
                    throw "aLRT threshold must be between 85 and 100";
                }
                ++cnt;
                params.aLRT_replicates = convert_int(argv[cnt]);
                if (params.aLRT_replicates < 1000
                    && params.aLRT_replicates != 0) {
                    throw "aLRT replicates must be at least 1000";
                }
                continue;
            }
            if (arg=="-alrt" || arg=="--alrt") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -alrt <#replicates | 0>";
                }
                int reps = convert_int(argv[cnt]);
                if (reps == 0) {
                    params.aLRT_test = true;
                }
                else {
                    params.aLRT_replicates = reps;
                    if (params.aLRT_replicates < 1000) {
                        throw "aLRT replicates must be at least 1000";
                    }
                }
                continue;
            }
            if (arg=="-abayes" || arg=="--abayes") {
                params.aBayes_test = true;
                continue;
            }
            if (arg=="-lbp" || arg=="--lbp") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -lbp <#replicates>";
                }
                params.localbp_replicates = convert_int(argv[cnt]);
                if (params.localbp_replicates < 1000
                    && params.localbp_replicates != 0) {
                    throw "Local bootstrap (LBP) replicates must be at least 1000";
                }
                continue;
            }
            if (arg=="-wsl" || arg=="--sitelh") {
                params.print_site_lh = WSL_SITE;
                continue;
            }
            if (arg=="-wpl" || arg=="--partlh") {
                params.print_partition_lh = true;
                continue;
            }
            if (arg=="-wslg" || arg=="-wslr") {
                params.print_site_lh = WSL_RATECAT;
                continue;
            }
            if (arg=="-wslm") {
                params.print_site_lh = WSL_MIXTURE;
                continue;
            }
            if (arg=="-wslmr" || arg=="-wslrm") {
                params.print_site_lh = WSL_MIXTURE_RATECAT;
                continue;
            }
            if (arg=="-wspr") {
                params.print_site_prob = WSL_RATECAT;
                continue;
            }
            if (arg=="-wspm") {
                params.print_site_prob = WSL_MIXTURE;
                continue;
            }
            if (arg=="-wspmr" || arg=="-wsprm") {
                params.print_site_prob = WSL_MIXTURE_RATECAT;
                continue;
            }
            if (arg=="-asr" || arg=="--ancestral") {
                params.print_ancestral_sequence = AST_MARGINAL;
                params.ignore_identical_seqs = false;
                continue;
            }
            if (arg=="-asr-min" || arg=="--asr-min") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -asr-min <probability>";
                }
                params.min_ancestral_prob = convert_double(argv[cnt]);
                if (params.min_ancestral_prob < 0 || params.min_ancestral_prob > 1) {
                    throw "Minimum ancestral probability [-asr-min] must be between 0 and 1.0";
                }
                continue;
            }
            if (arg=="-asr-joint") {
                params.print_ancestral_sequence = AST_JOINT;
                params.ignore_identical_seqs = false;
                continue;
            }
            if (arg=="-wsr" || arg=="--rate") {
                params.print_site_rate |= 1;
                continue;
            }
            if (arg=="--mlrate") {
                params.print_site_rate |= 2;
                continue;
            }
            if (arg=="-wsptrees") {
                params.print_trees_site_posterior = 1;
                continue;
            }
            if (arg=="-wsf") {
                params.print_site_state_freq = WSF_POSTERIOR_MEAN;
                continue;
            }
            if (arg=="--freq-max" || arg=="-fmax") {
                params.print_site_state_freq = WSF_POSTERIOR_MAX;
                continue;
            }
            if (arg=="-wba") {
                params.print_bootaln = true;
                continue;
            }
            if (arg=="-wbsf") {
                params.print_boot_site_freq = true;
                continue;
            }
            if (arg=="-wsa") {
                params.print_subaln = true;
                continue;
            }
            if (arg=="-wtl") {
                params.print_tree_lh = true;
                continue;
            }
            if (arg=="-wpi") {
                params.print_partition_info = true;
                params.print_conaln = true;
                continue;
            }
            if (arg=="-wca") {
                params.print_conaln = true;
                continue;
            }
            if (arg=="-wsplits") {
                params.print_splits_file = true;
                continue;
            }
            if (arg=="--no-splits.nex") {
                params.print_splits_nex_file = false;
                continue;
            }
            if (arg=="-ns") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -ns <num_simulations>";
                }
                params.whtest_simulations = convert_int(argv[cnt]);
                if (params.whtest_simulations < 1) {
                    throw "Wrong number of simulations for WH-test";
                }
                continue;
            }
            if (arg=="-mr") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -mr <rate_file>";
                }
                params.rate_file = argv[cnt];
                continue;
            }
            if (arg=="-cat_mean") {
                params.mcat_type |= MCAT_MEAN;
                continue;
            }
            if (arg=="-cat_nolog") {
                params.mcat_type &= (127 - MCAT_LOG);
                continue;
            }
            if (arg=="-cat_site") {
                params.mcat_type &= (127 - MCAT_PATTERN);
                continue;
            }
            if (arg=="-tina") {
                params.do_pars_multistate = true;
                params.ignore_checkpoint = true;
                continue;
            }
            if (arg=="-pval") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -pval <gene_pvalue_file>";
                }
                params.gene_pvalue_file = argv[cnt];
                continue;
            }
            if (arg=="-nnitest") {
                params.testNNI = true;
                continue;
            }
            if (arg=="-anni") {
                params.approximate_nni = true;
                continue;
            }
            if (arg=="-nnicut") {
                params.estimate_nni_cutoff = true;
                //nni_cutoff = -5.41/2;
                continue;
            }
            if (arg=="-nnichi2") {
                params.nni_cutoff = -5.41 / 2;
                continue;
            }
            if (arg=="-nnicutval") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -nnicutval <log_diff_value>";
                }
                params.nni_cutoff = convert_double(argv[cnt]);
                if (params.nni_cutoff >= 0) {
                    throw "cutoff value for -nnicutval must be negative";
                }
                continue;
            }
            if (arg=="-nnisort") {
                params.nni_sort = true;
                continue;
            }
            if (arg=="-plog") {
                params.gene_pvalue_loga = true;
                continue;
            }
            if (arg=="-dmp") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -dmp <ncbi_taxid>";
                }
                params.ncbi_taxid = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-dmplevel"
                || arg=="-dmprank") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -dmprank <ncbi_taxon_rank>";
                }
                params.ncbi_taxon_level = argv[cnt];
                continue;
            }
            if (arg=="-dmpignore") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -dmpignore <ncbi_ignore_level>";
                }
                params.ncbi_ignore_level = argv[cnt];
                continue;
            }
            if (arg=="-dmpname") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -dmpname <ncbi_names_file>";
                }
                params.ncbi_names_file = argv[cnt];
                continue;
            }
            if (arg=="-eco") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -eco <eco_dag_file>";
                }
                params.eco_dag_file = argv[cnt];
                continue;
            }
            if (arg=="-k%") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -k% <k in %>";
                }
                //convert_range(argv[cnt], params.k_percent, params.sub_size, params.step_size);
                params.k_percent = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-diet") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -diet <d in %>";
                }
                convert_range(argv[cnt], params.diet_min, params.diet_max,
                              params.diet_step);
                //params.diet = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-up") {
                params.upper_bound = true;
                continue;
            }
            if (arg=="-upNNI") {
                params.upper_bound_NNI = true;
            }
            if (arg=="-upFrac") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -upFrac <fraction>";
                }
                params.upper_bound_frac = convert_double(argv[cnt]);
            }
            if (arg=="-ecoR") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -ecoR <run number>";
                }
                params.eco_run = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-bb" || arg=="-B" || arg=="--ufboot" ||
                arg=="-J" || arg=="--ufjack") {
                if ((arg=="-J" || arg=="--ufjack") && params.jackknife_prop == 0.0) {
                    params.jackknife_prop = 0.5;
                }
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -B <#replicates>";
                }
                if (params.stop_condition == SC_FIXED_ITERATION) {
                    throw("Ultrafast bootstrap does not work with -fast, -te or -n option");
                }
                params.gbo_replicates = convert_int(argv[cnt]);
                //params.avoid_duplicated_trees = true;
                if (params.gbo_replicates < 1000) {
                    throw "#replicates must be >= 1000";
                }
                params.consensus_type = CT_CONSENSUS_TREE;
                params.stop_condition = SC_BOOTSTRAP_CORRELATION;
                //params.nni5Branches = true;
                continue;
            }
            if (arg=="-beps" || arg=="--beps") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -beps <epsilon>";
                }
                params.ufboot_epsilon = convert_double(argv[cnt]);
                if (params.ufboot_epsilon <= 0.0) {
                    throw "Epsilon must be positive";
                }
                continue;
            }
            if (arg=="-wbt" || arg=="--wbt" || arg=="--boot-trees") {
                params.print_ufboot_trees = 1;
                continue;
            }
            if (arg=="-wbtl" || arg=="--wbtl") {
                // print ufboot trees with branch lengths
                params.print_ufboot_trees = 2;
                continue;
            }
            if (arg=="-bs") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -bs <begin_sampling_size>";
                }
                params.check_gbo_sample_size = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-bmax") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -bmax <max_candidate_trees>";
                }
                params.max_candidate_trees = convert_int(argv[cnt]);
                continue;
            }
            //James B. 23-Dec-2020 Next line had a | where || was intended.
            if (arg=="-bcor" || arg=="--bcor") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -bcor <min_correlation>";
                }
                params.min_correlation = convert_double(argv[cnt]);
                continue;
            }
            if (arg=="--bnni" || arg=="-bnni") {
                params.ufboot2corr = true;
                // print ufboot trees with branch lengths
                // params.print_ufboot_trees = 2;
                // Diep: relocate to be below this for loop
                continue;
            }
            if (arg=="-u2c_nni5") {
                params.u2c_nni5 = true;
                continue;
            }
            if (arg=="-nstep" || arg=="--nstep") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -nstep <step_iterations>";
                }
                params.step_iterations = convert_int(argv[cnt]);
                if (params.step_iterations < 10
                    || params.step_iterations % 2 == 1)
                    throw "At least step size of 10 and even number please";
                params.min_iterations = params.step_iterations;
                continue;
            }
            if (arg=="-boff") {
                params.online_bootstrap = false;
                continue;
            }
            if (arg=="--jack-prop") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --jack-prop jackknife_proportion";
                }
                params.jackknife_prop = convert_double(argv[cnt]);
                if (params.jackknife_prop <= 0.0 || params.jackknife_prop >= 1.0) {
                    throw "Jackknife proportion must be between 0.0 and 1.0";
                }
                continue;
            }
            if (arg=="--robust-phy") {
                if (params.robust_median) {
                    throw "Can't couple --robust-phy with --robust-median";
                }
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --robust-phy proportion_of_best_sites_to_keep";
                }
                params.robust_phy_keep = convert_double(argv[cnt]);
                if (params.robust_phy_keep <= 0.0 || params.robust_phy_keep > 1.0) {
                    throw "--robust-phy parameter must be between 0 and 1";
                }
                params.optimize_by_newton    = false;
                params.optimize_alg_gammai   = "Brent";
                params.optimize_alg_freerate = "2-BFGS";
                continue;
            }

            if (arg=="--robust-median") {
                if (params.robust_phy_keep < 1.0) {
                    throw "Can't couple --robust-phy with --robust-median";
                }
                params.robust_median         = true;
                params.optimize_by_newton    = false;
                params.optimize_alg_gammai   = "Brent";
                params.optimize_alg_freerate = "2-BFGS";
                continue;
            }

            if (arg=="-mem" || arg=="--mem") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -mem max_mem_size";
                }
                params.lh_mem_save = LM_MEM_SAVE;
                int end_pos;
                double mem = convert_double(argv[cnt], end_pos);
                if (mem < 0) {
                    throw "-mem must be non-negative";
                }
                char suffix = argv[cnt][end_pos];
                if ( suffix == 'G') {
                    params.max_mem_size = mem * 1073741824.0;
                    params.max_mem_is_in_bytes = true;
                } else if ( suffix == 'M') {
                    params.max_mem_size = mem * 1048576.0;
                    params.max_mem_is_in_bytes = true;
                } else if ( suffix == '%'){
                    params.max_mem_size = mem * 0.01;
                    params.max_mem_is_in_bytes = false;
                    if (params.max_mem_size > 1) {
                        throw "-mem percentage must be between 0 and 100";
                    }
                } else {
                    if (mem > 1) {
                        throw "Invalid -mem option. Example: -mem 200M, -mem 10G -mem 50% -mem 0.5";
                    }
                    params.max_mem_is_in_bytes = false;
                    params.max_mem_size = mem;
                }
                continue;
            }
            if (arg=="--save-mem-buffer") {
                params.buffer_mem_save = true;
                continue;
            }
            if (arg=="--no-save-mem-buffer") {
                params.buffer_mem_save = false;
                continue;
            }
            if (arg=="-nodiff") {
                params.distinct_trees = false;
                continue;
            }
            if (arg=="-treediff") {
                params.distinct_trees = true;
                continue;
            }
            if (arg=="-norell") {
                params.use_rell_method = false;
                continue;
            }
            if (arg=="-elw") {
                params.use_elw_method = true;
                continue;
            }
            if (arg=="-noweight") {
                params.use_weighted_bootstrap = false;
                continue;
            }
            if (arg=="-nomore") {
                params.use_max_tree_per_bootstrap = true;
                continue;
            }
            if (arg=="-bweight") {
                params.use_weighted_bootstrap = true;
                continue;
            }
            if (arg=="-bmore") {
                params.use_max_tree_per_bootstrap = false;
                continue;
            }
            if (arg=="-gz") {
                params.do_compression = true;
                continue;
            }
            if (arg=="-newheu") {
                params.new_heuristic = true;
                // Enable RAxML kernel
                continue;
            }
            if (arg=="-maxtime") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -maxtime <time_in_minutes>";
                }
                params.maxtime = convert_double(argv[cnt]);
                params.min_iterations = 1000000;
                params.stop_condition = SC_REAL_TIME;
                continue;
            }
            if (arg=="--ninit" || arg=="-ninit") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -ninit <number_of_parsimony_trees>";
                }
                params.numInitTrees = convert_int(argv[cnt]);
                if (params.numInitTrees < 0) {
                    throw "-ninit must be non-negative";
                }
                if (params.numInitTrees < params.numNNITrees) {
                    params.numNNITrees = params.numInitTrees;
                }
                continue;
            }
            if (arg=="-fast" || arg=="--fast") {
                // fast search option to resemble FastTree
                if (params.gbo_replicates != 0) {
                    throw("Ultrafast bootstrap (-bb) does not work with -fast option");
                }
                params.numInitTrees = 2;
                if (params.min_iterations == -1) {
                    params.min_iterations = 2;
                }
                params.stop_condition = SC_FIXED_ITERATION;
                params.modelEps = 0.05;
                params.suppress_list_of_sequences = true;
                params.suppress_zero_distance_warnings = true;
                params.suppress_duplicate_sequence_warnings = true;
                params.optimize_alg_freerate = "1-BFGS";
                params.opt_gammai = false;
                continue;
            }
            if (arg=="-fss") {
                params.fixStableSplits = true;
                //params.five_plus_five = true;
                continue;
            }
            if (arg=="--stable-thres") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --stable-thres <support_value_threshold>";
                }
                params.stableSplitThreshold = convert_double(argv[cnt]);
                continue;
            }
            if (arg=="-ff") {
                params.five_plus_five = true;
                continue;
            }
            if (arg=="-tabu") {
                params.fixStableSplits = true;
                params.tabu = true;
                params.maxCandidates = params.numSupportTrees;
                continue;
            }
            if (arg=="--adt-pert") {
                if (params.tabu) {
                    throw("option -tabu and --adt-pert cannot be combined");
                }
                params.adaptPertubation = true;
                params.stableSplitThreshold = 1.0;
                continue;
            }
            if (arg=="-memcheck") {
                params.memCheck = true;
                continue;
            }
            if (arg=="--ntop" || arg=="-ntop") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -ntop <number_of_top_parsimony_trees>";
                }
                params.numNNITrees = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="--num-sup-trees") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --num-sup-trees <number_of_support_trees>";
                }
                params.numSupportTrees = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-fixai") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -fixai <alpha_invar_file>";
                }
                params.alpha_invar_file = argv[cnt];
                continue;
            }
            if (arg=="--opt-gamma-inv") {
                params.opt_gammai = true;
                continue;
            }
            if (arg=="--no-opt-gamma-inv") {
                params.opt_gammai = false;
                continue;
            }
            if (arg=="--opt-gammai-fast") {
                params.opt_gammai_fast = true;
                params.opt_gammai = true;
                continue;
            }
            if (arg=="--opt-gammai-kb") {
                params.opt_gammai_keep_bran = true;
                params.opt_gammai = true;
                continue;
            }
            if (arg=="--adaptive-eps") {
                params.testAlphaEpsAdaptive = true;
                continue;
            }
            if (arg=="--rand-alpha") {
                params.randomAlpha = true;
                continue;
            }
            if (arg=="-eai") {
                params.exh_ai = true;
                continue;
            }
            if (arg=="-poplim") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -poplim <max_pop_size>";
                }
                params.maxCandidates = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="--nbest" ||arg=="-nbest") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -nbest <number_of_candidate_trees>";
                }
                params.popSize = convert_int(argv[cnt]);
                ASSERT(params.popSize < params.numInitTrees);
                continue;
            }
            if (arg=="-beststart") {
                params.bestStart = true;
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -best_start <binary_alignment_file>";
                }
                params.binary_aln_file = argv[cnt];
                continue;
            }
            if (arg=="-pll") {
                throw("-pll option is discontinued.");
                params.pll = true;
                continue;
            }
            if (arg=="-me" || arg=="--epsilon") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -me <model_epsilon>";
                }
                params.modelEps = convert_double(argv[cnt]);
                if (params.modelEps <= 0.0) {
                    throw "Model epsilon must be positive";
                }
                if (params.modelEps > 1.0) {
                    throw "Model epsilon must not be larger than 1.0";
                }
                continue;
            }
            if (arg=="--mf-epsilon") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --mf-epsilon <modelfinder_epsilon>";
                }
                params.modelfinder_eps = convert_double(argv[cnt]);
                if (params.modelfinder_eps <= 0.0) {
                    throw "ModelFinder epsilon must be positive";
                }
                if (params.modelEps > 1.0) {
                    throw "ModelFinder epsilon must not be larger than 1.0";
                }
                continue;
            }
            if (arg=="-pars_ins") {
                params.reinsert_par = true;
                continue;
            }
            if (arg=="-allnni" || arg=="--allnni") {
                params.speednni = false;
                continue;
            }
            if (arg=="-snni") {
                params.snni = true;
                // dont need to turn this on here
                //params.autostop = true;
                //params.speednni = true;
                // Minh: why do you turn this on? it doubles curPerStrength at some point
                //params.adaptPert = true;
                continue;
            }
            if (arg=="-iqpnni") {
                params.snni = false;
                params.start_tree = STT_BIONJ;
                params.numNNITrees = 1;
                continue;
            }
            if (arg=="--nstop" || arg=="-nstop") {
                if (params.stop_condition != SC_BOOTSTRAP_CORRELATION) {
                    params.stop_condition = SC_UNSUCCESS_ITERATION;
                }
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -nstop <#iterations>";
                }
                params.unsuccess_iteration = convert_int(argv[cnt]);
                if (params.unsuccess_iteration <= 0) {
                    throw "-nstop iterations must be positive";
                }
                params.max_iterations = max(params.max_iterations,
                                            params.unsuccess_iteration*10);
                continue;
            }
            if (arg=="-lsbran") {
                params.leastSquareBranch = true;
                continue;
            }
            if (arg=="-manuel") {
                params.manuel_analytic_approx = true;
                continue;
            }
            if (arg=="-parsbran") {
                params.pars_branch_length = true;
                continue;
            }
            if (arg=="-bayesbran") {
                if (!params.compute_likelihood) {
                    throw "cannot combine -bayesbran and -no-ml options";
                }
                params.bayes_branch_length = true;
                continue;
            }
            if (arg=="-fivebran" || arg=="-nni5") {
                params.nni5 = true;
                params.nni_type = NNI5;
                continue;
            }
            if (arg=="-onebran" || arg=="-nni1") {
                params.nni_type = NNI1;
                params.nni5 = false;
                continue;
            }
            if (arg=="-nni-eval") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -nni-eval <num_evaluation>";
                }
                params.nni5_num_eval = convert_int(argv[cnt]);
                if (params.nni5_num_eval < 1) {
                    throw("Positive -nni-eval expected");
                }
                continue;
            }
            if (arg=="-bl-eval") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -bl-eval <num_evaluation>";
                }
                params.brlen_num_traversal = convert_int(argv[cnt]);
                if (params.brlen_num_traversal < 1) {
                    throw("Positive -bl-eval expected");
                }
                continue;
            }
            if (arg=="-smooth") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -smooth <num_iterations>";
                }
                params.numSmoothTree = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-lsnni") {
                params.leastSquareNNI = true;
                continue;
            }
            if (arg=="-lsvar") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -lsvar <o|ft|fm|st|p>";
                }
                arg = string_to_lower(argv[cnt]);
                if (arg=="o" || arg=="ols") {
                    params.ls_var_type = OLS;
                    continue;
                }
                if (arg=="ft" || arg=="first_taylor") {
                    params.ls_var_type = WLS_FIRST_TAYLOR;
                    continue;
                }
                if (arg=="fm" || arg=="fitch_margoliash") {
                    params.ls_var_type = WLS_FITCH_MARGOLIASH;
                    continue;
                }
                if (arg=="st"  || arg=="second_taylor") {
                    params.ls_var_type = WLS_SECOND_TAYLOR;
                    continue;
                }
                if (arg=="p"  || arg=="pauplin") {
                    params.ls_var_type = WLS_PAUPLIN;
                } else {
                    throw "Use -lsvar <o|ft|fm|st|p>";
                }
                continue;
            }
            if (arg=="-eps") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -eps <log-likelihood epsilon>";
                }
                params.loglh_epsilon = convert_double(argv[cnt]);
                continue;
            }
            if (arg=="-pb") { // Enable parsimony branch length estimation
                params.parbran = true;
                continue;
            }
            if (arg=="-x") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -x <iteration_multiple>";
                }
                params.iteration_multiple = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-sp_iter") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -sp_iter <number_iteration>";
                }
                params.speedup_iter = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-avh") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -avh <arndt_#bootstrap>";
                }
                params.avh_test = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-bootlh") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -bootlh <#replicates>";
                }
                params.bootlh_test = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-bootpart") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -bootpart <part1_length,part2_length,...>";
                }
                params.bootlh_partitions = argv[cnt];
                continue;
            }
            if (arg=="-AIC") {
                params.model_test_criterion = MTC_AIC;
                continue;
            }
            if (arg=="-AICc" || arg=="-AICC") {
                params.model_test_criterion = MTC_AICC;
                continue;
            }
            if (arg=="-merit" || arg=="--merit") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -merit AIC|AICC|BIC";
                }
                if (arg=="AIC") {
                    params.model_test_criterion = MTC_AIC;
                }
                else if (arg=="AICc" || arg=="AICC") {
                    params.model_test_criterion = MTC_AICC;
                }
                else if (arg=="BIC") {
                    params.model_test_criterion = MTC_BIC;
                }
                else {
                    throw "Use -merit AIC|AICC|BIC";
                }
                continue;
            }
            if (arg=="-ms") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -ms <model_test_sample_size>";
                }
                params.model_test_sample_size = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="-nt" || arg=="-c" ||
                arg=="-T"  || arg=="--threads") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -nt <num_threads|AUTO>";
                }
                if (iEquals(argv[cnt], "AUTO")) {
                    params.num_threads = 0;
                }
                else {
                    params.num_threads = convert_int(argv[cnt]);
                    if (params.num_threads < 1) {
                        throw "At least 1 thread please";
                    }
                }
                continue;
            }
            if (arg=="-ntmax" || arg=="--threads-max") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -ntmax <num_threads_max>";
                }
                params.num_threads_max = convert_int(argv[cnt]);
                if (params.num_threads_max < 1) {
                    throw "At least 1 thread please";
                }
                continue;
            }
            if (arg=="--thread-model") {
                params.openmp_by_model = true;
                continue;
            }
            if (arg=="--thread-site") {
                params.openmp_by_model = false;
                continue;
            }
            if (arg=="-ct") {
                params.count_trees = true;
                continue;
            }
            if (arg=="--sprrad" || arg=="--radius" || arg=="-spr-radius" ) {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -sprrad <SPR radius used in parsimony search>";
                }
                params.spr_radius = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="--mpcost") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --mpcost <parsimony_cost_file>";
                }
                params.sankoff_cost_file = argv[cnt];
                continue;
            }
            if (arg=="-no_rescale_gamma_invar") {
                params.no_rescale_gamma_invar = true;
                continue;
            }
            if (arg=="-wsi") {
                params.compute_seq_identity_along_tree = true;
                continue;
            }
            if (arg=="--no-seq-comp") {
                params.compute_seq_composition = false;
                continue;
            }
            if (arg=="-t" || arg=="-te" || arg=="--tree") {
                if (arg=="-te") {
                    if (params.gbo_replicates != 0) {
                        throw("Ultrafast bootstrap does not work with -te option");
                    }
                    params.min_iterations = 0;
                    params.stop_condition = SC_FIXED_ITERATION;
                }
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -t,-te <start_tree | BIONJ | PARS | PLLPARS | PJ | RANDOM>";
                }
                else if (!parseTreeName(argv[cnt], params)) {
                    params.user_file = argv[cnt];
                    if (params.min_iterations == 0) {
                        params.start_tree = STT_USER_TREE;
                    }
                }
                continue;
            }
            if (arg=="--no-ml-tree") {
                params.modelfinder_ml_tree = false;
                continue;
            }
            if (arg=="--tree-fix") {
                if (params.gbo_replicates != 0) {
                    outError("Ultrafast bootstrap does not work with -te option");
                }
                params.min_iterations = 0;
                params.stop_condition = SC_FIXED_ITERATION;
            }
            if (arg=="-g") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -g <constraint_tree>";
                }
                params.constraint_tree_file = argv[cnt];
                continue;
            }
            if (arg=="-lmap" || arg=="--lmap") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -lmap <likelihood_mapping_num_quartets>";
                }
                if (iEquals(argv[cnt], "ALL")) {
                    params.lmap_num_quartets = 0;
                }
                else {
                    params.lmap_num_quartets = convert_int64(argv[cnt]);
                    if (params.lmap_num_quartets < 0) {
                        throw "Number of quartets must be >= 1";
                    }
                }
                continue;
            }
            if (arg=="-lmclust" || arg=="--lmclust") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -lmclust <likelihood_mapping_cluster_file>";
                }
                params.lmap_cluster_file = argv[cnt];
                // '-keep_ident' is currently required to allow a 1-to-1 mapping of the
                // user-given groups (HAS) - possibly obsolete in the future versions
                params.ignore_identical_seqs = false;
                if (params.lmap_num_quartets < 0) {
                    params.lmap_num_quartets = 0;
                }
                continue;
            }
            if (arg=="-wql" || arg=="--quartetlh") {
                params.print_lmap_quartet_lh = true;
                continue;
            }
            if (arg=="-mixlen") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -mixlen <number of mixture branch lengths for heterotachy model>";
                }
                params.num_mixlen = convert_int(argv[cnt]);
                if (params.num_mixlen < 1) {
                    throw("-mixlen must be >= 1");
                }
                continue;
            }
            if (arg=="--link-alpha") {
                params.link_alpha = true;
                continue;
            }
            if (arg=="--link-model") {
                params.link_model = true;
                continue;
            }
            if (arg=="--model-joint") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --model-joint MODEL_NAME";
                }
                params.model_joint = argv[cnt];
                params.link_model = true;
                continue;
            }
            if (arg=="--unlink-tree") {
                params.partition_type = TOPO_UNLINKED;
                params.ignore_identical_seqs = false;
                continue;
            }
            if (arg=="-redo" || arg=="--redo") {
                params.ignore_checkpoint = true;
                // 2020-04-27: SEMANTIC CHANGE: also redo ModelFinder
                params.model_test_again = true;
                continue;
            }
            if (arg=="-tredo" || arg=="--tredo" || arg=="--redo-tree") {
                params.ignore_checkpoint = true;
                continue;
            }
            if (arg=="-undo" || arg=="--undo") {
                params.force_unfinished = true;
                continue;
            }
            if (arg=="-cptime" || arg=="--cptime") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use -cptime <checkpoint_time_interval>";
                }
                params.checkpoint_dump_interval = convert_int(argv[cnt]);
                continue;
            }
            if (arg=="--no-log") {
                params.suppress_output_flags |= OUT_LOG;
                continue;
            }
            if (arg=="--no-treefile") {
                params.suppress_output_flags |= OUT_TREEFILE;
                continue;
            }
            if (arg=="--no-iqtree") {
                params.suppress_output_flags |= OUT_IQTREE;
                continue;
            }
            if (arg=="--no-outfiles") {
                params.suppress_output_flags |= OUT_LOG + OUT_TREEFILE + OUT_IQTREE;
                continue;
            }
            // -- Mon Apr 17 21:18:23 BST 2017
            // DONE Minh: merged correctly.
            if (arg=="--scaling-squaring") {
                params.matrix_exp_technique = MET_SCALING_SQUARING;
                continue;
            }
            if (arg=="--eigenlib") {
                params.matrix_exp_technique = MET_EIGEN3LIB_DECOMPOSITION;
                continue;
            }
            if (arg=="--eigen") {
                params.matrix_exp_technique = MET_EIGEN_DECOMPOSITION;
                continue;
            }
            if (arg=="--lie-markov") {
                params.matrix_exp_technique = MET_LIE_MARKOV_DECOMPOSITION;
                continue;
            }
            if (arg=="--no-uniqueseq") {
                params.suppress_output_flags |= OUT_UNIQUESEQ;
                continue;
            }
            if (arg=="--dating") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --dating LSD";
                }
                params.dating_method = argv[cnt];
                if (params.dating_method != "LSD") {
                    throw "Currently only LSD (least-square dating) method is supported";
                }
                continue;
            }
            if (arg=="--date") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --date <date_file>|TAXNAME";
                }
                if (params.dating_method == "") {
                    params.dating_method = "LSD";
                }
                params.date_file = argv[cnt];
                continue;
            }
            if (arg=="--date-tip") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --date-tip <YYYY[-MM-DD]>";
                }
                if (params.dating_method == "") {
                    params.dating_method = "LSD";
                }
                params.date_tip = argv[cnt];
                continue;
            }
            if (arg=="--date-root") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --date-root <YYYY[-MM-DD]>";
                }
                if (params.dating_method == "") {
                    params.dating_method = "LSD";
                }
                params.date_root = argv[cnt];
                continue;
            }
            
            if (arg=="--date-no-outgroup") {
                params.date_with_outgroup = false;
                continue;
            }
            if (arg=="--date-outgroup") {
                params.date_with_outgroup = true;
                continue;
            }
            if (arg=="--date-ci") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --date-ci <number_of_replicates>";
                }
                params.date_replicates = convert_int(argv[cnt]);
                if (params.date_replicates < 1) {
                    throw "--date-ci must be positive";
                }
                continue;
            }
            if (arg=="--clock-sd") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --clock-sd <standard_dev_of_lognormal_relaxed_lock>";
                }
                params.clock_stddev = convert_double(argv[cnt]);
                if (params.clock_stddev < 0) {
                    throw "--clock-sd must be non-negative";
                }
                continue;
            }
            if (arg=="--date-outlier") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --date-outlier <z_score_for_removing_outlier_nodes>";
                }
                params.date_outlier = convert_double(argv[cnt]);
                if (params.date_outlier < 0) {
                    throw "--date-outlier must be non-negative";
                }
                continue;
            }
            if (arg=="--date-debug") {
                params.date_debug = true;
                continue;
            }
            if (arg=="--suppress-list-of-sequences") {
                params.suppress_list_of_sequences = true;
                continue;
            }
            if (arg=="--suppress-zero-distance") {
                params.suppress_zero_distance_warnings = true;
                continue;
            }
            if (arg=="--suppress-duplicate-sequence") {
                params.suppress_duplicate_sequence_warnings = true;
                continue;
            }
            if (arg=="--date-options") {
                ++cnt;
                if (cnt >= argc) {
                    throw "Use --date-options <extra_options_for_dating_method>";
                }
                params.dating_options = argv[cnt];
                continue;
            }
            if (arg=="-progress-bar" || arg=="--progress-bar" || arg=="-bar") {
                #if USE_PROGRESS_DISPLAY
                progress_display::setProgressDisplay(true);
                #endif
                continue;
            }
            if (argv[cnt][0] == '-') {
                string err = "Invalid \"";
                err += argv[cnt];
                err += "\" option.";
                throw err;
            } else {
                if (params.user_file.empty()) {
                    params.user_file = argv[cnt];
                }
                else {
                    params.out_file = argv[cnt];
                }
            }
        }
        // try
        catch (const char *str) {
            if (MPIHelper::getInstance().isMaster()) {
                outError(str);
            }
            else {
                exit(EXIT_SUCCESS);
            }
            //} catch (char *str) {
            //outError(str);
        } catch (string str) {
            if (MPIHelper::getInstance().isMaster()) {
                outError(str);
            }
            else {
                exit(EXIT_SUCCESS);
            }
        } catch (...) {
            string err = "Unknown argument \"";
            err += argv[cnt];
            err += "\"";
            if (MPIHelper::getInstance().isMaster()) {
                outError(err);
            }
            else {
                exit(EXIT_SUCCESS);
            }
        }

    } // for
    if (params.user_file.empty() && !params.aln_file && !params.ngs_file && !params.ngs_mapped_reads && !params.partition_file) {
#ifdef IQ_TREE
        quickStartGuide();
//        usage_iqtree(argv, false);
#else
        usage(argv, false);
#endif
    }

//    if (params.do_au_test)
//        outError("The AU test is temporarily disabled due to numerical issue when bp-RELL=0");

    if (params.root != NULL && params.is_rooted) {
        outError("Not allowed to specify both -o <taxon> and -root");
    }
    if (params.model_test_and_tree && params.partition_type != BRLEN_OPTIMIZE) {
        outError("-mtree not allowed with edge-linked partition model (-spp or -q)");
    }
    if (params.do_au_test && params.topotest_replicates == 0) {
        outError("For AU test please specify number of bootstrap replicates via -zb option");
    }
    if (params.lh_mem_save == LM_MEM_SAVE && params.partition_file) {
        outError("-mem option does not work with partition models yet");
    }
    if (params.gbo_replicates && params.num_bootstrap_samples) {
        outError("UFBoot (-bb) and standard bootstrap (-b) must not be specified together");
    }
    if ((params.model_name.find("ONLY") != string::npos || (params.model_name.substr(0,2) == "MF" && params.model_name.substr(0,3) != "MFP")) && (params.gbo_replicates || params.num_bootstrap_samples)) {
        outError("ModelFinder only cannot be combined with bootstrap analysis");
    }
    if (params.num_runs > 1 && !params.treeset_file.empty()) {
        outError("Can't combine --runs and -z options");
    }
    if (params.num_runs > 1 && params.lmap_num_quartets >= 0) {
        outError("Can't combine --runs and -lmap options");
    }
    if (params.terrace_analysis && !params.partition_file) {
        params.terrace_analysis = false;
    }
    if (params.constraint_tree_file && params.partition_type == TOPO_UNLINKED) {
        outError("-g constraint tree option does not work with -S yet.");
    }
    if (params.num_bootstrap_samples && params.partition_type == TOPO_UNLINKED) {
        outError("-b bootstrap option does not work with -S yet.");
    }
    if (params.dating_method != "") {
        #ifndef USE_LSD2
            outError("IQ-TREE was not compiled with LSD2 library, rerun cmake with -DUSE_LSD2=ON option");
        #endif
    }
    if (params.date_file.empty()) {
        if (params.date_root.empty() ^ params.date_tip.empty())
            outError("Both --date-root and --date-tip must be provided when --date file is absent");
    }
    // Diep:
    if(params.ufboot2corr == true) {
        if(params.gbo_replicates <= 0) {
            params.ufboot2corr = false;
        }
        else {
            params.stop_condition = SC_UNSUCCESS_ITERATION;
        }
        params.print_ufboot_trees = 2; // 2017-09-25: fix bug regarding the order of -bb 1000 -bnni -wbt
    }
    if (params.out_prefix.empty()) {
        if (params.eco_dag_file) {
            params.out_prefix = params.eco_dag_file;
        }
        else if (!params.user_file.empty() &&
                 params.consensus_type == CT_ASSIGN_SUPPORT_EXTENDED) {
            params.out_prefix = params.user_file;
        }
        else if (params.partition_file) {
            params.out_prefix = params.partition_file;
            if (params.out_prefix.back() == '/' ||
                params.out_prefix.back() == '\\') {
                params.out_prefix.pop_back();
            }
        } else if (params.aln_file) {
            params.out_prefix = params.aln_file;
            if (params.out_prefix.back() == '/' ||
                params.out_prefix.back() == '\\') {
                params.out_prefix.pop_back();
            }
        } else if (params.ngs_file) {
            params.out_prefix = params.ngs_file;
        }
        else if (params.ngs_mapped_reads) {
            params.out_prefix = params.ngs_mapped_reads;
        }
        else {
            params.out_prefix = params.user_file;
        }
    }

    if (params.model_name.find("LINK") != string::npos ||
        params.model_name.find("MERGE") != string::npos) {
        if (params.partition_merge == MERGE_NONE) {
            params.partition_merge = MERGE_RCLUSTERF;
        }
    }

    //    if (MPIHelper::getInstance().isWorker()) {
    // BUG: setting out_prefix this way cause access to stack, which is cleaned up after returning from this function
//        string newPrefix = string(params.out_prefix) + "."  + NumberToString(MPIHelper::getInstance().getProcessID()) ;
//        params.out_prefix = (char *) newPrefix.c_str();
//    }
    
    if (!params.additional_alignment_files.empty()) {
        params.incremental = true;
    }
}

void usage(char* argv[]) {
    printCopyright(cout);
    cout << "Usage: " << argv[0] << " [OPTIONS] <file_name> [<output_file>]" << endl;
    cout << "GENERAL OPTIONS:" << endl;
    cout << "  -hh               Print this help dialog" << endl;
    cout << "  -h                Print help options for phylogenetic inference" << endl;
    cout << "  <file_name>       User tree in NEWICK format or split network in NEXUS format" << endl;
    cout << "  <output_file>     Output file to store results, default is '<file_name>.pda'" << endl;
    cout << "  -k <num_taxa>     Find optimal set of size <num_taxa>" << endl;
    cout << "  -k <min>:<max>    Find optimal sets of size from <min> to <max>" << endl;
    cout << "  -k <min>:<max>:<step>" << endl;
    cout << "                    Find optimal sets of size min, min+step, min+2*step,..." << endl;
    cout << "  -o <taxon>        Root name to compute rooted PD (default: unrooted)" << endl;
    cout << "  -if <file>        File containing taxa to be included into optimal sets" << endl;
    cout << "  -e <file>         File containing branch/split scale and taxa weights" << endl;
    cout << "  -all              Identify all multiple optimal sets" << endl;
    cout << "  -lim <max_limit>  The maximum number of optimal sets for each k if -a is specified" << endl;
    cout << "  -min              Compute minimal sets (default: maximal)" << endl;
    cout << "  -1out             Print taxa sets and scores to separate files" << endl;
    cout << "  -oldout           Print output compatible with version 0.3" << endl;
    cout << "  -v                Verbose mode" << endl;
    cout << endl;
    cout << "OPTIONS FOR PHYLOGENETIC DIVERSITY (PD):" << endl;
    cout << "  -root             Make the tree ROOTED, default is unrooted" << endl;
    cout << "    NOTE: this option and -o <taxon> cannot be both specified" << endl;
    cout << "  -g                Run greedy algorithm only (default: auto)" << endl;
    cout << "  -pr               Run pruning algorithm only (default: auto)" << endl;
    cout << endl;
    /*
    cout << "OPTIONS FOR SPLIT DIVERSITY:" << endl;
    cout << "  -exhaust          Force to use exhaustive search" << endl;
    cout << "    NOTE: by default, the program applies dynamic programming algorithm" << endl;
    cout << "          on circular networks and exhaustive search on general networks" << endl;
    cout << endl;*/
    cout << "OPTIONS FOR BUDGET CONSTRAINTS:" << endl;
    cout << "  -u <file>         File containing total budget and taxa preservation costs" << endl;
    cout << "  -b <budget>       Total budget to conserve taxa" << endl;
    cout << "  -b <min>:<max>    Find all sets with budget from <min> to <max>" << endl;
    cout << "  -b <min>:<max>:<step>" << endl;
    cout << "                    Find optimal sets with budget min, min+step, min+2*step,..." << endl;
    cout << endl;
    cout << "OPTIONS FOR AREA ANALYSIS:" << endl;
    cout << "  -ts <taxa_file>   Compute/maximize PD/SD of areas (combine with -k to maximize)" << endl;
    cout << "  -excl             Compute exclusive PD/SD" << endl;
    cout << "  -endem            Compute endemic PD/SD" << endl;
    cout << "  -compl <areas>    Compute complementary PD/SD given the listed <areas>" << endl;
    cout << endl;

    cout << "OPTIONS FOR VIABILITY CONSTRAINTS:" << endl;
    cout << "  -eco <food_web>   File containing food web matrix" << endl;
    cout << "  -k% <n>           Find optimal set of size relative the total number of taxa" << endl;
    cout << "  -diet <min_diet>  Minimum diet portion (%) to be preserved for each predator" << endl;
    cout << endl;
    //if (!full_command) exit(0);

    cout << "MISCELLANEOUS:" << endl;
    cout << "  -dd <sample_size> Compute PD distribution of random sets of size k" << endl;
    /*
    cout << "  -gbo <sitelh_file> Compute and output the alignment of (normalized)" << endl;
    cout << "                    expected frequencies given in site_ll_file" << endl;
	*/

    //	cout << "  -rep <times>        Repeat algorithm a number of times." << endl;
    //	cout << "  -noout              Print no output file." << endl;
    cout << endl;
    //cout << "HIDDEN OPTIONS: see the source code file pda.cpp::parseArg()" << endl;

    exit(0);
}

void usage_iqtree(char* argv[], bool full_command) {
    printCopyright(cout);
    cout << "Usage: iqtree [-s ALIGNMENT] [-p PARTITION] [-m MODEL] [-t TREE] ..." << endl << endl;
    cout << "GENERAL OPTIONS:" << endl
    << "  -h, --help           Print (more) help usages" << endl
    << "  -s FILE[,...,FILE]   PHYLIP/FASTA/NEXUS/CLUSTAL/MSF alignment file(s)" << endl
    << "  -s DIR               Directory of alignment files" << endl
    << "  --seqtype STRING     BIN, DNA, AA, NT2AA, CODON, MORPH (default: auto-detect)" << endl
    << "  -t FILE|PARS|RAND    Starting tree (default: 99 parsimony and BIONJ)" << endl
    << "  -o TAX[,...,TAX]     Outgroup taxon (list) for writing .treefile" << endl
    << "  --prefix STRING      Prefix for all output files (default: aln/partition)" << endl
    << "  --seed NUM           Random seed number, normally used for debugging purpose" << endl
    << "  --safe               Safe likelihood kernel to avoid numerical underflow" << endl
    << "  --mem NUM[G|M|%]     Maximal RAM usage in GB | MB | %" << endl
    << "  --runs NUM           Number of indepedent runs (default: 1)" << endl
    << "  -v, --verbose        Verbose mode, printing more messages to screen" << endl
    << "  -V, --version        Display version number" << endl
    << "  --quiet              Quiet mode, suppress printing to screen (stdout)" << endl
    << "  -fconst f1,...,fN    Add constant patterns into alignment (N=no. states)" << endl
    << "  --epsilon NUM        Likelihood epsilon for parameter estimate (default 0.01)" << endl
#ifdef _OPENMP
    << "  -T NUM|AUTO          No. cores/threads or AUTO-detect (default: 1)" << endl
    << "  --threads-max NUM    Max number of threads for -T AUTO (default: all cores)" << endl
#endif
    << endl << "CHECKPOINT:" << endl
    << "  --redo               Redo both ModelFinder and tree search" << endl
    << "  --redo-tree          Restore ModelFinder and only redo tree search" << endl
    << "  --undo               Revoke finished run, used when changing some options" << endl
    << "  --cptime NUM         Minimum checkpoint interval (default: 60 sec and adapt)" << endl
    << endl << "PARTITION MODEL:" << endl
    << "  -p FILE|DIR          NEXUS/RAxML partition file or directory with alignments" << endl
    << "                       Edge-linked proportional partition model" << endl
    << "  -q FILE|DIR          Like -p but edge-linked equal partition model " << endl
    << "  -Q FILE|DIR          Like -p but edge-unlinked partition model" << endl
    << "  -S FILE|DIR          Like -p but separate tree inference" << endl
    << "  --subsample NUM      Randomly sub-sample partitions (negative for complement)" << endl
    << "  --subsample-seed NUM Random number seed for --subsample" << endl
    << endl << "LIKELIHOOD/QUARTET MAPPING:" << endl
    << "  --lmap NUM           Number of quartets for likelihood mapping analysis" << endl
    << "  --lmclust FILE       NEXUS file containing clusters for likelihood mapping" << endl
    << "  --quartetlh          Print quartet log-likelihoods to .quartetlh file" << endl
    << endl << "TREE SEARCH ALGORITHM:" << endl
//            << "  -pll                 Use phylogenetic likelihood library (PLL) (default: off)" << endl
    << "  --ninit NUM          Number of initial parsimony trees (default: 100)" << endl
    << "  --ntop NUM           Number of top initial trees (default: 20)" << endl
    << "  --nbest NUM          Number of best trees retained during search (defaut: 5)" << endl
    << "  -n NUM               Fix number of iterations to stop (default: OFF)" << endl
    << "  --nstop NUM          Number of unsuccessful iterations to stop (default: 100)" << endl
    << "  --perturb NUM        Perturbation strength for randomized NNI (default: 0.5)" << endl
    << "  --radius NUM         Radius for parsimony SPR search (default: 6)" << endl
    << "  --allnni             Perform more thorough NNI search (default: OFF)" << endl
    << "  -g FILE              (Multifurcating) topological constraint tree file" << endl
    << "  --fast               Fast search to resemble FastTree" << endl
    << "  --polytomy           Collapse near-zero branches into polytomy" << endl
    << "  --tree-fix           Fix -t tree (no tree search performed)" << endl
    << "  --treels             Write locally optimal trees into .treels file" << endl
    << "  --show-lh            Compute tree likelihood without optimisation" << endl
#ifdef IQTREE_TERRAPHAST
    << "  --terrace            Check if the tree lies on a phylogenetic terrace" << endl
#endif
//            << "  -iqp                 Use the IQP tree perturbation (default: randomized NNI)" << endl
//            << "  -iqpnni              Switch back to the old IQPNNI tree search algorithm" << endl
    << endl << "ULTRAFAST BOOTSTRAP/JACKKNIFE:" << endl
    << "  -B, --ufboot NUM     Replicates for ultrafast bootstrap (>=1000)" << endl
    << "  -J, --ufjack NUM     Replicates for ultrafast jackknife (>=1000)" << endl
    << "  --jack-prop NUM      Subsampling proportion for jackknife (default: 0.5)" << endl
    << "  --sampling STRING    GENE|GENESITE resampling for partitions (default: SITE)" << endl
    << "  --boot-trees         Write bootstrap trees to .ufboot file (default: none)" << endl
    << "  --wbtl               Like --boot-trees but also writing branch lengths" << endl
//            << "  -n <#iterations>     Minimum number of iterations (default: 100)" << endl
    << "  --nmax NUM           Maximum number of iterations (default: 1000)" << endl
    << "  --nstep NUM          Iterations for UFBoot stopping rule (default: 100)" << endl
    << "  --bcor NUM           Minimum correlation coefficient (default: 0.99)" << endl
    << "  --beps NUM           RELL epsilon to break tie (default: 0.5)" << endl
    << "  --bnni               Optimize UFBoot trees by NNI on bootstrap alignment" << endl
    << endl << "NON-PARAMETRIC BOOTSTRAP/JACKKNIFE:" << endl
    << "  -b, --boot NUM       Replicates for bootstrap + ML tree + consensus tree" << endl
    << "  -j, --jack NUM       Replicates for jackknife + ML tree + consensus tree" << endl
    << "  --jack-prop NUM      Subsampling proportion for jackknife (default: 0.5)" << endl
    << "  --bcon NUM           Replicates for bootstrap + consensus tree" << endl
    << "  --bonly NUM          Replicates for bootstrap only" << endl
#ifdef USE_BOOSTER
    << "  --tbe                Transfer bootstrap expectation" << endl
#endif
//            << "  -t <threshold>       Minimum bootstrap support [0...1) for consensus tree" << endl
    << endl << "SINGLE BRANCH TEST:" << endl
    << "  --alrt NUM           Replicates for SH approximate likelihood ratio test" << endl
    << "  --alrt 0             Parametric aLRT test (Anisimova and Gascuel 2006)" << endl
    << "  --abayes             approximate Bayes test (Anisimova et al. 2011)" << endl
    << "  --lbp NUM            Replicates for fast local bootstrap probabilities" << endl
    << endl << "MODEL-FINDER:" << endl
    << "  -m TESTONLY          Standard model selection (like jModelTest, ProtTest)" << endl
    << "  -m TEST              Standard model selection followed by tree inference" << endl
    << "  -m MF                Extended model selection with FreeRate heterogeneity" << endl
    << "  -m MFP               Extended model selection followed by tree inference" << endl
    << "  -m ...+LM            Additionally test Lie Markov models" << endl
    << "  -m ...+LMRY          Additionally test Lie Markov models with RY symmetry" << endl
    << "  -m ...+LMWS          Additionally test Lie Markov models with WS symmetry" << endl
    << "  -m ...+LMMK          Additionally test Lie Markov models with MK symmetry" << endl
    << "  -m ...+LMSS          Additionally test strand-symmetric models" << endl
    << "  --mset STRING        Restrict search to models supported by other programs" << endl
    << "                       (raxml, phyml or mrbayes)" << endl
    << "  --mset STR,...       Comma-separated model list (e.g. -mset WAG,LG,JTT)" << endl
    << "  --msub STRING        Amino-acid model source" << endl
    << "                       (nuclear, mitochondrial, chloroplast or viral)" << endl
    << "  --mfreq STR,...      List of state frequencies" << endl
    << "  --mrate STR,...      List of rate heterogeneity among sites" << endl
    << "                       (e.g. -mrate E,I,G,I+G,R is used for -m MF)" << endl
    << "  --cmin NUM           Min categories for FreeRate model [+R] (default: 2)" << endl
    << "  --cmax NUM           Max categories for FreeRate model [+R] (default: 10)" << endl
    << "  --merit AIC|AICc|BIC  Akaike|Bayesian information criterion (default: BIC)" << endl
//            << "  -msep                Perform model selection and then rate selection" << endl
    << "  --mtree              Perform full tree search for every model" << endl
    << "  --madd STR,...       List of mixture models to consider" << endl
    << "  --mdef FILE          Model definition NEXUS file (see Manual)" << endl
    << "  --modelomatic        Find best codon/protein/DNA models (Whelan et al. 2015)" << endl

    << endl << "PARTITION-FINDER:" << endl
    << "  --merge              Merge partitions to increase model fit" << endl
    << "  --merge greedy|rcluster|rclusterf" << endl
    << "                       Set merging algorithm (default: rclusterf)" << endl
    << "  --merge-model 1|all  Use only 1 or all models for merging (default: 1)" << endl
    << "  --merge-model STR,..." << endl
    << "                       Comma-separated model list for merging" << endl
    << "  --merge-rate 1|all   Use only 1 or all rate heterogeneity (default: 1)" << endl
    << "  --merge-rate STR,..." << endl
    << "                       Comma-separated rate list for merging" << endl
    << "  --rcluster NUM       Percentage of partition pairs for rcluster algorithm" << endl
    << "  --rclusterf NUM      Percentage of partition pairs for rclusterf algorithm" << endl
    << "  --rcluster-max NUM   Max number of partition pairs (default: 10*partitions)" << endl

    << endl << "SUBSTITUTION MODEL:" << endl
    << "  -m STRING            Model name string (e.g. GTR+F+I+G)" << endl
    << "                 DNA:  HKY (default), JC, F81, K2P, K3P, K81uf, TN/TrN, TNef," << endl
    << "                       TIM, TIMef, TVM, TVMef, SYM, GTR, or 6-digit model" << endl
    << "                       specification (e.g., 010010 = HKY)" << endl
    << "             Protein:  LG (default), Poisson, cpREV, mtREV, Dayhoff, mtMAM," << endl
    << "                       JTT, WAG, mtART, mtZOA, VT, rtREV, DCMut, PMB, HIVb," << endl
    << "                       HIVw, JTTDCMut, FLU, Blosum62, GTR20, mtMet, mtVer, mtInv, FLAVI," << endl
    << "			Q.LG, Q.pfam, Q.pfam_gb, Q.bird, Q.mammal, Q.insect, Q.plant, Q.yeast" << endl
    << "     Protein mixture:  C10,...,C60, EX2, EX3, EHO, UL2, UL3, EX_EHO, LG4M, LG4X" << endl
    << "              Binary:  JC2 (default), GTR2" << endl
    << "     Empirical codon:  KOSI07, SCHN05" << endl
    << "   Mechanistic codon:  GY (default), MG, MGK, GY0K, GY1KTS, GY1KTV, GY2K," << endl
    << "                       MG1KTS, MG1KTV, MG2K" << endl
    << "Semi-empirical codon:  XX_YY where XX is empirical and YY is mechanistic model" << endl
    << "      Morphology/SNP:  MK (default), ORDERED, GTR" << endl
    << "      Lie Markov DNA:  1.1, 2.2b, 3.3a, 3.3b, 3.3c, 3.4, 4.4a, 4.4b, 4.5a," << endl
    << "                       4.5b, 5.6a, 5.6b, 5.7a, 5.7b, 5.7c, 5.11a, 5.11b, 5.11c," << endl
    << "                       5.16, 6.6, 6.7a, 6.7b, 6.8a, 6.8b, 6.17a, 6.17b, 8.8," << endl
    << "                       8.10a, 8.10b, 8.16, 8.17, 8.18, 9.20a, 9.20b, 10.12," << endl
    << "                       10.34, 12.12 (optionally prefixed by RY, WS or MK)" << endl
    << "      Non-reversible:  STRSYM (strand symmetric model, equiv. WS6.6)," << endl
    << "                       NONREV, UNREST (unrestricted model, equiv. 12.12)" << endl
    << "           Otherwise:  Name of file containing user-model parameters" << endl
    << endl << "STATE FREQUENCY:" << endl
    << "  -m ...+F             Empirically counted frequencies from alignment" << endl
    << "  -m ...+FO            Optimized frequencies by maximum-likelihood" << endl
    << "  -m ...+FQ            Equal frequencies" << endl
    << "  -m ...+FRY           For DNA, freq(A+G)=1/2=freq(C+T)" << endl
    << "  -m ...+FWS           For DNA, freq(A+T)=1/2=freq(C+G)" << endl
    << "  -m ...+FMK           For DNA, freq(A+C)=1/2=freq(G+T)" << endl
    << "  -m ...+Fabcd         4-digit constraint on ACGT frequency" << endl
    << "                       (e.g. +F1221 means f_A=f_T, f_C=f_G)" << endl
    << "  -m ...+FU            Amino-acid frequencies given protein matrix" << endl
    << "  -m ...+F1x4          Equal NT frequencies over three codon positions" << endl
    << "  -m ...+F3x4          Unequal NT frequencies over three codon positions" << endl

    << endl << "RATE HETEROGENEITY AMONG SITES:" << endl
    << "  -m ...+I             A proportion of invariable sites" << endl
    << "  -m ...+G[n]          Discrete Gamma model with n categories (default n=4)" << endl
    << "  -m ...*G[n]          Discrete Gamma model with unlinked model parameters" << endl
    << "  -m ...+I+G[n]        Invariable sites plus Gamma model with n categories" << endl
    << "  -m ...+R[n]          FreeRate model with n categories (default n=4)" << endl
    << "  -m ...*R[n]          FreeRate model with unlinked model parameters" << endl
    << "  -m ...+I+R[n]        Invariable sites plus FreeRate model with n categories" << endl
    << "  -m ...+Hn            Heterotachy model with n classes" << endl
    << "  -m ...*Hn            Heterotachy model with n classes and unlinked parameters" << endl
    << "  --alpha-min NUM      Min Gamma shape parameter for site rates (default: 0.02)" << endl
    << "  --gamma-median       Median approximation for +G site rates (default: mean)" << endl
    << "  --rate               Write empirical Bayesian site rates to .rate file" << endl
    << "  --mlrate             Write maximum likelihood site rates to .mlrate file" << endl
//            << "  --mhrate             Computing site-specific rates to .mhrate file using" << endl
//            << "                       Meyer & von Haeseler (2003) method" << endl

    << endl << "POLYMORPHISM AWARE MODELS (PoMo):"                                           << endl
    << "  -s FILE              Input counts file (see manual)"                               << endl
    << "  -m ...+P             DNA substitution model (see above) used with PoMo"            << endl
    << "  -m ...+N<POPSIZE>    Virtual population size (default: 9)"                         << endl
// TODO DS: Maybe change default to +WH.
    << "  -m ...+WB|WH|S]      Weighted binomial sampling"       << endl
    << "  -m ...+WH            Weighted hypergeometric sampling" << endl
    << "  -m ...+S             Sampled sampling"              << endl
    << "  -m ...+G[n]          Discrete Gamma rate with n categories (default n=4)"    << endl
// TODO DS: Maybe change default to +WH.

    << endl << "COMPLEX MODELS:" << endl
    << "  -m \"MIX{m1,...,mK}\"  Mixture model with K components" << endl
    << "  -m \"FMIX{f1,...fK}\"  Frequency mixture model with K components" << endl
    << "  --mix-opt            Optimize mixture weights (default: detect)" << endl
    << "  -m ...+ASC           Ascertainment bias correction" << endl
    << "  --tree-freq FILE     Input tree to infer site frequency model" << endl
    << "  --site-freq FILE     Input site frequency model file" << endl
    << "  --freq-max           Posterior maximum instead of mean approximation" << endl

    << endl << "TREE TOPOLOGY TEST:" << endl
    << "  --trees FILE         Set of trees to evaluate log-likelihoods" << endl
    << "  --test NUM           Replicates for topology test" << endl
    << "  --test-weight        Perform weighted KH and SH tests" << endl
    << "  --test-au            Approximately unbiased (AU) test (Shimodaira 2002)" << endl
    << "  --sitelh             Write site log-likelihoods to .sitelh file" << endl

    << endl << "ANCESTRAL STATE RECONSTRUCTION:" << endl
    << "  --ancestral          Ancestral state reconstruction by empirical Bayes" << endl
    << "  --asr-min NUM        Min probability of ancestral state (default: equil freq)" << endl

    << endl << "TEST OF SYMMETRY:" << endl
    << "  --symtest               Perform three tests of symmetry" << endl
    << "  --symtest-only          Do --symtest then exist" << endl
//    << "  --bisymtest             Perform three binomial tests of symmetry" << endl
//    << "  --symtest-perm NUM      Replicates for permutation tests of symmetry" << endl
    << "  --symtest-remove-bad    Do --symtest and remove bad partitions" << endl
    << "  --symtest-remove-good   Do --symtest and remove good partitions" << endl
    << "  --symtest-type MAR|INT  Use MARginal/INTernal test when removing partitions" << endl
    << "  --symtest-pval NUMER    P-value cutoff (default: 0.05)" << endl
    << "  --symtest-keep-zero     Keep NAs in the tests" << endl

    << endl << "CONCORDANCE FACTOR ANALYSIS:" << endl
    << "  -t FILE              Reference tree to assign concordance factor" << endl
    << "  --gcf FILE           Set of source trees for gene concordance factor (gCF)" << endl
    << "  --df-tree            Write discordant trees associated with gDF1" << endl
    << "  --scf NUM            Number of quartets for site concordance factor (sCF)" << endl
    << "  -s FILE              Sequence alignment for --scf" << endl
    << "  -p FILE|DIR          Partition file or directory for --scf" << endl
    << "  --cf-verbose         Write CF per tree/locus to cf.stat_tree/_loci" << endl
    << "  --cf-quartet         Write sCF for all resampled quartets to .cf.quartet" << endl

#ifdef USE_LSD2
    << endl << "TIME TREE RECONSTRUCTION:" << endl
    << "  --date FILE          File containing dates of tips or ancestral nodes" << endl
    << "  --date TAXNAME       Extract dates from taxon names after last '|'" << endl
    << "  --date-tip STRING    Tip dates as a real number or YYYY-MM-DD" << endl
    << "  --date-root STRING   Root date as a real number or YYYY-MM-DD" << endl
    << "  --date-ci NUM        Number of replicates to compute confidence interval" << endl
    << "  --clock-sd NUM       Std-dev for lognormal relaxed clock (default: 0.2)" << endl
    << "  --date-no-outgroup   Exclude outgroup from time tree" << endl
    << "  --date-outlier NUM   Z-score cutoff to remove outlier tips/nodes (e.g. 3)" << endl
    << "  --date-options \"..\"  Extra options passing directly to LSD2" << endl
    << "  --dating STRING      Dating method: LSD for least square dating (default)" << endl
#endif
    << endl;
    

//            << endl << "TEST OF MODEL HOMOGENEITY:" << endl
//            << "  -m WHTEST            Testing model (GTR+G) homogeneity assumption using" << endl
//            << "                       Weiss & von Haeseler (2003) method" << endl
//            << "  -ns <#simulations>   #Simulations to obtain null-distribution (default: 1000)" << endl

    if (full_command)
    cout
        << endl << "CONSENSUS RECONSTRUCTION:" << endl
        << "  -t FILE              Set of input trees for consensus reconstruction" << endl
        << "  --sup-min NUM        Min split support, 0.5 for majority-rule consensus" << endl
        << "                       (default: 0, extended consensus)" << endl
        << "  --burnin NUM         Burnin number of trees to ignore" << endl
        << "  --con-tree           Compute consensus tree to .contree file" << endl
        << "  --con-net            Computing consensus network to .nex file" << endl
        << "  --support FILE       Assign support values into this tree from -t trees" << endl
        //<< "  -sup2 FILE           Like -sup but -t trees can have unequal taxon sets" << endl
        << "  --suptag STRING      Node name (or ALL) to assign tree IDs where node occurs" << endl
        << endl << "TREE DISTANCE BY ROBINSON-FOULDS (RF) METRIC:" << endl
        << "  --tree-dist-all      Compute all-to-all RF distances for -t trees" << endl
        << "  --tree-dist FILE     Compute RF distances between -t trees and this set" << endl
        << "  --tree-dist2 FILE    Like -rf but trees can have unequal taxon sets" << endl
    //            << "  -rf_adj              Computing RF distances of adjacent trees in <treefile>" << endl
    //            << "  -wja                 Write ancestral sequences by joint reconstruction" << endl


        << endl

        << "GENERATING RANDOM TREES:" << endl
        << "  -r NUM               No. taxa for Yule-Harding random tree" << endl
        << "  --rand UNI|CAT|BAL   UNIform | CATerpillar | BALanced random tree" << endl
        //<< "  --rand NET           Random circular split network" << endl
        << "  --rlen NUM NUM NUM   min, mean, and max random branch lengths" << endl

        << endl << "MISCELLANEOUS:" << endl
        << "  --keep-ident         Keep identical sequences (default: remove & finally add)" << endl
        << "  -blfix               Fix branch lengths of user tree passed via -te" << endl
        << "  -blscale             Scale branch lengths of user tree passed via -t" << endl
        << "  -blmin               Min branch length for optimization (default 0.000001)" << endl
        << "  -blmax               Max branch length for optimization (default 100)" << endl
        << "  -wslr                Write site log-likelihoods per rate category" << endl
        << "  -wslm                Write site log-likelihoods per mixture class" << endl
        << "  -wslmr               Write site log-likelihoods per mixture+rate class" << endl
        << "  -wspr                Write site probabilities per rate category" << endl
        << "  -wspm                Write site probabilities per mixture class" << endl
        << "  -wspmr               Write site probabilities per mixture+rate class" << endl
        << "  --partlh             Write partition log-likelihoods to .partlh file" << endl
        << "  --no-outfiles        Suppress printing output files" << endl
        << "  --eigenlib           Use Eigen3 library" << endl
        << "  -alninfo             Print alignment sites statistics to .alninfo" << endl
    //            << "  -d <file>            Reading genetic distances from file (default: JC)" << endl
    //			<< "  -d <outfile>         Calculate the distance matrix inferred from tree" << endl
    //			<< "  -stats <outfile>     Output some statistics about branch lengths" << endl
    //			<< "  -comp <treefile>     Compare tree with each in the input trees" << endl;

        << endl;

    if (full_command) {
        //TODO Print other options here (to be added)
    }
    exit(0);
}

void quickStartGuide() {
    printCopyright(cout);
    cout << "Command-line examples (replace 'iqtree2 ...' by actual path to executable):" << endl << endl
         << "1. Infer maximum-likelihood tree from a sequence alignment (example.phy)" << endl
         << "   with the best-fit model automatically selected by ModelFinder:" << endl
         << "     iqtree2 -s example.phy" << endl << endl
         << "2. Perform ModelFinder without subsequent tree inference:" << endl
         << "     iqtree2 -s example.phy -m MF" << endl
         << "   (use '-m TEST' to resemble jModelTest/ProtTest)" << endl << endl
         << "3. Combine ModelFinder, tree search, ultrafast bootstrap and SH-aLRT test:" << endl
         << "     iqtree2 -s example.phy --alrt 1000 -B 1000" << endl << endl
         << "4. Perform edge-linked proportional partition model (example.nex):" << endl
         << "     iqtree2 -s example.phy -p example.nex" << endl
         << "   (replace '-p' by '-Q' for edge-unlinked model)" << endl << endl
         << "5. Find best partition scheme by possibly merging partitions:" << endl
         << "     iqtree2 -s example.phy -p example.nex -m MF+MERGE" << endl
         << "   (use '-m TESTMERGEONLY' to resemble PartitionFinder)" << endl << endl
         << "6. Find best partition scheme followed by tree inference and bootstrap:" << endl
         << "     iqtree2 -s example.phy -p example.nex -m MFP+MERGE -B 1000" << endl << endl
#ifdef _OPENMP
         << "7. Use 4 CPU cores to speed up computation: add '-T 4' option" << endl << endl
#endif
         << "8. Polymorphism-aware model with HKY nucleotide model and Gamma rate:" << endl
         << "     iqtree2 -s counts_file.cf -m HKY+P+G" << endl << endl
         << "9. PoMo mixture with virtual popsize 5 and weighted binomial sampling:" << endl
         << "     iqtree2 -s counts_file.cf -m \"MIX{HKY+P{EMP},JC+P}+N5+WB\"" << endl << endl
         << "To show all available options: run 'iqtree2 -h'" << endl << endl
         << "Have a look at the tutorial and manual for more information:" << endl
         << "     http://www.iqtree.org" << endl << endl;
    exit(0);
}

InputType detectInputFile(const char *input_file) {
    if (!fileExists(input_file)) {
        outError("File not found ", input_file);
    }
    try {
        igzstream in;
        in.exceptions(ios::failbit | ios::badbit);
        in.open(input_file);

        unsigned char ch, ch2;
        int count = 0;
        do {
            in >> ch;
        } while (ch <= 32 && !in.eof() && count++ < 20);
        in >> ch2;
        in.close();
        switch (ch) {
            case '#': return IN_NEXUS;
            case '(': return IN_NEWICK;
            case '[': return IN_NEWICK;
            case '>': return IN_FASTA;
            case 'C': if (ch2 == 'L') return IN_CLUSTAL;
                      else if (ch2 == 'O') return IN_COUNTS;
                      else return IN_OTHER;
            case '!': if (ch2 == '!') return IN_MSF; else return IN_OTHER;
            default:
                if (isdigit(ch)) return IN_PHYLIP;
                return IN_OTHER;
        }
    } catch (ios::failure) {
        outError("Cannot read file ", input_file);
    } catch (...) {
        outError("Cannot read file ", input_file);
    }
    return IN_OTHER;
}

bool overwriteFile(const char *filename) {
    ifstream infile(filename);
    if (infile.is_open()) {
        cout << "Overwrite " << filename << " (y/n)? ";
        char ch;
        cin >> ch;
        if (ch != 'Y' && ch != 'y') {
            infile.close();
            return false;
        }
    }
    infile.close();
    return true;
}

void parseAreaName(char *area_names, set<string> &areas) {
    string all = area_names;
    while (!all.empty()) {
        auto pos = all.find(',');
        if (pos == std::string::npos) {
            pos = all.length();
        }
        areas.insert(all.substr(0, pos));
        if (pos >= (signed int) all.length()) {
            all = "";
        }
        else {
            all = all.substr(pos + 1);
        }
    }
}

double logFac(const int num) {
    if (num < 0) {
        return -1.0;
    }
    if (num == 0) {
        return 0.0;
    }
    double ret = 0;
    for (int i = 1; i <= num; ++i) {
        ret += log((double) i);
    }
    return ret;
}

template <typename I>
I random_element(I begin, I end)
{
    const unsigned long n = std::distance(begin, end);
    const unsigned long divisor = (RAND_MAX + 1) / n;

    unsigned long k;
    do { k = std::rand() / divisor; } while (k >= n);

    return std::advance(begin, k);
}

template <class T>
inline T quantile(const vector<T>& v, const double q) {
    unsigned int size = v.size();
    if (q <= 0) return *std::min_element(v.begin(), v.end());
    if (q >= 1) return *std::max_element(v.begin(), v.end());
    //double pos = (size - 1) * q;
    //unsigned int ind = (unsigned int)(pos);
    //double delta = pos - ind;
    vector<T> w(size);
    std::copy(v, v.begin() + size, w.begin());
}

#define RAN_STANDARD 1
#define RAN_SPRNG    2
#define RAN_RAND4    3

#define RAN_TYPE 2

#if RAN_TYPE == RAN_STANDARD

int init_random(int seed) {
    srand(seed);
    cout << "(Using rand() - Standard Random Number Generator)" << endl;
    return seed;
}

int finish_random() {
	return 0;
}


#elif RAN_TYPE == RAN_RAND4
/******************************************************************************/
/* random numbers generator  (Numerical recipes)                              */
/******************************************************************************/

/* variable */
long _idum;

/* definitions */
#define IM1 2147483563
#define IM2 2147483399
#define AM (1.0/IM1)
#define IMM1 (IM1-1)
#define IA1 40014
#define IA2 40692
#define IQ1 53668
#define IQ2 52774
#define IR1 12211
#define IR2 3791
#define NTAB 32
#define NDIV (1+IMM1/NTAB)
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

double randomunitintervall()
/* Long period (> 2e18) random number generator. Returns a uniform random
   deviate between 0.0 and 1.0 (exclusive of endpoint values).

   Source:
   Press et al., "Numerical recipes in C", Cambridge University Press, 1992
   (chapter 7 "Random numbers", ran2 random number generator) */ {
    int j;
    long k;
    static long _idum2 = 123456789;
    static long iy = 0;
    static long iv[NTAB];
    double temp;

    if (_idum <= 0) {
        if (-(_idum) < 1)
            _idum = 1;
        else
            _idum = -(_idum);
        _idum2 = (_idum);
        for (j = NTAB + 7; j >= 0; j--) {
            k = (_idum) / IQ1;
            _idum = IA1 * (_idum - k * IQ1) - k*IR1;
            if (_idum < 0)
                _idum += IM1;
            if (j < NTAB)
                iv[j] = _idum;
        }
        iy = iv[0];
    }
    k = (_idum) / IQ1;
    _idum = IA1 * (_idum - k * IQ1) - k*IR1;
    if (_idum < 0)
        _idum += IM1;
    k = _idum2 / IQ2;
    _idum2 = IA2 * (_idum2 - k * IQ2) - k*IR2;
    if (_idum2 < 0)
        _idum2 += IM2;
    j = iy / NDIV;
    iy = iv[j] - _idum2;
    iv[j] = _idum;
    if (iy < 1)
        iy += IMM1;
    if ((temp = AM * iy) > RNMX)
        return RNMX;
    else
        return temp;
} /* randomunitintervall */

#undef IM1
#undef IM2
#undef AM
#undef IMM1
#undef IA1
#undef IA2
#undef IQ1
#undef IQ2
#undef IR1
#undef IR2
#undef NTAB
#undef NDIV
#undef EPS
#undef RNMX

int init_random(int seed) /* RAND4 */ {
    //    srand((unsigned) time(NULL));
    //    if (seed < 0)
    // 	seed = rand();
    _idum = -(long) seed;
#ifndef PARALLEL
    cout << "(Using RAND4 Random Number Generator)" << endl;
#else /* PARALLEL */
    {
        int n;
        if (PP_IamMaster) {
            cout << "(Using RAND4 Random Number Generator with leapfrog method)" << endl;
        }
        for (n = 0; n < PP_Myid; ++n)
            (void) randomunitintervall();
        if (verbose_mode >= VB_MED) {
            cout << "(" << PP_Myid << ") !!! random seed set to " << seed << ", " << n << " drawn !!!" << endl;
        }
    }
#endif
    return (seed);
} /* initrandom */

int finish_random() {
	return 0;
}
/******************/

#else /* SPRNG */

/******************/

int *randstream;

int init_random(int seed, bool write_info, int** rstream) {
    //    srand((unsigned) time(NULL));
    if (seed < 0)
        seed = make_sprng_seed();
#ifndef PARALLEL
    if (write_info)
    	cout << "(Using SPRNG - Scalable Parallel Random Number Generator)" << endl;
    if (rstream) {
        *rstream = init_sprng(0, 1, seed, SPRNG_DEFAULT); /*init stream*/
    } else {
        randstream = init_sprng(0, 1, seed, SPRNG_DEFAULT); /*init stream*/
        if (verbose_mode >= VB_MED) {
            print_sprng(randstream);
        }
    }
#else /* PARALLEL */
    if (PP_IamMaster && write_info) {
        cout << "(Using SPRNG - Scalable Parallel Random Number Generator)" << endl;
    }
    /* MPI_Bcast(&seed, 1, MPI_UNSIGNED, PP_MyMaster, MPI_COMM_WORLD); */
    if (rstream) {
        *rstream = init_sprng(PP_Myid, PP_NumProcs, seed, SPRNG_DEFAULT); /*initialize stream*/
    } else {
        randstream = init_sprng(PP_Myid, PP_NumProcs, seed, SPRNG_DEFAULT); /*initialize stream*/
        if (verbose_mode >= VB_MED) {
            cout << "(" << PP_Myid << ") !!! random seed set to " << seed << " !!!" << endl;
            print_sprng(randstream);
        }
    }
#endif /* PARALLEL */
    return (seed);
} /* initrandom */

int finish_random(int *rstream) {
    if (rstream)
        return free_sprng(rstream);
    else
        return free_sprng(randstream);
}

#endif /* USE_SPRNG */

/******************/

/* returns a random integer in the range [0; n - 1] */
int random_int(int n, int *rstream) {
    return (int) floor(random_double(rstream) * n);
} /* randominteger */

/* returns a random integer in the range [a; b] */
int random_int(int a, int b) {
	ASSERT(b > a);
	//return a + (RAND_MAX * rand() + rand()) % (b + 1 - a);
	return a + random_int(b - a);
}

double random_double(int *rstream) {
#ifndef FIXEDINTRAND
#ifndef PARALLEL
#if RAN_TYPE == RAN_STANDARD
    return ((double) rand()) / ((double) RAND_MAX + 1);
#elif RAN_TYPE == RAN_SPRNG
    if (rstream)
        return sprng(rstream);
    else
        return sprng(randstream);
#else /* NO_SPRNG */
    return randomunitintervall();
#endif /* NO_SPRNG */
#else /* NOT PARALLEL */
#if RAN_TYPE == RAN_SPRNG
    if (rstream)
        return sprng(rstream);
    else
        return sprng(randstream);
#else /* NO_SPRNG */
    int m;
    for (m = 1; m < PP_NumProcs; ++m)
        (void) randomunitintervall();
    PP_randn += (m - 1);
    ++PP_rand;
    return randomunitintervall();
#endif /* NO_SPRNG */
#endif /* NOT PARALLEL */
#else /* FIXEDINTRAND */
    cerr << "!!! fixed \"random\" integers for testing purposes !!!" << endl;
    return 0.0;
#endif /* FIXEDINTRAND */

}

void random_resampling(int n, IntVector &sample, int *rstream) {
    sample.resize(n, 0);
    if (Params::getInstance().jackknife_prop == 0.0) {
        // boostrap resampling
        for (int i = 0; i < n; ++i) {
            int j = random_int(n, rstream);
            ++(sample[j]);
        }
    } else {
        // jackknife resampling
        int total = static_cast<int>(floor((1.0 - Params::getInstance().jackknife_prop) * n));
        if (total <= 0) {
            outError("Jackknife sample size is zero");
        }
        // make sure jackknife samples have exacly the same size
        for (int num = 0; num < total; ) {
            for (int i = 0; i < n; ++i) {
                if (!sample[i]) {
                    if (random_double(rstream) < Params::getInstance().jackknife_prop) {
                        continue;
                    }
                    sample[i] = 1;
                    ++num;
                    if (num >= total) {
                        break;
                    }
                }
            }
        }
    }
}


/* Following part is taken from ModelTest software */
#define	BIGX            20.0                                 /* max value to represent exp (x) */
#define	LOG_SQRT_PI     0.5723649429247000870717135          /* log (sqrt (pi)) */
#define	I_SQRT_PI       0.5641895835477562869480795          /* 1 / sqrt (pi) */
#define	Z_MAX           6.0                                  /* maximum meaningful z value */
#define	ex(x)           (((x) < -BIGX) ? 0.0 : exp (x))

/************** Normalz: probability of normal z value *********************/

/*
ALGORITHM:	Adapted from a polynomial approximation in:
                        Ibbetson D, Algorithm 209
                        Collected Algorithms of the CACM 1963 p. 616
                Note:
                        This routine has six digit accuracy, so it is only useful for absolute
                        z values < 6.  For z values >= to 6.0, Normalz() returns 0.0.
 */

double Normalz(double z) /*VAR returns cumulative probability from -oo to z VAR normal z value */ {
    double y, x, w;

    if (z == 0.0)
        x = 0.0;
    else {
        y = 0.5 * fabs(z);
        if (y >= (Z_MAX * 0.5))
            x = 1.0;
        else if (y < 1.0) {
            w = y*y;
            x = ((((((((0.000124818987 * w
                    - 0.001075204047) * w + 0.005198775019) * w
                    - 0.019198292004) * w + 0.059054035642) * w
                    - 0.151968751364) * w + 0.319152932694) * w
                    - 0.531923007300) * w + 0.797884560593) * y * 2.0;
        } else {
            y -= 2.0;
            x = (((((((((((((-0.000045255659 * y
                    + 0.000152529290) * y - 0.000019538132) * y
                    - 0.000676904986) * y + 0.001390604284) * y
                    - 0.000794620820) * y - 0.002034254874) * y
                    + 0.006549791214) * y - 0.010557625006) * y
                    + 0.011630447319) * y - 0.009279453341) * y
                    + 0.005353579108) * y - 0.002141268741) * y
                    + 0.000535310849) * y + 0.999936657524;
        }
    }
    return (z > 0.0 ? ((x + 1.0) * 0.5) : ((1.0 - x) * 0.5));
}


/**************  ChiSquare: probability of chi square value *************/

/*ALGORITHM Compute probability of chi square value.
Adapted from: 	Hill, I. D. and Pike, M. C.  Algorithm 299.Collected Algorithms for the CACM 1967 p. 243
Updated for rounding errors based on remark inACM TOMS June 1985, page 185. Found in Perlman.lib*/

double computePValueChiSquare(double x, int df) /* x: obtained chi-square value,  df: degrees of freedom */ {
    double a, y, s;
    double e, c, z;
    int even; /* true if df is an even number */

    if (x <= 0.0 || df < 1)
        return (1.0);

    y = 1;

    a = 0.5 * x;
    even = (2 * (df / 2)) == df;
    if (df > 1)
        y = ex(-a);
    s = (even ? y : (2.0 * Normalz(-sqrt(x))));
    if (df > 2) {
        x = 0.5 * (df - 1.0);
        z = (even ? 1.0 : 0.5);
        if (a > BIGX) {
            e = (even ? 0.0 : LOG_SQRT_PI);
            c = log(a);
            while (z <= x) {
                e = log(z) + e;
                s += ex(c * z - a - e);
                z += 1.0;
            }
            return (s);
        } else {
            e = (even ? 1.0 : (I_SQRT_PI / sqrt(a)));
            c = 0.0;
            while (z <= x) {
                e = e * (a / z);
                c = c + e;
                z += 1.0;
            }
            return (c * y + s);
        }
    } else
        return (s);
}

void trimString(string &str) {
    str.erase(0, str.find_first_not_of(" \n\r\t"));
    str.erase(str.find_last_not_of(" \n\r\t")+1);
}

Params& Params::getInstance() {
    static Params instance;
    return instance;
}

int countPhysicalCPUCores() {
    #ifdef _OPENMP
    return omp_get_num_procs();
    #else
    return std::thread::hardware_concurrency();
    #endif
    /*
    uint32_t registers[4];
    unsigned logicalcpucount;
    unsigned physicalcpucount;
#if defined(_WIN32) || defined(WIN32)
    SYSTEM_INFO systeminfo;
    GetSystemInfo( &systeminfo );
    logicalcpucount = systeminfo.dwNumberOfProcessors;
#else
    logicalcpucount = sysconf( _SC_NPROCESSORS_ONLN );
#endif
    if (logicalcpucount < 1) logicalcpucount = 1;
    return logicalcpucount;
    
    if (logicalcpucount % 2 != 0)
        return logicalcpucount;
    __asm__ __volatile__ ("cpuid " :
                          "=a" (registers[0]),
                          "=b" (registers[1]),
                          "=c" (registers[2]),
                          "=d" (registers[3])
                          : "a" (1), "c" (0));

    unsigned CPUFeatureSet = registers[3];
    bool hyperthreading = CPUFeatureSet & (1 << 28);    
    if (hyperthreading){
        physicalcpucount = logicalcpucount / 2;
    } else {
        physicalcpucount = logicalcpucount;
    }
    if (physicalcpucount < 1) physicalcpucount = 1;
    return physicalcpucount;
     */
}

// stacktrace.h (c) 2008, Timo Bingmann from http://idlebox.net/
// published under the WTFPL v2.0

/** Print a demangled stack backtrace of the caller function to FILE* out. */

#if  !defined(Backtrace_FOUND)

// donothing for WIN32
void print_stacktrace(ostream &out, unsigned int max_frames) {}

#else

void print_stacktrace(ostream &out, unsigned int max_frames)
{
#ifdef _OPENMP
#pragma omp master
{
#endif
    out << "STACK TRACE FOR DEBUGGING:" << endl;

    // storage array for stack trace address data
    void* addrlist[max_frames+1];

    // retrieve current stack addresses
    int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void*));

//    if (addrlen == 0) {
//        out << "  <empty, possibly corrupt>" << endl;
//        return;
//    }

    // resolve addresses into strings containing "filename(function+address)",
    // this array must be free()-ed
    char** symbollist = backtrace_symbols(addrlist, addrlen);

    // allocate string which will be filled with the demangled function name
    size_t funcnamesize = 256;
    char* funcname = (char*)malloc(funcnamesize);

    // iterate over the returned symbol lines. skip the first, it is the
    // address of this function.
    for (int i = 1; i < addrlen; ++i) {
	char *begin_name = 0, *begin_offset = 0;

	// find parentheses and +address offset surrounding the mangled name:
#ifdef __clang__
      // OSX style stack trace
      for ( char *p = symbollist[i]; *p; ++p )
      {
         if (( *p == '_' ) && ( *(p-1) == ' ' ))
            begin_name = p-1;
         else if ( *p == '+' )
            begin_offset = p-1;
      }

      if ( begin_name && begin_offset && ( begin_name < begin_offset ))
      {
         *begin_name++ = '\0';
         *begin_offset++ = '\0';

         // mangled name is now in [begin_name, begin_offset) and caller
         // offset in [begin_offset, end_offset). now apply
         // __cxa_demangle():
         int status;
         char* ret = abi::__cxa_demangle( begin_name, &funcname[0],
                                          &funcnamesize, &status );
         if ( status == 0 )
         {
            funcname = ret; // use possibly realloc()-ed string
//            out << "  " << symbollist[i] << " : " << funcname << "+"<< begin_offset << endl;
            out << i << "   "  << funcname << endl;
         } else {
            // demangling failed. Output function name as a C function with
            // no arguments.
//             out << "  " << symbollist[i] << " : " << begin_name << "()+"<< begin_offset << endl;
            out << i << "   " << begin_name << "()" << endl;
         }

#else // !DARWIN - but is posix
         // ./module(function+0x15c) [0x8048a6d]
    char *end_offset = 0;
	for (char *p = symbollist[i]; *p; ++p)
	{
	    if (*p == '(')
		begin_name = p;
	    else if (*p == '+')
		begin_offset = p;
	    else if (*p == ')' && begin_offset) {
		end_offset = p;
		break;
	    }
	}

	if (begin_name && begin_offset && end_offset
	    && begin_name < begin_offset)
	{
	    *begin_name++ = '\0';
	    *begin_offset++ = '\0';
	    *end_offset = '\0';

	    // mangled name is now in [begin_name, begin_offset) and caller
	    // offset in [begin_offset, end_offset). now apply
	    // __cxa_demangle():

	    int status;
	    char* ret = abi::__cxa_demangle(begin_name,
					    funcname, &funcnamesize, &status);
	    if (status == 0) {
            funcname = ret; // use possibly realloc()-ed string
//            out << "  " << symbollist[i] << " : " << funcname << "+"<< begin_offset << endl;
            out << i << "   " << funcname << endl;
	    }
	    else {
            // demangling failed. Output function name as a C function with
            // no arguments.
//            out << "  " << symbollist[i] << " : " << begin_name << "()+"<< begin_offset << endl;
            out << i << "   " << begin_name << "()" << endl;
	    }
#endif
	}
	else
	{
	    // couldn't parse the line? print the whole line.
//	    out << i << ". " << symbollist[i] << endl;
	}
    }

    free(funcname);
    free(symbollist);
#ifdef _OPENMP
}
#endif

}

#endif // Backtrace_FOUND

bool memcmpcpy(void * destination, const void * source, size_t num) {
    bool diff = (memcmp(destination, source, num) != 0);
    memcpy(destination, source, num);
    return diff;
}

// Pairing function: see https://en.wikipedia.org/wiki/Pairing_function
int pairInteger(int int1, int int2) {
    if (int1 <= int2) {
        return ((int1 + int2)*(int1 + int2 + 1)/2 + int2);
    } else {
        return ((int1 + int2)*(int1 + int2 + 1)/2 + int1);
    }
}

double binomial_coefficient_log(unsigned int N, unsigned int n) {
    static DoubleVector logv;
    if (logv.size() <= 0) {
        logv.push_back(0.0);
        logv.push_back(0.0);
    }
    if (n < N-n) {
        n = N-n;
    }
    if (n==0) {
        return 0.0;
    }
    if (N >= logv.size()) {
        for (auto i = logv.size(); i <= N; ++i) {
            logv.push_back(log((double) i));
        }
    }
    double binom_log = 0.0;
    for (unsigned int i = n+1; i <= N; ++i) {
        binom_log += logv[i] - logv[i-n];
    }
    return binom_log;
}

double binomial_dist(unsigned int k, unsigned int N, double p) {
  double binom_log = binomial_coefficient_log(N, k);
  double res_log = binom_log + log(p)*k + log(1-p)*(N-k);
  return exp(res_log);
}

double hypergeometric_dist(unsigned int k, unsigned int n,
                           unsigned int K, unsigned int N) {
  if (n > N)
    outError("Invalid parameters for hypergeometric distribution.");
  if (k > K || (n-k) > (N-K))
    return 0.0;
  double num_successes_log = binomial_coefficient_log(K, k);
  double num_failures_log = binomial_coefficient_log(N-K, n-k);
  double num_total_log = binomial_coefficient_log(N,n);
  return exp(num_successes_log + num_failures_log - num_total_log);
}

// Calculate the Frobenius norm of an N x N matrix M (flattened, rows
// concatenated) and linearly scaled by SCALE.
 double frob_norm(double m[], int n, double scale) {
   double sum = 0;
   for (int i = 0; i < n; ++i) {
     for (int j = 0; j < n; ++j) {
       sum += m[i*n + j] * m[i*n + j] * scale * scale;
     }
   }
   return sqrt(sum);
 }
