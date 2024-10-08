/*    Copyright (c) 2010-2019, Delft University of Technology
 *    All rigths reserved
 *
 *    This file is part of the Tudat. Redistribution and use in source and
 *    binary forms, with or without modification, are permitted exclusively
 *    under the terms of the Modified BSD license. You should have received
 *    a copy of the license with this file. If not, please or visit:
 *    http://tudat.tudelft.nl/LICENSE.
 *
 *    References
 *      Vallado, D. A., Crawford, P., Hujsak, R., & Kelso, T. Revisiting Spacetrack Report #3:
 *          Rev 1, Proceedings of the AIAA/AAS astro Specialist Conference. Keystone, CO,
 *          2006.
 *
 */

#include <iomanip>

#include "tudat/astro/basic_astro/physicalConstants.h"
#include "tudat/astro/gravitation/sphericalHarmonicsGravityField.h"
#include "tudat/math/basic/coordinateConversions.h"
#include "tudat/math/basic/basicMathematicsFunctions.h"
#include "tudat/math/basic/legendrePolynomials.h"

namespace tudat
{

namespace gravitation
{


Eigen::Matrix3d SphericalHarmonicsGravityField::getInertiaTensor(  )
{
    return gravitation::getInertiaTensorFromGravityField( shared_from_this(), scaledMeanMomentOfInertia_ );
}

//! Compute gravitational acceleration due to multiple spherical harmonics terms, defined using geodesy-normalization.
Eigen::Vector3d computeGeodesyNormalizedGravitationalAccelerationSum(
        const Eigen::Vector3d& positionOfBodySubjectToAcceleration,
        const double gravitationalParameter,
        const double equatorialRadius,
        const Eigen::MatrixXd& cosineHarmonicCoefficients,
        const Eigen::MatrixXd& sineHarmonicCoefficients,
        std::shared_ptr< basic_mathematics::SphericalHarmonicsCache > sphericalHarmonicsCache,
        std::map< std::pair< int, int >, Eigen::Vector3d >& accelerationPerTerm,
        const bool saveSeparateTerms,
        const Eigen::Matrix3d& accelerationRotation )
{
    // Set highest degree and order.
    const int highestDegree = cosineHarmonicCoefficients.rows( );
    const int highestOrder = cosineHarmonicCoefficients.cols( );

    // Declare spherical position vector.
    Eigen::Vector3d sphericalpositionOfBodySubjectToAcceleration = coordinate_conversions::
            convertCartesianToSpherical( positionOfBodySubjectToAcceleration );
    sphericalpositionOfBodySubjectToAcceleration( 1 ) = mathematical_constants::PI / 2.0 -
            sphericalpositionOfBodySubjectToAcceleration( 1 );

    double sineOfAngle = std::sin( sphericalpositionOfBodySubjectToAcceleration( 1 ) );
    sphericalHarmonicsCache->update( sphericalpositionOfBodySubjectToAcceleration( 0 ),
                                     sineOfAngle,
                                     sphericalpositionOfBodySubjectToAcceleration( 2 ),
                                     equatorialRadius );

    std::shared_ptr< basic_mathematics::LegendreCache > legendreCacheReference =
            sphericalHarmonicsCache->getLegendreCache( );

    // Compute gradient premultiplier.
    const double preMultiplier = gravitationalParameter / equatorialRadius;

    // Initialize gradient vector.
    Eigen::Vector3d sphericalGradient = Eigen::Vector3d::Zero( );

    Eigen::Matrix3d transformationToCartesianCoordinates = coordinate_conversions::getSphericalToCartesianGradientMatrix(
                positionOfBodySubjectToAcceleration );

    // Loop through all degrees.
    for ( int degree = 0; degree < highestDegree; degree++ )
    {
        // Loop through all orders.
        for ( int order = 0; ( order <= degree ) && ( order < highestOrder ); order++ )
        {
            // Compute geodesy-normalized Legendre polynomials.
            const double legendrePolynomial = legendreCacheReference->getLegendrePolynomial( degree, order );

            // Compute geodesy-normalized Legendre polynomial derivative.
            const double legendrePolynomialDerivative = legendreCacheReference->getLegendrePolynomialDerivative(
                        degree, order );

            // Compute the potential gradient of a single spherical harmonic term.
            if( saveSeparateTerms )
            {
                accelerationPerTerm[ std::make_pair( degree, order ) ] =
                        basic_mathematics::computePotentialGradient(
                            sphericalpositionOfBodySubjectToAcceleration,
                            preMultiplier,
                            degree,
                            order,
                            cosineHarmonicCoefficients( degree, order ),
                            sineHarmonicCoefficients( degree, order ),
                            legendrePolynomial,
                            legendrePolynomialDerivative, sphericalHarmonicsCache );
                sphericalGradient += accelerationPerTerm[ std::make_pair( degree, order ) ];
                accelerationPerTerm[ std::make_pair( degree, order ) ] =
                        accelerationRotation * (
                            transformationToCartesianCoordinates * accelerationPerTerm[ std::make_pair( degree, order ) ] );
            }
            else
            {
                // Compute the potential gradient of a single spherical harmonic term.
                sphericalGradient += basic_mathematics::computePotentialGradient(
                            sphericalpositionOfBodySubjectToAcceleration,
                            preMultiplier,
                            degree,
                            order,
                            cosineHarmonicCoefficients( degree, order ),
                            sineHarmonicCoefficients( degree, order ),
                            legendrePolynomial,
                            legendrePolynomialDerivative, sphericalHarmonicsCache );
            }
        }
    }


    // Convert from spherical gradient to Cartesian gradient (which equals acceleration vector) and
    // return the resulting acceleration vector.
    return accelerationRotation * ( transformationToCartesianCoordinates * sphericalGradient );
}

//! Compute gravitational acceleration due to single spherical harmonics term.
Eigen::Vector3d computeSingleGeodesyNormalizedGravitationalAcceleration(
        const Eigen::Vector3d& positionOfBodySubjectToAcceleration,
        const double gravitationalParameter,
        const double equatorialRadius,
        const int degree,
        const int order,
        const double cosineHarmonicCoefficient,
        const double sineHarmonicCoefficient,
        std::shared_ptr< basic_mathematics::SphericalHarmonicsCache > sphericalHarmonicsCache )
{
    // Declare spherical position vector.
    Eigen::Vector3d sphericalpositionOfBodySubjectToAcceleration = coordinate_conversions::
            convertCartesianToSpherical( positionOfBodySubjectToAcceleration );
    sphericalpositionOfBodySubjectToAcceleration( 1 ) = mathematical_constants::PI / 2.0 -
            sphericalpositionOfBodySubjectToAcceleration( 1 );


    double sineOfAngle = std::sin( sphericalpositionOfBodySubjectToAcceleration( 1 ) );
    sphericalHarmonicsCache->update( sphericalpositionOfBodySubjectToAcceleration( 0 ),
                                     sineOfAngle,
                                     sphericalpositionOfBodySubjectToAcceleration( 2 ),
                                     equatorialRadius );

    // Compute gradient premultiplier.
    const double preMultiplier = gravitationalParameter / equatorialRadius;

    // Compute geodesy-normalized Legendre polynomials.
    const double legendrePolynomial = sphericalHarmonicsCache->getLegendreCache( )->getLegendrePolynomial( degree, order );

    // Compute geodesy-normalized Legendre polynomial derivative.
    const double legendrePolynomialDerivative =
            sphericalHarmonicsCache->getLegendreCache( )->getLegendrePolynomialDerivative( degree, order );

    // Compute the potential gradient of a single spherical harmonic term.
    Eigen::Vector3d sphericalGradient = basic_mathematics::computePotentialGradient(
                sphericalpositionOfBodySubjectToAcceleration,
                preMultiplier,
                degree,
                order,
                cosineHarmonicCoefficient,
                sineHarmonicCoefficient,
                legendrePolynomial,
                legendrePolynomialDerivative, sphericalHarmonicsCache );

    // Convert from spherical gradient to Cartesian gradient (which equals acceleration vector),
    // and return resulting acceleration vector.
    return coordinate_conversions::convertSphericalToCartesianGradient(
                sphericalGradient, positionOfBodySubjectToAcceleration );
}

//! Function to calculate the gravitational potential from a spherical harmonic field expansion.
double calculateSphericalHarmonicGravitationalPotential(
        const Eigen::Vector3d& bodyFixedPosition, const double gravitationalParameter,
        const double referenceRadius,
        const Eigen::MatrixXd& cosineCoefficients,
        const Eigen::MatrixXd& sineCoefficients,
        std::shared_ptr< basic_mathematics::SphericalHarmonicsCache > sphericalHarmonicsCache,
        const int minimumumDegree,
        const int minimumumOrder )
{
    // Initialize (distance/reference radius)^n (n=ratioToPowerDegree)
    double ratioToPowerDegree = 1.0;
    double radiusRatio = referenceRadius / bodyFixedPosition.norm( );

    // Declare local variables used in calculation
    double legendrePolynomial = 0.0;
    double singleDegreeTerm = 0.0;

    // Determine body fixed spherical position of body udnergoing acceleration.
    Eigen::Vector3d sphericalPositon =
            coordinate_conversions::convertCartesianToSpherical( bodyFixedPosition );
    double latitude = mathematical_constants::PI / 2.0 - sphericalPositon.y( );
    double longitude = sphericalPositon.z( );

    double potential = 0.0;
    int startDegree = 0;

    // Initialize value of potential to 1 (C_{0,0})
    if( minimumumDegree == 0 )
    {
        potential = 1.0;
        startDegree = 1;
    }
    else
    {
        startDegree = minimumumDegree;
        ratioToPowerDegree *= basic_mathematics::raiseToIntegerPower< double >( radiusRatio, startDegree - 1 );
    }

    basic_mathematics::LegendreCache& legendreCacheReference = *sphericalHarmonicsCache->getLegendreCache( );
    legendreCacheReference.update( std::sin( latitude ) );

    // Iterate over all degrees
    for( int degree = startDegree; degree < cosineCoefficients.rows( ); degree++ )
    {
        singleDegreeTerm = 0.0;

        // Iterate over all orders in current degree for which coefficients are provided.
        for( int order = minimumumOrder; ( order < cosineCoefficients.cols( ) &&
                                           order <= degree ); order++ )
        {
            // Calculate legendre polynomial (geodesy-normalized) at current degree and order
            legendrePolynomial = basic_mathematics::computeGeodesyLegendrePolynomialFromCache(
                        degree, order, legendreCacheReference );

            // Calculate contribution to potential from current degree and order
            singleDegreeTerm += legendrePolynomial * ( cosineCoefficients( degree, order ) *
                                                       std::cos( order * longitude ) +
                                                       sineCoefficients( degree, order ) *
                                                       std::sin( order * longitude ) );
        }

        // Add potential contributions from current degree to toal value.
        ratioToPowerDegree *= radiusRatio;
        potential += singleDegreeTerm * ratioToPowerDegree;
    }

    // Multiply by central term and return
    return potential * gravitationalParameter / bodyFixedPosition.norm( );
}

//! Function to determine a body's inertia tensor from its degree two unnormalized gravity field coefficients
Eigen::Matrix3d getInertiaTensor(
        const double c20Coefficient,
        const double c21Coefficient,
        const double c22Coefficient,
        const double s21Coefficient,
        const double s22Coefficient,
        const double scaledMeanMomentOfInertia,
        const double bodyMass,
        const double referenceRadius )
{
    // Compute inertia tensor
    double scalingConstant = bodyMass * referenceRadius * referenceRadius;
    Eigen::Matrix3d inertiaTensor =
            ( Eigen::Matrix3d( )<< c20Coefficient / 3.0 - 2.0 * c22Coefficient, -2.0 * s22Coefficient, -c21Coefficient,
              -2.0 * s22Coefficient, c20Coefficient / 3.0 + 2.0 * c22Coefficient, -s21Coefficient,
              -c21Coefficient, -s21Coefficient, -2.0 * c20Coefficient / 3.0 ).finished( );

    return scalingConstant * ( inertiaTensor + Eigen::Matrix3d::Identity( ) * scaledMeanMomentOfInertia );
}

//! Function to determine a body's inertia tensor from its unnormalized gravity field coefficients
Eigen::Matrix3d getInertiaTensor(
        const Eigen::MatrixXd& unnormalizedCosineCoefficients,
        const Eigen::MatrixXd& unnormalizedSineCoefficients,
        const double scaledMeanMomentOfInertia,
        const double bodyMass,
        const double referenceRadius )
{
    return getInertiaTensor(
                unnormalizedCosineCoefficients( 2, 0 ), unnormalizedCosineCoefficients( 2, 1 ),
                unnormalizedCosineCoefficients( 2, 2 ),
                unnormalizedSineCoefficients( 2, 1 ), unnormalizedSineCoefficients( 2, 2 ),
                scaledMeanMomentOfInertia, bodyMass, referenceRadius );
}

//! Function to determine a body's inertia tensor from its gravity field model
Eigen::Matrix3d getInertiaTensorFromGravityField(
        const std::shared_ptr< SphericalHarmonicsGravityField > sphericalHarmonicGravityField,
        const double scaledMeanMomentOfInertia )
{
    // Denormalize coefficients if needed, and compute inertia tensor
    if( sphericalHarmonicGravityField->areCoefficientsGeodesyNormalized( ) )
    {
        Eigen::MatrixXd normalizedCosineCoefficients = Eigen::Matrix3d::Zero( );
        Eigen::MatrixXd normalizedSineCoefficient = Eigen::Matrix3d::Zero( );
        basic_mathematics::convertGeodesyNormalizedToUnnormalizedCoefficients(
                    sphericalHarmonicGravityField->getCosineCoefficients( ).block( 0, 0, 3, 3 ),
                    sphericalHarmonicGravityField->getSineCoefficients( ).block( 0, 0, 3, 3 ),
                    normalizedCosineCoefficients,
                    normalizedSineCoefficient );

        return getInertiaTensor(
                    normalizedCosineCoefficients, normalizedSineCoefficient,
                    scaledMeanMomentOfInertia,
                    sphericalHarmonicGravityField->getGravitationalParameter( ) / physical_constants::GRAVITATIONAL_CONSTANT,
                    sphericalHarmonicGravityField->getReferenceRadius( ) );
    }
    else
    {
        return getInertiaTensor(
                    sphericalHarmonicGravityField->getCosineCoefficients( ),
                    sphericalHarmonicGravityField->getSineCoefficients( ),
                    scaledMeanMomentOfInertia,
                    sphericalHarmonicGravityField->getGravitationalParameter( ) / physical_constants::GRAVITATIONAL_CONSTANT,
                    sphericalHarmonicGravityField->getReferenceRadius( ) );
    }
}

//! Retrieve degree 2 spherical harmonic coefficients from inertia tensor and assiciated parameters
void getDegreeTwoSphericalHarmonicCoefficients(
        const Eigen::Matrix3d inertiaTensor, const double bodyGravitationalParameter, const double referenceRadius,
        const bool useNormalizedCoefficients,
        Eigen::MatrixXd& cosineCoefficients, Eigen::MatrixXd& sineCoefficients, double& scaledMeanMomentOfInertia )
{
    double scalingTerm = bodyGravitationalParameter * referenceRadius * referenceRadius /
            physical_constants::GRAVITATIONAL_CONSTANT;

    cosineCoefficients.setZero( );
    cosineCoefficients( 0, 0 ) = 1.0;
    sineCoefficients.setZero( );

    cosineCoefficients( 2, 0 ) = ( 0.5 * inertiaTensor( 0, 0 ) + 0.5 * inertiaTensor( 1, 1 ) - inertiaTensor( 2, 2 ) ) / scalingTerm;
    cosineCoefficients( 2, 2 ) = ( -0.25 * inertiaTensor( 0, 0 ) + 0.25 * inertiaTensor( 1, 1 ) ) / scalingTerm;
    cosineCoefficients( 2, 1 ) = -inertiaTensor( 2, 0 ) / scalingTerm;
    sineCoefficients( 2, 1 ) = -inertiaTensor( 2, 1 ) / scalingTerm;
    sineCoefficients( 2, 2 ) = -0.5 * inertiaTensor( 1, 0 ) / scalingTerm;

    if( useNormalizedCoefficients )
    {
        basic_mathematics::geodesyNormalizeUnnormalizedCoefficients(
                    cosineCoefficients, sineCoefficients );
    }

    scaledMeanMomentOfInertia = ( inertiaTensor( 0, 0 ) + inertiaTensor( 1, 1 ) + inertiaTensor( 2, 2 ) ) /
            ( 3.0 * scalingTerm );
}

std::tuple< Eigen::MatrixXd, Eigen::MatrixXd, double > getDegreeTwoSphericalHarmonicCoefficients(
        const Eigen::Matrix3d inertiaTensor, const double bodyGravitationalParameter, const double referenceRadius,
        const int maximumCoefficientDegree, const bool useNormalizedCoefficients )
{
     Eigen::MatrixXd cosineCoefficients = Eigen::MatrixXd::Zero( maximumCoefficientDegree + 1, maximumCoefficientDegree + 1 );
     Eigen::MatrixXd sineCoefficients = Eigen::MatrixXd::Zero( maximumCoefficientDegree + 1, maximumCoefficientDegree + 1 );
     double scaledMeanMomentOfInertia;

     getDegreeTwoSphericalHarmonicCoefficients(
             inertiaTensor, bodyGravitationalParameter, referenceRadius, useNormalizedCoefficients,
             cosineCoefficients, sineCoefficients, scaledMeanMomentOfInertia );

     return std::make_tuple( cosineCoefficients, sineCoefficients, scaledMeanMomentOfInertia );
}

} // namespace gravitation

} // namespace tudat
