/*    Copyright (c) 2010-2023, Delft University of Technology
 *    All rigths reserved
 *
 *    This file is part of the Tudat. Redistribution and use in source and
 *    binary forms, with or without modification, are permitted exclusively
 *    under the terms of the Modified BSD license. You should have received
 *    a copy of the license with this file. If not, please or visit:
 *    http://tudat.tudelft.nl/LICENSE.
 */

#ifndef TUDAT_PARSEODFFILE_H
#define TUDAT_PARSEODFFILE_H

#include "tudat/basics/utilities.h"
#include "tudat/io/readOdfFile.h"
#include "tudat/astro/observation_models/observableTypes.h"
#include "tudat/astro/earth_orientation/terrestrialTimeScaleConverter.h"
#include "tudat/astro/basic_astro/timeConversions.h"
#include "tudat/simulation/estimation_setup/observations.h"
#include "tudat/simulation/environment_setup/body.h"
#include "tudat/math/interpolators/lookupScheme.h"
#include "tudat/math/quadrature/trapezoidQuadrature.h"

namespace tudat
{

namespace orbit_determination
{

observation_models::ObservableType getObservableTypeForOdfId( const int odfId );



std::string getStationNameFromStationId ( const int networkId, const int stationId );

class ProcessedOdfFileSingleLinkData
{
public:

    ProcessedOdfFileSingleLinkData( ){ }

    virtual ~ProcessedOdfFileSingleLinkData( ){ }

    std::vector< double > observationTimes_;
    std::vector< Eigen::Matrix< double, Eigen::Dynamic, 1 > > observableValues_;
    std::vector< double > receiverDownlinkDelays_;

    std::vector< int > downlinkBandIds_;
    std::vector< int > uplinkBandIds_;
    std::vector< int > referenceBandIds_;

    std::vector< std::string > originFiles_;

    observation_models::ObservableType observableType_;

    std::string transmittingStation_;
    std::string receivingStation_;

    std::map< double, Eigen::Matrix< double, Eigen::Dynamic, 1 > > getUnprocessedObservables( )
    {
        return utilities::createMapFromVectors( observationTimes_, observableValues_ );
    }

    std::vector< Eigen::Matrix< double, Eigen::Dynamic, 1 > > getUnprocessedObservablesVector( )
    {
        return observableValues_;
    }

    std::map< double, Eigen::Matrix< double, Eigen::Dynamic, 1 > > getProcessedObservables( )
    {
        return utilities::createMapFromVectors( observationTimes_, getProcessedObservablesVector( ) );
    }

    virtual std::vector< Eigen::Matrix< double, Eigen::Dynamic, 1 > > getProcessedObservablesVector( )
    {
        return observableValues_;
    }

    std::vector< double > getObservationTimesUtc (  )
    {
        return observationTimes_;
    }

    std::vector< double > getObservationTimesTdb (
            const simulation_setup::SystemOfBodies& bodies )
    {
        earth_orientation::TerrestrialTimeScaleConverter timeScaleConverter =
                earth_orientation::TerrestrialTimeScaleConverter( );

        std::vector< Eigen::Vector3d > earthFixedPositions;
        for ( unsigned int i = 0; i < observationTimes_.size( ); ++i )
        {
            earthFixedPositions.push_back(
                    bodies.getBody( "Earth" )->getGroundStation( receivingStation_ )->getStateInPlanetFixedFrame< double, double >(
                            observationTimes_.at( i ) ).segment( 0, 3 )
                            );
        }

        return timeScaleConverter.getCurrentTimes(
                basic_astrodynamics::utc_scale, basic_astrodynamics::tdb_scale, observationTimes_,
                earthFixedPositions );
    }

};


class ProcessedOdfFileDopplerData: public ProcessedOdfFileSingleLinkData
{
public:

    ~ProcessedOdfFileDopplerData( ){ }

    std::vector< int > receiverChannels_;
    std::vector< double > referenceFrequencies_;
    std::vector< double > countInterval_;
    std::vector< double > transmitterUplinkDelays_;
    std::vector< bool > receiverRampingFlags_;

    std::vector< Eigen::Matrix< double, Eigen::Dynamic, 1 > > getProcessedObservablesVector( )
    {
        throw std::runtime_error("Getting range rate observables not implemented");
        return observableValues_;
    }

    std::map< double, bool > getReceiverRampingFlags( )
    {
        return utilities::createMapFromVectors( observationTimes_, receiverRampingFlags_ );
    }

    std::map< double, double > getReferenceFrequencies( )
    {
        return utilities::createMapFromVectors( observationTimes_, referenceFrequencies_ );
    }

    std::map< double, double > getCountInterval( )
    {
        return utilities::createMapFromVectors( observationTimes_, countInterval_ );
    }
};

class StationFrequencyInterpolator
{
public:
    //! Constructor
    StationFrequencyInterpolator( ) { }

    //! Destructor
    ~StationFrequencyInterpolator( ) { }

    virtual double getCurrentFrequency( const double lookupTime ) = 0;

    virtual double getFrequencyIntegral( const double quadratureStartTime, const double quadratureEndTime ) = 0;

    virtual double getAveragedFrequencyIntegral( const double quadratureStartTime, const double quadratureEndTime )
    {
        return getFrequencyIntegral( quadratureStartTime, quadratureEndTime ) / ( quadratureEndTime - quadratureStartTime );
    }

private:

};


class ConstantFrequencyInterpolator: public StationFrequencyInterpolator
{
public:
    //! Constructor
    ConstantFrequencyInterpolator( double frequency ):
        StationFrequencyInterpolator( ),
        frequency_( frequency )
    { }

    //! Destructor
    ~ConstantFrequencyInterpolator( ) { }

    double getCurrentFrequency( const double lookupTime )
    {
        return frequency_;
    }

    double getFrequencyIntegral( const double quadratureStartTime, const double quadratureEndTime )
    {
        return frequency_ * ( quadratureEndTime - quadratureStartTime );
    }

    double getAveragedFrequencyIntegral( const double quadratureStartTime, const double quadratureEndTime )
    {
        return getFrequencyIntegral( quadratureStartTime, quadratureEndTime ) / ( quadratureEndTime - quadratureStartTime );
    }

private:

    double frequency_;
};

// TODO: test computation of frequencies and integral
class PiecewiseLinearFrequencyInterpolator: public StationFrequencyInterpolator
{
public:
    PiecewiseLinearFrequencyInterpolator(
            std::vector< std::shared_ptr< input_output::OdfRampBlock > > rampBlock ):
        StationFrequencyInterpolator( )
    {
        for( unsigned int i = 0; i < rampBlock.size( ); i++ )
        {
            startTimes_.push_back( rampBlock.at( i )->getRampStartTime( ) );
            endTimes_.push_back( rampBlock.at( i )->getRampEndTime( ) );
            rampRates_.push_back( rampBlock.at( i )->getRampRate( ) );
            startFrequencies_.push_back( rampBlock.at( i )->getRampStartFrequency( ) );
        }

        startTimeLookupScheme_ = std::make_shared< interpolators::HuntingAlgorithmLookupScheme< double > >(
                startTimes_ );
    }

    PiecewiseLinearFrequencyInterpolator(
            const std::vector< double >& startTimes_,
            const std::vector< double >& endTimes_,
            const std::vector< double >& rampRates_,
            const std::vector< double >& startFrequency_ ):
        StationFrequencyInterpolator( ),
        startTimes_( startTimes_ ), endTimes_( endTimes_ ), rampRates_( rampRates_ ), startFrequencies_( startFrequency_ )
    {
        startTimeLookupScheme_ = std::make_shared< interpolators::HuntingAlgorithmLookupScheme< double > >(
                startTimes_ );
    }

    double getFrequencyIntegral( const double quadratureStartTime, const double quadratureEndTime )
    {
        std::vector< double > quadratureTimes;
        std::vector < double > quadratureFrequencies;

        // Point corresponding to first partial ramp
        quadratureTimes.push_back ( quadratureStartTime );
        quadratureFrequencies.push_back ( getCurrentFrequency( quadratureStartTime ) );

        // Points corresponding to full ramps
        for( unsigned int i = 1; i < startTimes_.size( ) && startTimes_.at( i ) < quadratureEndTime; i++ )
        {
            quadratureTimes.push_back( startTimes_.at( i ) );
            quadratureFrequencies.push_back( startFrequencies_.at( i ) );
        }

        // Point corresponding to final partial ramp
        quadratureTimes.push_back ( quadratureEndTime );
        quadratureFrequencies.push_back ( getCurrentFrequency( quadratureEndTime ) );

        return numerical_quadrature::performTrapezoidalQuadrature( quadratureTimes, quadratureFrequencies );
    }

    double getCurrentFrequency( const double lookupTime )
    {
        int lowerNearestNeighbour = startTimeLookupScheme_->findNearestLowerNeighbour( lookupTime );

        if( lookupTime > endTimes_.at( lowerNearestNeighbour ) || lookupTime < startTimes_.at ( lowerNearestNeighbour ) )
        {
            throw std::runtime_error(
                    "Error when interpolating ramp reference frequency: look up time (" + std::to_string( lookupTime ) +
                    ") is outside the ramp table interval (" + std::to_string( startTimes_.at( 0 ) ) + " to " +
                    std::to_string( startTimes_.back( ) ) + ")." );
        }

        return startFrequencies_.at( lowerNearestNeighbour ) +
               rampRates_.at( lowerNearestNeighbour ) * ( lookupTime - startTimes_.at( lowerNearestNeighbour ) );
    }

    std::vector< double > getStartTimes ( )
    {
        return startTimes_;
    }

    std::vector< double > getEndTimes ( )
    {
        return endTimes_;
    }

    std::vector< double > getRampRates ( )
    {
        return rampRates_;
    }

    std::vector< double > getStartFrequencies ( )
    {
        return startFrequencies_;
    }

private:

    std::vector< double > startTimes_;
    std::vector< double > endTimes_;
    std::vector< double > rampRates_;
    std::vector< double > startFrequencies_;

    std::shared_ptr< interpolators::LookUpScheme< double > > startTimeLookupScheme_;

};


// All time intervals are assumed to have the same size
class PiecewiseConstantFrequencyInterpolator: public StationFrequencyInterpolator
{
public:
    //! Constructor
    PiecewiseConstantFrequencyInterpolator( std::vector< double > frequencies,
                                            std::vector< double > referenceTimes,
                                            double timeIntervalsSize ):
        StationFrequencyInterpolator( ),
        frequencies_( frequencies ),
        referenceTimes_( referenceTimes ),
        timeIntervalsSize_( timeIntervalsSize )
    {
        if ( frequencies.size( ) != referenceTimes.size( ) )
        {
            throw std::runtime_error("Error when creating piecewise constant frequency interpolator: size of time stamps and "
                                     "frequencies are not consistent.");
        }

        startTimeLookupScheme_ = std::make_shared< interpolators::HuntingAlgorithmLookupScheme< double > >(
                referenceTimes_ );
    }

    //! Destructor
    ~PiecewiseConstantFrequencyInterpolator( ) { }

    double getCurrentFrequency( const double lookupTime )
    {
        int lowerNearestNeighbour = startTimeLookupScheme_->findNearestLowerNeighbour( lookupTime );
        int higherNearestNeighbour = lowerNearestNeighbour + 1;

        // Look-up time closer to lower nearest neighbour
        if ( lookupTime - referenceTimes_.at( lowerNearestNeighbour ) <=  referenceTimes_.at( higherNearestNeighbour ) - lookupTime ||
            lowerNearestNeighbour == referenceTimes_.size( ) - 1 )
        {
            return frequencies_.at( lowerNearestNeighbour );
        }
        // Look-up time closer to higher nearest neighbour
        else
        {
            return frequencies_.at( higherNearestNeighbour );
        }
    }

    double getFrequencyIntegral( const double quadratureStartTime, const double quadratureEndTime )
    {
        throw std::runtime_error("Computation of integral not implemented for piecewise constant frequency.");
    }

    double getAveragedFrequencyIntegral( const double quadratureStartTime, const double quadratureEndTime )
    {
        double referenceTime = quadratureStartTime + ( quadratureEndTime - quadratureStartTime ) / 2.0;

        if ( ( referenceTime - quadratureStartTime ) / ( timeIntervalsSize_ / 2.0 ) - 1.0 > 1e-12 ||
            ( quadratureEndTime - referenceTime ) / ( timeIntervalsSize_ / 2.0 ) - 1.0 > 1e-12 )
        {
            throw std::runtime_error("Error when computing the averaged integral of piecewise constant frequency: "
                                     "the specified time interval does not coincide with any piecewise interval.");
        }

        return getCurrentFrequency( referenceTime );
    }

private:

    std::vector< double > frequencies_;
    std::vector< double > referenceTimes_;

    double timeIntervalsSize_;

    std::shared_ptr< interpolators::LookUpScheme< double > > startTimeLookupScheme_;
};


class ProcessedOdfFileContents
{
public:

    std::string spacecraftName_;

    std::map< observation_models::ObservableType, std::map< observation_models::LinkEnds,
        std::shared_ptr< ProcessedOdfFileSingleLinkData > > > processedDataBlocks_;

    std::map< int, std::shared_ptr< PiecewiseLinearFrequencyInterpolator > > rampInterpolators_;
};


std::shared_ptr< PiecewiseLinearFrequencyInterpolator > mergeRampDataInterpolators(
        const std::vector< std::shared_ptr< PiecewiseLinearFrequencyInterpolator > >& interpolatorList );

void addOdfFileContentsToMergedContents(
        const observation_models::ObservableType observableType,
        std::shared_ptr< ProcessedOdfFileSingleLinkData > mergedOdfFileContents,
        std::shared_ptr< ProcessedOdfFileSingleLinkData > blockToAdd );

std::shared_ptr< ProcessedOdfFileContents > mergeOdfFileContents(
        const std::vector< std::shared_ptr< ProcessedOdfFileContents > > odfFileContents );

void addOdfDataBlockToProcessedData(
        const observation_models::ObservableType currentObservableType,
        const std::shared_ptr< input_output::OdfDataBlock > rawDataBlock,
        const std::shared_ptr< ProcessedOdfFileSingleLinkData > processedDataBlock );

observation_models::LinkEnds getLinkEndsFromOdfBlock (
        const std::shared_ptr< input_output::OdfDataBlock > dataBlock,
        std::string spacecraftName );

std::shared_ptr< ProcessedOdfFileContents > processOdfFileContents(
        const std::shared_ptr< input_output::OdfRawFileContents > rawOdfData,
        bool verbose = true );

template< typename TimeType = double >
observation_models::ObservationAncilliarySimulationSettings< TimeType > createOdfAncillarySettings
        ( std::shared_ptr< ProcessedOdfFileSingleLinkData > odfDataContents,
          unsigned int dataIndex )
{
    if ( dataIndex >= odfDataContents->observationTimes_.size( ) )
    {
        throw std::runtime_error("Error when creating ODF data ancillary settings: specified data index is larger than data size.");
    }

    observation_models::ObservationAncilliarySimulationSettings< TimeType > ancillarySettings =
            observation_models::ObservationAncilliarySimulationSettings< TimeType >( );

    observation_models::ObservableType currentObservableType = odfDataContents->observableType_;

    ancillarySettings.setAncilliaryDoubleData(
                observation_models::doppler_reference_frequency, currentObservableType->referenceFrequencies_.at( dataIndex ) );

    if ( std::dynamic_pointer_cast< ProcessedOdfFileDopplerData >( odfDataContents ) != nullptr )
    {
        std::shared_ptr< ProcessedOdfFileDopplerData > dopplerDataBlock
                = std::dynamic_pointer_cast< ProcessedOdfFileDopplerData >( odfDataContents );

        ancillarySettings.setAncilliaryDoubleData(
                observation_models::doppler_integration_time, dopplerDataBlock->getCountInterval( ).at( dataIndex ) );
        ancillarySettings.setAncilliaryDoubleData(
                observation_models::doppler_reference_frequency, dopplerDataBlock->referenceFrequencies_.at( dataIndex ) );

        if ( currentObservableType == observation_models::n_way_differenced_range )
        {
            ancillarySettings.setAncilliaryDoubleVectorData(
                observation_models::retransmission_delays,  std::vector< double >{
                    dopplerDataBlock->transmitterUplinkDelays_.at( dataIndex ), 0.0,
                    dopplerDataBlock->receiverDownlinkDelays_.at( dataIndex ) } );
        }
        else
        {
            ancillarySettings.setAncilliaryDoubleVectorData(
                observation_models::retransmission_delays, std::vector< double >{
                    dopplerDataBlock->transmitterUplinkDelays_.at( dataIndex ),
                    dopplerDataBlock->receiverDownlinkDelays_.at( dataIndex ) } );
        }

    }
    else
    {
        throw std::runtime_error("Error when casting ODF processed data: data type not identified.");
    }
}

template< typename ObservationScalarType = double, typename TimeType = double >
void separateSingleLinkOdfData(
        observation_models::ObservableType currentObservableType,
        std::shared_ptr< ProcessedOdfFileSingleLinkData > odfSingleLinkData,
        std::vector< std::vector< TimeType > >& observationTimes,
        std::vector< std::vector< Eigen::Matrix< ObservationScalarType, Eigen::Dynamic, 1 > > >& observables,
        std::vector< observation_models::ObservationAncilliarySimulationSettings< TimeType > >& ancillarySettings,
        const simulation_setup::SystemOfBodies& bodies )
{
    // Initialize vectors
    observationTimes.clear( );
    observables.clear( );
    ancillarySettings.clear( );

    // Get time and observables vectors
    std::vector< double > observationTimesTdb = odfSingleLinkData->getObservationTimesTdb( bodies );
    std::vector< Eigen::Matrix< double, Eigen::Dynamic, 1 > > observablesVector =
            odfSingleLinkData->getProcessedObservablesVector( );

    for ( unsigned int i = 0; i < odfSingleLinkData->observationTimes_.size( ); ++i )
    {
        observation_models::ObservationAncilliarySimulationSettings< TimeType > currentAncillarySettings =
                createOdfAncillarySettings( odfSingleLinkData, i );

        bool newAncillarySettings = true;

        for ( unsigned int j = 0; j < ancillarySettings.size( ); ++j )
        {
            if ( ancillarySettings.at( j ) == currentAncillarySettings )
            {
                newAncillarySettings = false;
                observationTimes.at( j ).push_back( observationTimesTdb.at( i ) );
                observables.at( j ).push_back( observablesVector.at( i ) );
                break;
            }
        }

        if ( newAncillarySettings )
        {
            observationTimes.push_back ( std::vector< TimeType >{ observationTimesTdb.at( i ) } );
            observables.push_back( std::vector< Eigen::Matrix< ObservationScalarType, Eigen::Dynamic, 1 > >{
                observablesVector.at( i ) } );
            ancillarySettings.push_back( currentAncillarySettings );
        }
    }
}

template< typename ObservationScalarType = double, typename TimeType = double >
std::shared_ptr< observation_models::ObservationCollection< ObservationScalarType, TimeType > > createOdfObservationCollection(
        std::shared_ptr< ProcessedOdfFileContents > processedOdfFileContents,
        const simulation_setup::SystemOfBodies& bodies,
        const std::shared_ptr< simulation_setup::ObservationDependentVariableCalculator > dependentVariableCalculator = nullptr )
{

    std::map< observation_models::ObservableType, std::map< observation_models::LinkEnds, std::vector< std::shared_ptr<
            observation_models::SingleObservationSet< ObservationScalarType, TimeType > > > > > sortedObservationSets;

    for ( auto observableTypeIterator = processedOdfFileContents->processedDataBlocks_.begin( );
            observableTypeIterator != processedOdfFileContents->processedDataBlocks_.end( ); ++observableTypeIterator )
    {
        observation_models::ObservableType currentObservableType = observableTypeIterator->first;

        for ( auto linkEndsIterator = observableTypeIterator->second.begin( );
                linkEndsIterator != observableTypeIterator->second.end( ); ++linkEndsIterator )
        {
            observation_models::LinkEnds currentLinkEnds = linkEndsIterator->first;
            std::shared_ptr< ProcessedOdfFileSingleLinkData > currentOdfSingleLinkData = linkEndsIterator->second;

            // Reset vector of observation sets
            sortedObservationSets.at( currentObservableType ).at( currentLinkEnds ).clear( );

            // Get vectors of times, observations, and ancillary settings for the current observable type and link ends
            std::vector< std::vector< TimeType > > observationTimes;
            std::vector< std::vector< Eigen::Matrix< ObservationScalarType, Eigen::Dynamic, 1 > > > observables;
            std::vector< observation_models::ObservationAncilliarySimulationSettings< TimeType > > ancillarySettings;

            // Fill vectors
            separateSingleLinkOdfData(
                    currentObservableType, currentOdfSingleLinkData, observationTimes, observables, ancillarySettings,
                    bodies );

            // Create the single observation sets and save them
            for ( unsigned int i = 0; i < observationTimes.size( ); ++i )
            {
                if ( dependentVariableCalculator != nullptr )
                {
//                    sortedObservationSets.at( currentObservableType ).at( currentLinkEnds ).push_back(
//                        currentObservableType, currentLinkEnds, observables.at( i ), observationTimes.at( i ),
//                        observation_models::receiver,
//                        dependentVariableCalculator->calculateDependentVariables( ),
//                        dependentVariableCalculator, ancillarySettings.at( i ) );
                    throw std::runtime_error( "Computation of dependent variables for ODF observables is not implemented." );
                }
                else
                {
                    sortedObservationSets.at( currentObservableType ).at( currentLinkEnds ).push_back(
                        currentObservableType, currentLinkEnds, observables.at( i ), observationTimes.at( i ),
                        observation_models::receiver,
                        std::vector< Eigen::Matrix< ObservationScalarType, Eigen::Dynamic, 1 > >( ),
                        nullptr, ancillarySettings.at( i ) );
                }

            }
        }
    }

}

} // namespace orbit_determination

} // namespace tudat

#endif // TUDAT_PARSEODFFILE_H
