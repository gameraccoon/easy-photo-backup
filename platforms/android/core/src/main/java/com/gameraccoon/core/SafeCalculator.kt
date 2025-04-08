package com.gameraccoon.core

import uniffi.client_ffi.Calculator
import uniffi.client_ffi.ComputationResult
import uniffi.client_ffi.safeAdditionOperator

class SafeCalculator {
  // Functional core; imperative shell. This is purely internal state with an imperative API
  // wrapper.
  private var calc = Calculator()

  private val addOp = safeAdditionOperator()

  val lastValue: ComputationResult?
    get() = calc.lastResult()

  fun add(lhs: Long, rhs: Long): ComputationResult {
    calc = calc.calculate(addOp, lhs, rhs)

    // Note that it is not possible for lastResult to be anything but
    // a computed value at this point, so we can expose a nicer
    // interface to Swift consumers of the low-level library.
    return calc.lastResult()!!
  }
}
