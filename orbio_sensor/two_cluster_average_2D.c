#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "app.h"

#define MAX_POINTS (NUM_CAL_SWEEPS)

/*
 * Simple 1D k-means clustering (k = 2).
 *
 * Given a list of numbers that are roughly grouped around two
 * different values, this finds the two cluster centers (averages)
 * by iteratively:
 *   1. Assigning each point to the nearer of the two current centers
 *   2. Recomputing each center as the average of its assigned points
 * until the centers stop changing (convergence).
 */

static int cluster_average(int *points, int n, int *assignment, int cluster_id)
{
    int sum = 0.0;
    int count = 0;
    for (int i = 0; i < n; i++)
    {
        if (assignment[i] == cluster_id)
        {
            sum += points[i];
            count++;
        }
    }
    return (count > 0) ? (sum / count) : 0.0;
}

#ifdef USE_1D

static int assignment[MAX_POINTS];

void two_means(int *points, int n, int *cent0, int *cent1)
{
    /* Initialize centers: just use min and max as a reasonable starting guess */
    int min_val = points[0], max_val = points[0];
    for (int i = 1; i < n; i++)
    {
        if (points[i] < min_val) min_val = points[i];
        if (points[i] > max_val) max_val = points[i];
    }
    int c0 = min_val;
    int c1 = max_val;

    int changed = 1;
    int max_iterations = 100;
    int iter = 0;

    while (changed && iter < max_iterations)
    {
        changed = 0;
        iter++;

        /* Step 1: Assign each point to the nearer center */
        for (int i = 0; i < n; i++)
        {
            int d0 = abs(points[i] - c0);
            int d1 = abs(points[i] - c1);
            int new_assignment = (d0 <= d1) ? 0 : 1;
            if (new_assignment != assignment[i])
            {
                changed = 1;
            }
            assignment[i] = new_assignment;
        }

        /* Step 2: Recompute centers as the average of assigned points */
        int new_c0 = cluster_average(points, n, assignment, 0);
        int new_c1 = cluster_average(points, n, assignment, 1);

        if (new_c0 != c0 || new_c1 != c1)
        {
            changed = 1;
        }
        c0 = new_c0;
        c1 = new_c1;
    }

    *cent0 = c0;
    *cent1 = c1;

    //printf("2-means converged after %d iteration(s) to %d and %d\n\r", iter, c0, c1);
}

#endif

/*
 * Simple 2D k-means clustering (k = 2).
 *
 * Same algorithm as two_means() above, but points now have an X and
 * a Y coordinate (given as two parallel arrays). Distance is computed
 * as squared Euclidean distance -- squared, rather than true distance,
 * so everything stays in integer arithmetic (no sqrt/float needed).
 * Comparing squared distances gives the same nearer/farther result as
 * comparing true distances.
 */

static int assignment_2D[MAX_POINTS];

void two_means_2D(int *Xpoints, int *Ypoints, int n, int *Xcent0, int *Ycent0, int *Xcent1, int *Ycent1)
{
    /*
     * Initialize centers: use the points with min X and max X as a
     * reasonable starting guess (analogous to the 1D min/max heuristic).
     * NOTE: this assumes the two clusters are reasonably separable along
     * the X axis. If your clusters separate mainly along Y instead, swap
     * this to scan Ypoints, or use the pair of mutually-farthest points.
     */
    int min_idx = 0, max_idx = 0;
    for (int i = 1; i < n; i++)
    {
        if (Xpoints[i] < Xpoints[min_idx]) min_idx = i;
        if (Xpoints[i] > Xpoints[max_idx]) max_idx = i;
    }
    int c0x = Xpoints[min_idx], c0y = Ypoints[min_idx];
    int c1x = Xpoints[max_idx], c1y = Ypoints[max_idx];

    /*
     * Reset assignment state for this call -- see the matching comment in
     * two_means() above. Same static-array-carryover risk applies here.
     */
    for (int i = 0; i < n; i++)
    {
        assignment_2D[i] = -1;
    }

    int changed = 1;
    int max_iterations = 100;
    int iter = 0;

    while (changed && iter < max_iterations)
    {
        changed = 0;
        iter++;

        /* Step 1: Assign each point to the nearer center (squared distance) */
        for (int i = 0; i < n; i++)
        {
            long long dx0 = Xpoints[i] - c0x;
            long long dy0 = Ypoints[i] - c0y;
            long long dx1 = Xpoints[i] - c1x;
            long long dy1 = Ypoints[i] - c1y;

            long long d0 = dx0 * dx0 + dy0 * dy0;
            long long d1 = dx1 * dx1 + dy1 * dy1;

            int new_assignment = (d0 <= d1) ? 0 : 1;
            if (new_assignment != assignment_2D[i])
            {
                changed = 1;
            }
            assignment_2D[i] = new_assignment;
        }

        /* Step 2: Recompute centers as the average of assigned points */
        int new_c0x = cluster_average(Xpoints, n, assignment_2D, 0);
        int new_c0y = cluster_average(Ypoints, n, assignment_2D, 0);
        int new_c1x = cluster_average(Xpoints, n, assignment_2D, 1);
        int new_c1y = cluster_average(Ypoints, n, assignment_2D, 1);

        if (new_c0x != c0x || new_c0y != c0y || new_c1x != c1x || new_c1y != c1y)
        {
            changed = 1;
        }
        c0x = new_c0x; c0y = new_c0y;
        c1x = new_c1x; c1y = new_c1y;
    }

    *Xcent0 = c0x; *Ycent0 = c0y;
    *Xcent1 = c1x; *Ycent1 = c1y;

    //printf("2-means (2D) converged after %d iteration(s) to (%d,%d) and (%d,%d)\n\r", iter, c0x, c0y, c1x, c1y);
}

/* End of file */
