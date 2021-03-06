/***************************************************************************
 *   Copyright (C) 2009 by BUI Quang Minh   *
 *   minh.bui@univie.ac.at   *
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
#ifndef MODELFACTORY_H
#define MODELFACTORY_H

#include "utils/tools.h"
#include "modelsubst.h"
#include "rateheterogeneity.h"
#include "nclextra/modelsblock.h"
#include "utils/checkpoint.h"
#include "alignment/alignment.h"
#include "modelinfo.h" //for ModelInfo class

const double MIN_BRLEN_SCALE = 0.01;
const double MAX_BRLEN_SCALE = 100.0;

ModelsBlock *readModelsDefinition(Params &params);

/**
Store the transition matrix corresponding to evolutionary time so that one must not compute again. 
For efficiency purpose esp. for protein (20x20) or codon (61x61).
The values of the map contain 3 matricies consecutively: transition matrix, 1st, and 2nd derivative

	@author BUI Quang Minh <minh.bui@univie.ac.at>
*/
class ModelFactory : public unordered_map<int, double*>, public Optimization, public CheckpointFactory
{
public:

	/**
		constructor
		create substitution model with possible rate heterogeneity. Create proper class objects
		for two variables: model and site_rate. It takes the following field of params into account:
			num_rate_cats, freq_type, store_trans_matrix
		@param params program parameters
        @param model_name full model name
		@param tree associated phylogenetic tree
	*/
	ModelFactory(Params &params, string &model_name, PhyloTree *tree,
                 ModelsBlock *models_block, PhyloTree* report_to_tree);

    string getDefaultModelName(PhyloTree *tree, Params &params);
    
    StateFreqType getDefaultFrequencyTypeForSequenceType(SeqType seq_type);

    /**
     blank constructor
     */
    
    ModelFactory();
    
    
    void initializeModelAlias(ModelsBlock *models_block,
                              string& model_str);
    void moveRateParameters(string& model_str, string& rate_str);
    void moveFrequencyParameters(string& rate_str, string& model_str,
                                 string& freq_str);
    void moveErrorModelParameter(string rate_str, string model_str);
    void removeSamplingParametersFromRateString(bool pomo,
                                                std::string rate_str);
    void initializePoMo(bool pomo, ModelInfo& rate_info,
                        std::string& rate_str, std::string& model_str);
    void initializeFrequency(const Params& params, PhyloTree* tree,
                             string& freq_str, string& freq_params,
                             StateFreqType &freq_type,
                             bool& optimize_mixmodel_weight);
    void initializeModel(const std::string& model_name,
                         ModelsBlock *models_block,
                         ModelInfo& model_info, string& model_str,
                         StateFreqType freq_type, string& freq_params,
                         bool optimize_mixmodel_weight,
                         PhyloTree* tree,
                         PhyloTree* report_to_tree);
    void initializeAscertainmentCorrection(ModelInfo& rate_info,
                                           std::string &rate_str,
                                           PhyloTree* tree);
    void initializeRateHeterogeneity(const ModelInfo& rate_info,
                                     std::string& rate_str,
                                     const Params& params,
                                     PhyloTree* tree);
    void initializeFusedMixRate(ModelsBlock *models_block,
                                const std::string& model_name,
                                const std::string& model_str,
                                const std::string& freq_params,
                                const StateFreqType freq_type,
                                bool optimize_mixmodel_weight,
                                PhyloTree *tree,
                                PhyloTree* report_to_tree);
    
    /**
        set checkpoint object
        @param checkpoint
    */
    virtual void setCheckpoint(Checkpoint *checkpoint);

    /**
        start structure for checkpointing
    */
    virtual void startCheckpoint();

    /**
        save object into the checkpoint
    */
    virtual void saveCheckpoint();

    /**
        restore object from the checkpoint
    */
    virtual void restoreCheckpoint();

	/**
		get the name of the model
	*/
	//string getModelName();

	virtual void writeInfo(ostream &out) {}

	/**
		Start to store transition matrix for efficiency
	*/
	void startStoringTransMatrix();

	/**
		Stop storing transition matrix, e.g., when optimizing model parameters
	*/
	void stopStoringTransMatrix();

	/**
		Wrapper for computing the transition probability matrix from the model. It use ModelFactory
		that stores matrix computed before for effiency purpose.
		@param time time between two events
        @param mixture (optional) class for mixture model
		@param trans_matrix (OUT) the transition matrix between all pairs of states.
			Assume trans_matrix has size of num_states * num_states.
	*/
	void computeTransMatrix(double time, double *trans_matrix, int mixture = 0);

	/**
		Wrapper for computing the transition probability between two states.
		@param time time between two events
		@param state1 first state
		@param state2 second state
	*/
	double computeTrans(double time, int state1, int state2);

	/**
		Wrapper for computing the transition probability between two states
		@param time time between two events
		@param state1 first state
		@param state2 second state
		@param derv1 (OUT) 1st derivative
		@param derv2 (OUT) 2nd derivative
	*/
	virtual double computeTrans(double time, int state1, int state2, double &derv1, double &derv2);

	/**
		Wrapper for computing the transition probability matrix and the derivative 1 and 2 from the model.
		It use ModelFactory that stores matrix computed before for effiency purpose.
		@param time time between two events
        @param mixture (optional) class for mixture model
		@param trans_matrix (OUT) the transition matrix between all pairs of states. 
			Assume trans_matrix has size of num_states * num_states.
		@param trans_derv1 (OUT) the 1st derivative matrix between all pairs of states. 
		@param trans_derv2 (OUT) the 2nd derivative matrix between all pairs of states. 
	*/
	void computeTransDerv(double time, double *trans_matrix, 
		double *trans_derv1, double *trans_derv2, int mixture = 0);

	/**
		 destructor
	*/
    virtual ~ModelFactory();

    /**
     * @param brlen_type either BRLEN_OPTIMIZE, BRLEN_FIX or BRLEN_SCALE
     * @return #parameters of the model + # branches
     */
    virtual int getNParameters(int brlen_type);

	/**
		optimize model parameters and tree branch lengths
        NOTE 2016-08-20: refactor the semantic of fixed_len
		@param fixed_len 0: optimize branch lengths, 1: fix branch lengths, 2: scale branch lengths
        @param write_info TRUE to write model parameters every optimization step, FALSE to only print at the end
        @param logl_epsilon log-likelihood epsilon to stop
        @param gradient_epsilon gradient (derivative) epsilon to stop
        @param report_to_tree report proress to this tree
		@return the best likelihood 
	*/
	virtual double optimizeParameters(int fixed_len = BRLEN_OPTIMIZE, bool write_info = true,
                                      double logl_epsilon = 0.1, double gradient_epsilon = 0.0001,
                                      PhyloTree* report_to_tree = nullptr);

    /**
     *  optimize model parameters and tree branch lengths for the +I+G model
     *  using restart strategy.
     * @param fixed_len TRUE to fix branch lengths, default is false
     * @param write_info TRUE to write model parameters every optimization step, FALSE to only print at the end
     * @param logl_epsilon log-likelihood epsilon to stop
     * @param gradient_epsilon gradient (derivative) epsilon to stop
     * @param report_to_tree report proress to this tree
     *	@return the best likelihood
     */
    virtual double optimizeParametersGammaInvar(int fixed_len = BRLEN_OPTIMIZE,
                                                bool write_info = true,
                                                double logl_epsilon = 0.1,
                                                double gradient_epsilon = 0.0001,
                                                PhyloTree* report_to_tree = nullptr);

	/**
	 * @return TRUE if parameters are at the boundary that may cause numerical unstability
	 */
	virtual bool isUnstableParameters();

	/**
		pointer to the model, will not be deleted when deleting ModelFactory object
	*/
	ModelSubst *model;


	/**
		pointer to the site-rate heterogeneity, will not be deleted when deleting ModelFactory object
	*/
	RateHeterogeneity *site_rate;

	/* TRUE if a fused mixture and rate model, e.g. LG4M and LG4X */
	bool fused_mix_rate;

	/**
		TRUE to store transition matrix into this hash table for computation efficiency
	*/
	bool store_trans_matrix;

	/**
		TRUE for storing process
	*/
	bool is_storing;

	/**
	 * encoded constant sites that are unobservable and added in the alignment
	 * this involves likelihood function for ascertainment bias correction for morphological or SNP data (Lewis 2001)
	 */
	vector<Pattern> unobserved_ptns;

    /** ascertainment bias correction type */
    ASCType ASC_type;
    
    ASCType getASC() { return ASC_type; }
    
	/**
	 * optimize model and site_rate parameters
	 * @param gradient_epsilon to control stop
     * @param cur_logl current log-likelihood
	 */
	double optimizeParametersOnly(int num_steps, double gradient_epsilon,
                                  double cur_logl, PhyloTree* report_to_tree);

	/************* FOLLOWING FUNCTIONS SERVE FOR JOINT OPTIMIZATION OF MODEL AND RATE PARAMETERS *******/

	/**
	 * TRUE to optimize all parameters simultaneously, default: FALSE
	 */
	bool joint_optimize;

	/**
		return the number of dimensions
	*/
	virtual int getNDim();

	/**
		the target function which needs to be optimized
		@param x the input vector x
		@return the function value at x
	*/
	virtual double targetFunk(double x[]);

	double initGTRGammaIParameters(RateHeterogeneity *rate, ModelSubst *model, double initAlpha,
								 double initPInvar, double *initRates, double *initStateFreqs);

    double optimizeAllParameters(double gradient_epsilon);


protected:

	/**
		this function is served for the multi-dimension optimization. It should pack the model parameters
		into a vector that is index from 1 (NOTE: not from 0)
		@param variables (OUT) vector of variables, indexed from 1
	*/
	virtual void setVariables(double *variables);

	/**
		this function is served for the multi-dimension optimization. It should assign the model parameters
		from a vector of variables that is index from 1 (NOTE: not from 0)
		@param variables vector of variables, indexed from 1
		@return TRUE if parameters are changed, FALSE otherwise (2015-10-20)
	*/
	virtual bool getVariables(double *variables);

    DoubleVector optimizeGammaInvWithInitValue(int fixed_len, double logl_epsilon,
                                               double gradient_epsilon,
                                               double initPInv, double initAlpha,
                                               DoubleVector &lenvec, Checkpoint *model_ckp,
                                               PhyloTree* report_to_tree);
};

#endif
