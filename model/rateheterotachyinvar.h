/*
 * rateheterotachyinvar.h
 *
 *  Created on: Nov 10, 2016
 *      Author: minh
 */

#ifndef rateheterotachyinvar_hpp
#define rateheterotachyinvar_hpp

#include "rateinvar.h"
#include "rateheterotachy.h"

class RateHeterotachyInvar: public RateInvar, public RateHeterotachy {
public:

 	/**
		constructor
		@param ncat number of rate categories
		@param tree associated phylogenetic tree
	*/
    RateHeterotachyInvar(int ncat, string params, double p_invar_sites, PhyloTree *tree);

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
		return the number of dimensions
	*/
	virtual int getNDim() { return RateInvar::getNDim() + RateHeterotachy::getNDim(); }

	/**
		get the proportion of sites under a specified category.
		@param category category ID from 0 to #category-1
		@return the proportion of the specified category
	*/
	virtual double getProp(int category) { return prop[category]; }

	/**
		get the rate of a specified category. Default returns 1.0 since it is homogeneous model
		@param category category ID from 0 to #category-1
		@return the rate of the specified category
	*/
	virtual double getRate(int category);

	/**
	 * @return model name with parameters in form of e.g. GTR{a,b,c,d,e,f}
	 */
	virtual string getNameParams() {
		return RateInvar::getNameParams() + RateHeterotachy::getNameParams();
	}

	/**
		override function from Optimization class, used by the minimizeOneDimen() to optimize
		p_invar or gamma shape parameter.
		@param value value of p_invar (if cur_optimize == 1) or gamma shape (if cur_optimize == 0).
	*/
	virtual double computeFunction(double value);

	/**
	 * setup the bounds for joint optimization with BFGS
	 */
	virtual void setBounds(double *lower_bound, double *upper_bound, bool *bound_check);

	/**
		optimize parameters
		@return the best likelihood
	*/
	virtual double optimizeParameters(double gradient_epsilon,
                                      PhyloTree* report_to_tree);


	/**
		the target function which needs to be optimized
		@param x the input vector x
		@return the function value at x
	*/
	virtual double targetFunk(double x[]);

	/**
		write information
		@param out output stream
	*/
	virtual void writeInfo(ostream &out);

	/**
		write parameters, used with modeltest
		@param out output stream
	*/
	virtual void writeParameters(ostream &out);

	virtual void setNCategory(int ncat);

#ifdef _MSC_VER
	//MSVC generates warning messages about these member functions
	//being inherited "via dominance". Explictly declaring them
	//instead shuts those warnings up.
	virtual bool isHeterotachy()                     { return RateHeterotachy::isHeterotachy(); }
	virtual int  getNRate()                    const { return RateHeterotachy::getNRate();  }
	virtual int  getNDiscreteRate()            const { return RateHeterotachy::getNDiscreteRate(); }
	virtual void setProp(int category, double value) { RateHeterotachy::setProp(category, value); }
	virtual int  getFixParams()                      { return RateHeterotachy::getFixParams(); 	}
	virtual void setFixParams(int mode)              { RateHeterotachy::setFixParams(mode); }
	virtual void setOptimizeSteps(int steps)         { RateHeterotachy::setOptimizeSteps(steps);  }

	virtual double getPInvar()                 const { return RateInvar::getPInvar(); }
	virtual void   setPInvar(double pInvar)          { RateInvar::setPInvar(pInvar); }
	virtual bool   isFixPInvar()               const { return RateInvar::isFixPInvar(); }
	void    setFixPInvar(bool fixPInvar)             { RateInvar::setFixPInvar(fixPInvar); }
#endif

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

private:

	/**
		current parameter to optimize. 0 if gamma shape or 1 if p_invar.
	*/
	int cur_optimize;

};

#endif /* RATEFREEINVAR_H_ */
