#ifndef DIST_H
#define DIST_H

#include <math.h>

static double haversine(double alat, double alon, double blat, 
    double blon)
{
    double φ1 = alat * (M_PI/180); // φ, λ in radians
    double φ2 = blat * (M_PI/180);
    double Δφ = (blat-alat) * (M_PI/180);
    double Δλ = (blon-alon) * (M_PI/180);
	double Δφ2 = sin(Δφ/2);
	double Δλ2 = sin(Δλ/2);
    double a = Δφ2 * Δφ2 + cos(φ1) * cos(φ2) * Δλ2 * Δλ2;
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    double d = c * 6371e3; // in metres
    return d;
}

// distance on the unit sphere computed using Haversine formula
static double haversine_unit_rad(double φa, double λa, double φb, double λb) {
    if (φa == φb && λa == λb) {
        return 0;
    }
    double Δφ = φa - φb;
    double Δλ = λa - λb;
    double sinΔφ = sin(Δφ / 2);
    double sinΔλ = sin(Δλ / 2);
    double cosφa = cos(φa);
    double cosφb = cos(φb);
    return 2 * asin(sqrt(sinΔφ*sinΔφ+sinΔλ*sinΔλ*cosφa*cosφb));
}

// Algorithm from:
// Schubert, E., Zimek, A., & Kriegel, H.-P. (2013).
// Geodetic Distance Queries on R-Trees for Indexing Geographic Data.
// Lecture Notes in Computer Science, 146–164.
// doi:10.1007/978-3-642-40235-7_9
static double point_rect_dist_geodetic_rad(double φq, double λq, double φl, 
	double λl, double φh, double λh)
{
    double twoΠ  = 2 * M_PI;
    double halfΠ = M_PI / 2;

    // Simple case, point or invalid rect
    if (φl >= φh && λl >= λh) {
        return haversine_unit_rad(φl, λl, φq, λq);
    }

    if (λl <= λq && λq <= λh) {
        // q is between the bounding meridians of r
        // hence, q is north, south or within r
        if (φl <= φq && φq <= φh) { // Inside
            return 0;
        }

        if (φq < φl) { // South
            return φl - φq;
        }

        return φq - φh; // North
    }

    // determine if q is closer to the east or west edge of r to select edge for
    // tests below
    double Δλe = λl - λq;
    double Δλw = λq - λh;
    if (Δλe < 0) {
        Δλe += twoΠ;
    }
    if (Δλw < 0) {
        Δλw += twoΠ;
    }
    double Δλ;    // distance to closest edge
    double λedge; // longitude of closest edge
    if (Δλe <= Δλw) {
        Δλ = Δλe;
        λedge = λl;
    } else {
        Δλ = Δλw;
        λedge = λh;
    }

    double sinΔλ = sin(Δλ);
    double cosΔλ = cos(Δλ);
    double tanφq = tan(φq);

    if (Δλ >= halfΠ) {
        // If Δλ > 90 degrees (1/2 pi in radians) we're in one of the corners
        // (NW/SW or NE/SE depending on the edge selected). Compare against the
        // center line to decide which case we fall into
        double φmid = (φh + φl) / 2;
        if (tanφq >= tan(φmid)*cosΔλ) {
            return haversine_unit_rad(φq, λq, φh, λedge); // North corner
        }
        return haversine_unit_rad(φq, λq, φl, λedge); // South corner
    }

    if (tanφq >= tan(φh)*cosΔλ) {
        return haversine_unit_rad(φq, λq, φh, λedge); // North corner
    }

    if (tanφq <= tan(φl)*cosΔλ) {
        return haversine_unit_rad(φq, λq, φl, λedge); // South corner
    }

    // We're to the East or West of the rect, compute distance using cross-track
    // Note that this is a simplification of the cross track distance formula
    // valid since the track in question is a meridian.
    return asin(cos(φq) * sinΔλ);
}

static double point_rect_dist(double lat, double lon, double minlat,
    double minlon, double maxlat, double maxlon)
{
    return point_rect_dist_geodetic_rad(
        lat*(M_PI/180), lon*(M_PI/180),
        minlat*(M_PI/180), minlon*(M_PI/180),
        maxlat*(M_PI/180), maxlon*(M_PI/180)
    );
}


#endif
