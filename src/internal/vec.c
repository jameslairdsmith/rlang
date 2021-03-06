#include <rlang.h>


static
bool has_correct_length(sexp* x, r_ssize n) {
  return n < 0 || r_length(x) == n;
}

bool r_is_atomic(sexp* x, r_ssize n) {
  switch(r_typeof(x)) {
  case r_type_logical:
  case r_type_integer:
  case r_type_double:
  case r_type_complex:
  case r_type_character:
  case RAWSXP:
    return has_correct_length(x, n);
  default:
    return false;
  }
}

bool r_is_vector(sexp* x, r_ssize n) {
  switch(r_typeof(x)) {
  case r_type_logical:
  case r_type_integer:
  case r_type_double:
  case r_type_complex:
  case r_type_character:
  case RAWSXP:
  case VECSXP:
    return has_correct_length(x, n);
  default:
    return false;
  }
}

bool r_is_logical(sexp* x, r_ssize n) {
  return r_typeof(x) == r_type_logical && has_correct_length(x, n);
}

bool r_is_finite(sexp* x) {
  r_ssize n = r_length(x);

  switch(r_typeof(x)) {
  case r_type_integer: {
    const int* p_x = r_int_deref_const(x);
    for (r_ssize i = 0; i < n; ++i) {
      if (p_x[i] == r_ints_na) {
        return false;
      }
    }
    break;
  }
  case r_type_double: {
    const double* p_x = r_dbl_deref_const(x);
    for (r_ssize i = 0; i < n; ++i) {
      if (!isfinite(p_x[i])) {
        return false;
      }
    }
    break;
  }
  case r_type_complex: {
    const r_complex_t* p_x = r_cpl_deref_const(x);
    for (r_ssize i = 0; i < n; ++i) {
      if (!isfinite(p_x[i].r) || !isfinite(p_x[i].i)) {
        return false;
      }
    }
    break;
  }
  default:
    r_abort("Internal error: expected a numeric vector");
  }

  return true;
}
bool r_is_integer(sexp* x, r_ssize n, int finite) {
  if (r_typeof(x) != r_type_integer || !has_correct_length(x, n)) {
    return false;
  }
  if (finite >= 0 && (bool) finite != r_is_finite(x)) {
    return false;
  }
  return true;
}
bool r_is_double(sexp* x, r_ssize n, int finite) {
  if (r_typeof(x) != r_type_double || !has_correct_length(x, n)) {
    return false;
  }
  if (finite >= 0 && (bool) finite != r_is_finite(x)) {
    return false;
  }
  return true;
}

// Allow integers up to 2^52, same as R_XLEN_T_MAX when long vector
// support is enabled
#define RLANG_MAX_DOUBLE_INT 4503599627370496

bool r_is_integerish(sexp* x, r_ssize n, int finite) {
  if (r_typeof(x) == r_type_integer) {
    return r_is_integer(x, n, finite);
  }
  if (r_typeof(x) != r_type_double || !has_correct_length(x, n)) {
    return false;
  }

  r_ssize actual_n = r_length(x);
  const double* p_x = r_dbl_deref_const(x);
  bool actual_finite = true;

  for (r_ssize i = 0; i < actual_n; ++i) {
    double elt = p_x[i];

    if (!isfinite(elt)) {
      actual_finite = false;
      continue;
    }

    if (elt > RLANG_MAX_DOUBLE_INT) {
      return false;
    }

    // C99 guarantees existence of the int_least_N_t types, even on
    // machines that don't support arithmetic on width N:
    if (elt != (int_least64_t) elt) {
      return false;
    }
  }

  if (finite >= 0 && actual_finite != (bool) finite) {
    return false;
  }

  return true;
}

#undef RLANG_MAX_DOUBLE_INT

bool r_is_character(sexp* x, r_ssize n) {
  return r_typeof(x) == r_type_character && has_correct_length(x, n);
}
bool r_is_raw(sexp* x, r_ssize n) {
  return r_typeof(x) == r_type_raw && has_correct_length(x, n);
}


// Coercion ----------------------------------------------------------

sexp* rlang_vec_coercer(sexp* dest) {
  switch(r_typeof(dest)) {
  case r_type_logical: return rlang_ns_get("as_logical");
  case r_type_integer: return rlang_ns_get("as_integer");
  case r_type_double: return rlang_ns_get("as_double");
  case r_type_complex: return rlang_ns_get("as_complex");
  case r_type_character: return rlang_ns_get("as_character");
  case RAWSXP: return rlang_ns_get("as_bytes");
  default: r_abort("No coercion implemented for `%s`", Rf_type2str(r_typeof(dest)));
  }
}

void r_vec_poke_coerce_n(sexp* x, r_ssize offset,
                         sexp* y, r_ssize from, r_ssize n) {
  if (r_typeof(y) == r_typeof(x)) {
    r_vec_poke_n(x, offset, y, from, n);
    return ;
  }
  if (r_is_object(y)) {
    r_abort("Can't splice S3 objects");
  }

  // FIXME: This callbacks to rlang R coercers with an extra copy.
  sexp* coercer = rlang_vec_coercer(x);
  sexp* call = KEEP(Rf_lang2(coercer, y));
  sexp* coerced = KEEP(r_eval(call, R_BaseEnv));

  r_vec_poke_n(x, offset, coerced, from, n);
  FREE(2);
}

void r_vec_poke_coerce_range(sexp* x, r_ssize offset,
                             sexp* y, r_ssize from, r_ssize to) {
  r_vec_poke_coerce_n(x, offset, y, from, to - from + 1);
}
