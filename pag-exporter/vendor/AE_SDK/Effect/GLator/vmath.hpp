/*
   Copyright (C) 2006, 2007 Sony Computer Entertainment Inc.
   All rights reserved.

   Redistribution and use in source and binary forms,
   with or without modification, are permitted provided that the
   following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Sony Computer Entertainment Inc nor the names
      of its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <math.h>

#ifdef _VECTORMATH_DEBUG
#include <stdio.h>
#endif

namespace Vectormath {

namespace Aos {

//-----------------------------------------------------------------------------
// Forward Declarations
//

class Vector3;
class Vector4;
class Point3;
class Quat;
class Matrix3;
class Matrix4;
class Transform3;

// A 3-D vector in array-of-structures format
//
class Vector3
{
    float mX;
    float mY;
    float mZ;
#ifndef __GNUC__
    float d;
#endif

public:
    // Default constructor; does no initialization
    // 
    inline Vector3( ) { };

    // Copy a 3-D vector
    // 
    inline Vector3( const Vector3 & vec );

    // Construct a 3-D vector from x, y, and z elements
    // 
    inline Vector3( float x, float y, float z );

    // Copy elements from a 3-D point into a 3-D vector
    // 
    explicit inline Vector3( const Point3 & pnt );

    // Set all elements of a 3-D vector to the same scalar value
    // 
    explicit inline Vector3( float scalar );

    // Assign one 3-D vector to another
    // 
    inline Vector3 & operator =( const Vector3 & vec );

    // Set the x element of a 3-D vector
    // 
    inline Vector3 & setX( float x );

    // Set the y element of a 3-D vector
    // 
    inline Vector3 & setY( float y );

    // Set the z element of a 3-D vector
    // 
    inline Vector3 & setZ( float z );

    // Get the x element of a 3-D vector
    // 
    inline float getX( ) const;

    // Get the y element of a 3-D vector
    // 
    inline float getY( ) const;

    // Get the z element of a 3-D vector
    // 
    inline float getZ( ) const;

    // Set an x, y, or z element of a 3-D vector by index
    // 
    inline Vector3 & setElem( int idx, float value );

    // Get an x, y, or z element of a 3-D vector by index
    // 
    inline float getElem( int idx ) const;

    // Subscripting operator to set or get an element
    // 
    inline float & operator []( int idx );

    // Subscripting operator to get an element
    // 
    inline float operator []( int idx ) const;

    // Add two 3-D vectors
    // 
    inline const Vector3 operator +( const Vector3 & vec ) const;

    // Subtract a 3-D vector from another 3-D vector
    // 
    inline const Vector3 operator -( const Vector3 & vec ) const;

    // Add a 3-D vector to a 3-D point
    // 
    inline const Point3 operator +( const Point3 & pnt ) const;

    // Multiply a 3-D vector by a scalar
    // 
    inline const Vector3 operator *( float scalar ) const;

    // Divide a 3-D vector by a scalar
    // 
    inline const Vector3 operator /( float scalar ) const;

    // Perform compound assignment and addition with a 3-D vector
    // 
    inline Vector3 & operator +=( const Vector3 & vec );

    // Perform compound assignment and subtraction by a 3-D vector
    // 
    inline Vector3 & operator -=( const Vector3 & vec );

    // Perform compound assignment and multiplication by a scalar
    // 
    inline Vector3 & operator *=( float scalar );

    // Perform compound assignment and division by a scalar
    // 
    inline Vector3 & operator /=( float scalar );

    // Negate all elements of a 3-D vector
    // 
    inline const Vector3 operator -( ) const;

    // Construct x axis
    // 
    static inline const Vector3 xAxis( );

    // Construct y axis
    // 
    static inline const Vector3 yAxis( );

    // Construct z axis
    // 
    static inline const Vector3 zAxis( );

}
#ifdef __GNUC__
__attribute__ ((aligned(16)))
#endif
;

// Multiply a 3-D vector by a scalar
// 
inline const Vector3 operator *( float scalar, const Vector3 & vec );

// Multiply two 3-D vectors per element
// 
inline const Vector3 mulPerElem( const Vector3 & vec0, const Vector3 & vec1 );

// Divide two 3-D vectors per element
// NOTE: 
// Floating-point behavior matches standard library function divf4.
// 
inline const Vector3 divPerElem( const Vector3 & vec0, const Vector3 & vec1 );

// Compute the reciprocal of a 3-D vector per element
// NOTE: 
// Floating-point behavior matches standard library function recipf4.
// 
inline const Vector3 recipPerElem( const Vector3 & vec );

// Compute the square root of a 3-D vector per element
// NOTE: 
// Floating-point behavior matches standard library function sqrtf4.
// 
inline const Vector3 sqrtPerElem( const Vector3 & vec );

// Compute the reciprocal square root of a 3-D vector per element
// NOTE: 
// Floating-point behavior matches standard library function rsqrtf4.
// 
inline const Vector3 rsqrtPerElem( const Vector3 & vec );

// Compute the absolute value of a 3-D vector per element
// 
inline const Vector3 absPerElem( const Vector3 & vec );

// Copy sign from one 3-D vector to another, per element
// 
inline const Vector3 copySignPerElem( const Vector3 & vec0, const Vector3 & vec1 );

// Maximum of two 3-D vectors per element
// 
inline const Vector3 maxPerElem( const Vector3 & vec0, const Vector3 & vec1 );

// Minimum of two 3-D vectors per element
// 
inline const Vector3 minPerElem( const Vector3 & vec0, const Vector3 & vec1 );

// Maximum element of a 3-D vector
// 
inline float maxElem( const Vector3 & vec );

// Minimum element of a 3-D vector
// 
inline float minElem( const Vector3 & vec );

// Compute the sum of all elements of a 3-D vector
// 
inline float sum( const Vector3 & vec );

// Compute the dot product of two 3-D vectors
// 
inline float dot( const Vector3 & vec0, const Vector3 & vec1 );

// Compute the square of the length of a 3-D vector
// 
inline float lengthSqr( const Vector3 & vec );

// Compute the length of a 3-D vector
// 
inline float length( const Vector3 & vec );

// Normalize a 3-D vector
// NOTE: 
// The result is unpredictable when all elements of vec are at or near zero.
// 
inline const Vector3 normalize( const Vector3 & vec );

// Compute cross product of two 3-D vectors
// 
inline const Vector3 cross( const Vector3 & vec0, const Vector3 & vec1 );

// Outer product of two 3-D vectors
// 
inline const Matrix3 outer( const Vector3 & vec0, const Vector3 & vec1 );

// Pre-multiply a row vector by a 3x3 matrix
// 
inline const Vector3 rowMul( const Vector3 & vec, const Matrix3 & mat );

// Cross-product matrix of a 3-D vector
// 
inline const Matrix3 crossMatrix( const Vector3 & vec );

// Create cross-product matrix and multiply
// NOTE: 
// Faster than separately creating a cross-product matrix and multiplying.
// 
inline const Matrix3 crossMatrixMul( const Vector3 & vec, const Matrix3 & mat );

// Linear interpolation between two 3-D vectors
// NOTE: 
// Does not clamp t between 0 and 1.
// 
inline const Vector3 lerp( float t, const Vector3 & vec0, const Vector3 & vec1 );

// Spherical linear interpolation between two 3-D vectors
// NOTE: 
// The result is unpredictable if the vectors point in opposite directions.
// Does not clamp t between 0 and 1.
// 
inline const Vector3 slerp( float t, const Vector3 & unitVec0, const Vector3 & unitVec1 );

// Conditionally select between two 3-D vectors
// 
inline const Vector3 select( const Vector3 & vec0, const Vector3 & vec1, bool select1 );

#ifdef _VECTORMATH_DEBUG

// Print a 3-D vector
// NOTE: 
// Function is only defined when _VECTORMATH_DEBUG is defined.
// 
inline void print( const Vector3 & vec );

// Print a 3-D vector and an associated string identifier
// NOTE: 
// Function is only defined when _VECTORMATH_DEBUG is defined.
// 
inline void print( const Vector3 & vec, const char * name );

#endif

// A 4-D vector in array-of-structures format
//
class Vector4
{
    float mX;
    float mY;
    float mZ;
    float mW;

public:
    // Default constructor; does no initialization
    // 
    inline Vector4( ) { };

    // Copy a 4-D vector
    // 
    inline Vector4( const Vector4 & vec );

    // Construct a 4-D vector from x, y, z, and w elements
    // 
    inline Vector4( float x, float y, float z, float w );

    // Construct a 4-D vector from a 3-D vector and a scalar
    // 
    inline Vector4( const Vector3 & xyz, float w );

    // Copy x, y, and z from a 3-D vector into a 4-D vector, and set w to 0
    // 
    explicit inline Vector4( const Vector3 & vec );

    // Copy x, y, and z from a 3-D point into a 4-D vector, and set w to 1
    // 
    explicit inline Vector4( const Point3 & pnt );

    // Copy elements from a quaternion into a 4-D vector
    // 
    explicit inline Vector4( const Quat & quat );

    // Set all elements of a 4-D vector to the same scalar value
    // 
    explicit inline Vector4( float scalar );

    // Assign one 4-D vector to another
    // 
    inline Vector4 & operator =( const Vector4 & vec );

    // Set the x, y, and z elements of a 4-D vector
    // NOTE: 
    // This function does not change the w element.
    // 
    inline Vector4 & setXYZ( const Vector3 & vec );

    // Get the x, y, and z elements of a 4-D vector
    // 
    inline const Vector3 getXYZ( ) const;

    // Set the x element of a 4-D vector
    // 
    inline Vector4 & setX( float x );

    // Set the y element of a 4-D vector
    // 
    inline Vector4 & setY( float y );

    // Set the z element of a 4-D vector
    // 
    inline Vector4 & setZ( float z );

    // Set the w element of a 4-D vector
    // 
    inline Vector4 & setW( float w );

    // Get the x element of a 4-D vector
    // 
    inline float getX( ) const;

    // Get the y element of a 4-D vector
    // 
    inline float getY( ) const;

    // Get the z element of a 4-D vector
    // 
    inline float getZ( ) const;

    // Get the w element of a 4-D vector
    // 
    inline float getW( ) const;

    // Set an x, y, z, or w element of a 4-D vector by index
    // 
    inline Vector4 & setElem( int idx, float value );

    // Get an x, y, z, or w element of a 4-D vector by index
    // 
    inline float getElem( int idx ) const;

    // Subscripting operator to set or get an element
    // 
    inline float & operator []( int idx );

    // Subscripting operator to get an element
    // 
    inline float operator []( int idx ) const;

    // Add two 4-D vectors
    // 
    inline const Vector4 operator +( const Vector4 & vec ) const;

    // Subtract a 4-D vector from another 4-D vector
    // 
    inline const Vector4 operator -( const Vector4 & vec ) const;

    // Multiply a 4-D vector by a scalar
    // 
    inline const Vector4 operator *( float scalar ) const;

    // Divide a 4-D vector by a scalar
    // 
    inline const Vector4 operator /( float scalar ) const;

    // Perform compound assignment and addition with a 4-D vector
    // 
    inline Vector4 & operator +=( const Vector4 & vec );

    // Perform compound assignment and subtraction by a 4-D vector
    // 
    inline Vector4 & operator -=( const Vector4 & vec );

    // Perform compound assignment and multiplication by a scalar
    // 
    inline Vector4 & operator *=( float scalar );

    // Perform compound assignment and division by a scalar
    // 
    inline Vector4 & operator /=( float scalar );

    // Negate all elements of a 4-D vector
    // 
    inline const Vector4 operator -( ) const;

    // Construct x axis
    // 
    static inline const Vector4 xAxis( );

    // Construct y axis
    // 
    static inline const Vector4 yAxis( );

    // Construct z axis
    // 
    static inline const Vector4 zAxis( );

    // Construct w axis
    // 
    static inline const Vector4 wAxis( );

}
#ifdef __GNUC__
__attribute__ ((aligned(16)))
#endif
;

// Multiply a 4-D vector by a scalar
// 
inline const Vector4 operator *( float scalar, const Vector4 & vec );

// Multiply two 4-D vectors per element
// 
inline const Vector4 mulPerElem( const Vector4 & vec0, const Vector4 & vec1 );

// Divide two 4-D vectors per element
// NOTE: 
// Floating-point behavior matches standard library function divf4.
// 
inline const Vector4 divPerElem( const Vector4 & vec0, const Vector4 & vec1 );

// Compute the reciprocal of a 4-D vector per element
// NOTE: 
// Floating-point behavior matches standard library function recipf4.
// 
inline const Vector4 recipPerElem( const Vector4 & vec );

// Compute the square root of a 4-D vector per element
// NOTE: 
// Floating-point behavior matches standard library function sqrtf4.
// 
inline const Vector4 sqrtPerElem( const Vector4 & vec );

// Compute the reciprocal square root of a 4-D vector per element
// NOTE: 
// Floating-point behavior matches standard library function rsqrtf4.
// 
inline const Vector4 rsqrtPerElem( const Vector4 & vec );

// Compute the absolute value of a 4-D vector per element
// 
inline const Vector4 absPerElem( const Vector4 & vec );

// Copy sign from one 4-D vector to another, per element
// 
inline const Vector4 copySignPerElem( const Vector4 & vec0, const Vector4 & vec1 );

// Maximum of two 4-D vectors per element
// 
inline const Vector4 maxPerElem( const Vector4 & vec0, const Vector4 & vec1 );

// Minimum of two 4-D vectors per element
// 
inline const Vector4 minPerElem( const Vector4 & vec0, const Vector4 & vec1 );

// Maximum element of a 4-D vector
// 
inline float maxElem( const Vector4 & vec );

// Minimum element of a 4-D vector
// 
inline float minElem( const Vector4 & vec );

// Compute the sum of all elements of a 4-D vector
// 
inline float sum( const Vector4 & vec );

// Compute the dot product of two 4-D vectors
// 
inline float dot( const Vector4 & vec0, const Vector4 & vec1 );

// Compute the square of the length of a 4-D vector
// 
inline float lengthSqr( const Vector4 & vec );

// Compute the length of a 4-D vector
// 
inline float length( const Vector4 & vec );

// Normalize a 4-D vector
// NOTE: 
// The result is unpredictable when all elements of vec are at or near zero.
// 
inline const Vector4 normalize( const Vector4 & vec );

// Outer product of two 4-D vectors
// 
inline const Matrix4 outer( const Vector4 & vec0, const Vector4 & vec1 );

// Linear interpolation between two 4-D vectors
// NOTE: 
// Does not clamp t between 0 and 1.
// 
inline const Vector4 lerp( float t, const Vector4 & vec0, const Vector4 & vec1 );

// Spherical linear interpolation between two 4-D vectors
// NOTE: 
// The result is unpredictable if the vectors point in opposite directions.
// Does not clamp t between 0 and 1.
// 
inline const Vector4 slerp( float t, const Vector4 & unitVec0, const Vector4 & unitVec1 );

// Conditionally select between two 4-D vectors
// 
inline const Vector4 select( const Vector4 & vec0, const Vector4 & vec1, bool select1 );

#ifdef _VECTORMATH_DEBUG

// Print a 4-D vector
// NOTE: 
// Function is only defined when _VECTORMATH_DEBUG is defined.
// 
inline void print( const Vector4 & vec );

// Print a 4-D vector and an associated string identifier
// NOTE: 
// Function is only defined when _VECTORMATH_DEBUG is defined.
// 
inline void print( const Vector4 & vec, const char * name );

#endif

// A 3-D point in array-of-structures format
//
class Point3
{
    float mX;
    float mY;
    float mZ;
#ifndef __GNUC__
    float d;
#endif

public:
    // Default constructor; does no initialization
    // 
    inline Point3( ) { };

    // Copy a 3-D point
    // 
    inline Point3( const Point3 & pnt );

    // Construct a 3-D point from x, y, and z elements
    // 
    inline Point3( float x, float y, float z );

    // Copy elements from a 3-D vector into a 3-D point
    // 
    explicit inline Point3( const Vector3 & vec );

    // Set all elements of a 3-D point to the same scalar value
    // 
    explicit inline Point3( float scalar );

    // Assign one 3-D point to another
    // 
    inline Point3 & operator =( const Point3 & pnt );

    // Set the x element of a 3-D point
    // 
    inline Point3 & setX( float x );

    // Set the y element of a 3-D point
    // 
    inline Point3 & setY( float y );

    // Set the z element of a 3-D point
    // 
    inline Point3 & setZ( float z );

    // Get the x element of a 3-D point
    // 
    inline float getX( ) const;

    // Get the y element of a 3-D point
    // 
    inline float getY( ) const;

    // Get the z element of a 3-D point
    // 
    inline float getZ( ) const;

    // Set an x, y, or z element of a 3-D point by index
    // 
    inline Point3 & setElem( int idx, float value );

    // Get an x, y, or z element of a 3-D point by index
    // 
    inline float getElem( int idx ) const;

    // Subscripting operator to set or get an element
    // 
    inline float & operator []( int idx );

    // Subscripting operator to get an element
    // 
    inline float operator []( int idx ) const;

    // Subtract a 3-D point from another 3-D point
    // 
    inline const Vector3 operator -( const Point3 & pnt ) const;

    // Add a 3-D point to a 3-D vector
    // 
    inline const Point3 operator +( const Vector3 & vec ) const;

    // Subtract a 3-D vector from a 3-D point
    // 
    inline const Point3 operator -( const Vector3 & vec ) const;

    // Perform compound assignment and addition with a 3-D vector
    // 
    inline Point3 & operator +=( const Vector3 & vec );

    // Perform compound assignment and subtraction by a 3-D vector
    // 
    inline Point3 & operator -=( const Vector3 & vec );

}
#ifdef __GNUC__
__attribute__ ((aligned(16)))
#endif
;

// Multiply two 3-D points per element
// 
inline const Point3 mulPerElem( const Point3 & pnt0, const Point3 & pnt1 );

// Divide two 3-D points per element
// NOTE: 
// Floating-point behavior matches standard library function divf4.
// 
inline const Point3 divPerElem( const Point3 & pnt0, const Point3 & pnt1 );

// Compute the reciprocal of a 3-D point per element
// NOTE: 
// Floating-point behavior matches standard library function recipf4.
// 
inline const Point3 recipPerElem( const Point3 & pnt );

// Compute the square root of a 3-D point per element
// NOTE: 
// Floating-point behavior matches standard library function sqrtf4.
// 
inline const Point3 sqrtPerElem( const Point3 & pnt );

// Compute the reciprocal square root of a 3-D point per element
// NOTE: 
// Floating-point behavior matches standard library function rsqrtf4.
// 
inline const Point3 rsqrtPerElem( const Point3 & pnt );

// Compute the absolute value of a 3-D point per element
// 
inline const Point3 absPerElem( const Point3 & pnt );

// Copy sign from one 3-D point to another, per element
// 
inline const Point3 copySignPerElem( const Point3 & pnt0, const Point3 & pnt1 );

// Maximum of two 3-D points per element
// 
inline const Point3 maxPerElem( const Point3 & pnt0, const Point3 & pnt1 );

// Minimum of two 3-D points per element
// 
inline const Point3 minPerElem( const Point3 & pnt0, const Point3 & pnt1 );

// Maximum element of a 3-D point
// 
inline float maxElem( const Point3 & pnt );

// Minimum element of a 3-D point
// 
inline float minElem( const Point3 & pnt );

// Compute the sum of all elements of a 3-D point
// 
inline float sum( const Point3 & pnt );

// Apply uniform scale to a 3-D point
// 
inline const Point3 scale( const Point3 & pnt, float scaleVal );

// Apply non-uniform scale to a 3-D point
// 
inline const Point3 scale( const Point3 & pnt, const Vector3 & scaleVec );

// Scalar projection of a 3-D point on a unit-length 3-D vector
// 
inline float projection( const Point3 & pnt, const Vector3 & unitVec );

// Compute the square of the distance of a 3-D point from the coordinate-system origin
// 
inline float distSqrFromOrigin( const Point3 & pnt );

// Compute the distance of a 3-D point from the coordinate-system origin
// 
inline float distFromOrigin( const Point3 & pnt );

// Compute the square of the distance between two 3-D points
// 
inline float distSqr( const Point3 & pnt0, const Point3 & pnt1 );

// Compute the distance between two 3-D points
// 
inline float dist( const Point3 & pnt0, const Point3 & pnt1 );

// Linear interpolation between two 3-D points
// NOTE: 
// Does not clamp t between 0 and 1.
// 
inline const Point3 lerp( float t, const Point3 & pnt0, const Point3 & pnt1 );

// Conditionally select between two 3-D points
// 
inline const Point3 select( const Point3 & pnt0, const Point3 & pnt1, bool select1 );

#ifdef _VECTORMATH_DEBUG

// Print a 3-D point
// NOTE: 
// Function is only defined when _VECTORMATH_DEBUG is defined.
// 
inline void print( const Point3 & pnt );

// Print a 3-D point and an associated string identifier
// NOTE: 
// Function is only defined when _VECTORMATH_DEBUG is defined.
// 
inline void print( const Point3 & pnt, const char * name );

#endif

// A quaternion in array-of-structures format
//
class Quat
{
    float mX;
    float mY;
    float mZ;
    float mW;

public:
    // Default constructor; does no initialization
    // 
    inline Quat( ) { };

    // Copy a quaternion
    // 
    inline Quat( const Quat & quat );

    // Construct a quaternion from x, y, z, and w elements
    // 
    inline Quat( float x, float y, float z, float w );

    // Construct a quaternion from a 3-D vector and a scalar
    // 
    inline Quat( const Vector3 & xyz, float w );

    // Copy elements from a 4-D vector into a quaternion
    // 
    explicit inline Quat( const Vector4 & vec );

    // Convert a rotation matrix to a unit-length quaternion
    // 
    explicit inline Quat( const Matrix3 & rotMat );

    // Set all elements of a quaternion to the same scalar value
    // 
    explicit inline Quat( float scalar );

    // Assign one quaternion to another
    // 
    inline Quat & operator =( const Quat & quat );

    // Set the x, y, and z elements of a quaternion
    // NOTE: 
    // This function does not change the w element.
    // 
    inline Quat & setXYZ( const Vector3 & vec );

    // Get the x, y, and z elements of a quaternion
    // 
    inline const Vector3 getXYZ( ) const;

    // Set the x element of a quaternion
    // 
    inline Quat & setX( float x );

    // Set the y element of a quaternion
    // 
    inline Quat & setY( float y );

    // Set the z element of a quaternion
    // 
    inline Quat & setZ( float z );

    // Set the w element of a quaternion
    // 
    inline Quat & setW( float w );

    // Get the x element of a quaternion
    // 
    inline float getX( ) const;

    // Get the y element of a quaternion
    // 
    inline float getY( ) const;

    // Get the z element of a quaternion
    // 
    inline float getZ( ) const;

    // Get the w element of a quaternion
    // 
    inline float getW( ) const;

    // Set an x, y, z, or w element of a quaternion by index
    // 
    inline Quat & setElem( int idx, float value );

    // Get an x, y, z, or w element of a quaternion by index
    // 
    inline float getElem( int idx ) const;

    // Subscripting operator to set or get an element
    // 
    inline float & operator []( int idx );

    // Subscripting operator to get an element
    // 
    inline float operator []( int idx ) const;

    // Add two quaternions
    // 
    inline const Quat operator +( const Quat & quat ) const;

    // Subtract a quaternion from another quaternion
    // 
    inline const Quat operator -( const Quat & quat ) const;

    // Multiply two quaternions
    // 
    inline const Quat operator *( const Quat & quat ) const;

    // Multiply a quaternion by a scalar
    // 
    inline const Quat operator *( float scalar ) const;

    // Divide a quaternion by a scalar
    // 
    inline const Quat operator /( float scalar ) const;

    // Perform compound assignment and addition with a quaternion
    // 
    inline Quat & operator +=( const Quat & quat );

    // Perform compound assignment and subtraction by a quaternion
    // 
    inline Quat & operator -=( const Quat & quat );

    // Perform compound assignment and multiplication by a quaternion
    // 
    inline Quat & operator *=( const Quat & quat );

    // Perform compound assignment and multiplication by a scalar
    // 
    inline Quat & operator *=( float scalar );

    // Perform compound assignment and division by a scalar
    // 
    inline Quat & operator /=( float scalar );

    // Negate all elements of a quaternion
    // 
    inline const Quat operator -( ) const;

    // Construct an identity quaternion
    // 
    static inline const Quat identity( );

    // Construct a quaternion to rotate between two unit-length 3-D vectors
    // NOTE: 
    // The result is unpredictable if unitVec0 and unitVec1 point in opposite directions.
    // 
    static inline const Quat rotation( const Vector3 & unitVec0, const Vector3 & unitVec1 );

    // Construct a quaternion to rotate around a unit-length 3-D vector
    // 
    static inline const Quat rotation( float radians, const Vector3 & unitVec );

    // Construct a quaternion to rotate around the x axis
    // 
    static inline const Quat rotationX( float radians );

    // Construct a quaternion to rotate around the y axis
    // 
    static inline const Quat rotationY( float radians );

    // Construct a quaternion to rotate around the z axis
    // 
    static inline const Quat rotationZ( float radians );

}
#ifdef __GNUC__
__attribute__ ((aligned(16)))
#endif
;

// Multiply a quaternion by a scalar
// 
inline const Quat operator *( float scalar, const Quat & quat );

// Compute the conjugate of a quaternion
// 
inline const Quat conj( const Quat & quat );

// Use a unit-length quaternion to rotate a 3-D vector
// 
inline const Vector3 rotate( const Quat & unitQuat, const Vector3 & vec );

// Compute the dot product of two quaternions
// 
inline float dot( const Quat & quat0, const Quat & quat1 );

// Compute the norm of a quaternion
// 
inline float norm( const Quat & quat );

// Compute the length of a quaternion
// 
inline float length( const Quat & quat );

// Normalize a quaternion
// NOTE: 
// The result is unpredictable when all elements of quat are at or near zero.
// 
inline const Quat normalize( const Quat & quat );

// Linear interpolation between two quaternions
// NOTE: 
// Does not clamp t between 0 and 1.
// 
inline const Quat lerp( float t, const Quat & quat0, const Quat & quat1 );

// Spherical linear interpolation between two quaternions
// NOTE: 
// Interpolates along the shortest path between orientations.
// Does not clamp t between 0 and 1.
// 
inline const Quat slerp( float t, const Quat & unitQuat0, const Quat & unitQuat1 );

// Spherical quadrangle interpolation
// 
inline const Quat squad( float t, const Quat & unitQuat0, const Quat & unitQuat1, const Quat & unitQuat2, const Quat & unitQuat3 );

// Conditionally select between two quaternions
// 
inline const Quat select( const Quat & quat0, const Quat & quat1, bool select1 );

#ifdef _VECTORMATH_DEBUG

// Print a quaternion
// NOTE: 
// Function is only defined when _VECTORMATH_DEBUG is defined.
// 
inline void print( const Quat & quat );

// Print a quaternion and an associated string identifier
// NOTE: 
// Function is only defined when _VECTORMATH_DEBUG is defined.
// 
inline void print( const Quat & quat, const char * name );

#endif

// A 3x3 matrix in array-of-structures format
//
class Matrix3
{
    Vector3 mCol0;
    Vector3 mCol1;
    Vector3 mCol2;

public:
    // Default constructor; does no initialization
    // 
    inline Matrix3( ) { };

    // Copy a 3x3 matrix
    // 
    inline Matrix3( const Matrix3 & mat );

    // Construct a 3x3 matrix containing the specified columns
    // 
    inline Matrix3( const Vector3 & col0, const Vector3 & col1, const Vector3 & col2 );

    // Construct a 3x3 rotation matrix from a unit-length quaternion
    // 
    explicit inline Matrix3( const Quat & unitQuat );

    // Set all elements of a 3x3 matrix to the same scalar value
    // 
    explicit inline Matrix3( float scalar );

    // Assign one 3x3 matrix to another
    // 
    inline Matrix3 & operator =( const Matrix3 & mat );

    // Set column 0 of a 3x3 matrix
    // 
    inline Matrix3 & setCol0( const Vector3 & col0 );

    // Set column 1 of a 3x3 matrix
    // 
    inline Matrix3 & setCol1( const Vector3 & col1 );

    // Set column 2 of a 3x3 matrix
    // 
    inline Matrix3 & setCol2( const Vector3 & col2 );

    // Get column 0 of a 3x3 matrix
    // 
    inline const Vector3 getCol0( ) const;

    // Get column 1 of a 3x3 matrix
    // 
    inline const Vector3 getCol1( ) const;

    // Get column 2 of a 3x3 matrix
    // 
    inline const Vector3 getCol2( ) const;

    // Set the column of a 3x3 matrix referred to by the specified index
    // 
    inline Matrix3 & setCol( int col, const Vector3 & vec );

    // Set the row of a 3x3 matrix referred to by the specified index
    // 
    inline Matrix3 & setRow( int row, const Vector3 & vec );

    // Get the column of a 3x3 matrix referred to by the specified index
    // 
    inline const Vector3 getCol( int col ) const;

    // Get the row of a 3x3 matrix referred to by the specified index
    // 
    inline const Vector3 getRow( int row ) const;

    // Subscripting operator to set or get a column
    // 
    inline Vector3 & operator []( int col );

    // Subscripting operator to get a column
    // 
    inline const Vector3 operator []( int col ) const;

    // Set the element of a 3x3 matrix referred to by column and row indices
    // 
    inline Matrix3 & setElem( int col, int row, float val );

    // Get the element of a 3x3 matrix referred to by column and row indices
    // 
    inline float getElem( int col, int row ) const;

    // Add two 3x3 matrices
    // 
    inline const Matrix3 operator +( const Matrix3 & mat ) const;

    // Subtract a 3x3 matrix from another 3x3 matrix
    // 
    inline const Matrix3 operator -( const Matrix3 & mat ) const;

    // Negate all elements of a 3x3 matrix
    // 
    inline const Matrix3 operator -( ) const;

    // Multiply a 3x3 matrix by a scalar
    // 
    inline const Matrix3 operator *( float scalar ) const;

    // Multiply a 3x3 matrix by a 3-D vector
    // 
    inline const Vector3 operator *( const Vector3 & vec ) const;

    // Multiply two 3x3 matrices
    // 
    inline const Matrix3 operator *( const Matrix3 & mat ) const;

    // Perform compound assignment and addition with a 3x3 matrix
    // 
    inline Matrix3 & operator +=( const Matrix3 & mat );

    // Perform compound assignment and subtraction by a 3x3 matrix
    // 
    inline Matrix3 & operator -=( const Matrix3 & mat );

    // Perform compound assignment and multiplication by a scalar
    // 
    inline Matrix3 & operator *=( float scalar );

    // Perform compound assignment and multiplication by a 3x3 matrix
    // 
    inline Matrix3 & operator *=( const Matrix3 & mat );

    // Construct an identity 3x3 matrix
    // 
    static inline const Matrix3 identity( );

    // Construct a 3x3 matrix to rotate around the x axis
    // 
    static inline const Matrix3 rotationX( float radians );

    // Construct a 3x3 matrix to rotate around the y axis
    // 
    static inline const Matrix3 rotationY( float radians );

    // Construct a 3x3 matrix to rotate around the z axis
    // 
    static inline const Matrix3 rotationZ( float radians );

    // Construct a 3x3 matrix to rotate around the x, y, and z axes
    // 
    static inline const Matrix3 rotationZYX( const Vector3 & radiansXYZ );

    // Construct a 3x3 matrix to rotate around a unit-length 3-D vector
    // 
    static inline const Matrix3 rotation( float radians, const Vector3 & unitVec );

    // Construct a rotation matrix from a unit-length quaternion
    // 
    static inline const Matrix3 rotation( const Quat & unitQuat );

    // Construct a 3x3 matrix to perform scaling
    // 
    static inline const Matrix3 scale( const Vector3 & scaleVec );

};
// Multiply a 3x3 matrix by a scalar
// 
inline const Matrix3 operator *( float scalar, const Matrix3 & mat );

// Append (post-multiply) a scale transformation to a 3x3 matrix
// NOTE: 
// Faster than creating and multiplying a scale transformation matrix.
// 
inline const Matrix3 appendScale( const Matrix3 & mat, const Vector3 & scaleVec );

// Prepend (pre-multiply) a scale transformation to a 3x3 matrix
// NOTE: 
// Faster than creating and multiplying a scale transformation matrix.
// 
inline const Matrix3 prependScale( const Vector3 & scaleVec, const Matrix3 & mat );

// Multiply two 3x3 matrices per element
// 
inline const Matrix3 mulPerElem( const Matrix3 & mat0, const Matrix3 & mat1 );

// Compute the absolute value of a 3x3 matrix per element
// 
inline const Matrix3 absPerElem( const Matrix3 & mat );

// Transpose of a 3x3 matrix
// 
inline const Matrix3 transpose( const Matrix3 & mat );

// Compute the inverse of a 3x3 matrix
// NOTE: 
// Result is unpredictable when the determinant of mat is equal to or near 0.
// 
inline const Matrix3 inverse( const Matrix3 & mat );

// Determinant of a 3x3 matrix
// 
inline float determinant( const Matrix3 & mat );

// Conditionally select between two 3x3 matrices
// 
inline const Matrix3 select( const Matrix3 & mat0, const Matrix3 & mat1, bool select1 );

#ifdef _VECTORMATH_DEBUG

// Print a 3x3 matrix
// NOTE: 
// Function is only defined when _VECTORMATH_DEBUG is defined.
// 
inline void print( const Matrix3 & mat );

// Print a 3x3 matrix and an associated string identifier
// NOTE: 
// Function is only defined when _VECTORMATH_DEBUG is defined.
// 
inline void print( const Matrix3 & mat, const char * name );

#endif

// A 4x4 matrix in array-of-structures format
//
class Matrix4
{
    Vector4 mCol0;
    Vector4 mCol1;
    Vector4 mCol2;
    Vector4 mCol3;

public:
    // Default constructor; does no initialization
    // 
    inline Matrix4( ) { };

    // Copy a 4x4 matrix
    // 
    inline Matrix4( const Matrix4 & mat );

    // Construct a 4x4 matrix containing the specified columns
    // 
    inline Matrix4( const Vector4 & col0, const Vector4 & col1, const Vector4 & col2, const Vector4 & col3 );

    // Construct a 4x4 matrix from a 3x4 transformation matrix
    // 
    explicit inline Matrix4( const Transform3 & mat );

    // Construct a 4x4 matrix from a 3x3 matrix and a 3-D vector
    // 
    inline Matrix4( const Matrix3 & mat, const Vector3 & translateVec );

    // Construct a 4x4 matrix from a unit-length quaternion and a 3-D vector
    // 
    inline Matrix4( const Quat & unitQuat, const Vector3 & translateVec );

    // Set all elements of a 4x4 matrix to the same scalar value
    // 
    explicit inline Matrix4( float scalar );

    // Assign one 4x4 matrix to another
    // 
    inline Matrix4 & operator =( const Matrix4 & mat );

    // Set the upper-left 3x3 submatrix
    // NOTE: 
    // This function does not change the bottom row elements.
    // 
    inline Matrix4 & setUpper3x3( const Matrix3 & mat3 );

    // Get the upper-left 3x3 submatrix of a 4x4 matrix
    // 
    inline const Matrix3 getUpper3x3( ) const;

    // Set translation component
    // NOTE: 
    // This function does not change the bottom row elements.
    // 
    inline Matrix4 & setTranslation( const Vector3 & translateVec );

    // Get the translation component of a 4x4 matrix
    // 
    inline const Vector3 getTranslation( ) const;

    // Set column 0 of a 4x4 matrix
    // 
    inline Matrix4 & setCol0( const Vector4 & col0 );

    // Set column 1 of a 4x4 matrix
    // 
    inline Matrix4 & setCol1( const Vector4 & col1 );

    // Set column 2 of a 4x4 matrix
    // 
    inline Matrix4 & setCol2( const Vector4 & col2 );

    // Set column 3 of a 4x4 matrix
    // 
    inline Matrix4 & setCol3( const Vector4 & col3 );

    // Get column 0 of a 4x4 matrix
    // 
    inline const Vector4 getCol0( ) const;

    // Get column 1 of a 4x4 matrix
    // 
    inline const Vector4 getCol1( ) const;

    // Get column 2 of a 4x4 matrix
    // 
    inline const Vector4 getCol2( ) const;

    // Get column 3 of a 4x4 matrix
    // 
    inline const Vector4 getCol3( ) const;

    // Set the column of a 4x4 matrix referred to by the specified index
    // 
    inline Matrix4 & setCol( int col, const Vector4 & vec );

    // Set the row of a 4x4 matrix referred to by the specified index
    // 
    inline Matrix4 & setRow( int row, const Vector4 & vec );

    // Get the column of a 4x4 matrix referred to by the specified index
    // 
    inline const Vector4 getCol( int col ) const;

    // Get the row of a 4x4 matrix referred to by the specified index
    // 
    inline const Vector4 getRow( int row ) const;

    // Subscripting operator to set or get a column
    // 
    inline Vector4 & operator []( int col );

    // Subscripting operator to get a column
    // 
    inline const Vector4 operator []( int col ) const;

    // Set the element of a 4x4 matrix referred to by column and row indices
    // 
    inline Matrix4 & setElem( int col, int row, float val );

    // Get the element of a 4x4 matrix referred to by column and row indices
    // 
    inline float getElem( int col, int row ) const;

    // Add two 4x4 matrices
    // 
    inline const Matrix4 operator +( const Matrix4 & mat ) const;

    // Subtract a 4x4 matrix from another 4x4 matrix
    // 
    inline const Matrix4 operator -( const Matrix4 & mat ) const;

    // Negate all elements of a 4x4 matrix
    // 
    inline const Matrix4 operator -( ) const;

    // Multiply a 4x4 matrix by a scalar
    // 
    inline const Matrix4 operator *( float scalar ) const;

    // Multiply a 4x4 matrix by a 4-D vector
    // 
    inline const Vector4 operator *( const Vector4 & vec ) const;

    // Multiply a 4x4 matrix by a 3-D vector
    // 
    inline const Vector4 operator *( const Vector3 & vec ) const;

    // Multiply a 4x4 matrix by a 3-D point
    // 
    inline const Vector4 operator *( const Point3 & pnt ) const;

    // Multiply two 4x4 matrices
    // 
    inline const Matrix4 operator *( const Matrix4 & mat ) const;

    // Multiply a 4x4 matrix by a 3x4 transformation matrix
    // 
    inline const Matrix4 operator *( const Transform3 & tfrm ) const;

    // Perform compound assignment and addition with a 4x4 matrix
    // 
    inline Matrix4 & operator +=( const Matrix4 & mat );

    // Perform compound assignment and subtraction by a 4x4 matrix
    // 
    inline Matrix4 & operator -=( const Matrix4 & mat );

    // Perform compound assignment and multiplication by a scalar
    // 
    inline Matrix4 & operator *=( float scalar );

    // Perform compound assignment and multiplication by a 4x4 matrix
    // 
    inline Matrix4 & operator *=( const Matrix4 & mat );

    // Perform compound assignment and multiplication by a 3x4 transformation matrix
    // 
    inline Matrix4 & operator *=( const Transform3 & tfrm );

    // Construct an identity 4x4 matrix
    // 
    static inline const Matrix4 identity( );

    // Construct a 4x4 matrix to rotate around the x axis
    // 
    static inline const Matrix4 rotationX( float radians );

    // Construct a 4x4 matrix to rotate around the y axis
    // 
    static inline const Matrix4 rotationY( float radians );

    // Construct a 4x4 matrix to rotate around the z axis
    // 
    static inline const Matrix4 rotationZ( float radians );

    // Construct a 4x4 matrix to rotate around the x, y, and z axes
    // 
    static inline const Matrix4 rotationZYX( const Vector3 & radiansXYZ );

    // Construct a 4x4 matrix to rotate around a unit-length 3-D vector
    // 
    static inline const Matrix4 rotation( float radians, const Vector3 & unitVec );

    // Construct a rotation matrix from a unit-length quaternion
    // 
    static inline const Matrix4 rotation( const Quat & unitQuat );

    // Construct a 4x4 matrix to perform scaling
    // 
    static inline const Matrix4 scale( const Vector3 & scaleVec );

    // Construct a 4x4 matrix to perform translation
    // 
    static inline const Matrix4 translation( const Vector3 & translateVec );

    // Construct viewing matrix based on eye position, position looked at, and up direction
    // 
    static inline const Matrix4 lookAt( const Point3 & eyePos, const Point3 & lookAtPos, const Vector3 & upVec );

    // Construct a perspective projection matrix
    // 
    static inline const Matrix4 perspective( float fovyRadians, float aspect, float zNear, float zFar );

    // Construct a perspective projection matrix based on frustum
    // 
    static inline const Matrix4 frustum( float left, float right, float bottom, float top, float zNear, float zFar );

    // Construct an orthographic projection matrix
    // 
    static inline const Matrix4 orthographic( float left, float right, float bottom, float top, float zNear, float zFar );

};
// Multiply a 4x4 matrix by a scalar
// 
inline const Matrix4 operator *( float scalar, const Matrix4 & mat );

// Append (post-multiply) a scale transformation to a 4x4 matrix
// NOTE: 
// Faster than creating and multiplying a scale transformation matrix.
// 
inline const Matrix4 appendScale( const Matrix4 & mat, const Vector3 & scaleVec );

// Prepend (pre-multiply) a scale transformation to a 4x4 matrix
// NOTE: 
// Faster than creating and multiplying a scale transformation matrix.
// 
inline const Matrix4 prependScale( const Vector3 & scaleVec, const Matrix4 & mat );

// Multiply two 4x4 matrices per element
// 
inline const Matrix4 mulPerElem( const Matrix4 & mat0, const Matrix4 & mat1 );

// Compute the absolute value of a 4x4 matrix per element
// 
inline const Matrix4 absPerElem( const Matrix4 & mat );

// Transpose of a 4x4 matrix
// 
inline const Matrix4 transpose( const Matrix4 & mat );

// Compute the inverse of a 4x4 matrix
// NOTE: 
// Result is unpredictable when the determinant of mat is equal to or near 0.
// 
inline const Matrix4 inverse( const Matrix4 & mat );

// Compute the inverse of a 4x4 matrix, which is expected to be an affine matrix
// NOTE: 
// This can be used to achieve better performance than a general inverse when the specified 4x4 matrix meets the given restrictions.  The result is unpredictable when the determinant of mat is equal to or near 0.
// 
inline const Matrix4 affineInverse( const Matrix4 & mat );

// Compute the inverse of a 4x4 matrix, which is expected to be an affine matrix with an orthogonal upper-left 3x3 submatrix
// NOTE: 
// This can be used to achieve better performance than a general inverse when the specified 4x4 matrix meets the given restrictions.
// 
inline const Matrix4 orthoInverse( const Matrix4 & mat );

// Determinant of a 4x4 matrix
// 
inline float determinant( const Matrix4 & mat );

// Conditionally select between two 4x4 matrices
// 
inline const Matrix4 select( const Matrix4 & mat0, const Matrix4 & mat1, bool select1 );

#ifdef _VECTORMATH_DEBUG

// Print a 4x4 matrix
// NOTE: 
// Function is only defined when _VECTORMATH_DEBUG is defined.
// 
inline void print( const Matrix4 & mat );

// Print a 4x4 matrix and an associated string identifier
// NOTE: 
// Function is only defined when _VECTORMATH_DEBUG is defined.
// 
inline void print( const Matrix4 & mat, const char * name );

#endif

// A 3x4 transformation matrix in array-of-structures format
//
class Transform3
{
    Vector3 mCol0;
    Vector3 mCol1;
    Vector3 mCol2;
    Vector3 mCol3;

public:
    // Default constructor; does no initialization
    // 
    inline Transform3( ) { };

    // Copy a 3x4 transformation matrix
    // 
    inline Transform3( const Transform3 & tfrm );

    // Construct a 3x4 transformation matrix containing the specified columns
    // 
    inline Transform3( const Vector3 & col0, const Vector3 & col1, const Vector3 & col2, const Vector3 & col3 );

    // Construct a 3x4 transformation matrix from a 3x3 matrix and a 3-D vector
    // 
    inline Transform3( const Matrix3 & tfrm, const Vector3 & translateVec );

    // Construct a 3x4 transformation matrix from a unit-length quaternion and a 3-D vector
    // 
    inline Transform3( const Quat & unitQuat, const Vector3 & translateVec );

    // Set all elements of a 3x4 transformation matrix to the same scalar value
    // 
    explicit inline Transform3( float scalar );

    // Assign one 3x4 transformation matrix to another
    // 
    inline Transform3 & operator =( const Transform3 & tfrm );

    // Set the upper-left 3x3 submatrix
    // 
    inline Transform3 & setUpper3x3( const Matrix3 & mat3 );

    // Get the upper-left 3x3 submatrix of a 3x4 transformation matrix
    // 
    inline const Matrix3 getUpper3x3( ) const;

    // Set translation component
    // 
    inline Transform3 & setTranslation( const Vector3 & translateVec );

    // Get the translation component of a 3x4 transformation matrix
    // 
    inline const Vector3 getTranslation( ) const;

    // Set column 0 of a 3x4 transformation matrix
    // 
    inline Transform3 & setCol0( const Vector3 & col0 );

    // Set column 1 of a 3x4 transformation matrix
    // 
    inline Transform3 & setCol1( const Vector3 & col1 );

    // Set column 2 of a 3x4 transformation matrix
    // 
    inline Transform3 & setCol2( const Vector3 & col2 );

    // Set column 3 of a 3x4 transformation matrix
    // 
    inline Transform3 & setCol3( const Vector3 & col3 );

    // Get column 0 of a 3x4 transformation matrix
    // 
    inline const Vector3 getCol0( ) const;

    // Get column 1 of a 3x4 transformation matrix
    // 
    inline const Vector3 getCol1( ) const;

    // Get column 2 of a 3x4 transformation matrix
    // 
    inline const Vector3 getCol2( ) const;

    // Get column 3 of a 3x4 transformation matrix
    // 
    inline const Vector3 getCol3( ) const;

    // Set the column of a 3x4 transformation matrix referred to by the specified index
    // 
    inline Transform3 & setCol( int col, const Vector3 & vec );

    // Set the row of a 3x4 transformation matrix referred to by the specified index
    // 
    inline Transform3 & setRow( int row, const Vector4 & vec );

    // Get the column of a 3x4 transformation matrix referred to by the specified index
    // 
    inline const Vector3 getCol( int col ) const;

    // Get the row of a 3x4 transformation matrix referred to by the specified index
    // 
    inline const Vector4 getRow( int row ) const;

    // Subscripting operator to set or get a column
    // 
    inline Vector3 & operator []( int col );

    // Subscripting operator to get a column
    // 
    inline const Vector3 operator []( int col ) const;

    // Set the element of a 3x4 transformation matrix referred to by column and row indices
    // 
    inline Transform3 & setElem( int col, int row, float val );

    // Get the element of a 3x4 transformation matrix referred to by column and row indices
    // 
    inline float getElem( int col, int row ) const;

    // Multiply a 3x4 transformation matrix by a 3-D vector
    // 
    inline const Vector3 operator *( const Vector3 & vec ) const;

    // Multiply a 3x4 transformation matrix by a 3-D point
    // 
    inline const Point3 operator *( const Point3 & pnt ) const;

    // Multiply two 3x4 transformation matrices
    // 
    inline const Transform3 operator *( const Transform3 & tfrm ) const;

    // Perform compound assignment and multiplication by a 3x4 transformation matrix
    // 
    inline Transform3 & operator *=( const Transform3 & tfrm );

    // Construct an identity 3x4 transformation matrix
    // 
    static inline const Transform3 identity( );

    // Construct a 3x4 transformation matrix to rotate around the x axis
    // 
    static inline const Transform3 rotationX( float radians );

    // Construct a 3x4 transformation matrix to rotate around the y axis
    // 
    static inline const Transform3 rotationY( float radians );

    // Construct a 3x4 transformation matrix to rotate around the z axis
    // 
    static inline const Transform3 rotationZ( float radians );

    // Construct a 3x4 transformation matrix to rotate around the x, y, and z axes
    // 
    static inline const Transform3 rotationZYX( const Vector3 & radiansXYZ );

    // Construct a 3x4 transformation matrix to rotate around a unit-length 3-D vector
    // 
    static inline const Transform3 rotation( float radians, const Vector3 & unitVec );

    // Construct a rotation matrix from a unit-length quaternion
    // 
    static inline const Transform3 rotation( const Quat & unitQuat );

    // Construct a 3x4 transformation matrix to perform scaling
    // 
    static inline const Transform3 scale( const Vector3 & scaleVec );

    // Construct a 3x4 transformation matrix to perform translation
    // 
    static inline const Transform3 translation( const Vector3 & translateVec );

};
// Append (post-multiply) a scale transformation to a 3x4 transformation matrix
// NOTE: 
// Faster than creating and multiplying a scale transformation matrix.
// 
inline const Transform3 appendScale( const Transform3 & tfrm, const Vector3 & scaleVec );

// Prepend (pre-multiply) a scale transformation to a 3x4 transformation matrix
// NOTE: 
// Faster than creating and multiplying a scale transformation matrix.
// 
inline const Transform3 prependScale( const Vector3 & scaleVec, const Transform3 & tfrm );

// Multiply two 3x4 transformation matrices per element
// 
inline const Transform3 mulPerElem( const Transform3 & tfrm0, const Transform3 & tfrm1 );

// Compute the absolute value of a 3x4 transformation matrix per element
// 
inline const Transform3 absPerElem( const Transform3 & tfrm );

// Inverse of a 3x4 transformation matrix
// NOTE: 
// Result is unpredictable when the determinant of the left 3x3 submatrix is equal to or near 0.
// 
inline const Transform3 inverse( const Transform3 & tfrm );

// Compute the inverse of a 3x4 transformation matrix, expected to have an orthogonal upper-left 3x3 submatrix
// NOTE: 
// This can be used to achieve better performance than a general inverse when the specified 3x4 transformation matrix meets the given restrictions.
// 
inline const Transform3 orthoInverse( const Transform3 & tfrm );

// Conditionally select between two 3x4 transformation matrices
// 
inline const Transform3 select( const Transform3 & tfrm0, const Transform3 & tfrm1, bool select1 );

#ifdef _VECTORMATH_DEBUG

// Print a 3x4 transformation matrix
// NOTE: 
// Function is only defined when _VECTORMATH_DEBUG is defined.
// 
inline void print( const Transform3 & tfrm );

// Print a 3x4 transformation matrix and an associated string identifier
// NOTE: 
// Function is only defined when _VECTORMATH_DEBUG is defined.
// 
inline void print( const Transform3 & tfrm, const char * name );

#endif

} // namespace Aos
} // namespace Vectormath

/*
   Copyright (C) 2006, 2007 Sony Computer Entertainment Inc.
   All rights reserved.

   Redistribution and use in source and binary forms,
   with or without modification, are permitted provided that the
   following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Sony Computer Entertainment Inc nor the names
      of its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _VECTORMATH_VEC_AOS_CPP_H
#define _VECTORMATH_VEC_AOS_CPP_H
//-----------------------------------------------------------------------------
// Constants

#define _VECTORMATH_SLERP_TOL 0.999f

//-----------------------------------------------------------------------------
// Definitions

#ifndef _VECTORMATH_INTERNAL_FUNCTIONS
#define _VECTORMATH_INTERNAL_FUNCTIONS

#endif

namespace Vectormath {
namespace Aos {

inline Vector3::Vector3( const Vector3 & vec )
{
    mX = vec.mX;
    mY = vec.mY;
    mZ = vec.mZ;
}

inline Vector3::Vector3( float _x, float _y, float _z )
{
    mX = _x;
    mY = _y;
    mZ = _z;
}

inline Vector3::Vector3( const Point3 & pnt )
{
    mX = pnt.getX();
    mY = pnt.getY();
    mZ = pnt.getZ();
}

inline Vector3::Vector3( float scalar )
{
    mX = scalar;
    mY = scalar;
    mZ = scalar;
}

inline const Vector3 Vector3::xAxis( )
{
    return Vector3( 1.0f, 0.0f, 0.0f );
}

inline const Vector3 Vector3::yAxis( )
{
    return Vector3( 0.0f, 1.0f, 0.0f );
}

inline const Vector3 Vector3::zAxis( )
{
    return Vector3( 0.0f, 0.0f, 1.0f );
}

inline const Vector3 lerp( float t, const Vector3 & vec0, const Vector3 & vec1 )
{
    return ( vec0 + ( ( vec1 - vec0 ) * t ) );
}

inline const Vector3 slerp( float t, const Vector3 & unitVec0, const Vector3 & unitVec1 )
{
    float recipSinAngle, scale0, scale1, cosAngle, angle;
    cosAngle = dot( unitVec0, unitVec1 );
    if ( cosAngle < _VECTORMATH_SLERP_TOL ) {
        angle = acosf( cosAngle );
        recipSinAngle = ( 1.0f / sinf( angle ) );
        scale0 = ( sinf( ( ( 1.0f - t ) * angle ) ) * recipSinAngle );
        scale1 = ( sinf( ( t * angle ) ) * recipSinAngle );
    } else {
        scale0 = ( 1.0f - t );
        scale1 = t;
    }
    return ( ( unitVec0 * scale0 ) + ( unitVec1 * scale1 ) );
}

inline Vector3 & Vector3::operator =( const Vector3 & vec )
{
    mX = vec.mX;
    mY = vec.mY;
    mZ = vec.mZ;
    return *this;
}

inline Vector3 & Vector3::setX( float _x )
{
    mX = _x;
    return *this;
}

inline float Vector3::getX( ) const
{
    return mX;
}

inline Vector3 & Vector3::setY( float _y )
{
    mY = _y;
    return *this;
}

inline float Vector3::getY( ) const
{
    return mY;
}

inline Vector3 & Vector3::setZ( float _z )
{
    mZ = _z;
    return *this;
}

inline float Vector3::getZ( ) const
{
    return mZ;
}

inline Vector3 & Vector3::setElem( int idx, float value )
{
    *(&mX + idx) = value;
    return *this;
}

inline float Vector3::getElem( int idx ) const
{
    return *(&mX + idx);
}

inline float & Vector3::operator []( int idx )
{
    return *(&mX + idx);
}

inline float Vector3::operator []( int idx ) const
{
    return *(&mX + idx);
}

inline const Vector3 Vector3::operator +( const Vector3 & vec ) const
{
    return Vector3(
        ( mX + vec.mX ),
        ( mY + vec.mY ),
        ( mZ + vec.mZ )
    );
}

inline const Vector3 Vector3::operator -( const Vector3 & vec ) const
{
    return Vector3(
        ( mX - vec.mX ),
        ( mY - vec.mY ),
        ( mZ - vec.mZ )
    );
}

inline const Point3 Vector3::operator +( const Point3 & pnt ) const
{
    return Point3(
        ( mX + pnt.getX() ),
        ( mY + pnt.getY() ),
        ( mZ + pnt.getZ() )
    );
}

inline const Vector3 Vector3::operator *( float scalar ) const
{
    return Vector3(
        ( mX * scalar ),
        ( mY * scalar ),
        ( mZ * scalar )
    );
}

inline Vector3 & Vector3::operator +=( const Vector3 & vec )
{
    *this = *this + vec;
    return *this;
}

inline Vector3 & Vector3::operator -=( const Vector3 & vec )
{
    *this = *this - vec;
    return *this;
}

inline Vector3 & Vector3::operator *=( float scalar )
{
    *this = *this * scalar;
    return *this;
}

inline const Vector3 Vector3::operator /( float scalar ) const
{
    return Vector3(
        ( mX / scalar ),
        ( mY / scalar ),
        ( mZ / scalar )
    );
}

inline Vector3 & Vector3::operator /=( float scalar )
{
    *this = *this / scalar;
    return *this;
}

inline const Vector3 Vector3::operator -( ) const
{
    return Vector3(
        -mX,
        -mY,
        -mZ
    );
}

inline const Vector3 operator *( float scalar, const Vector3 & vec )
{
    return vec * scalar;
}

inline const Vector3 mulPerElem( const Vector3 & vec0, const Vector3 & vec1 )
{
    return Vector3(
        ( vec0.getX() * vec1.getX() ),
        ( vec0.getY() * vec1.getY() ),
        ( vec0.getZ() * vec1.getZ() )
    );
}

inline const Vector3 divPerElem( const Vector3 & vec0, const Vector3 & vec1 )
{
    return Vector3(
        ( vec0.getX() / vec1.getX() ),
        ( vec0.getY() / vec1.getY() ),
        ( vec0.getZ() / vec1.getZ() )
    );
}

inline const Vector3 recipPerElem( const Vector3 & vec )
{
    return Vector3(
        ( 1.0f / vec.getX() ),
        ( 1.0f / vec.getY() ),
        ( 1.0f / vec.getZ() )
    );
}

inline const Vector3 sqrtPerElem( const Vector3 & vec )
{
    return Vector3(
        sqrtf( vec.getX() ),
        sqrtf( vec.getY() ),
        sqrtf( vec.getZ() )
    );
}

inline const Vector3 rsqrtPerElem( const Vector3 & vec )
{
    return Vector3(
        ( 1.0f / sqrtf( vec.getX() ) ),
        ( 1.0f / sqrtf( vec.getY() ) ),
        ( 1.0f / sqrtf( vec.getZ() ) )
    );
}

inline const Vector3 absPerElem( const Vector3 & vec )
{
    return Vector3(
        fabsf( vec.getX() ),
        fabsf( vec.getY() ),
        fabsf( vec.getZ() )
    );
}

inline const Vector3 copySignPerElem( const Vector3 & vec0, const Vector3 & vec1 )
{
    return Vector3(
        ( vec1.getX() < 0.0f )? -fabsf( vec0.getX() ) : fabsf( vec0.getX() ),
        ( vec1.getY() < 0.0f )? -fabsf( vec0.getY() ) : fabsf( vec0.getY() ),
        ( vec1.getZ() < 0.0f )? -fabsf( vec0.getZ() ) : fabsf( vec0.getZ() )
    );
}

inline const Vector3 maxPerElem( const Vector3 & vec0, const Vector3 & vec1 )
{
    return Vector3(
        (vec0.getX() > vec1.getX())? vec0.getX() : vec1.getX(),
        (vec0.getY() > vec1.getY())? vec0.getY() : vec1.getY(),
        (vec0.getZ() > vec1.getZ())? vec0.getZ() : vec1.getZ()
    );
}

inline float maxElem( const Vector3 & vec )
{
    float result;
    result = (vec.getX() > vec.getY())? vec.getX() : vec.getY();
    result = (vec.getZ() > result)? vec.getZ() : result;
    return result;
}

inline const Vector3 minPerElem( const Vector3 & vec0, const Vector3 & vec1 )
{
    return Vector3(
        (vec0.getX() < vec1.getX())? vec0.getX() : vec1.getX(),
        (vec0.getY() < vec1.getY())? vec0.getY() : vec1.getY(),
        (vec0.getZ() < vec1.getZ())? vec0.getZ() : vec1.getZ()
    );
}

inline float minElem( const Vector3 & vec )
{
    float result;
    result = (vec.getX() < vec.getY())? vec.getX() : vec.getY();
    result = (vec.getZ() < result)? vec.getZ() : result;
    return result;
}

inline float sum( const Vector3 & vec )
{
    float result;
    result = ( vec.getX() + vec.getY() );
    result = ( result + vec.getZ() );
    return result;
}

inline float dot( const Vector3 & vec0, const Vector3 & vec1 )
{
    float result;
    result = ( vec0.getX() * vec1.getX() );
    result = ( result + ( vec0.getY() * vec1.getY() ) );
    result = ( result + ( vec0.getZ() * vec1.getZ() ) );
    return result;
}

inline float lengthSqr( const Vector3 & vec )
{
    float result;
    result = ( vec.getX() * vec.getX() );
    result = ( result + ( vec.getY() * vec.getY() ) );
    result = ( result + ( vec.getZ() * vec.getZ() ) );
    return result;
}

inline float length( const Vector3 & vec )
{
    return sqrtf( lengthSqr( vec ) );
}

inline const Vector3 normalize( const Vector3 & vec )
{
    float lenSqr, lenInv;
    lenSqr = lengthSqr( vec );
    lenInv = ( 1.0f / sqrtf( lenSqr ) );
    return Vector3(
        ( vec.getX() * lenInv ),
        ( vec.getY() * lenInv ),
        ( vec.getZ() * lenInv )
    );
}

inline const Vector3 cross( const Vector3 & vec0, const Vector3 & vec1 )
{
    return Vector3(
        ( ( vec0.getY() * vec1.getZ() ) - ( vec0.getZ() * vec1.getY() ) ),
        ( ( vec0.getZ() * vec1.getX() ) - ( vec0.getX() * vec1.getZ() ) ),
        ( ( vec0.getX() * vec1.getY() ) - ( vec0.getY() * vec1.getX() ) )
    );
}

inline const Vector3 select( const Vector3 & vec0, const Vector3 & vec1, bool select1 )
{
    return Vector3(
        ( select1 )? vec1.getX() : vec0.getX(),
        ( select1 )? vec1.getY() : vec0.getY(),
        ( select1 )? vec1.getZ() : vec0.getZ()
    );
}

#ifdef _VECTORMATH_DEBUG

inline void print( const Vector3 & vec )
{
    printf( "( %f %f %f )\n", vec.getX(), vec.getY(), vec.getZ() );
}

inline void print( const Vector3 & vec, const char * name )
{
    printf( "%s: ( %f %f %f )\n", name, vec.getX(), vec.getY(), vec.getZ() );
}

#endif

inline Vector4::Vector4( const Vector4 & vec )
{
    mX = vec.mX;
    mY = vec.mY;
    mZ = vec.mZ;
    mW = vec.mW;
}

inline Vector4::Vector4( float _x, float _y, float _z, float _w )
{
    mX = _x;
    mY = _y;
    mZ = _z;
    mW = _w;
}

inline Vector4::Vector4( const Vector3 & xyz, float _w )
{
    this->setXYZ( xyz );
    this->setW( _w );
}

inline Vector4::Vector4( const Vector3 & vec )
{
    mX = vec.getX();
    mY = vec.getY();
    mZ = vec.getZ();
    mW = 0.0f;
}

inline Vector4::Vector4( const Point3 & pnt )
{
    mX = pnt.getX();
    mY = pnt.getY();
    mZ = pnt.getZ();
    mW = 1.0f;
}

inline Vector4::Vector4( const Quat & quat )
{
    mX = quat.getX();
    mY = quat.getY();
    mZ = quat.getZ();
    mW = quat.getW();
}

inline Vector4::Vector4( float scalar )
{
    mX = scalar;
    mY = scalar;
    mZ = scalar;
    mW = scalar;
}

inline const Vector4 Vector4::xAxis( )
{
    return Vector4( 1.0f, 0.0f, 0.0f, 0.0f );
}

inline const Vector4 Vector4::yAxis( )
{
    return Vector4( 0.0f, 1.0f, 0.0f, 0.0f );
}

inline const Vector4 Vector4::zAxis( )
{
    return Vector4( 0.0f, 0.0f, 1.0f, 0.0f );
}

inline const Vector4 Vector4::wAxis( )
{
    return Vector4( 0.0f, 0.0f, 0.0f, 1.0f );
}

inline const Vector4 lerp( float t, const Vector4 & vec0, const Vector4 & vec1 )
{
    return ( vec0 + ( ( vec1 - vec0 ) * t ) );
}

inline const Vector4 slerp( float t, const Vector4 & unitVec0, const Vector4 & unitVec1 )
{
    float recipSinAngle, scale0, scale1, cosAngle, angle;
    cosAngle = dot( unitVec0, unitVec1 );
    if ( cosAngle < _VECTORMATH_SLERP_TOL ) {
        angle = acosf( cosAngle );
        recipSinAngle = ( 1.0f / sinf( angle ) );
        scale0 = ( sinf( ( ( 1.0f - t ) * angle ) ) * recipSinAngle );
        scale1 = ( sinf( ( t * angle ) ) * recipSinAngle );
    } else {
        scale0 = ( 1.0f - t );
        scale1 = t;
    }
    return ( ( unitVec0 * scale0 ) + ( unitVec1 * scale1 ) );
}

inline Vector4 & Vector4::operator =( const Vector4 & vec )
{
    mX = vec.mX;
    mY = vec.mY;
    mZ = vec.mZ;
    mW = vec.mW;
    return *this;
}

inline Vector4 & Vector4::setXYZ( const Vector3 & vec )
{
    mX = vec.getX();
    mY = vec.getY();
    mZ = vec.getZ();
    return *this;
}

inline const Vector3 Vector4::getXYZ( ) const
{
    return Vector3( mX, mY, mZ );
}

inline Vector4 & Vector4::setX( float _x )
{
    mX = _x;
    return *this;
}

inline float Vector4::getX( ) const
{
    return mX;
}

inline Vector4 & Vector4::setY( float _y )
{
    mY = _y;
    return *this;
}

inline float Vector4::getY( ) const
{
    return mY;
}

inline Vector4 & Vector4::setZ( float _z )
{
    mZ = _z;
    return *this;
}

inline float Vector4::getZ( ) const
{
    return mZ;
}

inline Vector4 & Vector4::setW( float _w )
{
    mW = _w;
    return *this;
}

inline float Vector4::getW( ) const
{
    return mW;
}

inline Vector4 & Vector4::setElem( int idx, float value )
{
    *(&mX + idx) = value;
    return *this;
}

inline float Vector4::getElem( int idx ) const
{
    return *(&mX + idx);
}

inline float & Vector4::operator []( int idx )
{
    return *(&mX + idx);
}

inline float Vector4::operator []( int idx ) const
{
    return *(&mX + idx);
}

inline const Vector4 Vector4::operator +( const Vector4 & vec ) const
{
    return Vector4(
        ( mX + vec.mX ),
        ( mY + vec.mY ),
        ( mZ + vec.mZ ),
        ( mW + vec.mW )
    );
}

inline const Vector4 Vector4::operator -( const Vector4 & vec ) const
{
    return Vector4(
        ( mX - vec.mX ),
        ( mY - vec.mY ),
        ( mZ - vec.mZ ),
        ( mW - vec.mW )
    );
}

inline const Vector4 Vector4::operator *( float scalar ) const
{
    return Vector4(
        ( mX * scalar ),
        ( mY * scalar ),
        ( mZ * scalar ),
        ( mW * scalar )
    );
}

inline Vector4 & Vector4::operator +=( const Vector4 & vec )
{
    *this = *this + vec;
    return *this;
}

inline Vector4 & Vector4::operator -=( const Vector4 & vec )
{
    *this = *this - vec;
    return *this;
}

inline Vector4 & Vector4::operator *=( float scalar )
{
    *this = *this * scalar;
    return *this;
}

inline const Vector4 Vector4::operator /( float scalar ) const
{
    return Vector4(
        ( mX / scalar ),
        ( mY / scalar ),
        ( mZ / scalar ),
        ( mW / scalar )
    );
}

inline Vector4 & Vector4::operator /=( float scalar )
{
    *this = *this / scalar;
    return *this;
}

inline const Vector4 Vector4::operator -( ) const
{
    return Vector4(
        -mX,
        -mY,
        -mZ,
        -mW
    );
}

inline const Vector4 operator *( float scalar, const Vector4 & vec )
{
    return vec * scalar;
}

inline const Vector4 mulPerElem( const Vector4 & vec0, const Vector4 & vec1 )
{
    return Vector4(
        ( vec0.getX() * vec1.getX() ),
        ( vec0.getY() * vec1.getY() ),
        ( vec0.getZ() * vec1.getZ() ),
        ( vec0.getW() * vec1.getW() )
    );
}

inline const Vector4 divPerElem( const Vector4 & vec0, const Vector4 & vec1 )
{
    return Vector4(
        ( vec0.getX() / vec1.getX() ),
        ( vec0.getY() / vec1.getY() ),
        ( vec0.getZ() / vec1.getZ() ),
        ( vec0.getW() / vec1.getW() )
    );
}

inline const Vector4 recipPerElem( const Vector4 & vec )
{
    return Vector4(
        ( 1.0f / vec.getX() ),
        ( 1.0f / vec.getY() ),
        ( 1.0f / vec.getZ() ),
        ( 1.0f / vec.getW() )
    );
}

inline const Vector4 sqrtPerElem( const Vector4 & vec )
{
    return Vector4(
        sqrtf( vec.getX() ),
        sqrtf( vec.getY() ),
        sqrtf( vec.getZ() ),
        sqrtf( vec.getW() )
    );
}

inline const Vector4 rsqrtPerElem( const Vector4 & vec )
{
    return Vector4(
        ( 1.0f / sqrtf( vec.getX() ) ),
        ( 1.0f / sqrtf( vec.getY() ) ),
        ( 1.0f / sqrtf( vec.getZ() ) ),
        ( 1.0f / sqrtf( vec.getW() ) )
    );
}

inline const Vector4 absPerElem( const Vector4 & vec )
{
    return Vector4(
        fabsf( vec.getX() ),
        fabsf( vec.getY() ),
        fabsf( vec.getZ() ),
        fabsf( vec.getW() )
    );
}

inline const Vector4 copySignPerElem( const Vector4 & vec0, const Vector4 & vec1 )
{
    return Vector4(
        ( vec1.getX() < 0.0f )? -fabsf( vec0.getX() ) : fabsf( vec0.getX() ),
        ( vec1.getY() < 0.0f )? -fabsf( vec0.getY() ) : fabsf( vec0.getY() ),
        ( vec1.getZ() < 0.0f )? -fabsf( vec0.getZ() ) : fabsf( vec0.getZ() ),
        ( vec1.getW() < 0.0f )? -fabsf( vec0.getW() ) : fabsf( vec0.getW() )
    );
}

inline const Vector4 maxPerElem( const Vector4 & vec0, const Vector4 & vec1 )
{
    return Vector4(
        (vec0.getX() > vec1.getX())? vec0.getX() : vec1.getX(),
        (vec0.getY() > vec1.getY())? vec0.getY() : vec1.getY(),
        (vec0.getZ() > vec1.getZ())? vec0.getZ() : vec1.getZ(),
        (vec0.getW() > vec1.getW())? vec0.getW() : vec1.getW()
    );
}

inline float maxElem( const Vector4 & vec )
{
    float result;
    result = (vec.getX() > vec.getY())? vec.getX() : vec.getY();
    result = (vec.getZ() > result)? vec.getZ() : result;
    result = (vec.getW() > result)? vec.getW() : result;
    return result;
}

inline const Vector4 minPerElem( const Vector4 & vec0, const Vector4 & vec1 )
{
    return Vector4(
        (vec0.getX() < vec1.getX())? vec0.getX() : vec1.getX(),
        (vec0.getY() < vec1.getY())? vec0.getY() : vec1.getY(),
        (vec0.getZ() < vec1.getZ())? vec0.getZ() : vec1.getZ(),
        (vec0.getW() < vec1.getW())? vec0.getW() : vec1.getW()
    );
}

inline float minElem( const Vector4 & vec )
{
    float result;
    result = (vec.getX() < vec.getY())? vec.getX() : vec.getY();
    result = (vec.getZ() < result)? vec.getZ() : result;
    result = (vec.getW() < result)? vec.getW() : result;
    return result;
}

inline float sum( const Vector4 & vec )
{
    float result;
    result = ( vec.getX() + vec.getY() );
    result = ( result + vec.getZ() );
    result = ( result + vec.getW() );
    return result;
}

inline float dot( const Vector4 & vec0, const Vector4 & vec1 )
{
    float result;
    result = ( vec0.getX() * vec1.getX() );
    result = ( result + ( vec0.getY() * vec1.getY() ) );
    result = ( result + ( vec0.getZ() * vec1.getZ() ) );
    result = ( result + ( vec0.getW() * vec1.getW() ) );
    return result;
}

inline float lengthSqr( const Vector4 & vec )
{
    float result;
    result = ( vec.getX() * vec.getX() );
    result = ( result + ( vec.getY() * vec.getY() ) );
    result = ( result + ( vec.getZ() * vec.getZ() ) );
    result = ( result + ( vec.getW() * vec.getW() ) );
    return result;
}

inline float length( const Vector4 & vec )
{
    return sqrtf( lengthSqr( vec ) );
}

inline const Vector4 normalize( const Vector4 & vec )
{
    float lenSqr, lenInv;
    lenSqr = lengthSqr( vec );
    lenInv = ( 1.0f / sqrtf( lenSqr ) );
    return Vector4(
        ( vec.getX() * lenInv ),
        ( vec.getY() * lenInv ),
        ( vec.getZ() * lenInv ),
        ( vec.getW() * lenInv )
    );
}

inline const Vector4 select( const Vector4 & vec0, const Vector4 & vec1, bool select1 )
{
    return Vector4(
        ( select1 )? vec1.getX() : vec0.getX(),
        ( select1 )? vec1.getY() : vec0.getY(),
        ( select1 )? vec1.getZ() : vec0.getZ(),
        ( select1 )? vec1.getW() : vec0.getW()
    );
}

#ifdef _VECTORMATH_DEBUG

inline void print( const Vector4 & vec )
{
    printf( "( %f %f %f %f )\n", vec.getX(), vec.getY(), vec.getZ(), vec.getW() );
}

inline void print( const Vector4 & vec, const char * name )
{
    printf( "%s: ( %f %f %f %f )\n", name, vec.getX(), vec.getY(), vec.getZ(), vec.getW() );
}

#endif

inline Point3::Point3( const Point3 & pnt )
{
    mX = pnt.mX;
    mY = pnt.mY;
    mZ = pnt.mZ;
}

inline Point3::Point3( float _x, float _y, float _z )
{
    mX = _x;
    mY = _y;
    mZ = _z;
}

inline Point3::Point3( const Vector3 & vec )
{
    mX = vec.getX();
    mY = vec.getY();
    mZ = vec.getZ();
}

inline Point3::Point3( float scalar )
{
    mX = scalar;
    mY = scalar;
    mZ = scalar;
}

inline const Point3 lerp( float t, const Point3 & pnt0, const Point3 & pnt1 )
{
    return ( pnt0 + ( ( pnt1 - pnt0 ) * t ) );
}

inline Point3 & Point3::operator =( const Point3 & pnt )
{
    mX = pnt.mX;
    mY = pnt.mY;
    mZ = pnt.mZ;
    return *this;
}

inline Point3 & Point3::setX( float _x )
{
    mX = _x;
    return *this;
}

inline float Point3::getX( ) const
{
    return mX;
}

inline Point3 & Point3::setY( float _y )
{
    mY = _y;
    return *this;
}

inline float Point3::getY( ) const
{
    return mY;
}

inline Point3 & Point3::setZ( float _z )
{
    mZ = _z;
    return *this;
}

inline float Point3::getZ( ) const
{
    return mZ;
}

inline Point3 & Point3::setElem( int idx, float value )
{
    *(&mX + idx) = value;
    return *this;
}

inline float Point3::getElem( int idx ) const
{
    return *(&mX + idx);
}

inline float & Point3::operator []( int idx )
{
    return *(&mX + idx);
}

inline float Point3::operator []( int idx ) const
{
    return *(&mX + idx);
}

inline const Vector3 Point3::operator -( const Point3 & pnt ) const
{
    return Vector3(
        ( mX - pnt.mX ),
        ( mY - pnt.mY ),
        ( mZ - pnt.mZ )
    );
}

inline const Point3 Point3::operator +( const Vector3 & vec ) const
{
    return Point3(
        ( mX + vec.getX() ),
        ( mY + vec.getY() ),
        ( mZ + vec.getZ() )
    );
}

inline const Point3 Point3::operator -( const Vector3 & vec ) const
{
    return Point3(
        ( mX - vec.getX() ),
        ( mY - vec.getY() ),
        ( mZ - vec.getZ() )
    );
}

inline Point3 & Point3::operator +=( const Vector3 & vec )
{
    *this = *this + vec;
    return *this;
}

inline Point3 & Point3::operator -=( const Vector3 & vec )
{
    *this = *this - vec;
    return *this;
}

inline const Point3 mulPerElem( const Point3 & pnt0, const Point3 & pnt1 )
{
    return Point3(
        ( pnt0.getX() * pnt1.getX() ),
        ( pnt0.getY() * pnt1.getY() ),
        ( pnt0.getZ() * pnt1.getZ() )
    );
}

inline const Point3 divPerElem( const Point3 & pnt0, const Point3 & pnt1 )
{
    return Point3(
        ( pnt0.getX() / pnt1.getX() ),
        ( pnt0.getY() / pnt1.getY() ),
        ( pnt0.getZ() / pnt1.getZ() )
    );
}

inline const Point3 recipPerElem( const Point3 & pnt )
{
    return Point3(
        ( 1.0f / pnt.getX() ),
        ( 1.0f / pnt.getY() ),
        ( 1.0f / pnt.getZ() )
    );
}

inline const Point3 sqrtPerElem( const Point3 & pnt )
{
    return Point3(
        sqrtf( pnt.getX() ),
        sqrtf( pnt.getY() ),
        sqrtf( pnt.getZ() )
    );
}

inline const Point3 rsqrtPerElem( const Point3 & pnt )
{
    return Point3(
        ( 1.0f / sqrtf( pnt.getX() ) ),
        ( 1.0f / sqrtf( pnt.getY() ) ),
        ( 1.0f / sqrtf( pnt.getZ() ) )
    );
}

inline const Point3 absPerElem( const Point3 & pnt )
{
    return Point3(
        fabsf( pnt.getX() ),
        fabsf( pnt.getY() ),
        fabsf( pnt.getZ() )
    );
}

inline const Point3 copySignPerElem( const Point3 & pnt0, const Point3 & pnt1 )
{
    return Point3(
        ( pnt1.getX() < 0.0f )? -fabsf( pnt0.getX() ) : fabsf( pnt0.getX() ),
        ( pnt1.getY() < 0.0f )? -fabsf( pnt0.getY() ) : fabsf( pnt0.getY() ),
        ( pnt1.getZ() < 0.0f )? -fabsf( pnt0.getZ() ) : fabsf( pnt0.getZ() )
    );
}

inline const Point3 maxPerElem( const Point3 & pnt0, const Point3 & pnt1 )
{
    return Point3(
        (pnt0.getX() > pnt1.getX())? pnt0.getX() : pnt1.getX(),
        (pnt0.getY() > pnt1.getY())? pnt0.getY() : pnt1.getY(),
        (pnt0.getZ() > pnt1.getZ())? pnt0.getZ() : pnt1.getZ()
    );
}

inline float maxElem( const Point3 & pnt )
{
    float result;
    result = (pnt.getX() > pnt.getY())? pnt.getX() : pnt.getY();
    result = (pnt.getZ() > result)? pnt.getZ() : result;
    return result;
}

inline const Point3 minPerElem( const Point3 & pnt0, const Point3 & pnt1 )
{
    return Point3(
        (pnt0.getX() < pnt1.getX())? pnt0.getX() : pnt1.getX(),
        (pnt0.getY() < pnt1.getY())? pnt0.getY() : pnt1.getY(),
        (pnt0.getZ() < pnt1.getZ())? pnt0.getZ() : pnt1.getZ()
    );
}

inline float minElem( const Point3 & pnt )
{
    float result;
    result = (pnt.getX() < pnt.getY())? pnt.getX() : pnt.getY();
    result = (pnt.getZ() < result)? pnt.getZ() : result;
    return result;
}

inline float sum( const Point3 & pnt )
{
    float result;
    result = ( pnt.getX() + pnt.getY() );
    result = ( result + pnt.getZ() );
    return result;
}

inline const Point3 scale( const Point3 & pnt, float scaleVal )
{
    return mulPerElem( pnt, Point3( scaleVal ) );
}

inline const Point3 scale( const Point3 & pnt, const Vector3 & scaleVec )
{
    return mulPerElem( pnt, Point3( scaleVec ) );
}

inline float projection( const Point3 & pnt, const Vector3 & unitVec )
{
    float result;
    result = ( pnt.getX() * unitVec.getX() );
    result = ( result + ( pnt.getY() * unitVec.getY() ) );
    result = ( result + ( pnt.getZ() * unitVec.getZ() ) );
    return result;
}

inline float distSqrFromOrigin( const Point3 & pnt )
{
    return lengthSqr( Vector3( pnt ) );
}

inline float distFromOrigin( const Point3 & pnt )
{
    return length( Vector3( pnt ) );
}

inline float distSqr( const Point3 & pnt0, const Point3 & pnt1 )
{
    return lengthSqr( ( pnt1 - pnt0 ) );
}

inline float dist( const Point3 & pnt0, const Point3 & pnt1 )
{
    return length( ( pnt1 - pnt0 ) );
}

inline const Point3 select( const Point3 & pnt0, const Point3 & pnt1, bool select1 )
{
    return Point3(
        ( select1 )? pnt1.getX() : pnt0.getX(),
        ( select1 )? pnt1.getY() : pnt0.getY(),
        ( select1 )? pnt1.getZ() : pnt0.getZ()
    );
}

#ifdef _VECTORMATH_DEBUG

inline void print( const Point3 & pnt )
{
    printf( "( %f %f %f )\n", pnt.getX(), pnt.getY(), pnt.getZ() );
}

inline void print( const Point3 & pnt, const char * name )
{
    printf( "%s: ( %f %f %f )\n", name, pnt.getX(), pnt.getY(), pnt.getZ() );
}

#endif

} // namespace Aos
} // namespace Vectormath

#endif

/*
   Copyright (C) 2006, 2007 Sony Computer Entertainment Inc.
   All rights reserved.

   Redistribution and use in source and binary forms,
   with or without modification, are permitted provided that the
   following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Sony Computer Entertainment Inc nor the names
      of its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _VECTORMATH_QUAT_AOS_CPP_H
#define _VECTORMATH_QUAT_AOS_CPP_H
//-----------------------------------------------------------------------------
// Definitions

#ifndef _VECTORMATH_INTERNAL_FUNCTIONS
#define _VECTORMATH_INTERNAL_FUNCTIONS

#endif

namespace Vectormath {
namespace Aos {

inline Quat::Quat( const Quat & quat )
{
    mX = quat.mX;
    mY = quat.mY;
    mZ = quat.mZ;
    mW = quat.mW;
}

inline Quat::Quat( float _x, float _y, float _z, float _w )
{
    mX = _x;
    mY = _y;
    mZ = _z;
    mW = _w;
}

inline Quat::Quat( const Vector3 & xyz, float _w )
{
    this->setXYZ( xyz );
    this->setW( _w );
}

inline Quat::Quat( const Vector4 & vec )
{
    mX = vec.getX();
    mY = vec.getY();
    mZ = vec.getZ();
    mW = vec.getW();
}

inline Quat::Quat( float scalar )
{
    mX = scalar;
    mY = scalar;
    mZ = scalar;
    mW = scalar;
}

inline const Quat Quat::identity( )
{
    return Quat( 0.0f, 0.0f, 0.0f, 1.0f );
}

inline const Quat lerp( float t, const Quat & quat0, const Quat & quat1 )
{
    return ( quat0 + ( ( quat1 - quat0 ) * t ) );
}

inline const Quat slerp( float t, const Quat & unitQuat0, const Quat & unitQuat1 )
{
    Quat start;
    float recipSinAngle, scale0, scale1, cosAngle, angle;
    cosAngle = dot( unitQuat0, unitQuat1 );
    if ( cosAngle < 0.0f ) {
        cosAngle = -cosAngle;
        start = ( -unitQuat0 );
    } else {
        start = unitQuat0;
    }
    if ( cosAngle < _VECTORMATH_SLERP_TOL ) {
        angle = acosf( cosAngle );
        recipSinAngle = ( 1.0f / sinf( angle ) );
        scale0 = ( sinf( ( ( 1.0f - t ) * angle ) ) * recipSinAngle );
        scale1 = ( sinf( ( t * angle ) ) * recipSinAngle );
    } else {
        scale0 = ( 1.0f - t );
        scale1 = t;
    }
    return ( ( start * scale0 ) + ( unitQuat1 * scale1 ) );
}

inline const Quat squad( float t, const Quat & unitQuat0, const Quat & unitQuat1, const Quat & unitQuat2, const Quat & unitQuat3 )
{
    Quat tmp0, tmp1;
    tmp0 = slerp( t, unitQuat0, unitQuat3 );
    tmp1 = slerp( t, unitQuat1, unitQuat2 );
    return slerp( ( ( 2.0f * t ) * ( 1.0f - t ) ), tmp0, tmp1 );
}

inline Quat & Quat::operator =( const Quat & quat )
{
    mX = quat.mX;
    mY = quat.mY;
    mZ = quat.mZ;
    mW = quat.mW;
    return *this;
}

inline Quat & Quat::setXYZ( const Vector3 & vec )
{
    mX = vec.getX();
    mY = vec.getY();
    mZ = vec.getZ();
    return *this;
}

inline const Vector3 Quat::getXYZ( ) const
{
    return Vector3( mX, mY, mZ );
}

inline Quat & Quat::setX( float _x )
{
    mX = _x;
    return *this;
}

inline float Quat::getX( ) const
{
    return mX;
}

inline Quat & Quat::setY( float _y )
{
    mY = _y;
    return *this;
}

inline float Quat::getY( ) const
{
    return mY;
}

inline Quat & Quat::setZ( float _z )
{
    mZ = _z;
    return *this;
}

inline float Quat::getZ( ) const
{
    return mZ;
}

inline Quat & Quat::setW( float _w )
{
    mW = _w;
    return *this;
}

inline float Quat::getW( ) const
{
    return mW;
}

inline Quat & Quat::setElem( int idx, float value )
{
    *(&mX + idx) = value;
    return *this;
}

inline float Quat::getElem( int idx ) const
{
    return *(&mX + idx);
}

inline float & Quat::operator []( int idx )
{
    return *(&mX + idx);
}

inline float Quat::operator []( int idx ) const
{
    return *(&mX + idx);
}

inline const Quat Quat::operator +( const Quat & quat ) const
{
    return Quat(
        ( mX + quat.mX ),
        ( mY + quat.mY ),
        ( mZ + quat.mZ ),
        ( mW + quat.mW )
    );
}

inline const Quat Quat::operator -( const Quat & quat ) const
{
    return Quat(
        ( mX - quat.mX ),
        ( mY - quat.mY ),
        ( mZ - quat.mZ ),
        ( mW - quat.mW )
    );
}

inline const Quat Quat::operator *( float scalar ) const
{
    return Quat(
        ( mX * scalar ),
        ( mY * scalar ),
        ( mZ * scalar ),
        ( mW * scalar )
    );
}

inline Quat & Quat::operator +=( const Quat & quat )
{
    *this = *this + quat;
    return *this;
}

inline Quat & Quat::operator -=( const Quat & quat )
{
    *this = *this - quat;
    return *this;
}

inline Quat & Quat::operator *=( float scalar )
{
    *this = *this * scalar;
    return *this;
}

inline const Quat Quat::operator /( float scalar ) const
{
    return Quat(
        ( mX / scalar ),
        ( mY / scalar ),
        ( mZ / scalar ),
        ( mW / scalar )
    );
}

inline Quat & Quat::operator /=( float scalar )
{
    *this = *this / scalar;
    return *this;
}

inline const Quat Quat::operator -( ) const
{
    return Quat(
        -mX,
        -mY,
        -mZ,
        -mW
    );
}

inline const Quat operator *( float scalar, const Quat & quat )
{
    return quat * scalar;
}

inline float dot( const Quat & quat0, const Quat & quat1 )
{
    float result;
    result = ( quat0.getX() * quat1.getX() );
    result = ( result + ( quat0.getY() * quat1.getY() ) );
    result = ( result + ( quat0.getZ() * quat1.getZ() ) );
    result = ( result + ( quat0.getW() * quat1.getW() ) );
    return result;
}

inline float norm( const Quat & quat )
{
    float result;
    result = ( quat.getX() * quat.getX() );
    result = ( result + ( quat.getY() * quat.getY() ) );
    result = ( result + ( quat.getZ() * quat.getZ() ) );
    result = ( result + ( quat.getW() * quat.getW() ) );
    return result;
}

inline float length( const Quat & quat )
{
    return sqrtf( norm( quat ) );
}

inline const Quat normalize( const Quat & quat )
{
    float lenSqr, lenInv;
    lenSqr = norm( quat );
    lenInv = ( 1.0f / sqrtf( lenSqr ) );
    return Quat(
        ( quat.getX() * lenInv ),
        ( quat.getY() * lenInv ),
        ( quat.getZ() * lenInv ),
        ( quat.getW() * lenInv )
    );
}

inline const Quat Quat::rotation( const Vector3 & unitVec0, const Vector3 & unitVec1 )
{
    float cosHalfAngleX2, recipCosHalfAngleX2;
    cosHalfAngleX2 = sqrtf( ( 2.0f * ( 1.0f + dot( unitVec0, unitVec1 ) ) ) );
    recipCosHalfAngleX2 = ( 1.0f / cosHalfAngleX2 );
    return Quat( ( cross( unitVec0, unitVec1 ) * recipCosHalfAngleX2 ), ( cosHalfAngleX2 * 0.5f ) );
}

inline const Quat Quat::rotation( float radians, const Vector3 & unitVec )
{
    float s, c, angle;
    angle = ( radians * 0.5f );
    s = sinf( angle );
    c = cosf( angle );
    return Quat( ( unitVec * s ), c );
}

inline const Quat Quat::rotationX( float radians )
{
    float s, c, angle;
    angle = ( radians * 0.5f );
    s = sinf( angle );
    c = cosf( angle );
    return Quat( s, 0.0f, 0.0f, c );
}

inline const Quat Quat::rotationY( float radians )
{
    float s, c, angle;
    angle = ( radians * 0.5f );
    s = sinf( angle );
    c = cosf( angle );
    return Quat( 0.0f, s, 0.0f, c );
}

inline const Quat Quat::rotationZ( float radians )
{
    float s, c, angle;
    angle = ( radians * 0.5f );
    s = sinf( angle );
    c = cosf( angle );
    return Quat( 0.0f, 0.0f, s, c );
}

inline const Quat Quat::operator *( const Quat & quat ) const
{
    return Quat(
        ( ( ( ( mW * quat.mX ) + ( mX * quat.mW ) ) + ( mY * quat.mZ ) ) - ( mZ * quat.mY ) ),
        ( ( ( ( mW * quat.mY ) + ( mY * quat.mW ) ) + ( mZ * quat.mX ) ) - ( mX * quat.mZ ) ),
        ( ( ( ( mW * quat.mZ ) + ( mZ * quat.mW ) ) + ( mX * quat.mY ) ) - ( mY * quat.mX ) ),
        ( ( ( ( mW * quat.mW ) - ( mX * quat.mX ) ) - ( mY * quat.mY ) ) - ( mZ * quat.mZ ) )
    );
}

inline Quat & Quat::operator *=( const Quat & quat )
{
    *this = *this * quat;
    return *this;
}

inline const Vector3 rotate( const Quat & quat, const Vector3 & vec )
{
    float tmpX, tmpY, tmpZ, tmpW;
    tmpX = ( ( ( quat.getW() * vec.getX() ) + ( quat.getY() * vec.getZ() ) ) - ( quat.getZ() * vec.getY() ) );
    tmpY = ( ( ( quat.getW() * vec.getY() ) + ( quat.getZ() * vec.getX() ) ) - ( quat.getX() * vec.getZ() ) );
    tmpZ = ( ( ( quat.getW() * vec.getZ() ) + ( quat.getX() * vec.getY() ) ) - ( quat.getY() * vec.getX() ) );
    tmpW = ( ( ( quat.getX() * vec.getX() ) + ( quat.getY() * vec.getY() ) ) + ( quat.getZ() * vec.getZ() ) );
    return Vector3(
        ( ( ( ( tmpW * quat.getX() ) + ( tmpX * quat.getW() ) ) - ( tmpY * quat.getZ() ) ) + ( tmpZ * quat.getY() ) ),
        ( ( ( ( tmpW * quat.getY() ) + ( tmpY * quat.getW() ) ) - ( tmpZ * quat.getX() ) ) + ( tmpX * quat.getZ() ) ),
        ( ( ( ( tmpW * quat.getZ() ) + ( tmpZ * quat.getW() ) ) - ( tmpX * quat.getY() ) ) + ( tmpY * quat.getX() ) )
    );
}

inline const Quat conj( const Quat & quat )
{
    return Quat( -quat.getX(), -quat.getY(), -quat.getZ(), quat.getW() );
}

inline const Quat select( const Quat & quat0, const Quat & quat1, bool select1 )
{
    return Quat(
        ( select1 )? quat1.getX() : quat0.getX(),
        ( select1 )? quat1.getY() : quat0.getY(),
        ( select1 )? quat1.getZ() : quat0.getZ(),
        ( select1 )? quat1.getW() : quat0.getW()
    );
}

#ifdef _VECTORMATH_DEBUG

inline void print( const Quat & quat )
{
    printf( "( %f %f %f %f )\n", quat.getX(), quat.getY(), quat.getZ(), quat.getW() );
}

inline void print( const Quat & quat, const char * name )
{
    printf( "%s: ( %f %f %f %f )\n", name, quat.getX(), quat.getY(), quat.getZ(), quat.getW() );
}

#endif

} // namespace Aos
} // namespace Vectormath

#endif

/*
   Copyright (C) 2006, 2007 Sony Computer Entertainment Inc.
   All rights reserved.

   Redistribution and use in source and binary forms,
   with or without modification, are permitted provided that the
   following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Sony Computer Entertainment Inc nor the names
      of its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

namespace Vectormath {
namespace Aos {

//-----------------------------------------------------------------------------
// Constants

#define _VECTORMATH_PI_OVER_2 1.570796327f

//-----------------------------------------------------------------------------
// Definitions

inline Matrix3::Matrix3( const Matrix3 & mat )
{
    mCol0 = mat.mCol0;
    mCol1 = mat.mCol1;
    mCol2 = mat.mCol2;
}

inline Matrix3::Matrix3( float scalar )
{
    mCol0 = Vector3( scalar );
    mCol1 = Vector3( scalar );
    mCol2 = Vector3( scalar );
}

inline Matrix3::Matrix3( const Quat & unitQuat )
{
    float qx, qy, qz, qw, qx2, qy2, qz2, qxqx2, qyqy2, qzqz2, qxqy2, qyqz2, qzqw2, qxqz2, qyqw2, qxqw2;
    qx = unitQuat.getX();
    qy = unitQuat.getY();
    qz = unitQuat.getZ();
    qw = unitQuat.getW();
    qx2 = ( qx + qx );
    qy2 = ( qy + qy );
    qz2 = ( qz + qz );
    qxqx2 = ( qx * qx2 );
    qxqy2 = ( qx * qy2 );
    qxqz2 = ( qx * qz2 );
    qxqw2 = ( qw * qx2 );
    qyqy2 = ( qy * qy2 );
    qyqz2 = ( qy * qz2 );
    qyqw2 = ( qw * qy2 );
    qzqz2 = ( qz * qz2 );
    qzqw2 = ( qw * qz2 );
    mCol0 = Vector3( ( ( 1.0f - qyqy2 ) - qzqz2 ), ( qxqy2 + qzqw2 ), ( qxqz2 - qyqw2 ) );
    mCol1 = Vector3( ( qxqy2 - qzqw2 ), ( ( 1.0f - qxqx2 ) - qzqz2 ), ( qyqz2 + qxqw2 ) );
    mCol2 = Vector3( ( qxqz2 + qyqw2 ), ( qyqz2 - qxqw2 ), ( ( 1.0f - qxqx2 ) - qyqy2 ) );
}

inline Matrix3::Matrix3( const Vector3 & _col0, const Vector3 & _col1, const Vector3 & _col2 )
{
    mCol0 = _col0;
    mCol1 = _col1;
    mCol2 = _col2;
}

inline Matrix3 & Matrix3::setCol0( const Vector3 & _col0 )
{
    mCol0 = _col0;
    return *this;
}

inline Matrix3 & Matrix3::setCol1( const Vector3 & _col1 )
{
    mCol1 = _col1;
    return *this;
}

inline Matrix3 & Matrix3::setCol2( const Vector3 & _col2 )
{
    mCol2 = _col2;
    return *this;
}

inline Matrix3 & Matrix3::setCol( int col, const Vector3 & vec )
{
    *(&mCol0 + col) = vec;
    return *this;
}

inline Matrix3 & Matrix3::setRow( int row, const Vector3 & vec )
{
    mCol0.setElem( row, vec.getElem( 0 ) );
    mCol1.setElem( row, vec.getElem( 1 ) );
    mCol2.setElem( row, vec.getElem( 2 ) );
    return *this;
}

inline Matrix3 & Matrix3::setElem( int col, int row, float val )
{
    Vector3 tmpV3_0;
    tmpV3_0 = this->getCol( col );
    tmpV3_0.setElem( row, val );
    this->setCol( col, tmpV3_0 );
    return *this;
}

inline float Matrix3::getElem( int col, int row ) const
{
    return this->getCol( col ).getElem( row );
}

inline const Vector3 Matrix3::getCol0( ) const
{
    return mCol0;
}

inline const Vector3 Matrix3::getCol1( ) const
{
    return mCol1;
}

inline const Vector3 Matrix3::getCol2( ) const
{
    return mCol2;
}

inline const Vector3 Matrix3::getCol( int col ) const
{
    return *(&mCol0 + col);
}

inline const Vector3 Matrix3::getRow( int row ) const
{
    return Vector3( mCol0.getElem( row ), mCol1.getElem( row ), mCol2.getElem( row ) );
}

inline Vector3 & Matrix3::operator []( int col )
{
    return *(&mCol0 + col);
}

inline const Vector3 Matrix3::operator []( int col ) const
{
    return *(&mCol0 + col);
}

inline Matrix3 & Matrix3::operator =( const Matrix3 & mat )
{
    mCol0 = mat.mCol0;
    mCol1 = mat.mCol1;
    mCol2 = mat.mCol2;
    return *this;
}

inline const Matrix3 transpose( const Matrix3 & mat )
{
    return Matrix3(
        Vector3( mat.getCol0().getX(), mat.getCol1().getX(), mat.getCol2().getX() ),
        Vector3( mat.getCol0().getY(), mat.getCol1().getY(), mat.getCol2().getY() ),
        Vector3( mat.getCol0().getZ(), mat.getCol1().getZ(), mat.getCol2().getZ() )
    );
}

inline const Matrix3 inverse( const Matrix3 & mat )
{
    Vector3 tmp0, tmp1, tmp2;
    float detinv;
    tmp0 = cross( mat.getCol1(), mat.getCol2() );
    tmp1 = cross( mat.getCol2(), mat.getCol0() );
    tmp2 = cross( mat.getCol0(), mat.getCol1() );
    detinv = ( 1.0f / dot( mat.getCol2(), tmp2 ) );
    return Matrix3(
        Vector3( ( tmp0.getX() * detinv ), ( tmp1.getX() * detinv ), ( tmp2.getX() * detinv ) ),
        Vector3( ( tmp0.getY() * detinv ), ( tmp1.getY() * detinv ), ( tmp2.getY() * detinv ) ),
        Vector3( ( tmp0.getZ() * detinv ), ( tmp1.getZ() * detinv ), ( tmp2.getZ() * detinv ) )
    );
}

inline float determinant( const Matrix3 & mat )
{
    return dot( mat.getCol2(), cross( mat.getCol0(), mat.getCol1() ) );
}

inline const Matrix3 Matrix3::operator +( const Matrix3 & mat ) const
{
    return Matrix3(
        ( mCol0 + mat.mCol0 ),
        ( mCol1 + mat.mCol1 ),
        ( mCol2 + mat.mCol2 )
    );
}

inline const Matrix3 Matrix3::operator -( const Matrix3 & mat ) const
{
    return Matrix3(
        ( mCol0 - mat.mCol0 ),
        ( mCol1 - mat.mCol1 ),
        ( mCol2 - mat.mCol2 )
    );
}

inline Matrix3 & Matrix3::operator +=( const Matrix3 & mat )
{
    *this = *this + mat;
    return *this;
}

inline Matrix3 & Matrix3::operator -=( const Matrix3 & mat )
{
    *this = *this - mat;
    return *this;
}

inline const Matrix3 Matrix3::operator -( ) const
{
    return Matrix3(
        ( -mCol0 ),
        ( -mCol1 ),
        ( -mCol2 )
    );
}

inline const Matrix3 absPerElem( const Matrix3 & mat )
{
    return Matrix3(
        absPerElem( mat.getCol0() ),
        absPerElem( mat.getCol1() ),
        absPerElem( mat.getCol2() )
    );
}

inline const Matrix3 Matrix3::operator *( float scalar ) const
{
    return Matrix3(
        ( mCol0 * scalar ),
        ( mCol1 * scalar ),
        ( mCol2 * scalar )
    );
}

inline Matrix3 & Matrix3::operator *=( float scalar )
{
    *this = *this * scalar;
    return *this;
}

inline const Matrix3 operator *( float scalar, const Matrix3 & mat )
{
    return mat * scalar;
}

inline const Vector3 Matrix3::operator *( const Vector3 & vec ) const
{
    return Vector3(
        ( ( ( mCol0.getX() * vec.getX() ) + ( mCol1.getX() * vec.getY() ) ) + ( mCol2.getX() * vec.getZ() ) ),
        ( ( ( mCol0.getY() * vec.getX() ) + ( mCol1.getY() * vec.getY() ) ) + ( mCol2.getY() * vec.getZ() ) ),
        ( ( ( mCol0.getZ() * vec.getX() ) + ( mCol1.getZ() * vec.getY() ) ) + ( mCol2.getZ() * vec.getZ() ) )
    );
}

inline const Matrix3 Matrix3::operator *( const Matrix3 & mat ) const
{
    return Matrix3(
        ( *this * mat.mCol0 ),
        ( *this * mat.mCol1 ),
        ( *this * mat.mCol2 )
    );
}

inline Matrix3 & Matrix3::operator *=( const Matrix3 & mat )
{
    *this = *this * mat;
    return *this;
}

inline const Matrix3 mulPerElem( const Matrix3 & mat0, const Matrix3 & mat1 )
{
    return Matrix3(
        mulPerElem( mat0.getCol0(), mat1.getCol0() ),
        mulPerElem( mat0.getCol1(), mat1.getCol1() ),
        mulPerElem( mat0.getCol2(), mat1.getCol2() )
    );
}

inline const Matrix3 Matrix3::identity( )
{
    return Matrix3(
        Vector3::xAxis( ),
        Vector3::yAxis( ),
        Vector3::zAxis( )
    );
}

inline const Matrix3 Matrix3::rotationX( float radians )
{
    float s, c;
    s = sinf( radians );
    c = cosf( radians );
    return Matrix3(
        Vector3::xAxis( ),
        Vector3( 0.0f, c, s ),
        Vector3( 0.0f, -s, c )
    );
}

inline const Matrix3 Matrix3::rotationY( float radians )
{
    float s, c;
    s = sinf( radians );
    c = cosf( radians );
    return Matrix3(
        Vector3( c, 0.0f, -s ),
        Vector3::yAxis( ),
        Vector3( s, 0.0f, c )
    );
}

inline const Matrix3 Matrix3::rotationZ( float radians )
{
    float s, c;
    s = sinf( radians );
    c = cosf( radians );
    return Matrix3(
        Vector3( c, s, 0.0f ),
        Vector3( -s, c, 0.0f ),
        Vector3::zAxis( )
    );
}

inline const Matrix3 Matrix3::rotationZYX( const Vector3 & radiansXYZ )
{
    float sX, cX, sY, cY, sZ, cZ, tmp0, tmp1;
    sX = sinf( radiansXYZ.getX() );
    cX = cosf( radiansXYZ.getX() );
    sY = sinf( radiansXYZ.getY() );
    cY = cosf( radiansXYZ.getY() );
    sZ = sinf( radiansXYZ.getZ() );
    cZ = cosf( radiansXYZ.getZ() );
    tmp0 = ( cZ * sY );
    tmp1 = ( sZ * sY );
    return Matrix3(
        Vector3( ( cZ * cY ), ( sZ * cY ), -sY ),
        Vector3( ( ( tmp0 * sX ) - ( sZ * cX ) ), ( ( tmp1 * sX ) + ( cZ * cX ) ), ( cY * sX ) ),
        Vector3( ( ( tmp0 * cX ) + ( sZ * sX ) ), ( ( tmp1 * cX ) - ( cZ * sX ) ), ( cY * cX ) )
    );
}

inline const Matrix3 Matrix3::rotation( float radians, const Vector3 & unitVec )
{
    float x, y, z, s, c, oneMinusC, xy, yz, zx;
    s = sinf( radians );
    c = cosf( radians );
    x = unitVec.getX();
    y = unitVec.getY();
    z = unitVec.getZ();
    xy = ( x * y );
    yz = ( y * z );
    zx = ( z * x );
    oneMinusC = ( 1.0f - c );
    return Matrix3(
        Vector3( ( ( ( x * x ) * oneMinusC ) + c ), ( ( xy * oneMinusC ) + ( z * s ) ), ( ( zx * oneMinusC ) - ( y * s ) ) ),
        Vector3( ( ( xy * oneMinusC ) - ( z * s ) ), ( ( ( y * y ) * oneMinusC ) + c ), ( ( yz * oneMinusC ) + ( x * s ) ) ),
        Vector3( ( ( zx * oneMinusC ) + ( y * s ) ), ( ( yz * oneMinusC ) - ( x * s ) ), ( ( ( z * z ) * oneMinusC ) + c ) )
    );
}

inline const Matrix3 Matrix3::rotation( const Quat & unitQuat )
{
    return Matrix3( unitQuat );
}

inline const Matrix3 Matrix3::scale( const Vector3 & scaleVec )
{
    return Matrix3(
        Vector3( scaleVec.getX(), 0.0f, 0.0f ),
        Vector3( 0.0f, scaleVec.getY(), 0.0f ),
        Vector3( 0.0f, 0.0f, scaleVec.getZ() )
    );
}

inline const Matrix3 appendScale( const Matrix3 & mat, const Vector3 & scaleVec )
{
    return Matrix3(
        ( mat.getCol0() * scaleVec.getX( ) ),
        ( mat.getCol1() * scaleVec.getY( ) ),
        ( mat.getCol2() * scaleVec.getZ( ) )
    );
}

inline const Matrix3 prependScale( const Vector3 & scaleVec, const Matrix3 & mat )
{
    return Matrix3(
        mulPerElem( mat.getCol0(), scaleVec ),
        mulPerElem( mat.getCol1(), scaleVec ),
        mulPerElem( mat.getCol2(), scaleVec )
    );
}

inline const Matrix3 select( const Matrix3 & mat0, const Matrix3 & mat1, bool select1 )
{
    return Matrix3(
        select( mat0.getCol0(), mat1.getCol0(), select1 ),
        select( mat0.getCol1(), mat1.getCol1(), select1 ),
        select( mat0.getCol2(), mat1.getCol2(), select1 )
    );
}

#ifdef _VECTORMATH_DEBUG

inline void print( const Matrix3 & mat )
{
    print( mat.getRow( 0 ) );
    print( mat.getRow( 1 ) );
    print( mat.getRow( 2 ) );
}

inline void print( const Matrix3 & mat, const char * name )
{
    printf("%s:\n", name);
    print( mat );
}

#endif

inline Matrix4::Matrix4( const Matrix4 & mat )
{
    mCol0 = mat.mCol0;
    mCol1 = mat.mCol1;
    mCol2 = mat.mCol2;
    mCol3 = mat.mCol3;
}

inline Matrix4::Matrix4( float scalar )
{
    mCol0 = Vector4( scalar );
    mCol1 = Vector4( scalar );
    mCol2 = Vector4( scalar );
    mCol3 = Vector4( scalar );
}

inline Matrix4::Matrix4( const Transform3 & mat )
{
    mCol0 = Vector4( mat.getCol0(), 0.0f );
    mCol1 = Vector4( mat.getCol1(), 0.0f );
    mCol2 = Vector4( mat.getCol2(), 0.0f );
    mCol3 = Vector4( mat.getCol3(), 1.0f );
}

inline Matrix4::Matrix4( const Vector4 & _col0, const Vector4 & _col1, const Vector4 & _col2, const Vector4 & _col3 )
{
    mCol0 = _col0;
    mCol1 = _col1;
    mCol2 = _col2;
    mCol3 = _col3;
}

inline Matrix4::Matrix4( const Matrix3 & mat, const Vector3 & translateVec )
{
    mCol0 = Vector4( mat.getCol0(), 0.0f );
    mCol1 = Vector4( mat.getCol1(), 0.0f );
    mCol2 = Vector4( mat.getCol2(), 0.0f );
    mCol3 = Vector4( translateVec, 1.0f );
}

inline Matrix4::Matrix4( const Quat & unitQuat, const Vector3 & translateVec )
{
    Matrix3 mat;
    mat = Matrix3( unitQuat );
    mCol0 = Vector4( mat.getCol0(), 0.0f );
    mCol1 = Vector4( mat.getCol1(), 0.0f );
    mCol2 = Vector4( mat.getCol2(), 0.0f );
    mCol3 = Vector4( translateVec, 1.0f );
}

inline Matrix4 & Matrix4::setCol0( const Vector4 & _col0 )
{
    mCol0 = _col0;
    return *this;
}

inline Matrix4 & Matrix4::setCol1( const Vector4 & _col1 )
{
    mCol1 = _col1;
    return *this;
}

inline Matrix4 & Matrix4::setCol2( const Vector4 & _col2 )
{
    mCol2 = _col2;
    return *this;
}

inline Matrix4 & Matrix4::setCol3( const Vector4 & _col3 )
{
    mCol3 = _col3;
    return *this;
}

inline Matrix4 & Matrix4::setCol( int col, const Vector4 & vec )
{
    *(&mCol0 + col) = vec;
    return *this;
}

inline Matrix4 & Matrix4::setRow( int row, const Vector4 & vec )
{
    mCol0.setElem( row, vec.getElem( 0 ) );
    mCol1.setElem( row, vec.getElem( 1 ) );
    mCol2.setElem( row, vec.getElem( 2 ) );
    mCol3.setElem( row, vec.getElem( 3 ) );
    return *this;
}

inline Matrix4 & Matrix4::setElem( int col, int row, float val )
{
    Vector4 tmpV3_0;
    tmpV3_0 = this->getCol( col );
    tmpV3_0.setElem( row, val );
    this->setCol( col, tmpV3_0 );
    return *this;
}

inline float Matrix4::getElem( int col, int row ) const
{
    return this->getCol( col ).getElem( row );
}

inline const Vector4 Matrix4::getCol0( ) const
{
    return mCol0;
}

inline const Vector4 Matrix4::getCol1( ) const
{
    return mCol1;
}

inline const Vector4 Matrix4::getCol2( ) const
{
    return mCol2;
}

inline const Vector4 Matrix4::getCol3( ) const
{
    return mCol3;
}

inline const Vector4 Matrix4::getCol( int col ) const
{
    return *(&mCol0 + col);
}

inline const Vector4 Matrix4::getRow( int row ) const
{
    return Vector4( mCol0.getElem( row ), mCol1.getElem( row ), mCol2.getElem( row ), mCol3.getElem( row ) );
}

inline Vector4 & Matrix4::operator []( int col )
{
    return *(&mCol0 + col);
}

inline const Vector4 Matrix4::operator []( int col ) const
{
    return *(&mCol0 + col);
}

inline Matrix4 & Matrix4::operator =( const Matrix4 & mat )
{
    mCol0 = mat.mCol0;
    mCol1 = mat.mCol1;
    mCol2 = mat.mCol2;
    mCol3 = mat.mCol3;
    return *this;
}

inline const Matrix4 transpose( const Matrix4 & mat )
{
    return Matrix4(
        Vector4( mat.getCol0().getX(), mat.getCol1().getX(), mat.getCol2().getX(), mat.getCol3().getX() ),
        Vector4( mat.getCol0().getY(), mat.getCol1().getY(), mat.getCol2().getY(), mat.getCol3().getY() ),
        Vector4( mat.getCol0().getZ(), mat.getCol1().getZ(), mat.getCol2().getZ(), mat.getCol3().getZ() ),
        Vector4( mat.getCol0().getW(), mat.getCol1().getW(), mat.getCol2().getW(), mat.getCol3().getW() )
    );
}

inline const Matrix4 inverse( const Matrix4 & mat )
{
    Vector4 res0, res1, res2, res3;
    float mA, mB, mC, mD, mE, mF, mG, mH, mI, mJ, mK, mL, mM, mN, mO, mP, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, detInv;
    mA = mat.getCol0().getX();
    mB = mat.getCol0().getY();
    mC = mat.getCol0().getZ();
    mD = mat.getCol0().getW();
    mE = mat.getCol1().getX();
    mF = mat.getCol1().getY();
    mG = mat.getCol1().getZ();
    mH = mat.getCol1().getW();
    mI = mat.getCol2().getX();
    mJ = mat.getCol2().getY();
    mK = mat.getCol2().getZ();
    mL = mat.getCol2().getW();
    mM = mat.getCol3().getX();
    mN = mat.getCol3().getY();
    mO = mat.getCol3().getZ();
    mP = mat.getCol3().getW();
    tmp0 = ( ( mK * mD ) - ( mC * mL ) );
    tmp1 = ( ( mO * mH ) - ( mG * mP ) );
    tmp2 = ( ( mB * mK ) - ( mJ * mC ) );
    tmp3 = ( ( mF * mO ) - ( mN * mG ) );
    tmp4 = ( ( mJ * mD ) - ( mB * mL ) );
    tmp5 = ( ( mN * mH ) - ( mF * mP ) );
    res0.setX( ( ( ( mJ * tmp1 ) - ( mL * tmp3 ) ) - ( mK * tmp5 ) ) );
    res0.setY( ( ( ( mN * tmp0 ) - ( mP * tmp2 ) ) - ( mO * tmp4 ) ) );
    res0.setZ( ( ( ( mD * tmp3 ) + ( mC * tmp5 ) ) - ( mB * tmp1 ) ) );
    res0.setW( ( ( ( mH * tmp2 ) + ( mG * tmp4 ) ) - ( mF * tmp0 ) ) );
    detInv = ( 1.0f / ( ( ( ( mA * res0.getX() ) + ( mE * res0.getY() ) ) + ( mI * res0.getZ() ) ) + ( mM * res0.getW() ) ) );
    res1.setX( ( mI * tmp1 ) );
    res1.setY( ( mM * tmp0 ) );
    res1.setZ( ( mA * tmp1 ) );
    res1.setW( ( mE * tmp0 ) );
    res3.setX( ( mI * tmp3 ) );
    res3.setY( ( mM * tmp2 ) );
    res3.setZ( ( mA * tmp3 ) );
    res3.setW( ( mE * tmp2 ) );
    res2.setX( ( mI * tmp5 ) );
    res2.setY( ( mM * tmp4 ) );
    res2.setZ( ( mA * tmp5 ) );
    res2.setW( ( mE * tmp4 ) );
    tmp0 = ( ( mI * mB ) - ( mA * mJ ) );
    tmp1 = ( ( mM * mF ) - ( mE * mN ) );
    tmp2 = ( ( mI * mD ) - ( mA * mL ) );
    tmp3 = ( ( mM * mH ) - ( mE * mP ) );
    tmp4 = ( ( mI * mC ) - ( mA * mK ) );
    tmp5 = ( ( mM * mG ) - ( mE * mO ) );
    res2.setX( ( ( ( mL * tmp1 ) - ( mJ * tmp3 ) ) + res2.getX() ) );
    res2.setY( ( ( ( mP * tmp0 ) - ( mN * tmp2 ) ) + res2.getY() ) );
    res2.setZ( ( ( ( mB * tmp3 ) - ( mD * tmp1 ) ) - res2.getZ() ) );
    res2.setW( ( ( ( mF * tmp2 ) - ( mH * tmp0 ) ) - res2.getW() ) );
    res3.setX( ( ( ( mJ * tmp5 ) - ( mK * tmp1 ) ) + res3.getX() ) );
    res3.setY( ( ( ( mN * tmp4 ) - ( mO * tmp0 ) ) + res3.getY() ) );
    res3.setZ( ( ( ( mC * tmp1 ) - ( mB * tmp5 ) ) - res3.getZ() ) );
    res3.setW( ( ( ( mG * tmp0 ) - ( mF * tmp4 ) ) - res3.getW() ) );
    res1.setX( ( ( ( mK * tmp3 ) - ( mL * tmp5 ) ) - res1.getX() ) );
    res1.setY( ( ( ( mO * tmp2 ) - ( mP * tmp4 ) ) - res1.getY() ) );
    res1.setZ( ( ( ( mD * tmp5 ) - ( mC * tmp3 ) ) + res1.getZ() ) );
    res1.setW( ( ( ( mH * tmp4 ) - ( mG * tmp2 ) ) + res1.getW() ) );
    return Matrix4(
        ( res0 * detInv ),
        ( res1 * detInv ),
        ( res2 * detInv ),
        ( res3 * detInv )
    );
}

inline const Matrix4 affineInverse( const Matrix4 & mat )
{
    Transform3 affineMat;
    affineMat.setCol0( mat.getCol0().getXYZ( ) );
    affineMat.setCol1( mat.getCol1().getXYZ( ) );
    affineMat.setCol2( mat.getCol2().getXYZ( ) );
    affineMat.setCol3( mat.getCol3().getXYZ( ) );
    return Matrix4( inverse( affineMat ) );
}

inline const Matrix4 orthoInverse( const Matrix4 & mat )
{
    Transform3 affineMat;
    affineMat.setCol0( mat.getCol0().getXYZ( ) );
    affineMat.setCol1( mat.getCol1().getXYZ( ) );
    affineMat.setCol2( mat.getCol2().getXYZ( ) );
    affineMat.setCol3( mat.getCol3().getXYZ( ) );
    return Matrix4( orthoInverse( affineMat ) );
}

inline float determinant( const Matrix4 & mat )
{
    float dx, dy, dz, dw, mA, mB, mC, mD, mE, mF, mG, mH, mI, mJ, mK, mL, mM, mN, mO, mP, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5;
    mA = mat.getCol0().getX();
    mB = mat.getCol0().getY();
    mC = mat.getCol0().getZ();
    mD = mat.getCol0().getW();
    mE = mat.getCol1().getX();
    mF = mat.getCol1().getY();
    mG = mat.getCol1().getZ();
    mH = mat.getCol1().getW();
    mI = mat.getCol2().getX();
    mJ = mat.getCol2().getY();
    mK = mat.getCol2().getZ();
    mL = mat.getCol2().getW();
    mM = mat.getCol3().getX();
    mN = mat.getCol3().getY();
    mO = mat.getCol3().getZ();
    mP = mat.getCol3().getW();
    tmp0 = ( ( mK * mD ) - ( mC * mL ) );
    tmp1 = ( ( mO * mH ) - ( mG * mP ) );
    tmp2 = ( ( mB * mK ) - ( mJ * mC ) );
    tmp3 = ( ( mF * mO ) - ( mN * mG ) );
    tmp4 = ( ( mJ * mD ) - ( mB * mL ) );
    tmp5 = ( ( mN * mH ) - ( mF * mP ) );
    dx = ( ( ( mJ * tmp1 ) - ( mL * tmp3 ) ) - ( mK * tmp5 ) );
    dy = ( ( ( mN * tmp0 ) - ( mP * tmp2 ) ) - ( mO * tmp4 ) );
    dz = ( ( ( mD * tmp3 ) + ( mC * tmp5 ) ) - ( mB * tmp1 ) );
    dw = ( ( ( mH * tmp2 ) + ( mG * tmp4 ) ) - ( mF * tmp0 ) );
    return ( ( ( ( mA * dx ) + ( mE * dy ) ) + ( mI * dz ) ) + ( mM * dw ) );
}

inline const Matrix4 Matrix4::operator +( const Matrix4 & mat ) const
{
    return Matrix4(
        ( mCol0 + mat.mCol0 ),
        ( mCol1 + mat.mCol1 ),
        ( mCol2 + mat.mCol2 ),
        ( mCol3 + mat.mCol3 )
    );
}

inline const Matrix4 Matrix4::operator -( const Matrix4 & mat ) const
{
    return Matrix4(
        ( mCol0 - mat.mCol0 ),
        ( mCol1 - mat.mCol1 ),
        ( mCol2 - mat.mCol2 ),
        ( mCol3 - mat.mCol3 )
    );
}

inline Matrix4 & Matrix4::operator +=( const Matrix4 & mat )
{
    *this = *this + mat;
    return *this;
}

inline Matrix4 & Matrix4::operator -=( const Matrix4 & mat )
{
    *this = *this - mat;
    return *this;
}

inline const Matrix4 Matrix4::operator -( ) const
{
    return Matrix4(
        ( -mCol0 ),
        ( -mCol1 ),
        ( -mCol2 ),
        ( -mCol3 )
    );
}

inline const Matrix4 absPerElem( const Matrix4 & mat )
{
    return Matrix4(
        absPerElem( mat.getCol0() ),
        absPerElem( mat.getCol1() ),
        absPerElem( mat.getCol2() ),
        absPerElem( mat.getCol3() )
    );
}

inline const Matrix4 Matrix4::operator *( float scalar ) const
{
    return Matrix4(
        ( mCol0 * scalar ),
        ( mCol1 * scalar ),
        ( mCol2 * scalar ),
        ( mCol3 * scalar )
    );
}

inline Matrix4 & Matrix4::operator *=( float scalar )
{
    *this = *this * scalar;
    return *this;
}

inline const Matrix4 operator *( float scalar, const Matrix4 & mat )
{
    return mat * scalar;
}

inline const Vector4 Matrix4::operator *( const Vector4 & vec ) const
{
    return Vector4(
        ( ( ( ( mCol0.getX() * vec.getX() ) + ( mCol1.getX() * vec.getY() ) ) + ( mCol2.getX() * vec.getZ() ) ) + ( mCol3.getX() * vec.getW() ) ),
        ( ( ( ( mCol0.getY() * vec.getX() ) + ( mCol1.getY() * vec.getY() ) ) + ( mCol2.getY() * vec.getZ() ) ) + ( mCol3.getY() * vec.getW() ) ),
        ( ( ( ( mCol0.getZ() * vec.getX() ) + ( mCol1.getZ() * vec.getY() ) ) + ( mCol2.getZ() * vec.getZ() ) ) + ( mCol3.getZ() * vec.getW() ) ),
        ( ( ( ( mCol0.getW() * vec.getX() ) + ( mCol1.getW() * vec.getY() ) ) + ( mCol2.getW() * vec.getZ() ) ) + ( mCol3.getW() * vec.getW() ) )
    );
}

inline const Vector4 Matrix4::operator *( const Vector3 & vec ) const
{
    return Vector4(
        ( ( ( mCol0.getX() * vec.getX() ) + ( mCol1.getX() * vec.getY() ) ) + ( mCol2.getX() * vec.getZ() ) ),
        ( ( ( mCol0.getY() * vec.getX() ) + ( mCol1.getY() * vec.getY() ) ) + ( mCol2.getY() * vec.getZ() ) ),
        ( ( ( mCol0.getZ() * vec.getX() ) + ( mCol1.getZ() * vec.getY() ) ) + ( mCol2.getZ() * vec.getZ() ) ),
        ( ( ( mCol0.getW() * vec.getX() ) + ( mCol1.getW() * vec.getY() ) ) + ( mCol2.getW() * vec.getZ() ) )
    );
}

inline const Vector4 Matrix4::operator *( const Point3 & pnt ) const
{
    return Vector4(
        ( ( ( ( mCol0.getX() * pnt.getX() ) + ( mCol1.getX() * pnt.getY() ) ) + ( mCol2.getX() * pnt.getZ() ) ) + mCol3.getX() ),
        ( ( ( ( mCol0.getY() * pnt.getX() ) + ( mCol1.getY() * pnt.getY() ) ) + ( mCol2.getY() * pnt.getZ() ) ) + mCol3.getY() ),
        ( ( ( ( mCol0.getZ() * pnt.getX() ) + ( mCol1.getZ() * pnt.getY() ) ) + ( mCol2.getZ() * pnt.getZ() ) ) + mCol3.getZ() ),
        ( ( ( ( mCol0.getW() * pnt.getX() ) + ( mCol1.getW() * pnt.getY() ) ) + ( mCol2.getW() * pnt.getZ() ) ) + mCol3.getW() )
    );
}

inline const Matrix4 Matrix4::operator *( const Matrix4 & mat ) const
{
    return Matrix4(
        ( *this * mat.mCol0 ),
        ( *this * mat.mCol1 ),
        ( *this * mat.mCol2 ),
        ( *this * mat.mCol3 )
    );
}

inline Matrix4 & Matrix4::operator *=( const Matrix4 & mat )
{
    *this = *this * mat;
    return *this;
}

inline const Matrix4 Matrix4::operator *( const Transform3 & tfrm ) const
{
    return Matrix4(
        ( *this * tfrm.getCol0() ),
        ( *this * tfrm.getCol1() ),
        ( *this * tfrm.getCol2() ),
        ( *this * Point3( tfrm.getCol3() ) )
    );
}

inline Matrix4 & Matrix4::operator *=( const Transform3 & tfrm )
{
    *this = *this * tfrm;
    return *this;
}

inline const Matrix4 mulPerElem( const Matrix4 & mat0, const Matrix4 & mat1 )
{
    return Matrix4(
        mulPerElem( mat0.getCol0(), mat1.getCol0() ),
        mulPerElem( mat0.getCol1(), mat1.getCol1() ),
        mulPerElem( mat0.getCol2(), mat1.getCol2() ),
        mulPerElem( mat0.getCol3(), mat1.getCol3() )
    );
}

inline const Matrix4 Matrix4::identity( )
{
    return Matrix4(
        Vector4::xAxis( ),
        Vector4::yAxis( ),
        Vector4::zAxis( ),
        Vector4::wAxis( )
    );
}

inline Matrix4 & Matrix4::setUpper3x3( const Matrix3 & mat3 )
{
    mCol0.setXYZ( mat3.getCol0() );
    mCol1.setXYZ( mat3.getCol1() );
    mCol2.setXYZ( mat3.getCol2() );
    return *this;
}

inline const Matrix3 Matrix4::getUpper3x3( ) const
{
    return Matrix3(
        mCol0.getXYZ( ),
        mCol1.getXYZ( ),
        mCol2.getXYZ( )
    );
}

inline Matrix4 & Matrix4::setTranslation( const Vector3 & translateVec )
{
    mCol3.setXYZ( translateVec );
    return *this;
}

inline const Vector3 Matrix4::getTranslation( ) const
{
    return mCol3.getXYZ( );
}

inline const Matrix4 Matrix4::rotationX( float radians )
{
    float s, c;
    s = sinf( radians );
    c = cosf( radians );
    return Matrix4(
        Vector4::xAxis( ),
        Vector4( 0.0f, c, s, 0.0f ),
        Vector4( 0.0f, -s, c, 0.0f ),
        Vector4::wAxis( )
    );
}

inline const Matrix4 Matrix4::rotationY( float radians )
{
    float s, c;
    s = sinf( radians );
    c = cosf( radians );
    return Matrix4(
        Vector4( c, 0.0f, -s, 0.0f ),
        Vector4::yAxis( ),
        Vector4( s, 0.0f, c, 0.0f ),
        Vector4::wAxis( )
    );
}

inline const Matrix4 Matrix4::rotationZ( float radians )
{
    float s, c;
    s = sinf( radians );
    c = cosf( radians );
    return Matrix4(
        Vector4( c, s, 0.0f, 0.0f ),
        Vector4( -s, c, 0.0f, 0.0f ),
        Vector4::zAxis( ),
        Vector4::wAxis( )
    );
}

inline const Matrix4 Matrix4::rotationZYX( const Vector3 & radiansXYZ )
{
    float sX, cX, sY, cY, sZ, cZ, tmp0, tmp1;
    sX = sinf( radiansXYZ.getX() );
    cX = cosf( radiansXYZ.getX() );
    sY = sinf( radiansXYZ.getY() );
    cY = cosf( radiansXYZ.getY() );
    sZ = sinf( radiansXYZ.getZ() );
    cZ = cosf( radiansXYZ.getZ() );
    tmp0 = ( cZ * sY );
    tmp1 = ( sZ * sY );
    return Matrix4(
        Vector4( ( cZ * cY ), ( sZ * cY ), -sY, 0.0f ),
        Vector4( ( ( tmp0 * sX ) - ( sZ * cX ) ), ( ( tmp1 * sX ) + ( cZ * cX ) ), ( cY * sX ), 0.0f ),
        Vector4( ( ( tmp0 * cX ) + ( sZ * sX ) ), ( ( tmp1 * cX ) - ( cZ * sX ) ), ( cY * cX ), 0.0f ),
        Vector4::wAxis( )
    );
}

inline const Matrix4 Matrix4::rotation( float radians, const Vector3 & unitVec )
{
    float x, y, z, s, c, oneMinusC, xy, yz, zx;
    s = sinf( radians );
    c = cosf( radians );
    x = unitVec.getX();
    y = unitVec.getY();
    z = unitVec.getZ();
    xy = ( x * y );
    yz = ( y * z );
    zx = ( z * x );
    oneMinusC = ( 1.0f - c );
    return Matrix4(
        Vector4( ( ( ( x * x ) * oneMinusC ) + c ), ( ( xy * oneMinusC ) + ( z * s ) ), ( ( zx * oneMinusC ) - ( y * s ) ), 0.0f ),
        Vector4( ( ( xy * oneMinusC ) - ( z * s ) ), ( ( ( y * y ) * oneMinusC ) + c ), ( ( yz * oneMinusC ) + ( x * s ) ), 0.0f ),
        Vector4( ( ( zx * oneMinusC ) + ( y * s ) ), ( ( yz * oneMinusC ) - ( x * s ) ), ( ( ( z * z ) * oneMinusC ) + c ), 0.0f ),
        Vector4::wAxis( )
    );
}

inline const Matrix4 Matrix4::rotation( const Quat & unitQuat )
{
    return Matrix4( Transform3::rotation( unitQuat ) );
}

inline const Matrix4 Matrix4::scale( const Vector3 & scaleVec )
{
    return Matrix4(
        Vector4( scaleVec.getX(), 0.0f, 0.0f, 0.0f ),
        Vector4( 0.0f, scaleVec.getY(), 0.0f, 0.0f ),
        Vector4( 0.0f, 0.0f, scaleVec.getZ(), 0.0f ),
        Vector4::wAxis( )
    );
}

inline const Matrix4 appendScale( const Matrix4 & mat, const Vector3 & scaleVec )
{
    return Matrix4(
        ( mat.getCol0() * scaleVec.getX( ) ),
        ( mat.getCol1() * scaleVec.getY( ) ),
        ( mat.getCol2() * scaleVec.getZ( ) ),
        mat.getCol3()
    );
}

inline const Matrix4 prependScale( const Vector3 & scaleVec, const Matrix4 & mat )
{
    Vector4 scale4;
    scale4 = Vector4( scaleVec, 1.0f );
    return Matrix4(
        mulPerElem( mat.getCol0(), scale4 ),
        mulPerElem( mat.getCol1(), scale4 ),
        mulPerElem( mat.getCol2(), scale4 ),
        mulPerElem( mat.getCol3(), scale4 )
    );
}

inline const Matrix4 Matrix4::translation( const Vector3 & translateVec )
{
    return Matrix4(
        Vector4::xAxis( ),
        Vector4::yAxis( ),
        Vector4::zAxis( ),
        Vector4( translateVec, 1.0f )
    );
}

inline const Matrix4 Matrix4::lookAt( const Point3 & eyePos, const Point3 & lookAtPos, const Vector3 & upVec )
{
    Matrix4 m4EyeFrame;
    Vector3 v3X, v3Y, v3Z;
    v3Y = normalize( upVec );
    v3Z = normalize( ( eyePos - lookAtPos ) );
    v3X = normalize( cross( v3Y, v3Z ) );
    v3Y = cross( v3Z, v3X );
    m4EyeFrame = Matrix4( Vector4( v3X ), Vector4( v3Y ), Vector4( v3Z ), Vector4( eyePos ) );
    return orthoInverse( m4EyeFrame );
}

inline const Matrix4 Matrix4::perspective( float fovyRadians, float aspect, float zNear, float zFar )
{
    float f, rangeInv;
    f = tanf( ( (float)( _VECTORMATH_PI_OVER_2 ) - ( 0.5f * fovyRadians ) ) );
    rangeInv = ( 1.0f / ( zNear - zFar ) );
    return Matrix4(
        Vector4( ( f / aspect ), 0.0f, 0.0f, 0.0f ),
        Vector4( 0.0f, f, 0.0f, 0.0f ),
        Vector4( 0.0f, 0.0f, ( ( zNear + zFar ) * rangeInv ), -1.0f ),
        Vector4( 0.0f, 0.0f, ( ( ( zNear * zFar ) * rangeInv ) * 2.0f ), 0.0f )
    );
}

inline const Matrix4 Matrix4::frustum( float left, float right, float bottom, float top, float zNear, float zFar )
{
    float sum_rl, sum_tb, sum_nf, inv_rl, inv_tb, inv_nf, n2;
    sum_rl = ( right + left );
    sum_tb = ( top + bottom );
    sum_nf = ( zNear + zFar );
    inv_rl = ( 1.0f / ( right - left ) );
    inv_tb = ( 1.0f / ( top - bottom ) );
    inv_nf = ( 1.0f / ( zNear - zFar ) );
    n2 = ( zNear + zNear );
    return Matrix4(
        Vector4( ( n2 * inv_rl ), 0.0f, 0.0f, 0.0f ),
        Vector4( 0.0f, ( n2 * inv_tb ), 0.0f, 0.0f ),
        Vector4( ( sum_rl * inv_rl ), ( sum_tb * inv_tb ), ( sum_nf * inv_nf ), -1.0f ),
        Vector4( 0.0f, 0.0f, ( ( n2 * inv_nf ) * zFar ), 0.0f )
    );
}

inline const Matrix4 Matrix4::orthographic( float left, float right, float bottom, float top, float zNear, float zFar )
{
    float sum_rl, sum_tb, sum_nf, inv_rl, inv_tb, inv_nf;
    sum_rl = ( right + left );
    sum_tb = ( top + bottom );
    sum_nf = ( zNear + zFar );
    inv_rl = ( 1.0f / ( right - left ) );
    inv_tb = ( 1.0f / ( top - bottom ) );
    inv_nf = ( 1.0f / ( zNear - zFar ) );
    return Matrix4(
        Vector4( ( inv_rl + inv_rl ), 0.0f, 0.0f, 0.0f ),
        Vector4( 0.0f, ( inv_tb + inv_tb ), 0.0f, 0.0f ),
        Vector4( 0.0f, 0.0f, ( inv_nf + inv_nf ), 0.0f ),
        Vector4( ( -sum_rl * inv_rl ), ( -sum_tb * inv_tb ), ( sum_nf * inv_nf ), 1.0f )
    );
}

inline const Matrix4 select( const Matrix4 & mat0, const Matrix4 & mat1, bool select1 )
{
    return Matrix4(
        select( mat0.getCol0(), mat1.getCol0(), select1 ),
        select( mat0.getCol1(), mat1.getCol1(), select1 ),
        select( mat0.getCol2(), mat1.getCol2(), select1 ),
        select( mat0.getCol3(), mat1.getCol3(), select1 )
    );
}

#ifdef _VECTORMATH_DEBUG

inline void print( const Matrix4 & mat )
{
    print( mat.getRow( 0 ) );
    print( mat.getRow( 1 ) );
    print( mat.getRow( 2 ) );
    print( mat.getRow( 3 ) );
}

inline void print( const Matrix4 & mat, const char * name )
{
    printf("%s:\n", name);
    print( mat );
}

#endif

inline Transform3::Transform3( const Transform3 & tfrm )
{
    mCol0 = tfrm.mCol0;
    mCol1 = tfrm.mCol1;
    mCol2 = tfrm.mCol2;
    mCol3 = tfrm.mCol3;
}

inline Transform3::Transform3( float scalar )
{
    mCol0 = Vector3( scalar );
    mCol1 = Vector3( scalar );
    mCol2 = Vector3( scalar );
    mCol3 = Vector3( scalar );
}

inline Transform3::Transform3( const Vector3 & _col0, const Vector3 & _col1, const Vector3 & _col2, const Vector3 & _col3 )
{
    mCol0 = _col0;
    mCol1 = _col1;
    mCol2 = _col2;
    mCol3 = _col3;
}

inline Transform3::Transform3( const Matrix3 & tfrm, const Vector3 & translateVec )
{
    this->setUpper3x3( tfrm );
    this->setTranslation( translateVec );
}

inline Transform3::Transform3( const Quat & unitQuat, const Vector3 & translateVec )
{
    this->setUpper3x3( Matrix3( unitQuat ) );
    this->setTranslation( translateVec );
}

inline Transform3 & Transform3::setCol0( const Vector3 & _col0 )
{
    mCol0 = _col0;
    return *this;
}

inline Transform3 & Transform3::setCol1( const Vector3 & _col1 )
{
    mCol1 = _col1;
    return *this;
}

inline Transform3 & Transform3::setCol2( const Vector3 & _col2 )
{
    mCol2 = _col2;
    return *this;
}

inline Transform3 & Transform3::setCol3( const Vector3 & _col3 )
{
    mCol3 = _col3;
    return *this;
}

inline Transform3 & Transform3::setCol( int col, const Vector3 & vec )
{
    *(&mCol0 + col) = vec;
    return *this;
}

inline Transform3 & Transform3::setRow( int row, const Vector4 & vec )
{
    mCol0.setElem( row, vec.getElem( 0 ) );
    mCol1.setElem( row, vec.getElem( 1 ) );
    mCol2.setElem( row, vec.getElem( 2 ) );
    mCol3.setElem( row, vec.getElem( 3 ) );
    return *this;
}

inline Transform3 & Transform3::setElem( int col, int row, float val )
{
    Vector3 tmpV3_0;
    tmpV3_0 = this->getCol( col );
    tmpV3_0.setElem( row, val );
    this->setCol( col, tmpV3_0 );
    return *this;
}

inline float Transform3::getElem( int col, int row ) const
{
    return this->getCol( col ).getElem( row );
}

inline const Vector3 Transform3::getCol0( ) const
{
    return mCol0;
}

inline const Vector3 Transform3::getCol1( ) const
{
    return mCol1;
}

inline const Vector3 Transform3::getCol2( ) const
{
    return mCol2;
}

inline const Vector3 Transform3::getCol3( ) const
{
    return mCol3;
}

inline const Vector3 Transform3::getCol( int col ) const
{
    return *(&mCol0 + col);
}

inline const Vector4 Transform3::getRow( int row ) const
{
    return Vector4( mCol0.getElem( row ), mCol1.getElem( row ), mCol2.getElem( row ), mCol3.getElem( row ) );
}

inline Vector3 & Transform3::operator []( int col )
{
    return *(&mCol0 + col);
}

inline const Vector3 Transform3::operator []( int col ) const
{
    return *(&mCol0 + col);
}

inline Transform3 & Transform3::operator =( const Transform3 & tfrm )
{
    mCol0 = tfrm.mCol0;
    mCol1 = tfrm.mCol1;
    mCol2 = tfrm.mCol2;
    mCol3 = tfrm.mCol3;
    return *this;
}

inline const Transform3 inverse( const Transform3 & tfrm )
{
    Vector3 tmp0, tmp1, tmp2, inv0, inv1, inv2;
    float detinv;
    tmp0 = cross( tfrm.getCol1(), tfrm.getCol2() );
    tmp1 = cross( tfrm.getCol2(), tfrm.getCol0() );
    tmp2 = cross( tfrm.getCol0(), tfrm.getCol1() );
    detinv = ( 1.0f / dot( tfrm.getCol2(), tmp2 ) );
    inv0 = Vector3( ( tmp0.getX() * detinv ), ( tmp1.getX() * detinv ), ( tmp2.getX() * detinv ) );
    inv1 = Vector3( ( tmp0.getY() * detinv ), ( tmp1.getY() * detinv ), ( tmp2.getY() * detinv ) );
    inv2 = Vector3( ( tmp0.getZ() * detinv ), ( tmp1.getZ() * detinv ), ( tmp2.getZ() * detinv ) );
    return Transform3(
        inv0,
        inv1,
        inv2,
        Vector3( ( -( ( inv0 * tfrm.getCol3().getX() ) + ( ( inv1 * tfrm.getCol3().getY() ) + ( inv2 * tfrm.getCol3().getZ() ) ) ) ) )
    );
}

inline const Transform3 orthoInverse( const Transform3 & tfrm )
{
    Vector3 inv0, inv1, inv2;
    inv0 = Vector3( tfrm.getCol0().getX(), tfrm.getCol1().getX(), tfrm.getCol2().getX() );
    inv1 = Vector3( tfrm.getCol0().getY(), tfrm.getCol1().getY(), tfrm.getCol2().getY() );
    inv2 = Vector3( tfrm.getCol0().getZ(), tfrm.getCol1().getZ(), tfrm.getCol2().getZ() );
    return Transform3(
        inv0,
        inv1,
        inv2,
        Vector3( ( -( ( inv0 * tfrm.getCol3().getX() ) + ( ( inv1 * tfrm.getCol3().getY() ) + ( inv2 * tfrm.getCol3().getZ() ) ) ) ) )
    );
}

inline const Transform3 absPerElem( const Transform3 & tfrm )
{
    return Transform3(
        absPerElem( tfrm.getCol0() ),
        absPerElem( tfrm.getCol1() ),
        absPerElem( tfrm.getCol2() ),
        absPerElem( tfrm.getCol3() )
    );
}

inline const Vector3 Transform3::operator *( const Vector3 & vec ) const
{
    return Vector3(
        ( ( ( mCol0.getX() * vec.getX() ) + ( mCol1.getX() * vec.getY() ) ) + ( mCol2.getX() * vec.getZ() ) ),
        ( ( ( mCol0.getY() * vec.getX() ) + ( mCol1.getY() * vec.getY() ) ) + ( mCol2.getY() * vec.getZ() ) ),
        ( ( ( mCol0.getZ() * vec.getX() ) + ( mCol1.getZ() * vec.getY() ) ) + ( mCol2.getZ() * vec.getZ() ) )
    );
}

inline const Point3 Transform3::operator *( const Point3 & pnt ) const
{
    return Point3(
        ( ( ( ( mCol0.getX() * pnt.getX() ) + ( mCol1.getX() * pnt.getY() ) ) + ( mCol2.getX() * pnt.getZ() ) ) + mCol3.getX() ),
        ( ( ( ( mCol0.getY() * pnt.getX() ) + ( mCol1.getY() * pnt.getY() ) ) + ( mCol2.getY() * pnt.getZ() ) ) + mCol3.getY() ),
        ( ( ( ( mCol0.getZ() * pnt.getX() ) + ( mCol1.getZ() * pnt.getY() ) ) + ( mCol2.getZ() * pnt.getZ() ) ) + mCol3.getZ() )
    );
}

inline const Transform3 Transform3::operator *( const Transform3 & tfrm ) const
{
    return Transform3(
        ( *this * tfrm.mCol0 ),
        ( *this * tfrm.mCol1 ),
        ( *this * tfrm.mCol2 ),
        Vector3( ( *this * Point3( tfrm.mCol3 ) ) )
    );
}

inline Transform3 & Transform3::operator *=( const Transform3 & tfrm )
{
    *this = *this * tfrm;
    return *this;
}

inline const Transform3 mulPerElem( const Transform3 & tfrm0, const Transform3 & tfrm1 )
{
    return Transform3(
        mulPerElem( tfrm0.getCol0(), tfrm1.getCol0() ),
        mulPerElem( tfrm0.getCol1(), tfrm1.getCol1() ),
        mulPerElem( tfrm0.getCol2(), tfrm1.getCol2() ),
        mulPerElem( tfrm0.getCol3(), tfrm1.getCol3() )
    );
}

inline const Transform3 Transform3::identity( )
{
    return Transform3(
        Vector3::xAxis( ),
        Vector3::yAxis( ),
        Vector3::zAxis( ),
        Vector3( 0.0f )
    );
}

inline Transform3 & Transform3::setUpper3x3( const Matrix3 & tfrm )
{
    mCol0 = tfrm.getCol0();
    mCol1 = tfrm.getCol1();
    mCol2 = tfrm.getCol2();
    return *this;
}

inline const Matrix3 Transform3::getUpper3x3( ) const
{
    return Matrix3( mCol0, mCol1, mCol2 );
}

inline Transform3 & Transform3::setTranslation( const Vector3 & translateVec )
{
    mCol3 = translateVec;
    return *this;
}

inline const Vector3 Transform3::getTranslation( ) const
{
    return mCol3;
}

inline const Transform3 Transform3::rotationX( float radians )
{
    float s, c;
    s = sinf( radians );
    c = cosf( radians );
    return Transform3(
        Vector3::xAxis( ),
        Vector3( 0.0f, c, s ),
        Vector3( 0.0f, -s, c ),
        Vector3( 0.0f )
    );
}

inline const Transform3 Transform3::rotationY( float radians )
{
    float s, c;
    s = sinf( radians );
    c = cosf( radians );
    return Transform3(
        Vector3( c, 0.0f, -s ),
        Vector3::yAxis( ),
        Vector3( s, 0.0f, c ),
        Vector3( 0.0f )
    );
}

inline const Transform3 Transform3::rotationZ( float radians )
{
    float s, c;
    s = sinf( radians );
    c = cosf( radians );
    return Transform3(
        Vector3( c, s, 0.0f ),
        Vector3( -s, c, 0.0f ),
        Vector3::zAxis( ),
        Vector3( 0.0f )
    );
}

inline const Transform3 Transform3::rotationZYX( const Vector3 & radiansXYZ )
{
    float sX, cX, sY, cY, sZ, cZ, tmp0, tmp1;
    sX = sinf( radiansXYZ.getX() );
    cX = cosf( radiansXYZ.getX() );
    sY = sinf( radiansXYZ.getY() );
    cY = cosf( radiansXYZ.getY() );
    sZ = sinf( radiansXYZ.getZ() );
    cZ = cosf( radiansXYZ.getZ() );
    tmp0 = ( cZ * sY );
    tmp1 = ( sZ * sY );
    return Transform3(
        Vector3( ( cZ * cY ), ( sZ * cY ), -sY ),
        Vector3( ( ( tmp0 * sX ) - ( sZ * cX ) ), ( ( tmp1 * sX ) + ( cZ * cX ) ), ( cY * sX ) ),
        Vector3( ( ( tmp0 * cX ) + ( sZ * sX ) ), ( ( tmp1 * cX ) - ( cZ * sX ) ), ( cY * cX ) ),
        Vector3( 0.0f )
    );
}

inline const Transform3 Transform3::rotation( float radians, const Vector3 & unitVec )
{
    return Transform3( Matrix3::rotation( radians, unitVec ), Vector3( 0.0f ) );
}

inline const Transform3 Transform3::rotation( const Quat & unitQuat )
{
    return Transform3( Matrix3( unitQuat ), Vector3( 0.0f ) );
}

inline const Transform3 Transform3::scale( const Vector3 & scaleVec )
{
    return Transform3(
        Vector3( scaleVec.getX(), 0.0f, 0.0f ),
        Vector3( 0.0f, scaleVec.getY(), 0.0f ),
        Vector3( 0.0f, 0.0f, scaleVec.getZ() ),
        Vector3( 0.0f )
    );
}

inline const Transform3 appendScale( const Transform3 & tfrm, const Vector3 & scaleVec )
{
    return Transform3(
        ( tfrm.getCol0() * scaleVec.getX( ) ),
        ( tfrm.getCol1() * scaleVec.getY( ) ),
        ( tfrm.getCol2() * scaleVec.getZ( ) ),
        tfrm.getCol3()
    );
}

inline const Transform3 prependScale( const Vector3 & scaleVec, const Transform3 & tfrm )
{
    return Transform3(
        mulPerElem( tfrm.getCol0(), scaleVec ),
        mulPerElem( tfrm.getCol1(), scaleVec ),
        mulPerElem( tfrm.getCol2(), scaleVec ),
        mulPerElem( tfrm.getCol3(), scaleVec )
    );
}

inline const Transform3 Transform3::translation( const Vector3 & translateVec )
{
    return Transform3(
        Vector3::xAxis( ),
        Vector3::yAxis( ),
        Vector3::zAxis( ),
        translateVec
    );
}

inline const Transform3 select( const Transform3 & tfrm0, const Transform3 & tfrm1, bool select1 )
{
    return Transform3(
        select( tfrm0.getCol0(), tfrm1.getCol0(), select1 ),
        select( tfrm0.getCol1(), tfrm1.getCol1(), select1 ),
        select( tfrm0.getCol2(), tfrm1.getCol2(), select1 ),
        select( tfrm0.getCol3(), tfrm1.getCol3(), select1 )
    );
}

#ifdef _VECTORMATH_DEBUG

inline void print( const Transform3 & tfrm )
{
    print( tfrm.getRow( 0 ) );
    print( tfrm.getRow( 1 ) );
    print( tfrm.getRow( 2 ) );
}

inline void print( const Transform3 & tfrm, const char * name )
{
    printf("%s:\n", name);
    print( tfrm );
}

#endif

inline Quat::Quat( const Matrix3 & tfrm )
{
    float trace, radicand, scale, xx, yx, zx, xy, yy, zy, xz, yz, zz, tmpx, tmpy, tmpz, tmpw, qx, qy, qz, qw;
    int negTrace, ZgtX, ZgtY, YgtX;
    int largestXorY, largestYorZ, largestZorX;

    xx = tfrm.getCol0().getX();
    yx = tfrm.getCol0().getY();
    zx = tfrm.getCol0().getZ();
    xy = tfrm.getCol1().getX();
    yy = tfrm.getCol1().getY();
    zy = tfrm.getCol1().getZ();
    xz = tfrm.getCol2().getX();
    yz = tfrm.getCol2().getY();
    zz = tfrm.getCol2().getZ();

    trace = ( ( xx + yy ) + zz );

    negTrace = ( trace < 0.0f );
    ZgtX = zz > xx;
    ZgtY = zz > yy;
    YgtX = yy > xx;
    largestXorY = ( !ZgtX || !ZgtY ) && negTrace;
    largestYorZ = ( YgtX || ZgtX ) && negTrace;
    largestZorX = ( ZgtY || !YgtX ) && negTrace;
    
    if ( largestXorY )
    {
        zz = -zz;
        xy = -xy;
    }
    if ( largestYorZ )
    {
        xx = -xx;
        yz = -yz;
    }
    if ( largestZorX )
    {
        yy = -yy;
        zx = -zx;
    }

    radicand = ( ( ( xx + yy ) + zz ) + 1.0f );
    scale = ( 0.5f * ( 1.0f / sqrtf( radicand ) ) );

    tmpx = ( ( zy - yz ) * scale );
    tmpy = ( ( xz - zx ) * scale );
    tmpz = ( ( yx - xy ) * scale );
    tmpw = ( radicand * scale );
    qx = tmpx;
    qy = tmpy;
    qz = tmpz;
    qw = tmpw;

    if ( largestXorY )
    {
        qx = tmpw;
        qy = tmpz;
        qz = tmpy;
        qw = tmpx;
    }
    if ( largestYorZ )
    {
        tmpx = qx;
        tmpz = qz;
        qx = qy;
        qy = tmpx;
        qz = qw;
        qw = tmpz;
    }

    mX = qx;
    mY = qy;
    mZ = qz;
    mW = qw;
}

inline const Matrix3 outer( const Vector3 & tfrm0, const Vector3 & tfrm1 )
{
    return Matrix3(
        ( tfrm0 * tfrm1.getX( ) ),
        ( tfrm0 * tfrm1.getY( ) ),
        ( tfrm0 * tfrm1.getZ( ) )
    );
}

inline const Matrix4 outer( const Vector4 & tfrm0, const Vector4 & tfrm1 )
{
    return Matrix4(
        ( tfrm0 * tfrm1.getX( ) ),
        ( tfrm0 * tfrm1.getY( ) ),
        ( tfrm0 * tfrm1.getZ( ) ),
        ( tfrm0 * tfrm1.getW( ) )
    );
}

inline const Vector3 rowMul( const Vector3 & vec, const Matrix3 & mat )
{
    return Vector3(
        ( ( ( vec.getX() * mat.getCol0().getX() ) + ( vec.getY() * mat.getCol0().getY() ) ) + ( vec.getZ() * mat.getCol0().getZ() ) ),
        ( ( ( vec.getX() * mat.getCol1().getX() ) + ( vec.getY() * mat.getCol1().getY() ) ) + ( vec.getZ() * mat.getCol1().getZ() ) ),
        ( ( ( vec.getX() * mat.getCol2().getX() ) + ( vec.getY() * mat.getCol2().getY() ) ) + ( vec.getZ() * mat.getCol2().getZ() ) )
    );
}

inline const Matrix3 crossMatrix( const Vector3 & vec )
{
    return Matrix3(
        Vector3( 0.0f, vec.getZ(), -vec.getY() ),
        Vector3( -vec.getZ(), 0.0f, vec.getX() ),
        Vector3( vec.getY(), -vec.getX(), 0.0f )
    );
}

inline const Matrix3 crossMatrixMul( const Vector3 & vec, const Matrix3 & mat )
{
    return Matrix3( cross( vec, mat.getCol0() ), cross( vec, mat.getCol1() ), cross( vec, mat.getCol2() ) );
}

} // namespace Aos
} // namespace Vectormath

namespace vmath
{
    using namespace Vectormath::Aos;

    inline Point3 project(Point3 p, Point3 a, Point3 b)
    {
        float t = dot(b - a, p - a) / distSqr(a, b);
        return a + t * (b - a);
    }

    inline Matrix4 pick_box(float centerX, float centerY, float width, float height, int viewport[4])
    {
        float sx = viewport[2] / width;
        float sy = viewport[3] / height;
        float tx = (viewport[2] + 2.0f * (viewport[0] - centerX)) / width;
        float ty = (viewport[3] + 2.0f * (viewport[1] - centerY)) / height;

        Vector4 c0(sx, 0, 0, tx);
        Vector4 c1(0, sy, 0, ty);
        Vector4 c2(0, 0, 1, 0);
        Vector4 c3(0, 0, 0, 1);

        return transpose(Matrix4(c0, c1, c2, c3));
    }

    inline Point3 perspective(Vector4 v)
    {
        return Point3(v.getX() / v.getW(), v.getY() / v.getW(), v.getZ() / v.getW());
    }

    inline Vector3 perp(Vector3 a)
    {
        Vector3 c = Vector3(1, 0, 0);
        Vector3 b = cross(a, c);
        if (lengthSqr(b) < 0.01f)
        {
            c = Vector3(0, 1, 0);
            b = cross(a, c);
        }
        return b;
    }

    inline Quat rotate(Quat a, Quat b)
    {
        float w = a.getW() * b.getW() - a.getX() * b.getX() - a.getY() * b.getY() - a.getZ() * b.getZ();
        float x = a.getW() * b.getX() + a.getX() * b.getW() + a.getY() * b.getZ() - a.getZ() * b.getY();
        float y = a.getW() * b.getY() + a.getY() * b.getW() + a.getZ() * b.getX() - a.getX() * b.getZ();
        float z = a.getW() * b.getZ() + a.getZ() * b.getW() + a.getX() * b.getY() - a.getY() * b.getX();
        Quat q(x, y, z, w);
        return normalize(q);
    }
}
