#include <Rcpp.h>
#include "SparseMatrixView.hpp"
#include "ColumnView.hpp"
#include "VectorSubsetView.hpp"
#include "SkipNAVectorSubsetView.hpp"
#include "quantile.hpp"
#include "my_utils.hpp"

using namespace Rcpp;



template<typename Functor>
NumericVector reduce_matrix_double(S4 matrix, bool na_rm, Functor op){
  dgCMatrixView sp_mat = wrap_dgCMatrix(matrix);
  ColumnView cv(&sp_mat);
  std::vector<double> result;
  result.reserve(sp_mat.ncol);
  if(na_rm){
    std::transform(cv.begin(), cv.end(), std::back_inserter(result),
      [op](ColumnView::col_container col) -> double {
        SkipNAVectorSubsetView<REALSXP> values_wrapper(&col.values);
        SkipNAVectorSubsetView<INTSXP> row_indices_wrapper(&col.row_indices);
        return op(values_wrapper, row_indices_wrapper, col.number_of_zeros);
      });
  }else{
    std::transform(cv.begin(), cv.end(), std::back_inserter(result),
      [op](ColumnView::col_container col) -> double {
        return op(col.values, col.row_indices, col.number_of_zeros);
      });
  }
  return wrap(result);
}

template<typename Functor>
IntegerVector reduce_matrix_int(S4 matrix, bool na_rm, Functor op){
  dgCMatrixView sp_mat = wrap_dgCMatrix(matrix);
  ColumnView cv(&sp_mat);
  std::vector<int> result;
  result.reserve(sp_mat.ncol);
  if(na_rm){
    std::transform(cv.begin(), cv.end(), std::back_inserter(result),
                   [op](ColumnView::col_container col) -> int {
                     SkipNAVectorSubsetView<REALSXP> values_wrapper(&col.values);
                     SkipNAVectorSubsetView<INTSXP> row_indices_wrapper(&col.row_indices);
                     return op(values_wrapper, row_indices_wrapper, col.number_of_zeros);
                   });
  }else{
    std::transform(cv.begin(), cv.end(), std::back_inserter(result),
                   [op](ColumnView::col_container col) -> int {
                     return op(col.values, col.row_indices, col.number_of_zeros);
                   });
  }
  return wrap(result);
}

template<typename Functor>
LogicalVector reduce_matrix_lgl(S4 matrix, bool na_rm, Functor op){
  dgCMatrixView sp_mat = wrap_dgCMatrix(matrix);
  ColumnView cv(&sp_mat);
  std::vector<int> result;
  result.reserve(sp_mat.ncol);
  if(na_rm){
    std::transform(cv.begin(), cv.end(), std::back_inserter(result),
                   [op](ColumnView::col_container col) -> int {
                     SkipNAVectorSubsetView<REALSXP> values_wrapper(&col.values);
                     SkipNAVectorSubsetView<INTSXP> row_indices_wrapper(&col.row_indices);
                     return op(values_wrapper, row_indices_wrapper, col.number_of_zeros);
                   });
  }else{
    std::transform(cv.begin(), cv.end(), std::back_inserter(result),
                   [op](ColumnView::col_container col) -> int {
                     return op(col.values, col.row_indices, col.number_of_zeros);
                   });
  }
  return wrap(result);
}


template<typename Functor>
NumericMatrix reduce_matrix_num_matrix(S4 matrix, bool na_rm, R_len_t n_res_columns, bool transpose, Functor op){
  dgCMatrixView sp_mat = wrap_dgCMatrix(matrix);
  ColumnView cv(&sp_mat);
  std::vector<std::vector<double> > result;
  result.reserve(sp_mat.ncol);
  if(na_rm){
    std::transform(cv.begin(), cv.end(), std::back_inserter(result),
                   [op](ColumnView::col_container col) -> std::vector<double> {
                     SkipNAVectorSubsetView<REALSXP> values_wrapper(&col.values);
                     SkipNAVectorSubsetView<INTSXP> row_indices_wrapper(&col.row_indices);
                     return op(values_wrapper, row_indices_wrapper, col.number_of_zeros);
                   });
  }else{
    std::transform(cv.begin(), cv.end(), std::back_inserter(result),
                   [op](ColumnView::col_container col) -> std::vector<double> {
                     return op(col.values, col.row_indices, col.number_of_zeros);
                   });
  }
  std::vector<double> result_flat = flatten(result);
  if(transpose){
    return Rcpp::transpose(NumericMatrix(n_res_columns, sp_mat.ncol, result_flat.begin()));
  }else{
    return NumericMatrix(n_res_columns, sp_mat.ncol, result_flat.begin());
  }
}


/*---------------Simple Aggregation Functions-----------------*/

// [[Rcpp::export]]
NumericVector dgCMatrix_colSums2(S4 matrix, bool na_rm){
  return reduce_matrix_double(matrix, na_rm, [](auto values, auto row_indices, int number_of_zeros) -> double{
    return std::accumulate(values.begin(), values.end(), 0.0);
  });
}


// [[Rcpp::export]]
NumericVector dgCMatrix_colMeans2(S4 matrix, bool na_rm){
  return reduce_matrix_double(matrix, na_rm, [](auto values, auto row_indices, int number_of_zeros) -> double{
    double accum = 0.0;
    R_len_t size = number_of_zeros;
    std::for_each(values.begin(), values.end(),
      [&](auto d){
        ++size;
        accum += d;
      });
    if(NumericVector::is_na(accum)){
      return accum;
    }else if(size == 0){
      return R_NaN;
    }else{
      return accum / size;
    }
  });
}


// [[Rcpp::export]]
NumericVector dgCMatrix_colMedians(S4 matrix, bool na_rm){
  return reduce_matrix_double(matrix, na_rm, [na_rm](auto values, auto row_indices, int number_of_zeros) -> double{
    if(! na_rm){
      bool any_na = std::any_of(values.begin(), values.end(), [](const double d) -> bool {
        return NumericVector::is_na(d);
      });
      if(any_na){
        return NA_REAL;
      }
    }
    R_len_t size = values.size();
    if(number_of_zeros > size/2.0){
      // Easy escape hatch
      return 0.0;
    }
    if(size + number_of_zeros == 0){
      return NA_REAL;
    }
    return quantile_sparse(values, number_of_zeros, 0.5);
  });
}


// [[Rcpp::export]]
NumericVector dgCMatrix_colVars(S4 matrix, bool na_rm){
  return reduce_matrix_double(matrix, na_rm, [](auto values, auto row_indices, int number_of_zeros) -> double{
    double accum = 0.0;
    double accum2 = 0.0;
    R_len_t size = number_of_zeros;
    std::for_each(values.begin(), values.end(),
                  [&](auto d){
                    ++size;
                    accum += d;
                    accum2 += pow(d, 2);
                  });
    if(NumericVector::is_na(accum)){
      return accum;
    }else if(size <= 1){
      return NA_REAL;    // Yes, var(3) actually returns NA instead of NaN
    }else{
      return ((accum2) - pow(accum , 2)/ size)  / (size-1);
    }
  });
}


// [[Rcpp::export]]
NumericVector dgCMatrix_colMins(S4 matrix, bool na_rm){
  return reduce_matrix_double(matrix, na_rm, [](auto values, auto row_indices, int number_of_zeros) -> double{
    auto min = std::min_element(values.begin(), values.end(), [](double a, double b) -> bool {
      return a < b;
    });
    if(min == values.end()){
      return number_of_zeros > 0 ? 0.0 : R_PosInf;
    }else{
      return std::min(*min, 0.0);
    }
  });
}

// [[Rcpp::export]]
NumericVector dgCMatrix_colMaxs(S4 matrix, bool na_rm){
  return reduce_matrix_double(matrix, na_rm, [](auto values, auto row_indices, int number_of_zeros) -> double{
    auto max_iter = std::max_element(values.begin(), values.end(), [](double a, double b) -> bool {
      return a < b;
    });
    if(max_iter == values.end()){
      return number_of_zeros > 0 ? 0.0 : R_NegInf;
    }else{
      return std::max(*max_iter, 0.0);
    }
  });
}


// [[Rcpp::export]]
NumericVector dgCMatrix_colProds(S4 matrix, bool na_rm){
  return reduce_matrix_double(matrix, na_rm, [na_rm](auto values, auto row_indices, int number_of_zeros) -> double {
    if(na_rm){
      if(number_of_zeros > 0){
        return 0.0;
      }else{
        return std::accumulate(values.begin(), values.end(), 1.0, [](double a, double b) -> double { return a * b;});
      }
    }else{
      bool any_na = std::any_of(values.begin(), values.end(), [](const double d) -> bool {
        return NumericVector::is_na(d);
      });
      if(any_na){
        return NA_REAL;
      }else{
        if(number_of_zeros > 0){
          return 0.0;
        }else{
          return std::accumulate(values.begin(), values.end(), 1.0, [](double a, double b) { return a * b; });
        }
      }
    }
  });
}


/*---------------Simple Detect Functions-----------------*/

// [[Rcpp::export]]
IntegerVector dgCMatrix_colCounts(S4 matrix, double value, bool na_rm){
  return reduce_matrix_int(matrix, na_rm, [value, na_rm](auto values, auto row_indices, int number_of_zeros) -> int{
    if(na_rm && value == 0.0){
      return number_of_zeros;
    }else if(na_rm){
      return std::count(values.begin(), values.end(), value);
    }else{
      bool contains_na = std::any_of(values.begin(), values.end(), [](const double d) -> bool{
        return NumericVector::is_na(d);
      });
      if(! contains_na){  // No NA's in the vector
        if(value == 0.0){
          return number_of_zeros;
        }else{
          return std::count(values.begin(), values.end(), value);
        }
      }else{
        return NA_REAL;
      }
    }
  });
}

// [[Rcpp::export]]
LogicalVector dgCMatrix_colAnyNAs(S4 matrix){
  return reduce_matrix_lgl(matrix, false, [](auto values, auto row_indices, int number_of_zeros) -> int{
    bool contains_na = std::any_of(values.begin(), values.end(), [](const double d) -> bool{
      return NumericVector::is_na(d);
    });
    return contains_na;
  });
}


// [[Rcpp::export]]
LogicalVector dgCMatrix_colAnys(S4 matrix, double value, bool na_rm){
  return reduce_matrix_lgl(matrix, na_rm, [value, na_rm](auto values, auto row_indices, int number_of_zeros) -> int{
    if(na_rm && value == 0.0){
      return number_of_zeros > 0;
    }else if(! na_rm && value == 0.0){
      bool all_na = std::all_of(values.begin(), values.end(), [](const double d) -> bool {
        return NumericVector::is_na(d);
      });
      if(all_na && !values.is_empty()){
        return NA_LOGICAL;
      }else{
        return number_of_zeros > 0;
      }
    }else if(na_rm){
      return std::any_of(values.begin(), values.end(),  [value](const double d) -> bool{
        return d == value;
      });
    }else{
      bool any_na = std::any_of(values.begin(), values.end(), [](const double d) -> bool {
        return NumericVector::is_na(d);
      });
      bool found_value = std::any_of(values.begin(), values.end(),  [value](const double d) -> bool{
        return d == value;
      });
      if(any_na){
        if(found_value){
          return true;
        }else{
          return NA_LOGICAL;
        }
      }else{
        return found_value;
      }
    }
  });
}



// [[Rcpp::export]]
LogicalVector dgCMatrix_colAlls(S4 matrix, double value, bool na_rm){
  Rcpp::IntegerVector dim = matrix.slot("Dim");
  R_len_t nrows = dim[0];
  return reduce_matrix_lgl(matrix, na_rm, [value, na_rm, nrows](auto values, auto row_indices, int number_of_zeros) -> int{
    if(value == 0.0){
      if(na_rm){
        return values.is_empty();
      }else{
        if(number_of_zeros == nrows){
          return true;
        }else{
          bool all_na = std::all_of(values.begin(), values.end(), [](const double d) -> bool {
            return NumericVector::is_na(d);
          });
          if(all_na){
            return NA_LOGICAL;
          }else{
            return false;
          }
        }
      }
    }else{
      if(number_of_zeros > 0){
        return false;
      }
      if(na_rm){
        return std::all_of(values.begin(), values.end(), [value](const double d) -> bool {
          return d == value;
        });
      }else{
        bool all_equal_or_na = std::all_of(values.begin(), values.end(), [value](const double d) -> bool {
          return d == value || NumericVector::is_na(d);
        });
        bool any_na = std::any_of(values.begin(), values.end(), [](const double d) -> bool {
          return NumericVector::is_na(d);
        });
        if(! all_equal_or_na){
          return false;
        }else if(all_equal_or_na && any_na){
          return NA_LOGICAL;
        }else if(all_equal_or_na && !any_na){
          return true;
        }
      }
    }
    return false;
  });
}


/*---------------Return matrix functions-----------------*/


// [[Rcpp::export]]
NumericMatrix dgCMatrix_colQuantiles(S4 matrix, NumericVector probs, bool na_rm){
  return reduce_matrix_num_matrix(matrix, na_rm, probs.size(), true, [na_rm, probs](auto values, auto row_indices, int number_of_zeros) -> std::vector<double> {
    if(! na_rm){
      bool any_na = std::any_of(values.begin(), values.end(), [](const double d) -> bool {
        return NumericVector::is_na(d);
      });
      if(any_na){
        std::vector<double> result(probs.size(), NA_REAL);
        return result;
      }
    }
    if(values.size() + number_of_zeros == 0){
      std::vector<double> result(probs.size(), NA_REAL);
      return result;
    }
    std::vector<double> result;
    result.reserve(probs.size());
    std::transform(probs.begin(), probs.end(), back_inserter(result), [values, number_of_zeros](double prob) -> double{
      return quantile_sparse(values, number_of_zeros, prob);
    });
    return result;
  });
}




/*---------------Cumulative functions-----------------*/


// [[Rcpp::export]]
NumericMatrix dgCMatrix_colCumsums(S4 matrix){
  Rcpp::IntegerVector dim = matrix.slot("Dim");
  R_len_t nrows = dim[0];
  return reduce_matrix_num_matrix(matrix, false, nrows, false, [nrows](auto values, auto row_indices, int number_of_zeros) -> std::vector<double>{
    std::vector<double> result(nrows);
    double acc = 0;
    auto row_it = row_indices.begin();
    auto val_it = values.begin();
    auto res_it = result.begin();
    for(int i = 0; i < nrows; ++i, ++res_it){
      if(row_it != row_indices.end() && i == *row_it){
        acc += *val_it;
        ++row_it;
        ++val_it;
      }
      *res_it = acc;
    }
    return result;
  });
}



// [[Rcpp::export]]
NumericMatrix dgCMatrix_colCumprods(S4 matrix){
  Rcpp::IntegerVector dim = matrix.slot("Dim");
  R_len_t nrows = dim[0];
  return reduce_matrix_num_matrix(matrix, false, nrows, false, [nrows](auto values, auto row_indices, int number_of_zeros) -> std::vector<double>{
    std::vector<double> result(nrows);
    double acc = 1;
    auto row_it = row_indices.begin();
    auto val_it = values.begin();
    auto res_it = result.begin();
    for(int i = 0; i < nrows; ++i, ++res_it){
      if(row_it != row_indices.end() && i == *row_it){
        acc *= *val_it;
        ++row_it;
        ++val_it;
      }else{
        acc = 0 * acc;
      }
      *res_it = acc;
    }
    return result;
  });
}


// [[Rcpp::export]]
NumericMatrix dgCMatrix_colCummins(S4 matrix){
  Rcpp::IntegerVector dim = matrix.slot("Dim");
  R_len_t nrows = dim[0];
  return reduce_matrix_num_matrix(matrix, false, nrows, false, [nrows](auto values, auto row_indices, int number_of_zeros) -> std::vector<double>{
    std::vector<double> result(nrows);
    auto row_it = row_indices.begin();
    auto val_it = values.begin();
    auto res_it = result.begin();
    int i = 0;
    double acc = 0.0;
    if(row_it != row_indices.end() && i == *row_it){
      acc = *val_it;
      ++row_it;
      ++val_it;
    }else{
      acc = 0.0;
    }
    *res_it = acc;
    ++res_it;
    for(i = 1; i < nrows; ++i, ++res_it){
      if(NumericVector::is_na(acc)){
        // Do nothing it will always stay NA
      }else if(row_it != row_indices.end() && i == *row_it){
        acc = std::min(*val_it, acc);
        ++row_it;
        ++val_it;
      }else{
        acc = std::min(0.0, acc);
      }
      *res_it = acc;
    }
    return result;
  });
}


// [[Rcpp::export]]
NumericMatrix dgCMatrix_colCummaxs(S4 matrix){
  Rcpp::IntegerVector dim = matrix.slot("Dim");
  R_len_t nrows = dim[0];
  return reduce_matrix_num_matrix(matrix, false, nrows, false, [nrows](auto values, auto row_indices, int number_of_zeros) -> std::vector<double>{
    std::vector<double> result(nrows);
    auto row_it = row_indices.begin();
    auto val_it = values.begin();
    auto res_it = result.begin();
    int i = 0;
    double acc = 0.0;
    if(row_it != row_indices.end() && i == *row_it){
      acc = *val_it;
      ++row_it;
      ++val_it;
    }else{
      acc = 0.0;
    }
    *res_it = acc;
    ++res_it;
    for(i = 1; i < nrows; ++i, ++res_it){
      if(NumericVector::is_na(acc)){
        // Do nothing it will always stay NA
      }else if(row_it != row_indices.end() && i == *row_it){
        acc = std::max(*val_it, acc);
        ++row_it;
        ++val_it;
      }else{
        acc = std::max(0.0, acc);
      }
      *res_it = acc;
    }
    return result;
  });
}


