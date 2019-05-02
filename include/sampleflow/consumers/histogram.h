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

#ifndef SAMPLEFLOW_CONSUMERS_HISTOGRAM_H
#define SAMPLEFLOW_CONSUMERS_HISTOGRAM_H

#include <sampleflow/consumer.h>
#include <sampleflow/types.h>

#include <mutex>
#include <type_traits>
#include <vector>
#include <tuple>
#include <cmath>
#include <ostream>


namespace SampleFlow
{
  namespace Consumers
  {
    /**
     * A Consumer class that implements the creation of a histogram of a
     * single scalar value represented by the samples. This histogram can then
     * be obtained by calling the get() function.
     *
     *
     * ### Threading model ###
     *
     * The implementation of this class is thread-safe, i.e., its
     * consume() member function can be called concurrently and from multiple
     * threads.
     *
     *
     * @tparam InputType The C++ type used for the samples $x_k$ processed
     *   by this class. In order to compute a histogram, this type must allow
     *   an ordering, or more specifically, putting values into bins. As a
     *   consequence, it needs to be *scalar*, i.e., it cannot be a vector of
     *   values. This is asserted by ensuring that the type satisfies the
     *   `std::is_arithmetic` property of C++11. If you have a sample type
     *   that is not scalar, for example if $x_k \in {\mathbb R}^n$, then
     *   you can of course generate histograms for each vector component
     *   individually. To this end, you can use the Filter implementation of
     *   the Filters::ComponentSplitter class that extracts individual
     *   components from a vector; this component splitter object would then
     *   be a filter placed between the original Producer of the vector-valued
     *   samples and this Consumer of scalar samples.
     */
    template <typename InputType>
    class Histogram : public Consumer<InputType>
    {
      public:
        static_assert (std::is_arithmetic<InputType>::value == true,
                       "This class can only be used for scalar input types.");

        /**
         * The type of the information generated by this class, i.e., the type
         * of the object returned by get(). In the current case, this is a vector
         * of triplets; the vector has one entry for each bin, and each bin
         * is represented by three elements:
         * - The left end point of the bin.
         * - The right end point of the bin.
         * - The number of samples in the bin.
         * You can access these three elements for the $i$th bin using code
         * such as
         * @code
         *   const double left_end_point  = std::get<0>(histogram.get()[i]);
         *   const double right_end_point = std::get<1>(histogram.get()[i]);
         *   const SampleFlow::types::sample_index
         *                n_samples_in_bin = std::get<2>(histogram.get()[i]);
         * @endcode
         */
        using value_type = std::vector<std::tuple<double,double,types::sample_index>>;

        /**
         * An enum type that describes how the subdivision of the interval
         * over which one wants to generate a histogram should be split
         * into bins:
         * - `linear`: The range `min_value...max_value` will be split
         *   into equal-sized bins.
         * - `logarithmic`: The range `min_value...max_value` will be split
         *   into bins so that they have equal size in logarithmic space. Put
         *   differently, the range `log(min_value)...log(max_value)` will
         *   be split into equal-sized intervals whose end points are then
         *   transformed back into regular space. This means that for each
         *   bin, the ratio of the right to the left end value is the same,
         *   whereas for the `linear` option above, it is the *difference*
         *   between the right and left end value of bins that is the same.
         *   Clearly, this logarithmic subdivision option requires that
         *   `min_value>0`.
         */
        enum class SubdivisionScheme
        {
          linear, logarithmic
        };


        /**
         * Constructor.
         *
         * @param[in] min_value The left end point of the range over which the
         *   histogram should be generated. Samples that have a value less than
         *   this end point will simply not be counted.
         * @param[in] max_value The right end point of the range over which the
         *   histogram should be generated. Samples that have a value larger than
         *   this end point will simply not be counted.
         * @param[in] n_subdivisions The number of bins this class represents,
         *   i.e., how many sub-intervals the range `min_value...max_value`
         *   will be split in.
         * @param[in] subdivision_scheme The way the range `min_value...max_value`
         *   will be split into sub-intervals on which to count samples. See the
         *   SubdivisionScheme type for an explanation of the options.
         */
        Histogram (const double min_value,
                   const double max_value,
                   const unsigned int n_subdivisions,
                   const SubdivisionScheme subdivision_scheme = SubdivisionScheme::linear);

        /**
         * Process one sample by computing which bin it lies in, and then
         * incrementing the number of samples in the bin.
         *
         * @param[in] sample The sample to process.
         * @param[in] aux_data Auxiliary data about this sample. The current
         *   class does not know what to do with any such data and consequently
         *   simply ignores it.
         */
        virtual
        void
        consume (InputType sample, AuxiliaryData /*aux_data*/) override;

        /**
         * Return the histogram in the format discussed in the documentation
         * of the `value_type` type.
         *
         * @return The information that completely characterizes the histogram.
         */
        value_type
        get () const;

        /**
         * Write the histogram into a file in such a way that it can
         * be visualized using the Gnuplot program. Internally, this function
         * calls get() and then converts the result of that function into a
         * format understandable by Gnuplot.
         *
         * In Gnuplot, you can then visualize the content of such a file using
         * the commands
         * @code
         *   set style data lines
         *   plot "histogram.txt"
         * @endcode
         * assuming that the data has been written into a file called
         * `histogram.txt`.
         *
         * @param[in,out] output_stream An rvalue reference to a stream object
         *   into which the data will be written. Because it is an rvalue, and
         *   not an lvalue reference, it is possible to write code such as
         *   @code
         *     histogram.write_gnuplot(std::ofstream("histogram.txt"));
         *   @endcode
         */
        void
        write_gnuplot (std::ostream &&output_stream) const;

      private:
        /**
         * A mutex used to lock access to all member variables when running
         * on multiple threads.
         */
        mutable std::mutex mutex;

        /**
         * Variables describing the bins. See the constructor for a description
         * of how they are interpreted.
         */
        const double             min_value;
        const double             max_value;
        const unsigned int       n_subdivisions;
        const SubdivisionScheme  subdivision_scheme;

        /**
         * A vector storing the number of samples so far encountered in each
         * of the bins of the histogram.
         */
        std::vector<types::sample_index> bins;

        /**
         * For a given `value`, compute the number of the bin it lies
         * in, taking into account the way the bins subdivide the
         * range for which a histogram is to be computed.
         */
        unsigned int bin_number (const double value) const;
    };



    template <typename InputType>
    Histogram<InputType>::
    Histogram (const double min_value,
               const double max_value,
               const unsigned int n_subdivisions,
               const SubdivisionScheme subdivision_scheme)
      :
      min_value (min_value),
      max_value (max_value),
      n_subdivisions (n_subdivisions),
      subdivision_scheme (subdivision_scheme),
      bins (n_subdivisions)
    {}



    template <typename InputType>
    void
    Histogram<InputType>::
    consume (InputType sample, AuxiliaryData /*aux_data*/)
    {
      // If a sample lies outside the bounds, just discard it:
      if (sample<min_value || sample>max_value)
        return;

      // Otherwise we need to update the histogram bins
      const unsigned int bin = bin_number(sample);

      std::lock_guard<std::mutex> lock(mutex);
      ++bins[bin];
    }



    template <typename InputType>
    typename Histogram<InputType>::value_type
    Histogram<InputType>::
    get () const
    {
      // First create the output table and breakpoints
      value_type return_value (n_subdivisions);
      for (unsigned int bin=0; bin<n_subdivisions; ++bin)
        {
          double bin_min, bin_max;

          switch (subdivision_scheme)
            {
              case SubdivisionScheme::linear:
              {
                bin_min = min_value + bin*(max_value-min_value)/n_subdivisions;
                bin_max = min_value + (bin+1)*(max_value-min_value)/n_subdivisions;

                break;
              }

              case SubdivisionScheme::logarithmic:
              {
                bin_min = std::exp(std::log(min_value) + bin*(std::log(max_value)-std::log(min_value))/n_subdivisions);
                bin_max = std::exp(std::log(min_value) + (bin+1)*(std::log(max_value)-std::log(min_value))/n_subdivisions);

                break;
              }

              default:
                bin_min = bin_max = 0;
            }

          std::get<0>(return_value[bin]) = bin_min;
          std::get<1>(return_value[bin]) = bin_max;
        }

      // Now fill the bin sizes under a lock as they are subject to
      // change from other threads:
      std::lock_guard<std::mutex> lock(mutex);
      for (unsigned int bin=0; bin<n_subdivisions; ++bin)
        {
          std::get<2>(return_value[bin]) = bins[bin];
        }

      return return_value;
    }



    template <typename InputType>
    void
    Histogram<InputType>::
    write_gnuplot(std::ostream &&output_stream) const
    {
      const auto histogram = get();

      // For each bin, draw three sides of a rectangle over the x-axis
      for (const auto &bin : histogram)
        {
          output_stream << std::get<0>(bin) << ' ' << 0 << '\n';
          output_stream << std::get<0>(bin) << ' ' << std::get<2>(bin) << '\n';
          output_stream << std::get<1>(bin) << ' ' << std::get<2>(bin) << '\n';
          output_stream << std::get<1>(bin) << ' ' << 0 << '\n';
          output_stream << '\n';
        }

      output_stream << std::flush;
    }



    template <typename InputType>
    unsigned int
    Histogram<InputType>::
    bin_number (const double value) const
    {
      assert (value>=min_value);
      assert (value<=max_value);
      switch (subdivision_scheme)
        {
          case SubdivisionScheme::linear:
            return std::max(0,
                            std::min(static_cast<int>(n_subdivisions)-1,
                                     static_cast<int>((value-min_value)/
                                                      ((max_value-min_value)/n_subdivisions))));

          case SubdivisionScheme::logarithmic:
            return std::max(0,
                            std::min(static_cast<int>(n_subdivisions)-1,
                                     static_cast<int>((std::log(value)-std::log(min_value))/
                                                      ((std::log(max_value)-std::log(min_value))/n_subdivisions))));

          default:
            return 0;
        }
    }
  }
}

#endif
