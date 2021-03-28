/*
 * modelunrest.cpp
 *
 *  Created on: 24/05/2016
 *      Author: mdw2
 */

#include "modelunrest.h"

ModelUnrest::ModelUnrest(PhyloTree *tree, string model_params,
                         PhyloTree* report_to_tree)
	: ModelMarkov(tree, false)
{
    num_params = getNumRateEntries() - 1;
    //ModelMarkov::setReversible in the ModelMarkov
    //constructor sets all the rates to 0.0.  But...
    for (int i=0; i <= num_params; i++) {
        rates[i] = 1.0;
    }
	if (model_params != "") {
		cout << "WARNING: Supplying model params to constructor"
             << " not yet properly implemented -- ignored" << endl;
		// TODO: parse model_params into model_parameters, then call setRates().
	}
    name = "UNREST";
    full_name = "Unrestricted model (non-reversible)";
    ModelMarkov::init(FREQ_ESTIMATE, report_to_tree);
}

/* static */ bool ModelUnrest::validModelName(string model_name) {
	return (model_name == "UNREST");
}

void ModelUnrest::setBounds(double *lower_bound, double *upper_bound,
                            bool *bound_check) {
	int i, ndim = getNDim();

	for (i = 1; i <= ndim; i++) {
		lower_bound[i] = MIN_RATE;
		upper_bound[i] = MAX_RATE;
		bound_check[i] = false;
	}
}

/*
void ModelUnrest::setRates() {
	// For UNREST, parameters are simply the off-diagonal rate matrix entries
	// (except [4,3] = rates[11], which is constrained to be 1)
	memcpy(rates, model_parameters, num_params*sizeof(double));
	rates[num_params]=1;
}
*/

void ModelUnrest::setStateFrequency(double* freq) {
    // DOES NOTHING
}

void ModelUnrest::startCheckpoint() {
    checkpoint->startStruct("ModelUnrest");
}

void ModelUnrest::saveCheckpoint() {
    startCheckpoint();
    if (!fixed_parameters)
        CKP_ARRAY_SAVE(getNumRateEntries(), rates);
    endCheckpoint();
    ModelMarkov::saveCheckpoint();
}

void ModelUnrest::restoreCheckpoint() {
    ModelMarkov::restoreCheckpoint();
    startCheckpoint();
    if (!fixed_parameters)
        CKP_ARRAY_RESTORE(getNumRateEntries(), rates);
    endCheckpoint();
    decomposeRateMatrix();
    if (phylo_tree)
        phylo_tree->clearAllPartialLH();
}
