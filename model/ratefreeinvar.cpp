/*
 * ratefreeinvar.cpp
 *
 *  Created on: Nov 7, 2014
 *      Author: minh
 */

#include "ratefreeinvar.h"

RateFreeInvar::RateFreeInvar(int ncat, double start_alpha, string params,
                             bool sorted_rates, double p_invar_sites,
                             string opt_alg, PhyloTree *tree)
    : RateInvar(p_invar_sites, tree)
    , RateFree(ncat, start_alpha, params, sorted_rates, opt_alg, tree)
{
	cur_optimize = 0;
	name = "+I" + name;
	full_name = "Invar+" + full_name;
    setNCategory(ncat);
}

void RateFreeInvar::startCheckpoint() {
    checkpoint->startStruct("RateFreeInvar" + convertIntToString(ncategory));
}

void RateFreeInvar::saveCheckpoint() {
    RateInvar::saveCheckpoint();
    RateFree::saveCheckpoint();
}

void RateFreeInvar::restoreCheckpoint() {
    RateInvar::restoreCheckpoint();
    RateFree::restoreCheckpoint();
}

void RateFreeInvar::setNCategory(int ncat) {
	RateFree::setNCategory(ncat);
	name = "+I" + name;
	full_name = "Invar+" + full_name;
}

double RateFreeInvar::computeFunction(double value) {
	p_invar = value;
	phylo_tree->clearAllPartialLH();
	return -phylo_tree->computeLikelihood();
}

double RateFreeInvar::targetFunk(double x[]) {
	return RateFree::targetFunk(x);
}

void RateFreeInvar::writeInfo(ostream &out) {
	RateInvar::writeInfo(out);
	RateFree::writeInfo(out);

}

/**
	write parameters, used with modeltest
	@param out output stream
*/
void RateFreeInvar::writeParameters(ostream &out) {
	RateInvar::writeParameters(out);
	RateFree::writeParameters(out);
}

void RateFreeInvar::setBounds(double *lower_bound, double *upper_bound,
                              bool *bound_check) {
	RateFree::setBounds(lower_bound, upper_bound, bound_check);
	if (RateInvar::getNDim() == 0) return;
	int ndim = getNDim()-1;
	RateInvar::setBounds(lower_bound+ndim, upper_bound+ndim, bound_check+ndim);
}

/**
	optimize parameters
	@return the best likelihood
*/
double RateFreeInvar::optimizeParameters(double gradient_epsilon,
                                         PhyloTree* report_to_tree) {
	double tree_lh;
	tree_lh = RateFree::optimizeParameters(gradient_epsilon,
                                           report_to_tree);
	return tree_lh;
}

void RateFreeInvar::setVariables(double *variables) {
	RateFree::setVariables(variables);
	if (RateInvar::getNDim() == 0) return;
	variables[getNDim()] = p_invar;
}

/**
	this function is served for the multi-dimension optimization. It should assign the model parameters
	from a vector of variables that is index from 1 (NOTE: not from 0)
	@param variables vector of variables, indexed from 1
*/
bool RateFreeInvar::getVariables(double *variables) {
	bool changed = RateFree::getVariables(variables);
	if (RateInvar::getNDim() == 0) return changed;
    changed |= (p_invar != variables[getNDim()]);
	p_invar = variables[getNDim()];
    return changed;
}

