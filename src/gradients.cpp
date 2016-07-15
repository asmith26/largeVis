// [[Rcpp::plugins(openmp)]]
// [[Rcpp::plugins(cpp11)]]
// [[Rcpp::depends(RcppArmadillo)]]
// [[Rcpp::depends(RcppProgress)]]
#include "largeVis.h"

/*
 * Efficient clamp
 */

// #ifdef __SSE2__
// #include <emmintrin.h>
// #include <x86intrin.h>
// double clamp ( double val, double minval, double maxval ){
//   __builtin_ia32_storesd( &val, __builtin_ia32_minsd( __builtin_ia32_maxsd(__builtin_ia32_loadupd(&val),
//                                                                            __builtin_ia32_loadupd(&minval)),
//                                                                            __builtin_ia32_loadupd(&maxval)));
//   return val;
// }
// #else
// inline double max(double val, double maxval) {
//   return (val > maxval) ? maxval : val;
// }
// inline double min(double val, double minval) {
//   return (val < minval) ? minval : val;
// }
// double clamp(double val, double cap) {
//   return min(max(val, -cap), cap);
// }
// #endif




// Parent class
Gradient::Gradient(const double g,
                   const int d) {
  gamma = g;
  D = d;
  cap = 5;
}
void Gradient::positiveGradient(const double* i,
                                const double* j,
                                double* holder) const {
  const double dist_squared = distAndVector(i, j, holder);
  _positiveGradient(dist_squared, holder);
}
void Gradient::negativeGradient(const double* i,
                                const double* k,
                                double* holder) const {
  const double dist_squared = distAndVector(i, k, holder);
  _negativeGradient(dist_squared, holder);
}
// Copies the vector sums into a vector while it computes distance^2 -
// useful in calculating the gradients during SGD
inline double Gradient::distAndVector(const double *x_i,
                                      const double *x_j,
                                      double *output) const {
  double cnt = 0;
  for (int d = 0; d < D; d++) {
    double t = x_i[d] - x_j[d];
    output[d] = t;
    cnt += t * t;
  }
  return cnt;
}

inline double Gradient::clamp(double val) const {
  return fmin(fmax(val, -cap), cap);
}

inline void Gradient::multModify(double *col, int D, double adj) const {
  for (int i = 0; i != D; i++) col[i] = clamp(col[i] * adj);
}

/*
 * Generalized gradient with an alpha parameter
 */
AlphaGradient::AlphaGradient(const double a,
                             const double g,
                             const int d) : Gradient(g, d) {
  alpha = a;
  alphagamma = a * g * 2;
  twoalpha = alpha * -2;
}
void AlphaGradient::_positiveGradient(const double dist_squared,
                                      double* holder) const {
  const double grad = twoalpha / (1 + alpha * dist_squared);
  multModify(holder, D, grad);
}
void AlphaGradient::_negativeGradient(const double dist_squared,
                                      double* holder) const {
  const double adk = alpha * dist_squared;
  double grad = alphagamma / (dist_squared * (adk + 1));
  multModify(holder, D, grad);
}

/*
 * Optimized gradient for alpha == 1
 */
AlphaOneGradient::AlphaOneGradient(const double g,
                                   const int d) : AlphaGradient(1, g, d) {
  this -> gamma = 2 * g;
}
void AlphaOneGradient::_positiveGradient(const double dist_squared,
                                         double* holder) const {
  const double grad = - 2 / (1 + dist_squared);
  multModify(holder, D, grad);
}
void AlphaOneGradient::_negativeGradient(const double dist_squared,
                                         double* holder) const {
  double grad = gamma / (1 + dist_squared) / (0.1 + dist_squared);
  multModify(holder, D, grad);
}

/*
 * Alternative probabilistic function (sigmoid)
 */

ExpGradient::ExpGradient(const double g,
                         const int d) : Gradient(g, d) {
  gammagamma = g * g;
  cap = gamma;
}
void ExpGradient::_positiveGradient(const double dist_squared,
                                   double* holder) const {
  const double expsq = exp(dist_squared);
  const double grad = (dist_squared > 4) ? -1 :
                                           -(expsq / (expsq + 1));
  multModify(holder, D, grad);
}
void ExpGradient::_negativeGradient(const double dist_squared,
                                   double* holder) const {
  const double grad = (dist_squared > gammagamma) ? 0 :
                                                    gamma / (1 + exp(dist_squared));
  multModify(holder, D, grad);
}

// /*
//  * Class for a gradient found using a pre-calculated lookup table
//  */
// LookupGradient::LookupGradient(double alpha,
//                                double gamma,
//                                int d,
//                                double bound,
//                                int steps) : Gradient(gamma, d) {
//   this -> alpha = alpha;
//   twoalpha = alpha * -2;
//   this -> bound = bound;
//   this -> steps = steps;
//   this -> boundsteps = (steps - 1) / bound;
//   negativeLookup = new double[steps];
//   double step = bound / steps;
//   double distsquared = 0;
//   for (int i = 1; i != steps; i++) {
//     distsquared +=step;
//     const double adk = alpha * distsquared;
//     negativeLookup[i] = 2 * alpha * gamma / (distsquared * (adk + 1));
//   }
//   negativeLookup[0] = negativeLookup[1];
// }
//
// inline int fastround(double r) {
//   return r + 0.5;
// }
//
// double LookupGradient::Lookup(const double dist_squared,
//                               double* table) const {
//   return (dist_squared > bound) ? table[steps - 1] :
//   table[fastround(dist_squared * boundsteps)];
// }
//
// void LookupGradient::_positiveGradient(const double dist_squared,
//                                        double* holder) const {
//   const double grad = twoalpha / (1 + alpha * dist_squared);
//   multModify(holder, D, grad);
// }
// bool LookupGradient::_negativeGradient(const double dist_squared,
//                                        double* holder) const {
//   if (dist_squared == 0) return true;
//   double grad = Lookup(dist_squared, negativeLookup);
//   multModify(holder, D, grad);
//   return false;
// }
//
