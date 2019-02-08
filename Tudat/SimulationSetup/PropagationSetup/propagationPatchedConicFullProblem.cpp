
/*    Copyright (c) 2010-2017, Delft University of Technology
 *    All rigths reserved
 *
 *    This file is part of the Tudat. Redistribution and use in source and
 *    binary forms, with or without modification, are permitted exclusively
 *    under the terms of the Modified BSD license. You should have received
 *    a copy of the license with this file. If not, please or visit:
 *    http://tudat.tudelft.nl/LICENSE.
 */

#include "Tudat/SimulationSetup/PropagationSetup/propagationPatchedConicFullProblem.h"
#include "Tudat/SimulationSetup/PropagationSetup/propagationLambertTargeterFullProblem.h"
#include "Tudat/Astrodynamics/TrajectoryDesign/exportTrajectory.h"

#include <Tudat/InputOutput/basicInputOutput.h>
#include <tudatExampleApplications/satellitePropagatorExamples/SatellitePropagatorExamples/applicationOutput.h>
#include <Tudat/SimulationSetup/tudatSimulationHeader.h>

#include "Tudat/Astrodynamics/TrajectoryDesign/captureLeg.h"
#include "Tudat/Astrodynamics/TrajectoryDesign/departureLegMga.h"
#include "Tudat/Astrodynamics/TrajectoryDesign/departureLegMga1DsmPosition.h"
#include "Tudat/Astrodynamics/TrajectoryDesign/departureLegMga1DsmVelocity.h"
#include "Tudat/Astrodynamics/TrajectoryDesign/planetTrajectory.h"
#include "Tudat/Astrodynamics/TrajectoryDesign/swingbyLegMga.h"
#include "Tudat/Astrodynamics/TrajectoryDesign/swingbyLegMga1DsmPosition.h"
#include "Tudat/Astrodynamics/TrajectoryDesign/swingbyLegMga1DsmVelocity.h"
#include "Tudat/Astrodynamics/TrajectoryDesign/trajectory.h"

namespace tudat
{

namespace propagators
{


//! Function to get default minimum pericenter radii for a list of bodiess
std::vector< double > getDefaultMinimumPericenterRadii( const std::vector< std::string >& bodyNames )
{
    std::vector< double > pericenterRadii;
    for( unsigned int i = 0; i < bodyNames.size( ); i ++ )
    {
        if( bodyNames.at( i ) == "Mercury" )
        {
            pericenterRadii.push_back( 2639.7E3 );
        }
        else if( bodyNames.at( i ) == "Venus" )
        {
            pericenterRadii.push_back( 6251.8E3 );
        }
        else if( bodyNames.at( i ) == "Earth" )
        {
            pericenterRadii.push_back( 6578.1E3 );
        }
        else if( bodyNames.at( i ) == "Mars" )
        {
            pericenterRadii.push_back( 3596.2E3 );
        }
        else if( bodyNames.at( i ) == "Jupiter" )
        {
            pericenterRadii.push_back( 72000.0E3 );
        }
        else if( bodyNames.at( i ) == "Saturn" )
        {
            pericenterRadii.push_back( 61000.0E3 );
        }
        else if( bodyNames.at( i ) == "Uranus" )
        {
            pericenterRadii.push_back( 26000.0E3 );
        }
        else if( bodyNames.at( i ) == "Neptune" )
        {
            pericenterRadii.push_back( 25000.0E3 );
        }
        else if( bodyNames.at( i ) == "Pluto" )
        {
            pericenterRadii.push_back( 1395.0E3 );
        }
        else
        {
            throw std::runtime_error(
                        "Error, could not recognize body " + bodyNames.at( i ) + " when getting minimum periapsis radius" );
        }
    }
    return pericenterRadii;
}


//! Function to setup a body map corresponding to the assumptions of a patched conics trajectory,
//! using default ephemerides for the central and transfer bodies.
simulation_setup::NamedBodyMap setupBodyMapFromEphemeridesForPatchedConicsTrajectory(
        const std::string& nameCentralBody,
        const std::string& nameBodyToPropagate,
        const std::vector< std::string >& nameTransferBodies)
{
    spice_interface::loadStandardSpiceKernels( );

    // Create central and transfer bodies.
    std::vector< std::string > bodiesToCreate;
    bodiesToCreate.push_back( nameCentralBody );
    for ( unsigned int i = 0 ; i < nameTransferBodies.size( ) ; i ++ )
    {
        bodiesToCreate.push_back( nameTransferBodies[ i ] );
    }


    std::map< std::string, std::shared_ptr< simulation_setup::BodySettings > > bodySettings =
            simulation_setup::getDefaultBodySettings( bodiesToCreate );

    std::string frameOrigin = "SSB";
    std::string frameOrientation = "ECLIPJ2000";


    // Define central body ephemeris settings.
    bodySettings[ nameCentralBody ]->ephemerisSettings = std::make_shared< simulation_setup::ConstantEphemerisSettings >(
                ( Eigen::Vector6d( ) << 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 ).finished( ), frameOrigin, frameOrientation );

    bodySettings[ nameCentralBody ]->ephemerisSettings->resetFrameOrientation( frameOrientation );
    bodySettings[ nameCentralBody ]->rotationModelSettings->resetOriginalFrame( frameOrientation );


    // Create body map.
    simulation_setup::NamedBodyMap bodyMap = createBodies( bodySettings );


    // Define body to propagate.
    bodyMap[ nameBodyToPropagate ] = std::make_shared< simulation_setup::Body >( );
    bodyMap[ nameBodyToPropagate ]->setEphemeris( std::make_shared< ephemerides::TabulatedCartesianEphemeris< > >(
                                                      std::shared_ptr< interpolators::OneDimensionalInterpolator
                                                      < double, Eigen::Vector6d > >( ), frameOrigin, frameOrientation ) );


    setGlobalFrameBodyEphemerides( bodyMap, frameOrigin, frameOrientation );


    return bodyMap;
}



//! Function to setup a body map corresponding to the assumptions of the patched conics trajectory,
//! the ephemerides of the transfer bodies being provided as inputs.
simulation_setup::NamedBodyMap setupBodyMapFromUserDefinedEphemeridesForPatchedConicsTrajectory(const std::string& nameCentralBody,
                                                                                                const std::string& nameBodyToPropagate,
                                                                                                const std::vector< std::string >& nameTransferBodies,
                                                                                                const std::vector< ephemerides::EphemerisPointer >& ephemerisVectorTransferBodies,
                                                                                                const std::vector< double >& gravitationalParametersTransferBodies)
{

    spice_interface::loadStandardSpiceKernels( );


    // Create central body object.
    std::vector< std::string > bodiesToCreate;
    bodiesToCreate.push_back( nameCentralBody );

    std::map< std::string, std::shared_ptr< simulation_setup::BodySettings > > bodySettings =
            simulation_setup::getDefaultBodySettings( bodiesToCreate );

    std::string frameOrigin = "SSB";
    std::string frameOrientation = "J2000";

    // Define central body ephemeris settings.
    bodySettings[ nameCentralBody ]->ephemerisSettings = std::make_shared< simulation_setup::ConstantEphemerisSettings >(
                ( Eigen::Vector6d( ) << 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 ).finished( ), frameOrigin, frameOrientation );

    bodySettings[ nameCentralBody ]->ephemerisSettings->resetFrameOrientation( frameOrientation );
    bodySettings[ nameCentralBody ]->rotationModelSettings->resetOriginalFrame( frameOrientation );


    // Create body map.
    simulation_setup::NamedBodyMap bodyMap = createBodies( bodySettings );

    bodyMap[ nameBodyToPropagate ] = std::make_shared< simulation_setup::Body >( );
    bodyMap[ nameBodyToPropagate ]->setEphemeris( std::make_shared< ephemerides::TabulatedCartesianEphemeris< > >(
                                                      std::shared_ptr< interpolators::OneDimensionalInterpolator
                                                      < double, Eigen::Vector6d > >( ), frameOrigin, frameOrientation ) );


    // Define ephemeris and gravity field for the transfer bodies.
    for ( unsigned int i = 0 ; i < nameTransferBodies.size( ) ; i ++ )
    {

        bodyMap[ nameTransferBodies[ i ] ] = std::make_shared< simulation_setup::Body >( );
        bodyMap[ nameTransferBodies[ i ] ]->setEphemeris( ephemerisVectorTransferBodies[ i ] );
        bodyMap[ nameTransferBodies[ i ] ]->setGravityFieldModel( simulation_setup::createGravityFieldModel(
                                                                      std::make_shared< simulation_setup::CentralGravityFieldSettings >( gravitationalParametersTransferBodies[ i ] ),
                                                                      nameTransferBodies[ i ] ) );
    }

    setGlobalFrameBodyEphemerides( bodyMap, frameOrigin, frameOrientation );

    return bodyMap;

}



//! Function to directly setup a vector of acceleration maps for a patched conics trajectory.
std::vector < basic_astrodynamics::AccelerationMap > setupAccelerationMapPatchedConicsTrajectory(
        const double numberOfLegs,
        const std::string& nameCentralBody,
        const std::string& nameBodyToPropagate,
        const simulation_setup::NamedBodyMap& bodyMap )
{
    std::vector< basic_astrodynamics::AccelerationMap > accelerationMapsVector;

    for( int i = 0 ; i < numberOfLegs ; i ++ )
    {
        accelerationMapsVector.push_back( setupAccelerationMapLambertTargeter(nameCentralBody, nameBodyToPropagate, bodyMap) );
    }

    return accelerationMapsVector;
}



//! Function to create the trajectory from the body map.
transfer_trajectories::Trajectory createTransferTrajectoryObject(
        const simulation_setup::NamedBodyMap& bodyMap,
        const std::vector< std::string >& transferBodyOrder,
        const std::string& centralBody,
        const std::vector< transfer_trajectories::TransferLegType >& transferLegTypes,
        const std::vector< double > trajectoryIndependentVariables,
        const std::vector< double > minimumPericenterRadii,
        const bool includeDepartureDeltaV,
        const double departureSemiMajorAxis,
        const double departureEccentricity,
        const bool includeArrivalDeltaV,
        const double arrivalSemiMajorAxis,
        const double arrivalEccentricity )
{
    int numberOfLegs = transferBodyOrder.size( );

    std::vector< ephemerides::EphemerisPointer > ephemerisVector;
    Eigen::VectorXd gravitationalParameterVector = Eigen::VectorXd::Zero( transferBodyOrder.size( ) );

    for( unsigned int i = 0; i < transferBodyOrder.size( ); i ++ )
    {
        if( bodyMap.count( transferBodyOrder.at( i ) ) != 0 )
        {
            ephemerisVector.push_back( bodyMap.at( transferBodyOrder.at( i ) )->getEphemeris( ) );
            gravitationalParameterVector( i ) =
                    bodyMap.at( transferBodyOrder.at( i ) )->getGravityFieldModel( )->getGravitationalParameter( );

        }
    }

    double centralBodyGravitationalParameter = TUDAT_NAN;
    if( bodyMap.count( centralBody ) != 0 )
    {
        centralBodyGravitationalParameter = bodyMap.at( centralBody )->getGravityFieldModel( )->getGravitationalParameter( );
    }
    else
    {
        throw std::runtime_error( "Error, central body " + centralBody + " not found when creating transfer trajectory object" );
    }

    Eigen::VectorXd semiMajorAxesVector =
            ( Eigen::VectorXd( 2 ) << departureSemiMajorAxis, arrivalSemiMajorAxis ).finished( );
    Eigen::VectorXd eccentricityVector =
            ( Eigen::VectorXd( 2 ) << departureEccentricity, arrivalEccentricity ).finished( );

    return transfer_trajectories::Trajectory(
                numberOfLegs, transferLegTypes, ephemerisVector, gravitationalParameterVector,
                utilities::convertStlVectorToEigenVector( trajectoryIndependentVariables ), centralBodyGravitationalParameter,
                utilities::convertStlVectorToEigenVector( minimumPericenterRadii ),
                semiMajorAxesVector, eccentricityVector, includeDepartureDeltaV, includeArrivalDeltaV );

}

//! Function to both calculate a patched conics leg without DSM and propagate the full dynamics problem.
void propagateMgaWithoutDsmAndFullProblem(
        simulation_setup::NamedBodyMap& bodyMap,
        const std::vector< std::string > departureAndArrivalBodies,
        const std::string centralBody,
        const Eigen::Vector3d cartesianPositionAtDeparture,
        const Eigen::Vector3d cartesianPositionAtArrival,
        const double initialTime,
        const double timeOfFlight,
        const std::pair< std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > >,
        std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > > propagatorSettings,
        const std::shared_ptr< numerical_integrators::IntegratorSettings< double > >& integratorSettings,
        std::map< double, Eigen::Vector6d >& patchedConicsResult,
        std::map< double, Eigen::Vector6d >& fullProblemResult)
{
    integratorSettings->initialTime_ = initialTime;

    // Compute the difference in state between the full problem and the Lambert targeter solution for the current leg.
    propagators::propagateLambertTargeterAndFullProblem( timeOfFlight, initialTime, bodyMap, centralBody,
                                                         propagatorSettings, integratorSettings,
                                                         patchedConicsResult, fullProblemResult, departureAndArrivalBodies,
                                                         bodyMap[ centralBody ]->getGravityFieldModel( )->getGravitationalParameter( ) ,
                                                         cartesianPositionAtDeparture, cartesianPositionAtArrival);
}



//! Function to both calculate a patched conics leg including a DSM and propagate the corresponding full dynamics problem.
void propagateMga1DsmVelocityAndFullProblem(
        simulation_setup::NamedBodyMap& bodyMap,
        const std::vector< std::string > departureAndArrivalBodies,
        const std::string& dsm,
        const std::string& centralBody,
        const Eigen::Vector3d cartesianPositionAtDeparture,
        const Eigen::Vector3d cartesianPositionDSM,
        const Eigen::Vector3d cartesianPositionAtArrival,
        const double initialTime,
        const double timeDsm,
        const double timeArrival,
        const transfer_trajectories::TransferLegType& legType,
        const std::vector< double >& trajectoryVariableVector,
        const double semiMajorAxis,
        const double eccentricity,
        Eigen::Vector3d& velocityAfterDeparture,
        Eigen::Vector3d& velocityBeforeArrival,
        const std::pair< std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > >,
        std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > > propagatorSettingsBeforeDsm,
        const std::pair< std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > >,
        std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > > propagatorSettingsAfterDsm,
        const std::shared_ptr< numerical_integrators::IntegratorSettings< double > >& integratorSettings,
        std::map< double, Eigen::Vector6d >& patchedConicsResultFromDepartureToDsm,
        std::map< double, Eigen::Vector6d >& fullProblemResultFromDepartureToDsm,
        std::map< double, Eigen::Vector6d >& patchedConicsResultFromDsmToArrival,
        std::map< double, Eigen::Vector6d >& fullProblemResultFromDsmToArrival)
{

    if( legType == transfer_trajectories::mga1DsmVelocity_Departure )
    {
        std::shared_ptr< transfer_trajectories::DepartureLegMga1DsmVelocity > departureLegMga1DsmVelocity =
                std::make_shared< transfer_trajectories::DepartureLegMga1DsmVelocity >(
                    cartesianPositionAtDeparture, cartesianPositionAtArrival, timeArrival - initialTime,
                    bodyMap[ departureAndArrivalBodies[ 0 ] ]->getEphemeris( )->getCartesianState( initialTime ).segment( 3, 3 ),
                bodyMap[ centralBody ]->getGravityFieldModel( )->getGravitationalParameter( ),
                bodyMap[ departureAndArrivalBodies[ 0 ] ]->getGravityFieldModel( )->getGravitationalParameter( ),
                semiMajorAxis, eccentricity,
                trajectoryVariableVector[ 0 ],
                trajectoryVariableVector[ 1 ],
                trajectoryVariableVector[ 2 ],
                trajectoryVariableVector[ 3 ], true  );

        double deltaV;
        Eigen::Vector3d departureBodyPosition;
        Eigen::Vector3d departureBodyVelocity;

        // Update value of velocity before arrival.
        departureLegMga1DsmVelocity->calculateLeg( velocityBeforeArrival, deltaV );

        // Update value of velocity after departure.
        departureLegMga1DsmVelocity->returnDepartureVariables( departureBodyPosition, departureBodyVelocity, velocityAfterDeparture );
    }
    else if( legType == transfer_trajectories::mga1DsmVelocity_Swingby )
    {
        std::shared_ptr< Eigen::Vector3d > pointerToVelocityBeforeArrival = std::make_shared< Eigen::Vector3d >( velocityBeforeArrival );

        std::shared_ptr< transfer_trajectories::SwingbyLegMga1DsmVelocity > swingbyLegMga1DsmVelocity =
                std::make_shared< transfer_trajectories::SwingbyLegMga1DsmVelocity >(
                    cartesianPositionAtDeparture, cartesianPositionAtArrival, timeArrival - initialTime,
                    bodyMap[ departureAndArrivalBodies[ 0 ] ]->getEphemeris( )->getCartesianState( initialTime ).segment( 3, 3 ),
                bodyMap[ centralBody ]->getGravityFieldModel( )->getGravitationalParameter( ),
                bodyMap[ departureAndArrivalBodies[ 0 ] ]->getGravityFieldModel( )->getGravitationalParameter( ),
                pointerToVelocityBeforeArrival,
                trajectoryVariableVector[ 0 ],
                trajectoryVariableVector[ 1 ],
                trajectoryVariableVector[ 2 ],
                trajectoryVariableVector[ 3 ]);

        double deltaV;
        Eigen::Vector3d departureBodyPosition;
        Eigen::Vector3d departureBodyVelocity;

        // Update value of velocity before arrival.
        swingbyLegMga1DsmVelocity->calculateLeg( velocityBeforeArrival, deltaV );

        // Update value of velocity after departure.
        swingbyLegMga1DsmVelocity->returnDepartureVariables( departureBodyPosition, departureBodyVelocity, velocityAfterDeparture );
    }


    // First part of the leg: propagation of the state from departure body to DSM location.
    integratorSettings->initialTime_ = initialTime;

    std::vector< std::string >legDepartureAndArrival;
    legDepartureAndArrival.push_back( departureAndArrivalBodies[ 0 ] );
    legDepartureAndArrival.push_back( dsm );

    propagateKeplerianOrbitLegAndFullProblem(
                timeDsm - initialTime, initialTime, bodyMap, centralBody,
                legDepartureAndArrival, velocityAfterDeparture, propagatorSettingsBeforeDsm, integratorSettings,
                patchedConicsResultFromDepartureToDsm, fullProblemResultFromDepartureToDsm,
                bodyMap[ centralBody ]->getGravityFieldModel( )->getGravitationalParameter( ),
                cartesianPositionAtDeparture );

    // Second part of the leg: Lambert targeter from DSM location to arrival body.
    legDepartureAndArrival.clear( );
    legDepartureAndArrival.push_back( dsm );
    legDepartureAndArrival.push_back( departureAndArrivalBodies[ 1 ] );

    integratorSettings->initialTime_ = timeDsm;

    propagateLambertTargeterAndFullProblem( timeArrival - timeDsm, timeDsm, bodyMap, centralBody,
                                            propagatorSettingsAfterDsm, integratorSettings,
                                            patchedConicsResultFromDsmToArrival, fullProblemResultFromDsmToArrival, legDepartureAndArrival,
                                            bodyMap[ centralBody]->getGravityFieldModel( )->getGravitationalParameter( ),
                                            cartesianPositionDSM, cartesianPositionAtArrival);

}



//! Function to both calculate a patched conics leg including a DSM and propagate the corresponding full dynamics problem.
void propagateMga1DsmPositionAndFullProblem(
        simulation_setup::NamedBodyMap& bodyMap,
        const std::vector< std::string > departureAndArrivalBodies,
        const std::string& dsm,
        const std::string& centralBody,
        const Eigen::Vector3d cartesianPositionAtDeparture,
        const Eigen::Vector3d cartesianPositionDSM,
        const Eigen::Vector3d cartesianPositionAtArrival,
        const double initialTime,
        const double timeDsm,
        const double timeArrival,
        const transfer_trajectories::TransferLegType& legType,
        const std::vector< double >& trajectoryVariableVector,
        const double minimumPericenterRadius,
        const double semiMajorAxis,
        const double eccentricity,
        Eigen::Vector3d& velocityAfterDeparture,
        Eigen::Vector3d& velocityBeforeArrival,
        const std::pair< std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > >,
        std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > > propagatorSettingsBeforeDsm,
        const std::pair< std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > >,
        std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > > propagatorSettingsAfterDsm,
        const std::shared_ptr< numerical_integrators::IntegratorSettings< double > >& integratorSettings,
        std::map< double, Eigen::Vector6d >& patchedConicsResultFromDepartureToDsm,
        std::map< double, Eigen::Vector6d >& fullProblemResultFromDepartureToDsm,
        std::map< double, Eigen::Vector6d >& patchedConicsResultFromDsmToArrival,
        std::map< double, Eigen::Vector6d >& fullProblemResultFromDsmToArrival)
{
    if( legType == transfer_trajectories::mga1DsmPosition_Departure )
    {

        std::shared_ptr< transfer_trajectories::DepartureLegMga1DsmPosition > departureLegMga1DsmPosition =
                std::make_shared< transfer_trajectories::DepartureLegMga1DsmPosition >(
                    cartesianPositionAtDeparture, cartesianPositionAtArrival, timeArrival - initialTime,
                    bodyMap[ departureAndArrivalBodies[ 0 ] ]->getEphemeris( )->getCartesianState( initialTime ).segment( 3, 3 ),
                bodyMap[ centralBody ]->getGravityFieldModel( )->getGravitationalParameter( ),
                bodyMap[ departureAndArrivalBodies[ 0 ] ]->getGravityFieldModel( )->getGravitationalParameter( ),
                semiMajorAxis, eccentricity,
                trajectoryVariableVector[ 0 ],
                trajectoryVariableVector[ 1 ],
                trajectoryVariableVector[ 2 ],
                trajectoryVariableVector[ 3 ], true  );

        double deltaV;
        Eigen::Vector3d departureBodyPosition;
        Eigen::Vector3d departureBodyVelocity;

        // Update value of velocity before arrival.
        departureLegMga1DsmPosition->calculateLeg( velocityBeforeArrival, deltaV );

        // Update value of velocity after departure.
        departureLegMga1DsmPosition->returnDepartureVariables( departureBodyPosition, departureBodyVelocity, velocityAfterDeparture );

    }

    if( legType == transfer_trajectories::mga1DsmPosition_Swingby )
    {

        std::shared_ptr< Eigen::Vector3d > pointerToVelocityBeforeArrival = std::make_shared< Eigen::Vector3d >( velocityBeforeArrival );

        std::shared_ptr< transfer_trajectories::MissionLeg > missionLeg;
        std::shared_ptr< transfer_trajectories::SwingbyLegMga1DsmPosition > swingbyLegMga1DsmPosition =
                std::make_shared< transfer_trajectories::SwingbyLegMga1DsmPosition >(
                    cartesianPositionAtDeparture, cartesianPositionAtArrival, timeArrival - initialTime,
                    bodyMap[ departureAndArrivalBodies[ 0 ] ]->getEphemeris( )->getCartesianState( initialTime ).segment( 3, 3 ),
                bodyMap[ centralBody ]->getGravityFieldModel( )->getGravitationalParameter( ),
                bodyMap[ departureAndArrivalBodies[ 0 ] ]->getGravityFieldModel( )->getGravitationalParameter( ),
                pointerToVelocityBeforeArrival, minimumPericenterRadius,
                trajectoryVariableVector[ 0 ],
                trajectoryVariableVector[ 1 ],
                trajectoryVariableVector[ 2 ],
                trajectoryVariableVector[ 3 ] );

        double deltaV;
        Eigen::Vector3d departureBodyPosition;
        Eigen::Vector3d departureBodyVelocity;

        // Update value of velocity before arrival.
        swingbyLegMga1DsmPosition->calculateLeg( velocityBeforeArrival, deltaV );

        // Update value of velocity after departure.
        swingbyLegMga1DsmPosition->returnDepartureVariables( departureBodyPosition, departureBodyVelocity, velocityAfterDeparture );


    }



    // First part of the leg: Lambert targeter from departure body to DSM location.

    integratorSettings->initialTime_ = initialTime;

    std::vector< std::string >legDepartureAndArrival;
    legDepartureAndArrival.push_back( departureAndArrivalBodies[ 0 ] );
    legDepartureAndArrival.push_back( dsm );

    propagateLambertTargeterAndFullProblem( timeDsm - initialTime, initialTime, bodyMap, centralBody,
                                            propagatorSettingsBeforeDsm, integratorSettings, patchedConicsResultFromDepartureToDsm,
                                            fullProblemResultFromDepartureToDsm, legDepartureAndArrival,
                                            bodyMap[ centralBody]->getGravityFieldModel( )->getGravitationalParameter( ),
                                            cartesianPositionAtDeparture, cartesianPositionDSM);


    // Second part of the leg: Lambert targeter from DSM to arrival body.

    legDepartureAndArrival.clear( );
    legDepartureAndArrival.push_back( dsm );
    legDepartureAndArrival.push_back( departureAndArrivalBodies[ 1 ] );

    integratorSettings->initialTime_ = timeDsm;

    propagateLambertTargeterAndFullProblem( timeArrival - timeDsm, timeDsm, bodyMap, centralBody,
                                            propagatorSettingsAfterDsm, integratorSettings, patchedConicsResultFromDsmToArrival,
                                            fullProblemResultFromDsmToArrival, legDepartureAndArrival,
                                            bodyMap[ centralBody]->getGravityFieldModel( )->getGravitationalParameter( ),
                                            cartesianPositionDSM, cartesianPositionAtArrival);

}



//! Function to propagate the motion of a body over a trajectory leg, both along a keplerian orbit and in a full dynamics problem.
void propagateKeplerianOrbitLegAndFullProblem(
        const double timeOfFlight,
        const double initialTime,
        const simulation_setup::NamedBodyMap& bodyMap,
        const std::string& centralBody,
        const std::vector<std::string>& departureAndArrivalBodies,
        const Eigen::Vector3d& velocityAfterDeparture,
        const std::pair< std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > >,
        std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > > propagatorSettings,
        const std::shared_ptr< numerical_integrators::IntegratorSettings< double > > integratorSettings,
        std::map< double, Eigen::Vector6d >& keplerianOrbitResult,
        std::map< double, Eigen::Vector6d >& fullProblemResult,
        const double centralBodyGravitationalParameter,
        const Eigen::Vector3d& cartesianPositionAtDeparture )
{
    // Clear output maps
    keplerianOrbitResult.clear( );
    fullProblemResult.clear( );

    // Retrieve the gravitational parameter of the relevant bodies.
    double gravitationalParameterCentralBody = ( centralBodyGravitationalParameter == centralBodyGravitationalParameter ) ?
                centralBodyGravitationalParameter :
                bodyMap.at( centralBody )->getGravityFieldModel( )->getGravitationalParameter( );

    // Get halved value of the time of flight, later used as initial time for the propagation.
    double halvedTimeOfFlight = timeOfFlight / 2.0;

    // Time at the end of the transfer
    double finalTime = initialTime + timeOfFlight;

    // Retrieve positions of departure and arrival bodies from ephemerides
    Eigen::Vector3d cartesianPositionAtDepartureForPatchedConics;
    if(  cartesianPositionAtDeparture != cartesianPositionAtDeparture )
    {
        // Cartesian position at departure
        if(  bodyMap.at( departureAndArrivalBodies.at( 0 ) )->getEphemeris( ) == nullptr)
        {
            throw std::runtime_error( "Ephemeris not defined for departure body." );
        }
        else
        {
            Eigen::Vector6d cartesianStateDepartureBody =
                    bodyMap.at( departureAndArrivalBodies.at( 0 ) )->getEphemeris( )->getCartesianState( initialTime );
            cartesianPositionAtDepartureForPatchedConics = cartesianStateDepartureBody.segment( 0, 3 );
        }
    }
    else
    {
        cartesianPositionAtDepartureForPatchedConics = cartesianPositionAtDeparture;
    }


    // Cartesian state at departure.
    Eigen::Vector6d cartesianStateAtDeparture;
    cartesianStateAtDeparture.segment( 0, 3 ) = cartesianPositionAtDepartureForPatchedConics;
    cartesianStateAtDeparture.segment( 3, 3 ) = velocityAfterDeparture;


    // Convert into keplerian elements
    Eigen::Vector6d keplerianStateAtDeparture = orbital_element_conversions::convertCartesianToKeplerianElements(
                cartesianStateAtDeparture, gravitationalParameterCentralBody );

    // Propagate the keplerian elements until half of the time of flight.
    Eigen::Vector6d keplerianStateAtHalvedTimeOfFlight = orbital_element_conversions::propagateKeplerOrbit( keplerianStateAtDeparture,
                                                                                                            halvedTimeOfFlight, gravitationalParameterCentralBody );

    // Convert the keplerian elements back into Cartesian elements.
    Eigen::Vector6d initialStatePropagation = orbital_element_conversions::convertKeplerianToCartesianElements(
                keplerianStateAtHalvedTimeOfFlight, gravitationalParameterCentralBody );





    Eigen::Vector6d cartesianStateKeplerianOrbit;


    // Define forward propagator settings variables.
    integratorSettings->initialTime_ = initialTime + halvedTimeOfFlight;

    // Define forward propagation settings
    std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > propagatorSettingsForwardPropagation;
    std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > propagatorSettingsBackwardPropagation;

    propagatorSettingsForwardPropagation = propagatorSettings.second;
    propagatorSettingsForwardPropagation->resetInitialStates( initialStatePropagation );

    propagatorSettingsBackwardPropagation = propagatorSettings.first;
    propagatorSettingsBackwardPropagation->resetInitialStates( initialStatePropagation );


    // Perform forward propagation.
    propagators::SingleArcDynamicsSimulator< > dynamicsSimulatorIntegrationForwards(
                bodyMap, integratorSettings, propagatorSettingsForwardPropagation );
    std::map< double, Eigen::VectorXd > stateHistoryFullProblemForwardPropagation = dynamicsSimulatorIntegrationForwards.
            getEquationsOfMotionNumericalSolution( );

    // Calculate the difference between the full problem and the Keplerian orbit solution along the forward propagation direction.
    for( std::map< double, Eigen::VectorXd >::iterator itr = stateHistoryFullProblemForwardPropagation.begin( );
         itr != stateHistoryFullProblemForwardPropagation.end( ); itr++ )
    {
        cartesianStateKeplerianOrbit = orbital_element_conversions::convertKeplerianToCartesianElements(
                    orbital_element_conversions::propagateKeplerOrbit(
                        orbital_element_conversions::convertCartesianToKeplerianElements(
                            initialStatePropagation, gravitationalParameterCentralBody ),
                        itr->first - ( initialTime + halvedTimeOfFlight ),
                        gravitationalParameterCentralBody ), gravitationalParameterCentralBody );
        keplerianOrbitResult[ itr->first ] = cartesianStateKeplerianOrbit;
        fullProblemResult[ itr->first ] = itr->second;
    }

    // Define backward propagator settings variables.
    integratorSettings->initialTimeStep_ = -integratorSettings->initialTimeStep_;
    integratorSettings->initialTime_ = initialTime + halvedTimeOfFlight;

    // Perform the backward propagation.
    propagators::SingleArcDynamicsSimulator< > dynamicsSimulatorIntegrationBackwards(bodyMap, integratorSettings, propagatorSettingsBackwardPropagation );
    std::map< double, Eigen::VectorXd > stateHistoryFullProblemBackwardPropagation =
            dynamicsSimulatorIntegrationBackwards.getEquationsOfMotionNumericalSolution( );

    // Calculate the difference between the full problem and the keplerian orbit solution along the backward propagation direction.
    for( std::map< double, Eigen::VectorXd >::iterator itr = stateHistoryFullProblemBackwardPropagation.begin( );
         itr != stateHistoryFullProblemBackwardPropagation.end( ); itr++ )
    {
        cartesianStateKeplerianOrbit = orbital_element_conversions::convertKeplerianToCartesianElements(
                    orbital_element_conversions::propagateKeplerOrbit(
                        orbital_element_conversions::convertCartesianToKeplerianElements(
                            initialStatePropagation, gravitationalParameterCentralBody ),
                        - ( initialTime + halvedTimeOfFlight) + itr->first,
                        gravitationalParameterCentralBody ), gravitationalParameterCentralBody );

        keplerianOrbitResult[ itr->first ] = cartesianStateKeplerianOrbit;
        fullProblemResult[ itr->first ] = itr->second;
    }

    // Reset initial integrator settings
    integratorSettings->initialTimeStep_ = -integratorSettings->initialTimeStep_;
}

//! Function to calculate the patched conics trajectory and to propagate the corresponding full problem.
void fullPropagationPatchedConicsTrajectory(
        simulation_setup::NamedBodyMap& bodyMap,
        const std::vector< std::string >& transferBodyOrder,
        const std::string& patchedConicCentralBody,
        const std::vector< transfer_trajectories::TransferLegType>& legTypeVector,
        const std::vector< double >& trajectoryVariableVector,
        const std::vector< double >& minimumPericenterRadiiVector,
        const std::vector< double >& semiMajorAxesVector,
        const std::vector< double >& eccentricitiesVector,
        const std::vector< std::pair< std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > >,
        std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > > > propagatorSettings,
        const std::shared_ptr< numerical_integrators::IntegratorSettings< double > >& integratorSettings,
        std::map< int, std::map< double, Eigen::Vector6d > >& patchedConicsResultForEachLeg,
        std::map< int, std::map< double, Eigen::Vector6d > >& fullProblemResultForEachLeg)
{
    int numberOfLegs = legTypeVector.size( );

    // Define the patched conic trajectory from the body map.
    transfer_trajectories::Trajectory trajectory = propagators::createTransferTrajectoryObject(
                bodyMap, transferBodyOrder, patchedConicCentralBody, legTypeVector, trajectoryVariableVector, minimumPericenterRadiiVector, true,
                semiMajorAxesVector[ 0 ], eccentricitiesVector[ 0 ], true, semiMajorAxesVector[ 1 ], eccentricitiesVector[ 1 ]);

    // Clear output maps.
    patchedConicsResultForEachLeg.clear( );
    fullProblemResultForEachLeg.clear( );

    // Calculate the trajectory.
    std::vector< Eigen::Vector3d > positionVector;
    std::vector< double > timeVector;
    std::vector< double > deltaVVector;
    double totalDeltaV;
    trajectory.calculateTrajectory( totalDeltaV );
    trajectory.maneuvers( positionVector, timeVector, deltaVVector );

    // Include manoeuvres between transfer bodies when required (considering that a deep space manoeuvre divides a leg into two smaller ones).
    std::vector< std::string > bodiesAndManoeuvresOrder;

    int counterDSMs = 1;
    for( int i = 0 ; i < numberOfLegs ; i ++ )
    {
        bodiesAndManoeuvresOrder.push_back(transferBodyOrder[ i ]);

        if( legTypeVector[ i ] != transfer_trajectories::mga_Departure && legTypeVector[ i ] != transfer_trajectories::mga_Swingby )
        {
            bodiesAndManoeuvresOrder.push_back("DSM" + std::to_string(counterDSMs) );
            counterDSMs++;
        }
    }

    int counterLegs = 0;
    int counterLegWithDSM = 0;

    Eigen::Vector3d departureBodyPosition;
    Eigen::Vector3d departureBodyVelocity;
    Eigen::Vector3d velocityAfterDeparture;
    Eigen::Vector3d velocityBeforeArrival;
    for( int i = 0 ; i < numberOfLegs - 1 ; i ++ )
    {
        // If the leg does not include any DSM.
        if( legTypeVector[ i ] == transfer_trajectories::mga_Departure || legTypeVector[ i ] == transfer_trajectories::mga_Swingby )
        {
            std::vector< std::string > departureAndArrivalBodies;
            departureAndArrivalBodies.push_back( bodiesAndManoeuvresOrder[ counterLegs ] );
            departureAndArrivalBodies.push_back( bodiesAndManoeuvresOrder[ counterLegs+ 1 ]);

            // Compute the difference in state between the full problem and the patched conics solution for the current leg.
            std::map< double, Eigen::Vector6d > patchedConicsResultCurrentLeg;
            std::map< double, Eigen::Vector6d > fullProblemResultCurrentLeg;

            propagators::propagateMgaWithoutDsmAndFullProblem(
                        bodyMap,  departureAndArrivalBodies, patchedConicCentralBody, positionVector[ counterLegs ], positionVector[ counterLegs+ 1 ],
                    timeVector[ counterLegs ],
                    timeVector[ counterLegs+ 1 ] - timeVector[ counterLegs ], propagatorSettings[ counterLegs ], integratorSettings,
                    patchedConicsResultCurrentLeg, fullProblemResultCurrentLeg);

            patchedConicsResultForEachLeg[ counterLegs ] = patchedConicsResultCurrentLeg;
            fullProblemResultForEachLeg[ counterLegs ] = fullProblemResultCurrentLeg;

            counterLegs++;
        }


        // If one DSM is included in the leg (velocity formulation).
        if(  legTypeVector[ i ] == transfer_trajectories::mga1DsmVelocity_Departure || legTypeVector[ i ] == transfer_trajectories::mga1DsmVelocity_Swingby )
        {
            std::vector< std::string > departureAndArrivalBodies;
            departureAndArrivalBodies.push_back( bodiesAndManoeuvresOrder[ counterLegs ] );
            departureAndArrivalBodies.push_back( bodiesAndManoeuvresOrder[ counterLegs + 2 ]);

            std::vector< double > trajectoryVariableVectorLeg;
            trajectoryVariableVectorLeg.push_back( trajectoryVariableVector[ numberOfLegs + 1 + (counterLegWithDSM * 4) ] );
            trajectoryVariableVectorLeg.push_back( trajectoryVariableVector[ numberOfLegs + 2 + (counterLegWithDSM * 4) ] );
            trajectoryVariableVectorLeg.push_back( trajectoryVariableVector[ numberOfLegs + 3 + (counterLegWithDSM * 4) ] );
            trajectoryVariableVectorLeg.push_back( trajectoryVariableVector[ numberOfLegs + 4 + (counterLegWithDSM * 4) ] );

            std::map< double, Eigen::Vector6d > patchedConicsResultFromDepartureToDsm;
            std::map< double, Eigen::Vector6d > fullProblemResultFromDepartureToDsm;
            std::map< double, Eigen::Vector6d > patchedConicsResultFromDsmToArrival;
            std::map< double, Eigen::Vector6d > fullProblemResultFromDsmToArrival;

            // Compute patched conics and full problem results along the leg.
            propagators::propagateMga1DsmVelocityAndFullProblem(
                        bodyMap, departureAndArrivalBodies,
                        bodiesAndManoeuvresOrder[ counterLegs+ 1 ],  patchedConicCentralBody, positionVector[ counterLegs ],
                    positionVector[ counterLegs+ 1 ], positionVector[ counterLegs + 2 ], timeVector[ counterLegs ], timeVector[ counterLegs+ 1 ],
                    timeVector[ counterLegs + 2 ], legTypeVector[ i ], trajectoryVariableVectorLeg, semiMajorAxesVector[ 0 ], eccentricitiesVector[ 0 ],
                    velocityAfterDeparture, velocityBeforeArrival, propagatorSettings[ counterLegs ], propagatorSettings[ counterLegs+ 1 ],
                    integratorSettings, patchedConicsResultFromDepartureToDsm, fullProblemResultFromDepartureToDsm,
                    patchedConicsResultFromDsmToArrival, fullProblemResultFromDsmToArrival);

            // Results for the first part of the leg (from departure body to DSM).
            patchedConicsResultForEachLeg[ counterLegs ] = patchedConicsResultFromDepartureToDsm;
            fullProblemResultForEachLeg[ counterLegs ] = fullProblemResultFromDepartureToDsm;
            counterLegs++;

            // Results for the second part of the leg (from DSM to arrival body ).
            patchedConicsResultForEachLeg[ counterLegs ] = patchedConicsResultFromDsmToArrival;
            fullProblemResultForEachLeg[ counterLegs ] = fullProblemResultFromDsmToArrival;
            counterLegs++;
            counterLegWithDSM++;
        }

        // If one DSM is included in the leg (position formulation).
        if( legTypeVector[ i ] == transfer_trajectories::mga1DsmPosition_Departure
                || legTypeVector[ i ] == transfer_trajectories::mga1DsmPosition_Swingby )
        {
            std::vector< std::string > departureAndArrivalBodies;
            departureAndArrivalBodies.push_back( bodiesAndManoeuvresOrder[ counterLegs ] );
            departureAndArrivalBodies.push_back( bodiesAndManoeuvresOrder[ counterLegs + 2 ]);

            std::vector< double > trajectoryVariableVectorLeg;
            trajectoryVariableVectorLeg.push_back( trajectoryVariableVector[ numberOfLegs + 1 + (counterLegWithDSM * 4) ] );
            trajectoryVariableVectorLeg.push_back( trajectoryVariableVector[ numberOfLegs + 2 + (counterLegWithDSM * 4) ] );
            trajectoryVariableVectorLeg.push_back( trajectoryVariableVector[ numberOfLegs + 3 + (counterLegWithDSM * 4) ] );
            trajectoryVariableVectorLeg.push_back( trajectoryVariableVector[ numberOfLegs + 4 + (counterLegWithDSM * 4) ] );

            std::map< double, Eigen::Vector6d > patchedConicsResultFromDepartureToDsm;
            std::map< double, Eigen::Vector6d > fullProblemResultFromDepartureToDsm;
            std::map< double, Eigen::Vector6d > patchedConicsResultFromDsmToArrival;
            std::map< double, Eigen::Vector6d > fullProblemResultFromDsmToArrival;

            // Compute patched conics and full problem results along the leg.
            propagators::propagateMga1DsmPositionAndFullProblem(
                        bodyMap, departureAndArrivalBodies,
                        bodiesAndManoeuvresOrder[ counterLegs+ 1 ], patchedConicCentralBody, positionVector[ counterLegs ],
                    positionVector[ counterLegs+ 1 ], positionVector[ counterLegs + 2 ], timeVector[ counterLegs ], timeVector[ counterLegs+ 1 ],
                    timeVector[ counterLegs + 2 ], legTypeVector[ i ], trajectoryVariableVectorLeg, minimumPericenterRadiiVector[ i ],
                    semiMajorAxesVector[ 0 ], eccentricitiesVector[ 0 ], velocityAfterDeparture, velocityBeforeArrival,
                    propagatorSettings[ counterLegs ], propagatorSettings[ counterLegs+ 1 ], integratorSettings,
                    patchedConicsResultFromDepartureToDsm, fullProblemResultFromDepartureToDsm, patchedConicsResultFromDsmToArrival,
                    fullProblemResultFromDsmToArrival);

            // Results for the first part of the leg (from departure body to DSM)
            patchedConicsResultForEachLeg[ counterLegs ] = patchedConicsResultFromDepartureToDsm;
            fullProblemResultForEachLeg[ counterLegs ] = fullProblemResultFromDepartureToDsm;
            counterLegs++;

            // Results for the second part of the leg (from DSM to arrival body )
            patchedConicsResultForEachLeg[ counterLegs ] = patchedConicsResultFromDsmToArrival;
            fullProblemResultForEachLeg[ counterLegs ] = fullProblemResultFromDsmToArrival;
            counterLegs++;
            counterLegWithDSM++;
        }
    }
}

std::pair< std::shared_ptr< propagators::PropagationTerminationSettings >,
            std::shared_ptr< propagators::PropagationTerminationSettings > > getSingleLegSphereOfInfluenceTerminationSettings(
        simulation_setup::NamedBodyMap& bodyMap,
        const std::string& bodyToPropagate,
        const std::string& centralBody,
        const std::string& departureBody,
        const std::string& arrivalBody,
        const double initialTimeCurrentLeg,
        const double finalTimeCurrentLeg )
{
    // Retrieve positions of departure and arrival bodies.
    Eigen::Vector3d cartesianPositionAtDeparture, cartesianPositionAtArrival;

    // Cartesian state at departure
    if(  bodyMap.at( departureBody )->getEphemeris( ) == nullptr )
    {
        throw std::runtime_error( "Ephemeris not defined for departure body." );
    }
    else
    {
        Eigen::Vector6d cartesianStateDepartureBody =
                bodyMap.at( departureBody )->getEphemeris( )->getCartesianState( initialTimeCurrentLeg);
        cartesianPositionAtDeparture = cartesianStateDepartureBody.segment( 0, 3 );
    }

    // Cartesian state at arrival
    if(  bodyMap.at( arrivalBody )->getEphemeris( ) == nullptr)
    {
        throw std::runtime_error( "Ephemeris not defined for arrival body." );
    }
    else
    {
        Eigen::Vector6d cartesianStateArrivalBody =
                bodyMap.at( arrivalBody )->getEphemeris( )->getCartesianState( finalTimeCurrentLeg );
        cartesianPositionAtArrival =  cartesianStateArrivalBody.segment( 0, 3 );
    }


    // Retrieve the gravitational parameter of the different bodies.
    double gravitationalParameterCentralBody =
            bodyMap.at( centralBody )->getGravityFieldModel( )->getGravitationalParameter( );
    double gravitationalParameterDepartureBody =
            bodyMap.at( departureBody )->getGravityFieldModel( )->getGravitationalParameter( );
    double gravitationalParameterArrivalBody =
            bodyMap.at( arrivalBody )->getGravityFieldModel( )->getGravitationalParameter( );

    double radiusSphereOfInfluenceDeparture;
    double radiusSphereOfInfluenceArrival;
    {
        double distanceDepartureToCentralBodies =
                ( bodyMap.at( centralBody )->getEphemeris( )->getCartesianState(
                      initialTimeCurrentLeg ).segment( 0, 3 ) - cartesianPositionAtDeparture.segment( 0, 3 ) ).norm( );
        double distanceArrivalToCentralBodies =
                ( bodyMap.at( centralBody )->getEphemeris( )->getCartesianState(
                      finalTimeCurrentLeg ).segment( 0, 3 ) - cartesianPositionAtArrival.segment( 0, 3 ) ).norm( );


        // Calculate radius sphere of influence for departure body.
        radiusSphereOfInfluenceDeparture = tudat::mission_geometry::computeSphereOfInfluence(
                    distanceDepartureToCentralBodies, gravitationalParameterDepartureBody, gravitationalParameterCentralBody );

        // Calculate radius sphere of influence for arrival body.
        radiusSphereOfInfluenceArrival = tudat::mission_geometry::computeSphereOfInfluence(
                    distanceArrivalToCentralBodies, gravitationalParameterArrivalBody, gravitationalParameterCentralBody );
    }

    double synodicPeriod;
    {
        // Calculate the synodic period.
        double orbitalPeriodDepartureBody = basic_astrodynamics::computeKeplerOrbitalPeriod(
                    orbital_element_conversions::convertCartesianToKeplerianElements(
                        bodyMap.at( departureBody )->
                        getEphemeris( )->getCartesianState( initialTimeCurrentLeg ), gravitationalParameterCentralBody )
                    [ orbital_element_conversions::semiMajorAxisIndex ],
                gravitationalParameterCentralBody, gravitationalParameterDepartureBody );

        double orbitalPeriodArrivalBody = basic_astrodynamics::computeKeplerOrbitalPeriod(
                    orbital_element_conversions::convertCartesianToKeplerianElements(
                        bodyMap.at( arrivalBody )->
                    getEphemeris( )->getCartesianState( initialTimeCurrentLeg ), gravitationalParameterCentralBody )
                [ orbital_element_conversions::semiMajorAxisIndex ],
                gravitationalParameterCentralBody, gravitationalParameterArrivalBody );

        if( orbitalPeriodDepartureBody < orbitalPeriodArrivalBody )
        {
            synodicPeriod = basic_astrodynamics::computeSynodicPeriod( orbitalPeriodDepartureBody, orbitalPeriodArrivalBody );
        }
        else
        {
            synodicPeriod = basic_astrodynamics::computeSynodicPeriod( orbitalPeriodArrivalBody, orbitalPeriodDepartureBody );
        }
    }


    // Create total propagator termination settings.
    std::vector< std::shared_ptr< PropagationTerminationSettings > >  forwardPropagationTerminationSettingsList;
    forwardPropagationTerminationSettingsList.push_back(
                std::make_shared< PropagationDependentVariableTerminationSettings >(
                    std::make_shared< SingleDependentVariableSaveSettings >(
                        relative_distance_dependent_variable, bodyToPropagate, arrivalBody ),
                radiusSphereOfInfluenceArrival, false ) );
    forwardPropagationTerminationSettingsList.push_back(
                std::make_shared< PropagationTimeTerminationSettings >( 2.0 * synodicPeriod ) );

    std::shared_ptr< PropagationTerminationSettings > forwardPropagationTerminationSettings =
            std::make_shared< PropagationHybridTerminationSettings >( forwardPropagationTerminationSettingsList, true );


    std::vector< std::shared_ptr< PropagationTerminationSettings > >  backwardPropagationTerminationSettingsList;
    backwardPropagationTerminationSettingsList.push_back(
                std::make_shared< PropagationDependentVariableTerminationSettings >(
                    std::make_shared< SingleDependentVariableSaveSettings >(
                        relative_distance_dependent_variable, bodyToPropagate, departureBody ),
                    radiusSphereOfInfluenceDeparture, false ) );
    backwardPropagationTerminationSettingsList.push_back(
                std::make_shared< PropagationTimeTerminationSettings >( 2.0 * synodicPeriod ) );

    std::shared_ptr< PropagationTerminationSettings > backwardPropagationTerminationSettings =
            std::make_shared< PropagationHybridTerminationSettings >( backwardPropagationTerminationSettingsList, true );

    return std::make_pair( backwardPropagationTerminationSettings, forwardPropagationTerminationSettings );
}

//! Function to calculate the patched conics trajectory and to propagate the corresponding full problem.
std::vector< std::pair< std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > >,
std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > > > getPatchedConicPropagatorSettings(
        simulation_setup::NamedBodyMap& bodyMap,
        const std::vector< basic_astrodynamics::AccelerationMap >& accelerationMap,
        const std::vector< std::string >& transferBodyOrder,
        const std::string& centralBody,
        const std::string& bodyToPropagate,
        const std::vector< transfer_trajectories::TransferLegType>& legTypeVector,
        const std::vector< double >& trajectoryVariableVector,
        const std::vector< double >& minimumPericenterRadiiVector,
        const std::vector< double >& semiMajorAxesVector,
        const std::vector< double >& eccentricitiesVector,
        const bool terminationSphereOfInfluence,
        const std::vector< std::shared_ptr< DependentVariableSaveSettings > > dependentVariablesToSave,
        const TranslationalPropagatorType propagator)
{

    // Define the patched conic trajectory from the body map.
    transfer_trajectories::Trajectory trajectory = propagators::createTransferTrajectoryObject(
                bodyMap, transferBodyOrder, centralBody, legTypeVector, trajectoryVariableVector, minimumPericenterRadiiVector, true,
                semiMajorAxesVector[ 0 ], eccentricitiesVector[ 0 ], true, semiMajorAxesVector[ 1 ], eccentricitiesVector[ 1 ] );

    // Calculate the trajectory.
    std::vector< double > timeVector;
    {
        std::vector< Eigen::Vector3d > positionVector;
        std::vector< double > deltaVVector;
        double totalDeltaV;
        trajectory.calculateTrajectory( totalDeltaV );
        trajectory.maneuvers( positionVector, timeVector, deltaVVector );
    }
    int numberOfLegs = legTypeVector.size( );
    int numberOfLegsIncludingDsm = ( (trajectoryVariableVector.size( ) - 1 - numberOfLegs) / 4.0 ) + numberOfLegs ;


    std::vector< std::pair< std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > >,
            std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > > > propagatorSettings;

    std::vector< std::string > centralBodyPropagation;
    centralBodyPropagation.push_back( centralBody );
    std::vector< std::string > bodyToPropagatePropagation;
    bodyToPropagatePropagation.push_back( bodyToPropagate );

    double initialTimeCurrentLeg;
    double finalTimeCurrentLeg;

    std::vector< std::pair< std::shared_ptr< propagators::PropagationTerminationSettings >,
            std::shared_ptr< propagators::PropagationTerminationSettings > > > terminationSettings;

    std::cout<<"Total: "<<numberOfLegsIncludingDsm<<" "<<numberOfLegs<<std::endl;
    if( numberOfLegsIncludingDsm != numberOfLegs )
    {
        for( int i = 0 ; i < numberOfLegsIncludingDsm - 1 ; i ++ )
        {
            initialTimeCurrentLeg = timeVector[ i ];
            finalTimeCurrentLeg = timeVector[ i + 1 ];

            std::cout<<i<<" "<<initialTimeCurrentLeg<<" "<<finalTimeCurrentLeg<<std::endl;

            terminationSettings.push_back(
                        std::make_pair(
                            std::make_shared< propagators::PropagationTimeTerminationSettings >( initialTimeCurrentLeg ),
                            std::make_shared< propagators::PropagationTimeTerminationSettings >( finalTimeCurrentLeg ) ) );
        }

        if( terminationSphereOfInfluence == true )
        {
            std::cerr << "Warning, the option to terminate on the sphere of influence is not yet available for trajectories including DSMs. "
                         "The backward and forward propagations stop at departure and arrival bodies respectively." << std::endl;
        }

    }
    else
    {
        for( int i = 0 ; i < numberOfLegs - 1 ; i ++ )
        {
            initialTimeCurrentLeg = timeVector[ i ];
            initialTimeCurrentLeg = timeVector[ i + 1 ];

            if( terminationSphereOfInfluence == true )
            {
                terminationSettings.push_back( getSingleLegSphereOfInfluenceTerminationSettings(
                                                   bodyMap, bodyToPropagate, centralBody, transferBodyOrder.at( i ),
                                                   transferBodyOrder.at( i + 1 ), initialTimeCurrentLeg, finalTimeCurrentLeg ) );
            }
            else
            {
                terminationSettings.push_back(
                            std::make_pair(
                                std::make_shared< propagators::PropagationTimeTerminationSettings >( initialTimeCurrentLeg ),
                                std::make_shared< propagators::PropagationTimeTerminationSettings >( finalTimeCurrentLeg ) ) );
            }
        }
    }


    // Create propagator settings.
    int counterLegsIncludingDsm = 0;
    Eigen::Vector6d initialState;

    for( int i = 0 ; i <  numberOfLegs - 1 ; i ++ )
    {

        std::shared_ptr< DependentVariableSaveSettings > currentDependentVariablesToSave =
                ( dependentVariablesToSave.size( ) != 0 ) ? dependentVariablesToSave.at( i ) : nullptr;

        propagatorSettings.push_back(
                    std::make_pair(
                        std::make_shared< TranslationalStatePropagatorSettings< double > >(
                            centralBodyPropagation, accelerationMap[ i ], bodyToPropagatePropagation, initialState,
                            terminationSettings[ counterLegsIncludingDsm].first, propagator, currentDependentVariablesToSave ),
                        std::make_shared< TranslationalStatePropagatorSettings< double > >(
                            centralBodyPropagation, accelerationMap[ i ], bodyToPropagatePropagation, initialState,
                            terminationSettings[ counterLegsIncludingDsm].second, propagator, currentDependentVariablesToSave ) ) );

        counterLegsIncludingDsm++;

        // If the leg includes one DSM, add another element to the propagator settings vector to take the second part of the leg into account.
        if( !( legTypeVector[ i ] == transfer_trajectories::mga_Departure ||
               legTypeVector[ i ] == transfer_trajectories::mga_Swingby ) )
        {
            std::shared_ptr< DependentVariableSaveSettings > currentDependentVariablesToSave =
                    ( dependentVariablesToSave.size( ) != 0 ) ? dependentVariablesToSave.at( i ) : nullptr;

            propagatorSettings.push_back(
                        std::make_pair(
                            std::make_shared< TranslationalStatePropagatorSettings< double > >(
                                centralBodyPropagation, accelerationMap[ i ], bodyToPropagatePropagation, initialState,
                                terminationSettings[ counterLegsIncludingDsm ].first, propagator, currentDependentVariablesToSave ),
                            std::make_shared< TranslationalStatePropagatorSettings< double > >(
                                centralBodyPropagation, accelerationMap[ i ], bodyToPropagatePropagation, initialState,
                                terminationSettings[ counterLegsIncludingDsm ].second, propagator, currentDependentVariablesToSave ) ) );
            counterLegsIncludingDsm++;
        }
    }

    return propagatorSettings;
}


//! Function to calculate the patched conics trajectory and to propagate the corresponding full problem.
void fullPropagationPatchedConicsTrajectory(
        simulation_setup::NamedBodyMap& bodyMap,
        const std::vector< basic_astrodynamics::AccelerationMap >& accelerationMap,
        const std::vector< std::string >& transferBodyOrder,
        const std::string& centralBody,
        const std::string& bodyToPropagate,
        const std::vector< transfer_trajectories::TransferLegType>& legTypeVector,
        const std::vector< double >& trajectoryVariableVector,
        const std::vector< double >& minimumPericenterRadiiVector,
        const std::vector< double >& semiMajorAxesVector,
        const std::vector< double >& eccentricitiesVector,
        const std::shared_ptr< numerical_integrators::IntegratorSettings< double > >& integratorSettings,
        std::map< int, std::map< double, Eigen::Vector6d > >& patchedConicsResultForEachLeg,
        std::map< int, std::map< double, Eigen::Vector6d > >& fullProblemResultForEachLeg,
        const bool terminationSphereOfInfluence,
        const std::vector< std::shared_ptr< DependentVariableSaveSettings > > dependentVariablesToSave,
        const TranslationalPropagatorType propagator)
{
    std::vector< std::pair< std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > >,
            std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > > > propagatorSettings =
            getPatchedConicPropagatorSettings(
                bodyMap, accelerationMap, transferBodyOrder, centralBody, bodyToPropagate, legTypeVector,
                trajectoryVariableVector, minimumPericenterRadiiVector, semiMajorAxesVector,
                eccentricitiesVector, terminationSphereOfInfluence, dependentVariablesToSave, propagator );

    // Calculate the patched conics trajectory and propagate the full dynamics problem.
    fullPropagationPatchedConicsTrajectory( bodyMap, transferBodyOrder, centralBody, legTypeVector,
                                            trajectoryVariableVector, minimumPericenterRadiiVector, semiMajorAxesVector,
                                            eccentricitiesVector, propagatorSettings, integratorSettings,
                                            patchedConicsResultForEachLeg, fullProblemResultForEachLeg );


}


//! Function to calculate the patched conics trajectory and to propagate the corresponding full problem
//! with the same acceleration map for every leg.
void fullPropagationPatchedConicsTrajectory(
        simulation_setup::NamedBodyMap& bodyMap,
        const basic_astrodynamics::AccelerationMap& accelerationMap,
        const std::vector< std::string >& transferBodyOrder,
        const std::string& centralBody,
        const std::string& bodyToPropagate,
        const std::vector< transfer_trajectories::TransferLegType>& legTypeVector,
        const std::vector< double >& trajectoryVariableVector,
        const std::vector< double >& minimumPericenterRadiiVector,
        const std::vector< double >& semiMajorAxesVector,
        const std::vector< double >& eccentricitiesVector,
        const std::shared_ptr< numerical_integrators::IntegratorSettings< double > >& integratorSettings,
        std::map< int, std::map< double, Eigen::Vector6d > >& patchedConicsResultForEachLeg,
        std::map< int, std::map< double, Eigen::Vector6d > >& fullProblemResultForEachLeg,
        const bool terminationSphereOfInfluence,
        const std::shared_ptr< DependentVariableSaveSettings > dependentVariablesToSave,
        const TranslationalPropagatorType propagator)
{

    int numberOfLegs = legTypeVector.size(  );

    // Create vectors with identical acceleration maps and dependent variables to save for each leg.
    std::vector< basic_astrodynamics::AccelerationMap > accelerationMapForEachLeg;
    std::vector< std::shared_ptr< DependentVariableSaveSettings > > dependentVariablesToSaveForEachLeg;

    for( int i = 0 ; i < numberOfLegs; i ++ )
    {
        accelerationMapForEachLeg.push_back( accelerationMap );
        dependentVariablesToSaveForEachLeg.push_back( dependentVariablesToSave );
    }

    // Compute difference between patched conics trajectory and full problem.
    fullPropagationPatchedConicsTrajectory(
                bodyMap, accelerationMapForEachLeg, transferBodyOrder, centralBody, bodyToPropagate, legTypeVector,
                trajectoryVariableVector, minimumPericenterRadiiVector, semiMajorAxesVector, eccentricitiesVector,
                integratorSettings, patchedConicsResultForEachLeg, fullProblemResultForEachLeg, terminationSphereOfInfluence,
                dependentVariablesToSaveForEachLeg, propagator);

}

//! Function to compute the difference in cartesian state between patched conics trajectory and full dynamics problem,
//! at both departure and arrival positions for each leg.
std::map< int, std::pair< Eigen::Vector6d, Eigen::Vector6d > > getDifferenceFullProblemWrtPatchedConicsTrajectory(
        simulation_setup::NamedBodyMap& bodyMap,
        const std::vector< basic_astrodynamics::AccelerationMap >& accelerationMap,
        const std::vector< std::string >& transferBodyOrder,
        const std::string& centralBody,
        const std::string& bodyToPropagate,
        const std::vector< transfer_trajectories::TransferLegType >& legTypeVector,
        const std::vector< double >& trajectoryVariableVector,
        const std::vector< double >& minimumPericenterRadiiVector,
        const std::vector< double >& semiMajorAxesVector,
        const std::vector< double >& eccentricitiesVector,
        const std::shared_ptr< numerical_integrators::IntegratorSettings< double > >& integratorSettings,
        const bool terminationSphereOfInfluence,
        const std::vector< std::shared_ptr< DependentVariableSaveSettings > > dependentVariablesToSave,
        const TranslationalPropagatorType propagator)
{
    int numberOfLegs = legTypeVector.size( );
    int numberLegsIncludingDSM = ( (trajectoryVariableVector.size( ) - 1 - numberOfLegs) / 4.0 ) + numberOfLegs ;


    // Compute difference between patched conics and full problem along the trajectory.
    std::map< int, std::map< double, Eigen::Vector6d > > patchedConicsResultForEachLeg;
    std::map< int, std::map< double, Eigen::Vector6d > > fullProblemResultForEachLeg;

    fullPropagationPatchedConicsTrajectory(
                bodyMap, accelerationMap, transferBodyOrder, centralBody, bodyToPropagate, legTypeVector,
                trajectoryVariableVector, minimumPericenterRadiiVector, semiMajorAxesVector, eccentricitiesVector,
                integratorSettings, patchedConicsResultForEachLeg, fullProblemResultForEachLeg, terminationSphereOfInfluence,
                dependentVariablesToSave, propagator);

    // Compute difference at departure and at arrival for each leg (considering that a leg including a DSM consists of two sub-legs).
    std::map< int, std::pair< Eigen::Vector6d, Eigen::Vector6d > > stateDifferenceAtArrivalAndDepartureForEachLeg;

    for( int i = 0 ; i < numberLegsIncludingDSM - 1 ; i ++ )
    {
        std::map< double, Eigen::Vector6d > patchedConicsResultCurrentLeg = patchedConicsResultForEachLeg[ i ];
        std::map< double, Eigen::Vector6d > fullProblemResultCurrentLeg = fullProblemResultForEachLeg[ i ];

        Eigen::Vector6d statePatchedConicsAtDeparture = patchedConicsResultCurrentLeg.begin( )->second;
        Eigen::Vector6d stateFullProblemAtDeparture = fullProblemResultCurrentLeg.begin( )->second;
        Eigen::Vector6d statePatchedConicsAtArrival = patchedConicsResultCurrentLeg.rbegin( )->second;
        Eigen::Vector6d stateFullProblemAtArrival = fullProblemResultCurrentLeg.rbegin( )->second;

        stateDifferenceAtArrivalAndDepartureForEachLeg[ i ] = std::make_pair(
                    statePatchedConicsAtDeparture - stateFullProblemAtDeparture,
                    statePatchedConicsAtArrival - stateFullProblemAtArrival);
    }

    return stateDifferenceAtArrivalAndDepartureForEachLeg;
}



//! Function to compute the difference in cartesian state between patched conics trajectory and full dynamics problem,
//! at both departure and arrival positions for each leg.
std::map< int, std::pair< Eigen::Vector6d, Eigen::Vector6d > > getDifferenceFullProblemWrtPatchedConicsTrajectory(
        simulation_setup::NamedBodyMap& bodyMap,
        const std::vector< std::string >& transferBodyOrder,
        const std::string& centralBody,
        const std::vector< transfer_trajectories::TransferLegType >& legTypeVector,
        const std::vector< double >& trajectoryVariableVector,
        const std::vector< double >& minimumPericenterRadiiVector,
        const std::vector< double >& semiMajorAxesVector,
        const std::vector< double >& eccentricitiesVector,
        const std::vector< std::pair< std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > >,
        std::shared_ptr< propagators::TranslationalStatePropagatorSettings< double > > > > propagatorSettings,
        const std::shared_ptr< numerical_integrators::IntegratorSettings< double > >& integratorSettings)
{
    int numberOfLegs = legTypeVector.size( );
    int numberLegsIncludingDSM = ( ( trajectoryVariableVector.size( ) - 1 - numberOfLegs) / 4.0 ) + numberOfLegs;

    // Compute difference between patched conics and full problem along the trajectory.
    std::map< int, std::map< double, Eigen::Vector6d > > patchedConicsResultForEachLeg;
    std::map< int, std::map< double, Eigen::Vector6d > > fullProblemResultForEachLeg;

    fullPropagationPatchedConicsTrajectory(
                bodyMap, transferBodyOrder, centralBody, legTypeVector,
                trajectoryVariableVector, minimumPericenterRadiiVector, semiMajorAxesVector, eccentricitiesVector,
                propagatorSettings, integratorSettings, patchedConicsResultForEachLeg, fullProblemResultForEachLeg );

    // Compute difference at departure and at arrival for each leg (considering that a leg including a DSM consists of two sub-legs).
    std::map< int, std::pair< Eigen::Vector6d, Eigen::Vector6d > > stateDifferenceAtArrivalAndDepartureForEachLeg;

    for( int i = 0 ; i < numberLegsIncludingDSM - 1 ; i ++ )
    {
        std::map< double, Eigen::Vector6d > patchedConicsResultCurrentLeg = patchedConicsResultForEachLeg[ i ];
        std::map< double, Eigen::Vector6d > fullProblemResultCurrentLeg = fullProblemResultForEachLeg[ i ];

        Eigen::Vector6d statePatchedConicsAtDeparture = patchedConicsResultCurrentLeg.begin( )->second;
        Eigen::Vector6d stateFullProblemAtDeparture = fullProblemResultCurrentLeg.begin( )->second;
        Eigen::Vector6d statePatchedConicsAtArrival = patchedConicsResultCurrentLeg.rbegin( )->second;
        Eigen::Vector6d stateFullProblemAtArrival = fullProblemResultCurrentLeg.rbegin( )->second;

        stateDifferenceAtArrivalAndDepartureForEachLeg[ i ] = std::make_pair(
                    statePatchedConicsAtDeparture - stateFullProblemAtDeparture,
                    statePatchedConicsAtArrival - stateFullProblemAtArrival);
    }

    return stateDifferenceAtArrivalAndDepartureForEachLeg;
}




//! Function to compute the difference in cartesian state between patched conics trajectory and full dynamics problem,
//! at both departure and arrival positions for each leg, using the same accelerations for each leg.
std::map< int, std::pair< Eigen::Vector6d, Eigen::Vector6d > > getDifferenceFullProblemWrtPatchedConicsTrajectory(
        simulation_setup::NamedBodyMap& bodyMap,
        const basic_astrodynamics::AccelerationMap& accelerationMap,
        const std::vector< std::string >& transferBodyOrder,
        const std::string& centralBody,
        const std::string& bodyToPropagate,
        const std::vector< transfer_trajectories::TransferLegType >& legTypeVector,
        const std::vector< double >& trajectoryVariableVector,
        const std::vector< double >& minimumPericenterRadiiVector,
        const std::vector< double >& semiMajorAxesVector,
        const std::vector< double >& eccentricitiesVector,
        const std::shared_ptr< numerical_integrators::IntegratorSettings< double > >& integratorSettings,
        const bool terminationSphereOfInfluence,
        const std::shared_ptr< DependentVariableSaveSettings > dependentVariablesToSave,
        const TranslationalPropagatorType propagator)
{

    int numberOfLegs = legTypeVector.size( );

    // Create vector with identical acceleration maps.
    std::vector< basic_astrodynamics::AccelerationMap > accelerationMapForEachLeg;
    std::vector< std::shared_ptr< DependentVariableSaveSettings > > dependentVariablesToSaveForEachLeg;

    for( int i = 0 ; i < numberOfLegs; i ++ )
    {
        accelerationMapForEachLeg.push_back( accelerationMap );
        dependentVariablesToSaveForEachLeg.push_back( dependentVariablesToSave );
    }


    // Compute difference at departure and at arrival for each leg (considering that a leg including a DSM consists of two sub-legs).
    std::map< int, std::pair< Eigen::Vector6d, Eigen::Vector6d > > stateDifferenceAtArrivalAndDepartureForEachLeg;

    stateDifferenceAtArrivalAndDepartureForEachLeg = getDifferenceFullProblemWrtPatchedConicsTrajectory(
                bodyMap, accelerationMapForEachLeg,
                transferBodyOrder, centralBody, bodyToPropagate,
                legTypeVector, trajectoryVariableVector,
                minimumPericenterRadiiVector, semiMajorAxesVector,
                eccentricitiesVector, integratorSettings, terminationSphereOfInfluence, dependentVariablesToSaveForEachLeg, propagator );

    return stateDifferenceAtArrivalAndDepartureForEachLeg;


}



}

}

