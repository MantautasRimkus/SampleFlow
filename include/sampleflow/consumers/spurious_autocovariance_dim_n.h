// ---------------------------------------------------------------------
//
// Copyright (C) 2019 by the SampleFlow authors.
//
// This file is part of the SampleFlow library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------

#ifndef SAMPLEFLOW_CONSUMERS_SPURIOUS_AUTOCOVARIANCE_H
#define SAMPLEFLOW_CONSUMERS_SPURIOUS_AUTOCOVARIANCE_H

#include <sampleflow/consumer.h>
#include <sampleflow/types.h>
#include <mutex>

#include <boost/numeric/ublas/matrix.hpp>


namespace SampleFlow
{
  namespace Consumers
  {
    /**
     * NOTICE: We call results as spurious autocovariance, because by definition it is not autocovariance.
     * A Consumer class that implements computing the running sample spurious autocovariance function:
     * \hat\gamma(l)=\frac{1}{n}\sum_{t=1}^{n-l}{(\bm{x}_{t+l}-\bar\bm{x})(bm{x}_{t}-\bar\bm{x})}
     *
     *This code for every new sample updates \hat\gamma(k), l=1,2,3...,k. Choice of k can be done
     *by setting it below.
     *%%%%%%%%%%%%Manty's comment: It would be great to set k in mcmc_test.cc, not here. I don't know
     *how to set it globally.
     *
     *Algorithm:
     *There are three parts: 1) When amount of samples (sample_n) is equal 0 2) When k>sample_n 3) Otherwise
     *Second and third parts are almost identical, just for "l" bigger than sample_n it is not possible to
     *get \gamma(l) estimation. Further description focus on third part (case with big enough sample_n)
     *
     *Let expand formula above and then denote some of its parts as \alpha and \beta:
     *\hat\gamma(l)=\frac{1}{n}\sum_{t=1}^{n-l}{(\bm{x}_{t+l}-\bar\bm{x_n})^T(bm{x}_{t}-\bar\bm{x_n})}=
     *=\frac{1}{n}\sum_{t=1}^{n-l}{(\bm{x}_{t+l})^T(bm{x}_{t})}-
     *-(\bar\bm{x_n}^T)\frac{1}{n}\sum_{t=1}^{n-l}{\bm{x}_{t+l}+(bm{x}_{t}}+
     * +\frac{n-l}{n}(\bar\bm{x_n}^T)(\bar\bm{x_n})=
     * =\alpha_n(l)-(\bar\bm{x_n}^T)\bm{beta_n(l)}+\frac{n-l}{n}(\bar\bm{x_n}^T)(\bar\bm{x_n}).
     *
     * During calculation, we need to update \alpha_{n+1}(l) (scalar),
     * \bm{beta_{n+1}(l)} (same dimension as sample) and sample mean \bar\bm{x_{n+1}
     *
     * Notice, that for each l, \alpha_{n}(l) and \bm{beta_{n}(l)} probably is different. So to save \alpha values
     * we need to have vector, while for \bm{\beta} - matrix.
     *
     * Updating algorithm for all these terms are equivalent to means update.
     *
     * %%%%%%%%%%%%Manty's comment: Almost all vector calculations is done element by element. I had
     * problems to get full row from matrix as valarray. After getting some knowledge in programming, it should
     * be updated to more efficient algorithms.
     *
     *
     * ### Threading model ###
     *
     * The implementation of this class is thread-safe, i.e., its
     * consume() member function can be called concurrently and from multiple
     * threads.
     *
     *
     * @tparam InputType The C++ type used for the samples $x_k$. In
     *   order to compute covariances, the same kind of requirements
     *   have to hold as listed for the Covariance class.
     */
    template <typename InputType>
    class Spurious_Autocovariance: public Consumer<InputType>
    {
      public:
        /**
         * The data type of the elements of the input type.
         */

       using scalar_type = typename InputType::value_type;

       /**
         * The type of the information generated by this class, i.e., in which
         * the autocovariance function is computed. By itself, for autocovariance there should
         * be enough to use vector. However, because we keep updating autocovariance, we need to save
         * parts used in calculation
         */

       using value_type = boost::numeric::ublas::matrix<scalar_type>;

        /**
         * Constructor.
         */

        Spurious_Autocovariance();

        /**
         * Process one sample by updating the previously computed covariance
         * matrix using this one sample.
         *
         * @param[in] sample The sample to process.
         * @param[in] aux_data Auxiliary data about this sample. The current
         *   class does not know what to do with any such data and consequently
         *   simply ignores it.
         */
        virtual
        void
        consume (InputType     sample,
                 AuxiliaryData aux_data) override;

        /**
         * A function that returns the covariance matrix computed from the
         * samples seen so far. If no samples have been processed so far, then
         * a default-constructed object of type InputType will be returned.
         *
         * @return The computed covariance matrix.
         */
        value_type
        get () const;

      private:
        /**
         * A mutex used to lock access to all member variables when running
         * on multiple threads.
         */

      mutable std::mutex mutex;

        /**
         * The current value of $\bar x_k$ as described in the introduction
         * of this class.
         */

        InputType           current_mean;

        /**
         * Parts for running autocovariation
         * Description of these parts is given above
         */

        boost::numeric::ublas::matrix<scalar_type> alpha;//First dot product of autocovariation
        boost::numeric::ublas::matrix<scalar_type> beta; //Sum of vectors for innerproduct
        boost::numeric::ublas::matrix<scalar_type> current_autocovariation;//Current autocovariaton

        /**
        * Save previous sample value
        * %%%%%%%%%%%%Manty's comment: I didn't find (I'm 100% there are) better way to save last k sample values
        * as just using one more object and assign i'th row of past_sample to i+1'th row of past_sample_replace
        *
        */

        boost::numeric::ublas::matrix<scalar_type> past_sample;
        boost::numeric::ublas::matrix<scalar_type> past_sample_replace;

        /**
         * The number of samples processed so far.
         */

        types::sample_index n_samples;
    };

    template <typename InputType>
    Spurious_Autocovariance<InputType>::
	Spurious_Autocovariance ()
      :
      n_samples (0)
    {}


    template <typename InputType>
    void
	Spurious_Autocovariance<InputType>::
    consume (InputType sample, AuxiliaryData /*aux_data*/)
    {
      std::lock_guard<std::mutex> lock(mutex);

      // If this is the first sample we see, initialize all components
      //. After the first sample, the autocovariance matrix
      // is the zero matrix since a single sample does not have any friends yet.

      //   %%%%%%%%%%%%Manty's comment: It would be great to set k in mcmc_test.cc, not here. I don't know
      //  Set how long is our autocovariance tail
      double k=10;

      if (n_samples == 0)
        {
          n_samples = 1;
          current_autocovariation.resize (k,1);
          alpha.resize(k,1);
          beta.resize(k,sample.size());
          for (unsigned int i=0; i<k; ++i){
        	  current_autocovariation(i,0) = 0;
          	  alpha(i,0) = 0;
          	  for (unsigned int j=0; j<sample.size(); ++j){
                  beta(i,j) = 0;
                  }
          	  }
          current_mean = sample;
          past_sample.resize(k,sample.size());
          past_sample_replace.resize(k,sample.size());
          for (unsigned int i=0; i<sample.size(); ++i) past_sample(0,i) = sample[i];
        }
      else if(n_samples < k)
		{
     	 ++n_samples;
    	for (unsigned int i=0; i<n_samples-1; ++i){

    		//Update first dot product (alpha)
    		    double alphaupd = 0;
    		    for (unsigned int j=0; j<sample.size(); ++j){
    		    	alphaupd += sample[j]*past_sample(i,j);
    		    	}
    		    alphaupd -= alpha(i,0);
    		    alphaupd /= n_samples;
    		    alpha(i,0) += alphaupd;

    		//Update second value (beta)
    		    InputType betaupd = sample;
    		    for (unsigned int j=0; j<sample.size(); ++j){
    		    	betaupd[j] += past_sample(i,j);
    		    	betaupd[j] -= beta(i,j);
    		    	betaupd[j] /= n_samples;
    		    	beta(i,j) += betaupd[j];
    		        }
    		}

    		//Save needed past values
    	for (unsigned int i=0; i<n_samples-1; ++i){
    		for (unsigned int j=0; j<sample.size(); ++j){
    			past_sample_replace(i+1,j)=past_sample(i,j);
    			}
    	}
    	for (unsigned int j=0; j<sample.size(); ++j) past_sample_replace(0,j) = sample[j];
    	past_sample = past_sample_replace;

    	// Then also update the running mean:
    	 InputType update = sample;
    	          update -= current_mean;
    	          update /= n_samples;
    	          current_mean += update;
		}
     else
     {
        ++n_samples;
         	for (unsigned int i=0; i<k; ++i){
         		//Update first dot product (alpha)
         		   double alphaupd = 0;
         		   for (unsigned int j=0; j<sample.size(); ++j){
         			   alphaupd += sample[j]*past_sample(i,j);//Nuo cia reikia pradeti keisti ir ivedineti ciklus
         		       }
         		   alphaupd -= alpha(i,0);
         		   alphaupd /= n_samples;
         		   alpha(i,0) += alphaupd;

         		//Update second value (beta)
         		    InputType betaupd = sample;
         		    for (unsigned int j=0; j<sample.size(); ++j){
         		    	betaupd[j] += past_sample(i,j);
         		    	betaupd[j] -= beta(i,j);
         		    	betaupd[j] /= n_samples;
         		    	beta(i,j) += betaupd[j];
         		    	}
         		    }
    		//Save needed past values
    	for (unsigned int i=0; i<k-1; ++i){
    		for (unsigned int j=0; j<sample.size(); ++j){
    			past_sample_replace(i+1,j)=past_sample(i,j);
    			}
    	}
    	for (unsigned int j=0; j<sample.size(); ++j) past_sample_replace(0,j) = sample[j];
    	past_sample = past_sample_replace;

    	// Then also update the running mean:
    	 InputType update = sample;
    	          update -= current_mean;
    	          update /= n_samples;
    	          current_mean += update;

          //Calculate autocovariance value using formula described above
        for (unsigned int i=0; i<k; ++i){
         	current_autocovariation(i,0) = alpha(i,0);
         	for (unsigned int j=0; j<sample.size(); ++j) current_autocovariation(i,0) -= current_mean[j]* beta(i,j);
         	current_autocovariation(i,0) += ((n_samples-1)/(n_samples))*(current_mean*current_mean).sum();
         }
     }
    }


    template <typename InputType>
    typename Spurious_Autocovariance<InputType>::value_type
	Spurious_Autocovariance<InputType>::
    get () const
    {
      std::lock_guard<std::mutex> lock(mutex);

      return current_autocovariation;
    }

  }
}

#endif