/*
 * modelfactorymixlen.cpp
 *
 *  Created on: Sep 2, 2015
 *      Author: minh
 */

#include "tree/phylotreemixlen.h"
#include "utils/timeutil.h"
#include "utils/tools.h"
#include "model/modelfactorymixlen.h"
#include "model/modelmarkov.h"
#include "model/modelmixture.h"
#include "rateheterotachy.h"
#ifdef _MSC_VER
#include <boost/scoped_array.hpp>
#endif

ModelFactoryMixlen::ModelFactoryMixlen(Params &params, string &model_name,
                                       PhyloTree *tree, ModelsBlock *models_block,
                                       PhyloTree* report_to_tree)
    : ModelFactory(params, model_name, tree, models_block, report_to_tree) {
    if (!tree->isMixlen()) {
        cerr << "Please add '-mixlen " << site_rate->getNRate()
             << "' option into the command line" << endl;
        outError("Sorry for the inconvience, please rerun IQ-TREE with option above");
    }
    if (tree->getMixlen() != site_rate->getNRate()) {
        ((PhyloTreeMixlen*)tree)->setMixlen(site_rate->getNRate());
//        outError("#heterotachy classes and #mixture branch lengths do not match");
    }
    if (fused_mix_rate) {
        // fix the rate of heterotachy
//        RateHeterotachy *hrate = (RateHeterotachy*)site_rate;
//        ModelMixture *mmodel = (ModelMixture*)model;

        /*
        if (site_rate->getFixParams() == 1) {
            // swap the weights between model and site_rate
            for (int i = 0; i < site_rate->getNRate(); i++) {
                model->setMixtureWeight(i, site_rate->getProp(i));
                site_rate->setProp(i, 1.0);
            }
        } else {
            // fix the weight of heterotachy model to 1
            site_rate->setFixParams(2);
            double fix_prop = (1.0-site_rate->getPInvar());
            for (int i = 0; i < site_rate->getNRate(); i++)
                site_rate->setProp(i, fix_prop);
        }
        */
    }
}

double ModelFactoryMixlen::optimizeParameters(int fixed_len, bool write_info,
                                              double logl_epsilon, double gradient_epsilon,
                                              PhyloTree* report_to_tree) {

	PhyloTreeMixlen *tree = (PhyloTreeMixlen*)site_rate->getTree();
	ASSERT(tree);
    
    tree->initializeMixlen(logl_epsilon, write_info, report_to_tree);

    double score = ModelFactory::optimizeParameters(fixed_len, write_info,
                                                    logl_epsilon, gradient_epsilon,
                                                    report_to_tree);

    return score;
}

string ModelFactoryMixlen::sortClassesByTreeLength() {

	PhyloTreeMixlen *tree = (PhyloTreeMixlen*)site_rate->getTree();

    // now sort the classes by tree lengths
    DoubleVector brlen;
    tree->saveBranchLengths(brlen);
    ASSERT(brlen.size() == tree->branchNum * tree->mixlen);

    // compute tree lengths
#ifndef _MSC_VER
    double treelen[tree->mixlen];
    int index[tree->mixlen];
#else
    boost::scoped_array<double> treelen(new double[tree->mixlen]);
    boost::scoped_array<int>    index  (new int[tree->mixlen]);
#endif
    memset(&treelen[0], 0, sizeof(double)*tree->mixlen);
    int i, j;
    for (i = 0; i < tree->mixlen; i++) {
        index[i] = i;
    }
    for (i = 0, j = 0; i < brlen.size(); i++, j++) {
        if (j == tree->mixlen) j = 0;
        treelen[j] += brlen[i];
    }

    // sort tree lengths and reorder branch lengths
    quicksort(&treelen[0], 0, tree->mixlen-1, &index[0]);
    bool sorted = true;
    for (j = 0; j < tree->mixlen; j++)
        if (index[j] != j) { sorted = false; break; };
    if (!sorted) {
        double score = tree->curScore;
        cout << "Reordering classes by tree lengths" << endl;
        DoubleVector sorted_brlen;
        sorted_brlen.resize(brlen.size());
        for (i = 0; i < tree->branchNum; i++) {
            for (j = 0; j < tree->mixlen; j++) {
                sorted_brlen[i*tree->mixlen + j] = brlen[i*tree->mixlen + index[j]];
            }
        }
        tree->restoreBranchLengths(sorted_brlen);

        ASSERT(tree->mixlen == site_rate->getNRate());
        // reoder class weights
#ifndef _MSC_VER
        double prop[site_rate->getNRate()];
#else
        boost::scoped_array<double> prop(new double[site_rate->getNRate()]);
#endif
        for (j = 0; j < site_rate->getNRate(); j++) {
            prop[j] = site_rate->getProp(index[j]);
        }
        for (j = 0; j < site_rate->getNRate(); j++) {
            site_rate->setProp(j, prop[j]);
        }
        // reorder mixture models
        if (fused_mix_rate) {
            ASSERT(model->getNMixtures() == site_rate->getNRate());
//            ModelMixture *mixmodel = (ModelMixture*)model;
            int nmix = model->getNMixtures();
#ifndef _MSC_VER
            ModelSubst *models[nmix];
#else
            boost::scoped_array<ModelSubst*> models(new ModelSubst * [nmix]);
#endif
            for (j = 0; j < nmix; j++) {
                models[j] = model->getMixtureClass(index[j]);
            }
            for (j = 0; j < nmix; j++) {
                model->setMixtureClass(j, models[j]);
            }
            for (j = 0; j < site_rate->getNRate(); j++) {
                prop[j] = model->getMixtureWeight(index[j]);
            }
            for (j = 0; j < site_rate->getNRate(); j++) {
                model->setMixtureWeight(j, prop[j]);
            }
            // assigning memory for individual models
            int m = 0;
            int num_states = model->num_states;
            int states_squared = num_states*num_states;
            for (m = 0; m < nmix; m++) {
                auto mix_class = ((ModelMarkov*)model->getMixtureClass(m));
                mix_class->setEigenvalues(&model->getEigenvalues()[m*num_states]);
                mix_class->setEigenvectors(&model->getEigenvectors()[m*states_squared]);
                auto i_evec = &model->getInverseEigenvectors()[m*states_squared];
                mix_class->setInverseEigenvectors(i_evec);
                auto i_evec_t = &model->getInverseEigenvectorsTransposed()[m*states_squared];
                mix_class->setInverseEigenvectorsTransposed(i_evec_t);
            }
            model->decomposeRateMatrix();
            site_rate->writeInfo(cout);
        }
        tree->clearAllPartialLH();
        ASSERT(fabs(score - tree->computeLikelihood()) < 0.1);
    }

    // update relative_rate
    /*
    double sum = 0.0;
    for (j = 0; j < tree->mixlen; j++)
        sum += treelen[j];
    sum = tree->mixlen/sum;
    for (j = 0; j < tree->mixlen; j++)
        tree->relative_rate->setRate(j, treelen[j]*sum);
    */
    return tree->getTreeString();
}

int ModelFactoryMixlen::getNParameters(int brlen_type) {
	int df = ModelFactory::getNParameters(brlen_type);
    if (brlen_type == BRLEN_OPTIMIZE) {
        df += site_rate->phylo_tree->branchNum
           * (site_rate->phylo_tree->getMixlen() - 1);
    }
    else if (brlen_type == BRLEN_SCALE) {
        df += (site_rate->phylo_tree->getMixlen() - 1);
    }
	return df;
}
